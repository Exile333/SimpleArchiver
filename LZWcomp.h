#pragma once
#include <iostream>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <string>
#include <istream>
#include <ostream>

class TLZWEncoder{
typedef uint32_t TWCoded;
private:
    static const char EEOF = 0;
    TWCoded CSz, MaxCSz;
    std::unordered_map<std::string, TWCoded> Dict;
    typedef struct {
        int8_t Byte;
        int8_t BitsUsed;
    } TLastByte;
    TLastByte LastByte;

    inline uint8_t PutBits(TWCoded word, std::ostream& out);
        //returns amount of bytes sent to 'out'
    void PutLastBits(std::ostream& out);
    void DictInit();
public:
    TLZWEncoder();
    
    uint64_t Encode(std::istream& in, std::ostream& out, uint8_t compDeg = 6); 
        //returns size of output file

    ~TLZWEncoder();
};

class TLZWDecoder{
typedef uint32_t TWCoded;
private:
    static const char EEOF = 0;
    TWCoded CSz, MaxCSz;
    std::unordered_map<TWCoded, std::string> Dict;
    typedef struct {
        uint8_t Byte;
        uint8_t BitsUsed;
    } TFirstByte;
    TFirstByte FirstByte;
    bool Err;
    std::string InErr;

    void DictInit();
    TWCoded GetCWord(std::istream& in);
public:
    TLZWDecoder();
    
    uint64_t Decode(std::istream& in, std::ostream& out, std::string& error, uint8_t compDeg = 6);

    ~TLZWDecoder();
};
