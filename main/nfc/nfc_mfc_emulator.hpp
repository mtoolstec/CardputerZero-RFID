#pragma once
#include "nfc_crypto1.hpp"
#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace nfc_app {
// MFC 1K read-only PICC session. All Crypto1 state belongs to the RF worker.
class MfcEmulator {
public:
    enum class State { Idle, Challenge, Authenticated };
    void reset() { state_ = State::Idle; }
    State state() const { return state_; }
    static uint16_t crc(const uint8_t *p, size_t n) {
        uint16_t c=0x6363;
        while(n--) { uint8_t v=*p++ ^ uint8_t(c); v^=v<<4; c=(c>>8)^(uint16_t(v)<<8)^(uint16_t(v)<<3)^(v>>4); }
        return c;
    }
    static uint32_t successor(uint32_t x, unsigned n) {
        x=__builtin_bswap32(x);
        while(n--) x=(x>>1)|(((x>>16)^(x>>18)^(x>>19)^(x>>21))&1)<<31;
        return __builtin_bswap32(x);
    }
    static std::vector<uint8_t> unpack(const std::vector<uint8_t>& raw) {
        const size_t n=raw.size()*8/9;
        std::vector<uint8_t> out(n,0);
        for(size_t b=0;b<n;++b) for(size_t bit=0;bit<8;++bit) {
            size_t pos=b*9+bit; out[b]|=((raw[pos/8]>>(pos%8))&1)<<bit;
        }
        return out;
    }
    // Produce a plaintext NT. Memory is the exact loaded 64-block dump.
    bool challenge(uint8_t block, bool key_b, uint32_t uid, uint32_t nonce,
                   const std::vector<uint8_t>& memory, std::vector<uint8_t>& nt, bool nested=false) {
        reset(); if(block>=64 || memory.size()!=1024) return false;
        sector_=block/4; key_b_=key_b;
        const size_t off=(sector_*4+3)*16+(key_b?10:0);
        uint64_t key=0; for(size_t i=0;i<6;++i) key=(key<<8)|memory[off+i];
        crypto_.init(key);
        uid_=uid; nonce_=nonce; state_=State::Challenge;
        nt={uint8_t(nonce>>24),uint8_t(nonce>>16),uint8_t(nonce>>8),uint8_t(nonce)};
        if (nested) {
            // Nested NT is encrypted while UID xor NT enters the new cipher,
            // rather than using the old session or zero-input keystream.
            std::vector<uint8_t> packed(5,0);
            for(size_t b=0;b<4;++b) {
                const uint8_t enc=nt[b]^crypto_.step8(uint8_t(uid>>(24-b*8))^nt[b]);
                for(size_t bit=0;bit<8;++bit) packed[(b*9+bit)/8]|=((enc>>bit)&1)<<((b*9+bit)%8);
                packed[(b*9+8)/8]|=(crypto_.parity_keystream_bit()^Crypto1Local::oddparity8(nt[b]))<<((b*9+8)%8);
            }
            nt=std::move(packed);
        } else crypto_.inject(uid,nonce);
        return true;
    }
    // Output is already encrypted and includes encrypted parity (nine bits/byte).
    bool handle(const std::vector<uint8_t>& raw,const std::vector<uint8_t>& memory,
                std::vector<uint8_t>& response,size_t& bits) {
        response.clear(); bits=0;
        auto data=unpack(raw);
        if(state_==State::Challenge) {
            if(raw.size()!=9 || data.size()!=8) { reset(); return false; }
            for(size_t i=0;i<4;++i) (void)crypto_.step8(data[i],true);
            const uint32_t ar=successor(nonce_,64);
            for(size_t i=4;i<8;++i) {
                uint8_t plain=data[i]^crypto_.step8(0);
                if(plain!=uint8_t(ar>>(24-(i-4)*8))) { reset(); return false; }
            }
            const uint32_t at=successor(nonce_,96);
            std::vector<uint8_t> plain={uint8_t(at>>24),uint8_t(at>>16),uint8_t(at>>8),uint8_t(at)};
            encrypt(plain,response,bits); state_=State::Authenticated; return true;
        }
        if(state_!=State::Authenticated || data.size()!=4) { reset(); return false; }
        for(auto& b:data) b^=crypto_.step8(0);
        const auto check=crc(data.data(),2);
        if(data[2]!=uint8_t(check) || data[3]!=uint8_t(check>>8)) { reset();return false; }
        if(data[0]==0x50) { reset(); return false; }
        const uint8_t block=data[1];
        if ((data[0]==0x60 || data[0]==0x61) && block<64) {
            const uint32_t next_nonce=successor(nonce_,32);
            if (!challenge(block,data[0]==0x61,uid_,next_nonce,memory,response,true)) return false;
            bits=36; return true;
        }
        // Do not mutate imported dumps. Unsupported commands receive an encrypted NAK.
        if(data[0]!=0x30 || block>=64 || block/4!=sector_) {
            uint8_t nak=0; for(unsigned i=0;i<4;++i) nak|=crypto_.step_with(false)<<i;
            response={nak};bits=4;return true;
        }
        std::vector<uint8_t> plain(memory.begin()+block*16,memory.begin()+block*16+16);
        // Sector trailer keys are not readable as plaintext on a real Classic card.
        if(block%4==3) std::fill(plain.begin(),plain.begin()+6,0);
        const auto c=crc(plain.data(),plain.size());plain.push_back(uint8_t(c));plain.push_back(uint8_t(c>>8));
        encrypt(plain,response,bits);return true;
    }
private:
    void encrypt(const std::vector<uint8_t>& plain,std::vector<uint8_t>& raw,size_t& bits) {
        bits=plain.size()*9;raw.assign((bits+7)/8,0);
        for(size_t b=0;b<plain.size();++b) {
            uint8_t enc=plain[b]^crypto_.step8(0);
            for(size_t bit=0;bit<8;++bit) raw[(b*9+bit)/8]|=((enc>>bit)&1)<<((b*9+bit)%8);
            const uint8_t parity=crypto_.parity_keystream_bit()^Crypto1Local::oddparity8(plain[b]);
            raw[(b*9+8)/8]|=parity<<((b*9+8)%8);
        }
    }
    Crypto1Local crypto_;
    State state_=State::Idle;
    uint32_t nonce_=0;
    uint32_t uid_=0;
    uint8_t sector_=0;
    bool key_b_=false;
};
}
