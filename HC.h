#pragma once
#include <iostream>
#include <istream>
#include <ostream>
#include <cstdint>
#include <experimental/filesystem>
#include <fstream>
#include <map>
#include <string>

class THEncoder{
private:
    static const int16_t ASize = 257;
    static constexpr int16_t CEOF = ASize - 1;
    uint64_t frq[ASize];

    typedef struct {
        int8_t Byte;
        int8_t BitsUsed;
    } TLastByte;
    TLastByte LastByte;
    std::string InErr;
    bool Err;

    uint8_t PutBits(std::ostream& out, uint64_t code, uint16_t cLgth);
        //returns amount of bytes sent to 'out'
    uint8_t PutLastBits(std::ostream& out);
        //returns if byte sent or not
    uint64_t PutCodebook(std::ostream& out, uint16_t* cLgth);
        //returns amount of bytes sent to 'out'
    void PyrSiftDown(uint64_t* prmd, int16_t elNo, uint16_t sz);
    void GetLength(uint64_t* frq, uint16_t notNull, uint16_t* cLgth, uint16_t* cNum, uint16_t* maxLgth);
public:
    THEncoder();

    uint64_t Encode(std::istream& in, std::ostream& out, std::string& error);
        //returns length of encoded message in bytes

    ~THEncoder();
};

class THDecoder{
private:
    static const int16_t ASize = 257;
    static constexpr int16_t NEOF = ASize - 1;
    static const int8_t CEOF = 0;
    uint64_t codes[ASize];

    typedef struct {
        uint8_t Byte;
        uint8_t BitsUsed;
    } TLastByte;
    TLastByte LastByte;
    std::string InErr;
    bool Err;

    uint16_t ReadSymb(std::istream& in, uint16_t* cLgth, uint64_t* codes, uint64_t* firstCodes, uint16_t maxLgth);
        //returns read symbol
    void ReadCodebook(std::istream& in, uint16_t* cLgth, uint16_t* cNum, uint16_t* maxLgth);
    void SetCodes(uint16_t* cLgth, uint16_t* cNum, uint16_t maxLgth, uint64_t* firstCodes);
public:
    THDecoder();

    uint64_t Decode(std::istream& in, std::ostream& out, std::string& error);

    ~THDecoder();
};
