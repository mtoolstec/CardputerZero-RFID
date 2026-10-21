#include "nfc_mfc_emulator.hpp"
#include <cassert>
#include <iostream>
using namespace nfc_app;
int main() {
    std::vector<uint8_t> mem(1024,0),nt,response;
    std::fill(mem.begin()+48,mem.begin()+54,0xFF);
    MfcEmulator emu;
    assert(emu.challenge(0,false,0x11223344,0x175B6804,mem,nt));
    assert((nt==std::vector<uint8_t>{0x17,0x5B,0x68,0x04}));
    // Independent PN532 reader trace captured during the IIC hardware test.
    const std::vector<uint8_t> reader={0xAD,0x58,0x4A,0x53,0x57,0x5D,0xCE,0x70,0xA6};
    size_t bits=0;
    assert(emu.handle(reader,mem,response,bits));
    assert(emu.state()==MfcEmulator::State::Authenticated && bits==36 && response.size()==5);
    mem[48]=0; // A different imported key must reject this same reader proof.
    assert(emu.challenge(0,false,0x11223344,0x175B6804,mem,nt));
    assert(!emu.handle(reader,mem,response,bits));
    assert(emu.state()==MfcEmulator::State::Idle);
    assert(!emu.challenge(64,false,0,1,mem,nt));
    mem.pop_back();assert(!emu.challenge(0,false,0,1,mem,nt));
    std::cout << "PN532 authentication trace and invalid-key/bounds checks passed\n";
}
