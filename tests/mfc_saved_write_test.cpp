#include "../main/nfc/nfc_i2c_device.hpp"
#include <cassert>
#include <iostream>
using namespace nfc_app;
struct Card {
    std::array<std::array<uint8_t,16>,64> data{};
    std::vector<uint8_t> tx,rx;
    int pending=-1,writes=0,fail_block=-1,corrupt_block=-1;
    bool gen4=false,wrong_capacity=false;
    int reads=0,unlocks=0;
    I2cGroveNfcDevice::St25rBackend backend() {
        I2cGroveNfcDevice::St25rBackend b;
        b.write_reg=[](uint8_t,uint8_t){return true;};
        b.read_reg=[this](uint8_t r,uint8_t &v){v=r==0x1A?0x10:r==0x1E?rx.size():0;return true;};
        b.fifo_write=[this](const uint8_t*p,uint8_t n){tx.assign(p,p+n);return true;};
        b.fifo_read=[this](uint8_t*p,uint8_t n){if(n>rx.size())return false;std::copy_n(rx.begin(),n,p);rx.erase(rx.begin(),rx.begin()+n);return true;};
        b.command=[this](uint8_t c){
            if(c==0xDB)rx.clear();
            if(gen4 && (c==0xC4 || c==0xC5) && tx.size()>=6 && tx[0]==0xCF) {
                if(tx[5]==0xC6) rx={0,1,2,3,4,5};
                else if(tx[5]==0xCD && tx.size()==23) {int block=tx[6];std::copy_n(tx.begin()+7,16,data[block].begin());++writes;rx={0x90,0};}
                else if(tx[5]==0xCE && tx.size()==7) {assert(writes==64);++reads;rx.assign(data[tx[6]].begin(),data[tx[6]].end());if(tx[6]==corrupt_block)rx[0]^=1;}
            } else if(c==0xC4){
                if(pending>=0){
                    if(pending!=fail_block){std::copy_n(tx.begin(),16,data[pending].begin());++writes;rx={10};}else rx={0};
                    pending=-1;
                }else if(tx.size()==2&&tx[0]==0xA0){pending=tx[1];rx={10};}
            }return true;
        };
        b.select=[this](std::vector<uint8_t>&u,uint8_t&s){u={1,2,3,4};s=wrong_capacity?0x18:8;return true;};
        b.unlock=[this](const std::string &){++unlocks;return !gen4;};
        b.read_plain=[this](uint8_t n,uint8_t*p){assert(writes==64);++reads;std::copy(data[n].begin(),data[n].end(),p);if(n==corrupt_block)p[0]^=1;return true;};
        return b;
    }
};
std::vector<std::string> dump(){
    std::vector<std::string> lines;
    for(int i=0;i<64;++i){char prefix[8];snprintf(prefix,sizeof(prefix),"%02d:",i);
        lines.push_back(std::string(prefix)+(i==0?"01020304040804000000000000000000":i%4==3?"FFFFFFFFFFFFFF078069FFFFFFFFFFFF":"00000000000000000000000000000000"));}
    return lines;
}
int main(){
    for(int scenario=0;scenario<9;++scenario){
        Card card;I2cGroveNfcDevice device;auto lines=dump();std::string error,last;
        if(scenario==1)lines[5]="05:FFFF";
        if(scenario==2)lines[3]="03:FFFFFFFFFFFF00000069FFFFFFFFFFFF";
        if(scenario==3)card.wrong_capacity=true;
        if(scenario==4){card.fail_block=4;card.data[4].fill(0xA5);}
        if(scenario==5 || scenario==8)card.corrupt_block=4;
        if(scenario>=6)card.gen4=true;
        device.set_st25r_backend(card.backend());
        bool ok=device.write_mfc_dump(lines,scenario>=6?4:1,scenario==7?"bad":"00000000",nullptr,[&](const std::string&s){last=s;},&error);
        if(scenario==0 || scenario==5 || scenario==6){assert(ok);assert(card.writes==64);assert(last.find("OK 64/64 blocks")==0);}
        if(scenario==0 || scenario==5){assert(card.reads==0);assert(card.unlocks==1);}
        else if(scenario==6)assert(card.reads==64);
        if(scenario!=0 && scenario!=5 && scenario!=6){assert(!ok);assert(!error.empty());if(scenario<4)assert(card.writes==0);else if(scenario==8)assert(card.writes==64);else assert(card.writes<64);}
    }
    std::cout<<"PASS: Gen1A single unlock and no verify, full Gen4 verify, bad Gen4 password, malformed dump, access bits, capacity, NAK and verify mismatch\n";
}
