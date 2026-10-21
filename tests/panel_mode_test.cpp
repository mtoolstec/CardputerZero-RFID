#include "../main/hal/hal_panel_mode.hpp"
#include <cassert>
#include <iostream>
int main() {
    const std::vector<unsigned char> header = {'M','I','P','I',' ','D','B','I',0,0,0,0,0,0,0,1};
    auto firmware = header;
    firmware.insert(firmware.end(), {0xC4,1,0x20,0x21,0,0x29,0});
    assert(rfid_board::panel_inversion_command(firmware) == 0x21);
    firmware.insert(firmware.end(), {0x20,0});
    assert(rfid_board::panel_inversion_command(firmware) == 0x20);
    firmware.push_back(0x21);
    assert(rfid_board::panel_inversion_command(firmware) == -1);
    firmware = header; firmware.insert(firmware.end(), {0xC4,2,0x21});
    assert(rfid_board::panel_inversion_command(firmware) == -1);
    assert(rfid_board::panel_inversion_command(header) == -1);
    assert(rfid_board::panel_inversion_command({0x21,0}) == -1);
    std::cout << "PASS: board firmware mode, payload bytes, truncation and bad header\n";
}
