#include "hal_screenshot.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

extern "C" unsigned lodepng_encode24(
    unsigned char **output, size_t *output_size,
    const unsigned char *image, unsigned width, unsigned height);
extern "C" void lv_free(void *pointer);

static bool ensure_directory(const char *dir)
{
    if (!dir || !dir[0]) return false;

    char path[512];
    if (snprintf(path, sizeof(path), "%s", dir) >= static_cast<int>(sizeof(path))) return false;
    for (char *cursor = path + 1; *cursor; ++cursor) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(path, 0755) != 0 && errno != EEXIST) return false;
        *cursor = '/';
    }
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

static const char *resolve_output_directory(const char *requested, char *fallback, size_t size)
{
    if (ensure_directory(requested) && access(requested, W_OK) == 0) return requested;

    const char *home = getenv("HOME");
    if (!home || !home[0]) home = "/home/pi";
    snprintf(fallback, size, "%s/rfid/screenshots", home);
    return ensure_directory(fallback) ? fallback : nullptr;
}

int hal_screenshot_save(const char *dir, char *saved_path, size_t saved_path_size)
{
    if (saved_path && saved_path_size) saved_path[0] = '\0';
    const char *fbdev = getenv("LV_FBDEV");
    if (!fbdev || !fbdev[0]) fbdev = getenv("APPLAUNCH_LINUX_FBDEV_DEVICE");
    if (!fbdev || !fbdev[0]) fbdev = "/dev/fb0";

    int fd = open(fbdev, O_RDONLY);
    if (fd < 0) return -1;

    struct fb_var_screeninfo variable_info;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &variable_info) < 0) {
        close(fd);
        return -2;
    }

    const int width = static_cast<int>(variable_info.xres);
    const int height = static_cast<int>(variable_info.yres);
    const int bits_per_pixel = static_cast<int>(variable_info.bits_per_pixel);
    int line_length = width * (bits_per_pixel / 8);
    struct fb_fix_screeninfo fixed_info;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fixed_info) == 0)
        line_length = static_cast<int>(fixed_info.line_length);

    const size_t framebuffer_size = static_cast<size_t>(line_length) * height;
    void *framebuffer = mmap(nullptr, framebuffer_size, PROT_READ, MAP_SHARED, fd, 0);
    if (framebuffer == MAP_FAILED) {
        close(fd);
        return -3;
    }

    char fallback_dir[512];
    const char *output_dir = resolve_output_directory(dir, fallback_dir, sizeof(fallback_dir));
    if (!output_dir) {
        munmap(framebuffer, framebuffer_size);
        close(fd);
        return -4;
    }

    const time_t now = time(nullptr);
    struct tm local_time;
    localtime_r(&now, &local_time);
    char filename[640];
    snprintf(filename, sizeof(filename), "%s/scr_%04d%02d%02d_%02d%02d%02d.png",
             output_dir, local_time.tm_year + 1900, local_time.tm_mon + 1,
             local_time.tm_mday, local_time.tm_hour, local_time.tm_min,
             local_time.tm_sec);

    if (width <= 0 || height <= 0 || (bits_per_pixel != 16 && bits_per_pixel != 32)) {
        munmap(framebuffer, framebuffer_size);
        close(fd);
        return -5;
    }

    std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y) {
        const uint8_t *row = static_cast<const uint8_t *>(framebuffer) + y * line_length;
        for (int x = 0; x < width; ++x) {
            uint8_t red = 0;
            uint8_t green = 0;
            uint8_t blue = 0;
            if (bits_per_pixel == 16) {
                const uint16_t pixel = reinterpret_cast<const uint16_t *>(row)[x];
                red = static_cast<uint8_t>(((pixel >> 11) & 0x1F) << 3);
                green = static_cast<uint8_t>(((pixel >> 5) & 0x3F) << 2);
                blue = static_cast<uint8_t>((pixel & 0x1F) << 3);
            } else if (bits_per_pixel == 32) {
                const uint32_t pixel = reinterpret_cast<const uint32_t *>(row)[x];
                red = static_cast<uint8_t>((pixel >> variable_info.red.offset) & 0xFF);
                green = static_cast<uint8_t>((pixel >> variable_info.green.offset) & 0xFF);
                blue = static_cast<uint8_t>((pixel >> variable_info.blue.offset) & 0xFF);
            }
            const size_t offset = (static_cast<size_t>(y) * width + x) * 3;
            rgb[offset] = red;
            rgb[offset + 1] = green;
            rgb[offset + 2] = blue;
        }
    }

    munmap(framebuffer, framebuffer_size);
    close(fd);
    unsigned char *png_data = nullptr;
    size_t png_size = 0;
    const unsigned png_error = lodepng_encode24(
        &png_data, &png_size, rgb.data(),
        static_cast<unsigned>(width), static_cast<unsigned>(height));
    if (png_error != 0) {
        std::fprintf(stderr, "[SCREENSHOT] PNG encode failed: %u\n", png_error);
        return -6;
    }

    FILE *file = fopen(filename, "wb");
    if (!file) {
        lv_free(png_data);
        return -7;
    }
    const bool wrote = fwrite(png_data, 1, png_size, file) == png_size;
    const bool closed = fclose(file) == 0;
    lv_free(png_data);
    if (!wrote || !closed) return -8;

    if (saved_path && saved_path_size)
        snprintf(saved_path, saved_path_size, "%s", filename);
    printf("[SCREENSHOT] Saved: %s (%dx%d %dbpp)\n",
           filename, width, height, bits_per_pixel);
    fflush(stdout);
    return 0;
}
