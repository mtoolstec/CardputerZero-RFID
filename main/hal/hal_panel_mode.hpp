#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <vector>

namespace rfid_board {

// MIPI DBI firmware: 16-byte header followed by command, length, payload.
// Read the board's setting instead of assuming INVON means inverted colours.
inline int panel_inversion_command(const std::vector<unsigned char> &firmware)
{
    static const unsigned char header[] = {'M','I','P','I',' ','D','B','I',0,0,0,0,0,0,0,1};
    if (firmware.size() < sizeof(header) ||
        !std::equal(std::begin(header), std::end(header), firmware.begin())) return -1;
    int inversion = -1;
    for (size_t offset = sizeof(header); offset < firmware.size();) {
        if (firmware.size() - offset < 2) return -1;
        const unsigned command = firmware[offset++];
        const unsigned length = firmware[offset++];
        if (length > firmware.size() - offset) return -1;
        if (length == 0 && (command == 0x20 || command == 0x21)) inversion = command;
        offset += length;
    }
    return inversion;
}

inline bool restore_panel_mode()
{
#if defined(__linux__)
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    std::ifstream driver("/sys/kernel/debug/dri/spi0.0/name");
    std::string name;
    std::getline(driver, name);
    if (name.find("panel-mipi-dbi") != 0) return false;
    std::ifstream file("/lib/firmware/cardputerzero,st7789v_lcd.bin", std::ios::binary);
    if (!file) return false;
    const std::vector<unsigned char> firmware((std::istreambuf_iterator<char>(file)), {});
    const int command = panel_inversion_command(firmware);
    if (command < 0) return false;
    const char *value = command == 0x21 ? "21\n" : "20\n";
    bool ok = false;
    if (FILE *output = std::fopen("/sys/kernel/debug/dri/spi0.0/command", "w")) {
        ok = std::fputs(value, output) >= 0;
        ok = std::fclose(output) == 0 && ok;
    } else {
        // APPLaunch runs as pi on stock images; reuse its existing sudo policy.
        // Fixed arguments only, bounded and noninteractive. No device data enters a shell.
        const char *restore = command == 0x21 ?
            "printf '21\\n' | /usr/bin/timeout 2s /usr/bin/sudo -n /usr/bin/tee /sys/kernel/debug/dri/spi0.0/command >/dev/null" :
            "printf '20\\n' | /usr/bin/timeout 2s /usr/bin/sudo -n /usr/bin/tee /sys/kernel/debug/dri/spi0.0/command >/dev/null";
        ok = std::system(restore) == 0;
    }
    std::fprintf(stderr, "[RFID] Factory panel mode 0x%02X: %s\n", command, ok ? "restored" : "failed");
    return ok;
#else
    return false;
#endif
}

struct PanelModeRecovery {
    bool enabled = true;
    ~PanelModeRecovery() { if (enabled) restore_panel_mode(); }
};

} // namespace rfid_board
