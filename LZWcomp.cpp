#include <iostream>
#include "LZWcomp.h"

inline uint8_t TLZWEncoder::PutBits(TWCoded word, std::ostream& out){
    uint8_t sent = 0;
    uint8_t btsAm = sizeof(TWCoded) * 8;
    uint8_t wSz = btsAm;
    TWCoded trueCSz = CSz - 1;
    while (!( ((TWCoded)1 << (btsAm - 1)) & (trueCSz)) ){
        wSz = --btsAm;
    }
    for (; wSz > 0; wSz--){
        if (word & ((TWCoded)1 << (wSz - 1))){
            LastByte.Byte |= (static_cast<uint8_t>(1) << (8 - LastByte.BitsUsed - 1));
        }
        LastByte.BitsUsed++;
        if (LastByte.BitsUsed == 8){
            out.put(static_cast<char>(LastByte.Byte));
            sent++;
            LastByte.Byte = 0;
            LastByte.BitsUsed = 0;
        }
    }
    return sent;
}

void TLZWEncoder::PutLastBits(std::ostream& out){
    out.put(static_cast<char>(LastByte.Byte));
    LastByte.Byte = 0;
    LastByte.BitsUsed = 0;
}

void TLZWEncoder::DictInit(){
    Dict.clear();
    CSz = 0;
    std::string word;
    CSz++;
    for (; CSz < 257; CSz++){
        word = static_cast<char>(CSz - 1);
        Dict.insert(std::pair<std::string, TWCoded>(word, CSz));
    }
}

TLZWEncoder::TLZWEncoder() {
    DictInit();
    LastByte.Byte = 0;
    LastByte.BitsUsed = 0;
    MaxCSz = CSz;
}

uint64_t TLZWEncoder::Encode(std::istream& in, std::ostream& out,
                             uint8_t compDeg){
    uint64_t codedSize = 0;
    constexpr int32_t cCoef = 512 * 1024;
    MaxCSz = cCoef * compDeg;
    bool isEOF = false;
    char lastRead;
    in.get(lastRead);
    while (!isEOF){
        for (; CSz < MaxCSz; CSz++){
            std::string word(1, lastRead);
            TWCoded* isWCoded = NULL;
            TWCoded wCoded = 0;
            while (Dict.count(word) && !in.eof()){
                isWCoded = &Dict[word];
                wCoded = *isWCoded;
                in.get(lastRead);
                word += lastRead;
            } 
            isWCoded = &Dict[word];
            codedSize += static_cast<uint64_t>(PutBits(wCoded, out));
            if (in.eof()){
                codedSize += static_cast<uint64_t>(PutBits(EEOF, out));
                isEOF = true;
                break;
            }
            *isWCoded = CSz;
        }
        if (isEOF){
            break;
        }
        DictInit();
    }
    if (LastByte.BitsUsed){
        PutLastBits(out);
        codedSize++;
    }
    return codedSize;
}

TLZWEncoder::~TLZWEncoder() {}

///////////////////////////////////////////////////////////////////////////////

typedef uint32_t TWCoded;

void TLZWDecoder::DictInit(){
    Dict.clear();
    CSz = 0;
    std::string word(1, EEOF);
    Dict.insert(std::pair<TWCoded, std::string>(CSz++, word));
    for (; CSz < 257; CSz++){
        word = static_cast<char>(CSz - 1);
        Dict.insert(std::pair<TWCoded, std::string>(CSz, word));
    }
}

TWCoded TLZWDecoder::GetCWord(std::istream& in){
    TWCoded trueCSz = CSz - 1;
    uint8_t wSz = sizeof(TWCoded) * 8;
    TWCoded word = 0;
    while (!( trueCSz & ((TWCoded)1 << (wSz - 1)) ) ){
        wSz--;
    }
    while (wSz > 0){
        if (FirstByte.BitsUsed == 8){
            if (in.eof()){
                Err = true;
                InErr = "Error: unexpected end of file.";
                return -1;
            }
            char c;
            in.get(c);
            FirstByte.Byte = static_cast<uint8_t>(c);
            FirstByte.BitsUsed = 0;
        }
        if (FirstByte.Byte & (static_cast<uint8_t>(1) << (8 - FirstByte.BitsUsed - 1))){
            word |= (TWCoded)1 << (wSz - 1);
        }
        FirstByte.BitsUsed++;
        wSz--;
    }
    return word;
}

TLZWDecoder::TLZWDecoder() {
    Err = false;
    InErr = "";
    FirstByte.Byte = 0;
    FirstByte.BitsUsed = 8;
}

uint64_t TLZWDecoder::Decode(std::istream& in, std::ostream& out, std::string& error, uint8_t compDeg){
    DictInit();
    MaxCSz = CSz;
    constexpr uint32_t cCoef = 512 * 1024;
    MaxCSz = cCoef * compDeg;
    TWCoded word = GetCWord(in);
    uint64_t msgSz;
    std::string* wDcd;
    std::string* lastWDcd = NULL;
    if (word == EEOF){
        return 1;
    }
    while(word != EEOF) {
        if (Err){
            error = InErr;
            InErr = "";
            Err = false;
            return 0;
        }
        wDcd = &Dict[word];
        if (wDcd->empty()){
            *wDcd = *lastWDcd + lastWDcd->front();
        }
        else if (lastWDcd != NULL){
            Dict.insert(std::pair<TWCoded, std::string>(CSz - 1, *lastWDcd + wDcd->front()));
        }
        out.write(wDcd->c_str(), wDcd->length());
        msgSz += wDcd->length();
        lastWDcd = wDcd;
        CSz++;
        if (CSz >= MaxCSz){
            DictInit();
            lastWDcd = NULL;
        }
        word = GetCWord(in);
    } 
    msgSz++;
    return msgSz;
}

TLZWDecoder::~TLZWDecoder() {}
