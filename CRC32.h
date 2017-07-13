#pragma once
#include <istream>
#include <cstdint>

uint32_t CRC32(std::istream& in, uint64_t* length){
    uint32_t crc_table[256];
    uint32_t crc;
    for (int i = 0; i < 256; i++){
        crc = i;
        for (int8_t j = 0; j < 8; j++){
            crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
        }
        crc_table[i] = crc;
    }
    crc = 0xFFFFFFFF;
    char c;
    uint64_t lgth = 0;
    while (in.get(c)){
        crc = crc_table[(crc ^ static_cast<uint32_t>(c)) & 0xFF] ^ (crc >> 8);
        lgth++;
    }
    *length = lgth;
    return crc ^ 0xFFFFFFFF; 
}
