#include "hal_settings.h"

#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------
void hal_time_str(char *buf, int buf_size)
{
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    if (tm_info) {
        strftime(buf, buf_size, "%H:%M", tm_info);
    } else {
        snprintf(buf, buf_size, "--:--");
    }
}

// ---------------------------------------------------------------------------
// Battery
// ---------------------------------------------------------------------------
static int find_battery_supply(char *out, size_t out_size)
{
    const char *base = "/sys/class/power_supply";
    DIR *dir = opendir(base);
    if (!dir) return 0;

    char fallback[320] = {0};
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char supply[320];
        char capacity[384];
        snprintf(supply, sizeof(supply), "%s/%s", base, entry->d_name);
        snprintf(capacity, sizeof(capacity), "%s/capacity", supply);
        if (access(capacity, R_OK) != 0) continue;

        if (strstr(entry->d_name, "bq27220") || strstr(entry->d_name, "bq27")) {
            snprintf(out, out_size, "%s", supply);
            closedir(dir);
            return 1;
        }
        if (!fallback[0]) snprintf(fallback, sizeof(fallback), "%s", supply);
    }
    closedir(dir);

    if (!fallback[0]) return 0;
    snprintf(out, out_size, "%s", fallback);
    return 1;
}

hal_battery_info_t hal_battery_read(void)
{
    hal_battery_info_t info = {0};
    char supply[320] = {0};
    if (!find_battery_supply(supply, sizeof(supply))) return info;

    char path[384];
    snprintf(path, sizeof(path), "%s/capacity", supply);
    FILE *f = fopen(path, "r");
    if (f) {
        int soc = 0;
        if (fscanf(f, "%d", &soc) == 1) {
            info.soc   = soc;
            info.valid = 1;
        }
        fclose(f);
    }
    return info;
}

// ---------------------------------------------------------------------------
// Backlight (stub)
// ---------------------------------------------------------------------------
int hal_backlight_read(void)  { return 100; }
int hal_backlight_max(void)   { return 100; }
int hal_backlight_write(int v){ (void)v; return 0; }

// ---------------------------------------------------------------------------
// Volume (stub)
// ---------------------------------------------------------------------------
int hal_volume_read(void)       { return 0; }
int hal_volume_write(int v)     { (void)v; return 0; }

// ---------------------------------------------------------------------------
// WiFi status. RFID does not manage connections, but mirrors APPLaunch state.
// ---------------------------------------------------------------------------
hal_wifi_status_t hal_wifi_get_status(void)
{
    hal_wifi_status_t ws = {0};
    char line[256];
    FILE *pipe = popen(
        "nmcli -t -f DEVICE,TYPE,STATE,CONNECTION dev status 2>/dev/null", "r");
    if (!pipe) return ws;

    while (fgets(line, sizeof(line), pipe)) {
        line[strcspn(line, "\r\n")] = '\0';
        char device[64] = {0};
        char type[32] = {0};
        char state[64] = {0};
        char connection[WIFI_SSID_MAX] = {0};
        if (sscanf(line, "%63[^:]:%31[^:]:%63[^:]:%63[^\n]",
                   device, type, state, connection) == 4 &&
            strcmp(type, "wifi") == 0 && strcmp(state, "connected") == 0) {
            ws.connected = 1;
            snprintf(ws.ssid, sizeof(ws.ssid), "%s", connection);
            break;
        }
    }
    pclose(pipe);
    if (!ws.connected) return ws;

    pipe = popen("nmcli -t -f IN-USE,SIGNAL dev wifi list 2>/dev/null", "r");
    if (pipe) {
        while (fgets(line, sizeof(line), pipe)) {
            if (line[0] == '*' && line[1] == ':') {
                ws.signal = atoi(line + 2);
                break;
            }
        }
        pclose(pipe);
    }

    pipe = popen("ip -4 -o addr show wlan0 2>/dev/null", "r");
    if (pipe) {
        if (fgets(line, sizeof(line), pipe)) {
            char *inet = strstr(line, "inet ");
            if (inet) {
                inet += 5;
                char *end = strchr(inet, '/');
                if (end) *end = '\0';
                snprintf(ws.ip, sizeof(ws.ip), "%s", inet);
            }
        }
        pclose(pipe);
    }
    return ws;
}
int hal_wifi_scan(hal_wifi_ap_t *out, int max_aps) { (void)out; (void)max_aps; return 0; }
int hal_wifi_connect(const char *ssid, const char *password) { (void)ssid; (void)password; return -1; }
int hal_wifi_disconnect(void) { return 0; }

// ---------------------------------------------------------------------------
// Bluetooth (stub)
// ---------------------------------------------------------------------------
hal_bt_status_t hal_bt_get_status(void) { hal_bt_status_t s = {0}; return s; }
int hal_bt_set_power(int on)              { (void)on; return 0; }
int hal_bt_scan(hal_bt_device_t *out, int max_devices) { (void)out; (void)max_devices; return 0; }
