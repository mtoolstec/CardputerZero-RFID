// Standalone regression: c++ -std=c++17 -I main/ui/components/nfc -I ../CardputerZero-Emulator/vendor tools/test_pn532_sniffer.cpp -o /tmp/test-sniffer
#include "nfc_protocol.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace nfc_app;
static std::vector<uint8_t> response(uint8_t cmd, const std::vector<uint8_t>& payload) {
    auto f = Pn532FrameCodec::build_command(cmd, payload);
    f[5] = 0xD5; --f[f.size()-2]; return f;
}
class TestTransport : public INfcTransport {
public:
    std::vector<uint8_t> bytes; size_t offset = 0;
    bool open(const TransportEndpoint&, std::string*) override { return true; }
    void close() override {}
    bool is_open() const override { return true; }
    TransportEndpoint endpoint() const override { return {}; }
    ssize_t write_bytes(const uint8_t*, size_t n, std::string*) override { return n; }
    ssize_t read_bytes(uint8_t* out, size_t n, int, std::string*) override {
        n = std::min({n, size_t(17), bytes.size()-offset});
        std::memcpy(out, bytes.data()+offset, n); offset += n; return n;
    }
    void append(const std::vector<uint8_t>& f) { bytes.insert(bytes.end(), f.begin(), f.end()); }
};
int main() {
    TestTransport t;
    t.append(response(0x21, {1,0})); // delayed previous log
    t.append(response(0x20, {8})); // unrelated firmware status
    t.append(response(0xAD, {0}));
    Pn532KillerClient client(&t);
    assert(client.sniff_enter_mode(false));
    t.bytes.clear(); t.offset = 0;
    t.append(response(0xAD, {0})); // delayed mode response
    std::vector<uint8_t> payload(162, 0);
    payload[0]=1; payload[2]=0x44; payload[3]=0x33; payload[4]=0x22; payload[5]=0x11;
    payload[18]=1; payload[19]=0x60;
    t.append(response(0x21, payload));
    const auto entries = client.sniff_get_mfkey_entries(false);
    assert(entries.size()==1 && entries[0].uid==0x11223344 && entries[0].sector==1);
    std::cout << "sniffer response matching and fragmented log: PASS\n";
}
