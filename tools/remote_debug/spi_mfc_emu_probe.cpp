// Hardware diagnostic only: run with the RFID app stopped to own SPI exclusively.
#include "nfc_spi_device.hpp"
#include "nfc_mfc_emulator.hpp"
#include <fstream>
#include <iostream>
#include <signal.h>
extern "C" const char *hal_path_nfc_log_dir() { return "/tmp/rfid-spi-probe-logs"; }
static volatile sig_atomic_t stop=0;
static void finish(int) { stop=1; }
int main(int argc,char **argv) {
    if(argc!=2) { std::cerr<<"usage: spi_mfc_emu_probe dump.json\n";return 2; }
    signal(SIGINT,finish);signal(SIGTERM,finish);
    nlohmann::json json;std::ifstream(argv[1])>>json;
    std::vector<uint8_t> memory;
    for(unsigned block=0;block<64;++block) {
        char index[4];snprintf(index,sizeof(index),"%02u",block);
        const auto hex=json.at("blocks").at(index).get<std::string>();
        if(hex.size()!=32) return 2;
        for(unsigned i=0;i<32;i+=2) memory.push_back(std::stoul(hex.substr(i,2),nullptr,16));
    }
    // APPLaunch turns off the external rail when RFID exits. The isolated
    // probe must acquire it too, and restore the preceding state on exit.
    struct RailGuard {
        int previous=0;
        RailGuard() { std::ifstream("/sys/class/leds/ext_5v_out/brightness")>>previous; nfc_app::NfcTransportFactory::set_ext_5v(true); std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
        ~RailGuard() { nfc_app::NfcTransportFactory::set_ext_5v(previous!=0); }
    } rail;
    nfc_app::NfcSpiDevice device;std::string error;
    if(!device.open("/dev/spidev0.2",&error) || !device.start_listener_a({memory[0],memory[1],memory[2],memory[3]},4,8,&error)) { std::cerr<<error<<'\n';return 1; }
    nfc_app::MfcEmulator mfc;
    const uint32_t uid=uint32_t(memory[0])<<24|uint32_t(memory[1])<<16|uint32_t(memory[2])<<8|memory[3];
    while(!stop) {
        std::vector<uint8_t> frame,response;size_t bits=0;
        if(!device.poll_listener_frame(frame,20)) continue;
        if(!device.listener_mfclassic_raw_rx()) mfc.reset();
        if(mfc.state()!=nfc_app::MfcEmulator::State::Idle) {
            const bool handled=mfc.handle(frame,memory,response,bits);
            const bool sent=handled && device.send_listener_raw_frame(response,bits);
            for(auto x:frame) std::fprintf(stderr,"%02X",x); std::fprintf(stderr," -> "); for(auto x:response) std::fprintf(stderr,"%02X",x); std::fprintf(stderr," ");
            std::cerr<<"reply state="<<int(mfc.state())<<" bits="<<bits<<" sent="<<sent<<'\n';
        } else if(frame.size()>=2 && (frame[0]==0x60 || frame[0]==0x61)) {
            if(mfc.challenge(frame[1],frame[0]==0x61,uid,0x175B6804,memory,response)) {
                device.listener_set_mfclassic_auth_rx(true);
                device.send_listener_frame(response.data(),response.size(),false);
            }
        }
    }
    device.stop_listener();
}
