#include "../main/nfc/nfc_protocol.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace nfc_app;

// Emulate authenticated 4K reads, including a transient block failure.
struct ClassicTransport : INfcTransport {
    std::vector<uint8_t> pending;
    bool failed_once = false;
    bool only_b = false;
    int failures_remaining = 0;
    bool permanent_failure = false;
    int rejected_auth = 0;
    int retried_reads = 0;
    bool open(const TransportEndpoint &, std::string *) override { return true; }
    void close() override {}
    bool is_open() const override { return true; }
    TransportEndpoint endpoint() const override { return {}; }
    ssize_t write_bytes(const uint8_t *bytes, size_t count, std::string *) override {
        std::vector<uint8_t> command;
        assert(Pn532FrameCodec::parse_first_frame({bytes, bytes + count}, &command));
        const auto op = command[1];
        std::vector<uint8_t> data = {0};
        if (op == 0x4A) data = {1};
        else if (op == 0x40 && (command[3] == 0x60 || command[3] == 0x61)) {
            const std::vector<uint8_t> key(command.begin()+5, command.begin()+11);
            const std::vector<uint8_t> uid(command.begin()+11, command.end());
            if (key != std::vector<uint8_t>({0x11,0x22,0x33,0x44,0x55,0x66}) || (only_b && command[3] != 0x61)) {
                data[0] = 1;
                ++rejected_auth;
            }
            assert(uid == std::vector<uint8_t>({3,4,5,6}));
        } else if (op == 0x40 && command[3] == 0x30) {
            const uint8_t block = command[4];
            if (block == 1 && (permanent_failure || failures_remaining-- > 0)) data = {1};
            else if (block == 13 && !failed_once) { failed_once = true; data = {1}; }
            else {
                if (block == 13) ++retried_reads;
                data.insert(data.end(), 16, block);
                if ((block < 128 && block % 4 == 3) || (block >= 128 && (block-128) % 16 == 15)) {
                    std::fill(data.begin()+1, data.begin()+7, 0);
                    std::fill(data.begin()+11, data.begin()+17, 0);
                }
            }
        } else assert(op == 0x52);
        std::vector<uint8_t> payload = {0xD5, uint8_t(op+1)};
        payload.insert(payload.end(), data.begin(), data.end());
        const uint8_t length = payload.size();
        pending = {0,0,0xFF,length,uint8_t(0-length)};
        pending.insert(pending.end(),payload.begin(),payload.end());
        uint8_t sum = 0;
        for (auto byte : payload) sum += byte;
        pending.push_back(uint8_t(0-sum)); pending.push_back(0);
        return count;
    }
    ssize_t read_bytes(uint8_t *out, size_t size, int, std::string *) override {
        const size_t count = std::min(size, pending.size());
        std::memcpy(out,pending.data(),count);
        pending.erase(pending.begin(),pending.begin()+count);
        return count;
    }
};
int main() {
    ClassicTransport transport;
    Pn532KillerClient client(&transport);
    std::vector<std::string> blocks;
    std::string error;
    std::vector<MifareSectorKey> key_map;
    assert(client.read_mifare_standard({4,1,2,3,4,5,6},40,&blocks,&error,nullptr,{"112233445566"},&key_map));
    assert(blocks.size()==256 && blocks[255]=="112233445566FFFFFFFF000000000000");
    assert(blocks[0]==std::string(32,'0')); // ordinary zero data must stay intact
    assert(key_map.size()==40);
    transport.rejected_auth=0;
    assert(client.read_mifare_standard({4,1,2,3,4,5,6},40,&blocks,&error,nullptr,{},&key_map));
    assert(transport.rejected_auth==0); // no dictionary sweep for a verified map
    transport.only_b=true;
    std::vector<MifareSectorKey> b_map={{0,true,{0x11,0x22,0x33,0x44,0x55,0x66}}};
    assert(client.read_mifare_standard({4,1,2,3,4,5,6},1,&blocks,&error,nullptr,{},&b_map));
    assert(blocks[3]=="00000000000003030303112233445566");
    assert(transport.retried_reads>=1);
    for (const auto &block : blocks) assert(block.size()==32);
    transport.only_b=false;
    transport.failures_remaining=3; // exhaust the first pass, recover only the missing block
    assert(client.read_mifare_standard({4,1,2,3,4,5,6},1,&blocks,&error,nullptr,{"112233445566"}));
    assert(blocks[1].size()==32);
    transport.permanent_failure=true;
    std::string last_line;
    assert(!client.read_mifare_standard({4,1,2,3,4,5,6},1,&blocks,&error,
        [&](const std::string &line) { last_line=line; },{"112233445566"}));
    assert(blocks[1].empty() && error.find("3/4")!=std::string::npos);
    assert(last_line.find("ERR incomplete 3/4") == 0);
    std::cout << "PASS: missing-block recovery, partial dump rejected; A/B trailer keys, zero data, cached map, dictionary, UID, retry, 4K\n";
}
