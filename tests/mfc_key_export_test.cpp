#include "../main/nfc/nfc_storage.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace nfc_app;
int main() {
    char directory[] = "/tmp/rfid-key-export-XXXXXX";
    assert(mkdtemp(directory));
    setenv("M5CZ_NFC_KEYS_DIR", directory, 1);
    NfcStorage storage;
    for (bool prefixed : {false, true}) {
        std::vector<std::string> blocks(256, std::string(32, '0'));
        for (int block = 3; block < 256; block += block < 127 ? 4 : 16)
            blocks[block] = "112233445566FF078069A0A1A2A3A4A5";
        // Data inside a large 4K sector must never be treated as a trailer.
        blocks[131] = "DEADBEEFCAFEFF078069CAFEBABE0001";
        if (prefixed) {
            blocks.resize(64); // CAP Gen1A 1K decimal block labels
            for (size_t i = 0; i < blocks.size(); ++i) {
                char label[8]; snprintf(label, sizeof(label), "%02zu:", i);
                blocks[i] = std::string(label) + blocks[i];
            }
        }
        std::string error;
        assert(storage.save_uid_key_file("11223344", blocks, &error));
        const auto keys = storage.load_key_file("11223344.dic");
        assert(keys.size() == 2);
        assert(keys[0] == "112233445566" && keys[1] == "A0A1A2A3A4A5");
    }
    std::vector<std::string> malformed(4, "03:112233445566FF078069A0A1A2A3A4A5FF");
    assert(!storage.save_uid_key_file("BAD", malformed));
    std::filesystem::remove_all(directory);
    std::cout << "PASS: CAP prefix, unprefixed 4K trailers, malformed blocks\n";
}
