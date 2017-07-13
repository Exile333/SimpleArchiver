#include "HC.h"

THEncoder::THEncoder() {
    for (int16_t i = 0; i < ASize; i++){
        frq[i] = 0;
    }
    LastByte.Byte = 0;
    LastByte.BitsUsed = 0;
}

uint8_t THEncoder::PutBits(std::ostream& out, uint64_t code, uint16_t cLgth){
    uint8_t bSent = 0;
    while (cLgth > 0){
        if ((static_cast<uint64_t>(1) << (cLgth - 1)) & code){
            LastByte.Byte |= (static_cast<uint8_t>(1) << (8 - LastByte.BitsUsed - 1));
        }
        LastByte.BitsUsed++;
        if (LastByte.BitsUsed == 8){
            out.put(static_cast<char>(LastByte.Byte));
            LastByte.Byte = 0;
            LastByte.BitsUsed = 0;
            bSent++;
        }
        cLgth--;
    }
    return bSent;
}

uint8_t THEncoder::PutLastBits(std::ostream& out){
    if (LastByte.BitsUsed > 0){
        out.put(static_cast<char>(LastByte.Byte));
        LastByte.Byte = 0;
        LastByte.BitsUsed = 0;
        return 1;
    }
    return 0;
}

uint64_t THEncoder::PutCodebook(std::ostream& out, uint16_t* cLgth){
    uint64_t bSz = 0;
    for (uint16_t i = 0; i < ASize; i++){
        bSz += PutBits(out, cLgth[i], 16);
    }
    return bSz;
}

void THEncoder::PyrSiftDown(uint64_t* prmd, int16_t elNo, uint16_t sz){
    while (elNo <= sz / 2){
        uint64_t minSon;
        if (elNo * 2 + 1 <= sz){
            minSon = std::min(prmd[prmd[elNo * 2]], prmd[prmd[elNo * 2 + 1]]);
        }
        else {
            minSon = prmd[prmd[elNo * 2]];
        }
        if (minSon >= prmd[prmd[elNo]]){
            break;
        }
        int16_t nextNo;
        if (minSon == prmd[prmd[elNo * 2]]){
            nextNo = elNo * 2;
        }
        else {
            nextNo = elNo * 2 + 1;
        }
        uint64_t temp = prmd[nextNo];
        prmd[nextNo] = prmd[elNo];
        prmd[elNo] = temp;
        elNo = nextNo;
    }
}

void THEncoder::GetLength(uint64_t* frq, uint16_t notNull, uint16_t* cLgth, uint16_t* cNum, uint16_t* maxLgth){
    if (notNull < 3){
        cLgth[CEOF] = 1;
        cNum[1] = 1;
        *maxLgth = 1;
        if (notNull == 2){
            for (uint16_t i = 0; i < ASize; i++){
                if (frq[i]){
                    cLgth[i] = 1;
                    cNum[1] = 2;
                    break;
                }
            }
        }
        return;
    }
    uint16_t sz = notNull * 2 + 1;
    uint64_t* prmd = new uint64_t [sz];
    for (uint16_t i = 0, j = 1; i < ASize; i++){
        if (frq[i]){
            prmd[j] = notNull + j;
            prmd[notNull + j] = frq[i];
            j++;
        }
    }
    uint16_t h = notNull;
    for (int16_t i = notNull / 2; i >= 1; i--){
        PyrSiftDown(prmd, i, h);
    }
    while (h > 1){
        uint64_t m1, m2;
        m1 = prmd[1];
        prmd[1] = prmd[h];
        h--;
        PyrSiftDown(prmd, 1, h);
        m2 = prmd[1];
        prmd[h + 1] = prmd[m1] + prmd[m2];
        prmd[1] = h + 1;
        prmd[m1] = h + 1;
        prmd[m2] = h + 1;
        PyrSiftDown(prmd, 1, h);
    }
    prmd[2] = 0;
    for (uint16_t i = 3; i < sz; i++){
        prmd[i] = prmd[prmd[i]] + 1;
    }
    *maxLgth = 0;
    for (uint16_t i = 0, j = 1; i < ASize; i++){
        if (frq[i]){
            cLgth[i] = static_cast<uint16_t>(prmd[notNull + j]);
            if (cLgth[i] > *maxLgth){
                *maxLgth = cLgth[i];
            }
            cNum[cLgth[i]]++;
            j++;
        }
    }
    delete[] prmd;
}

namespace fs = std::experimental::filesystem;

uint64_t THEncoder::Encode(std::istream& in, std::ostream& out, std::string& error){
    uint64_t encSz = 0;
    //filling frequencies table
    char c;
    uint16_t notNull = 0;
    std::string tmpFNm = fs::temp_directory_path();
    tmpFNm += "/MyLittleArchiverTemp";
    std::ofstream tmpFl(tmpFNm);
    while (in.get(c)){
        if (!frq[static_cast<uint8_t>(c)]){
            notNull++;
        }
        frq[static_cast<uint8_t>(c)]++;
        tmpFl.put(c);
    }
    tmpFl.close();
    frq[CEOF] = 1;
    notNull++;
    in.seekg(0, std::ios_base::beg);
    //finding codes' length
    uint16_t cLgth[ASize], cNum[ASize];
    uint16_t maxLgth = 0;
    for (int16_t i = 0; i < ASize; i++){
        cLgth[i] = 0;
        cNum[i] = 0;
    }   
    GetLength(frq, notNull, cLgth, cNum, &maxLgth);
    //setting codes
    uint64_t* stCodes = new uint64_t[maxLgth + 1];
    uint64_t codes[ASize];
    for (uint16_t i = 0; i < ASize; i++){
        if (i <= maxLgth){
            stCodes[i] = 0;
        }
        codes[i] = 0;
    }
    for (uint16_t prevLgth = maxLgth; prevLgth > 1; prevLgth--){
        stCodes[prevLgth - 1] = (stCodes[prevLgth] + cNum[prevLgth]) >> 1;
    }
    for (uint16_t i = 0; i < ASize; i++){
        uint64_t symLgth = cLgth[i];
        if (symLgth > 0){
            codes[i] = stCodes[symLgth];
            stCodes[symLgth]++;
        }
    }
    delete [] stCodes;
    //encoding the file
    encSz += PutCodebook(out, cLgth);
    std::ifstream tmpFlIn(tmpFNm);
    while (tmpFlIn.get(c)){
        encSz += PutBits(out, codes[static_cast<uint8_t>(c)], cLgth[static_cast<uint8_t>(c)]);
    }
    tmpFlIn.close();
    fs::remove(fs::path(tmpFNm));
    encSz += PutBits(out, codes[CEOF], cLgth[CEOF]);
    encSz += PutLastBits(out);
    return encSz;
}

THEncoder::~THEncoder() {}

///////////////////////////////////////////////////////////////////////////////

uint16_t THDecoder::ReadSymb(std::istream& in, uint16_t* cLgth, uint64_t* codes, uint64_t* firstCodes, uint16_t maxLgth){
    uint16_t rLgth = 0;
    uint64_t rCode = 0;
    while (rLgth <= maxLgth && rCode < firstCodes[rLgth]){
        rCode = rCode << 1;
        if (LastByte.BitsUsed == 8){
            if (in.eof()){
                Err = true;
                InErr = "Error: unexpected EOF.";
                return 0;
            }
            char c;
            in.get(c);
            LastByte.Byte = c;
            LastByte.BitsUsed = 0;
        }
        if (LastByte.Byte & (static_cast<uint8_t>(1) << (8 - LastByte.BitsUsed - 1))){
            rCode |= 1;
        }
        rLgth++;
        LastByte.BitsUsed++;
    }
    if (rLgth <= maxLgth){
        for (uint16_t i = 0; i < ASize; i++){
            if (cLgth[i] == rLgth && codes[i] == rCode){
                return i;
            }
        }
    }
    Err = true;
    InErr = "Error: invalid file.";
    return 0;
}

void THDecoder::ReadCodebook(std::istream& in, uint16_t* cLgth, uint16_t* cNum, uint16_t* maxLgth){
    *maxLgth = 0;
    for (uint16_t i = 0; i < ASize; i++){
        char c;
        uint16_t lgth = 0;
        for (uint8_t j = 0; j < 2; j++){
            in.get(c);
            lgth |= (static_cast<uint16_t>(static_cast<uint8_t>(c)) << (8 - (8 * j)));
        }
        if (lgth){
            if (lgth > ASize){
                Err = true;
                InErr = "Error: invalid file.";
                return;
            }
            cNum[lgth]++;
            *maxLgth = static_cast<uint16_t>(lgth) > *maxLgth ? static_cast<uint16_t>(lgth) : *maxLgth;
        }
        cLgth[i] = lgth;
    }
}

void THDecoder::SetCodes(uint16_t* cLgth, uint16_t* cNum, uint16_t maxLgth, uint64_t* firstCodes){
    uint64_t* stCodes = new uint64_t [maxLgth + 1];
    for (uint16_t i = 0; i <= maxLgth; i++){
        stCodes[i] = 0;
    }
    firstCodes[maxLgth] = 0;
    for (uint16_t prevLgth = maxLgth; prevLgth > 1; prevLgth--){
        stCodes[prevLgth - 1] = (stCodes[prevLgth] + cNum[prevLgth]) >> 1;
        firstCodes[prevLgth - 1] = (firstCodes[prevLgth] + cNum[prevLgth]) / 2;
    }
    firstCodes[0] = 1;
    for (uint16_t i = 0; i < ASize; i++){
        uint64_t symLgth = cLgth[i];
        if (symLgth){
            codes[i] = stCodes[symLgth];
            stCodes[symLgth]++;
        }
    }
    delete [] stCodes;
}

THDecoder::THDecoder(){
    Err = false;
    InErr = "";
    for (uint16_t i = 0; i < ASize; i++){
        codes[i] = 0;
    }
    LastByte.Byte = 0;
    LastByte.BitsUsed = 8;
}

uint64_t THDecoder::Decode(std::istream& in, std::ostream& out, std::string& error){
    uint16_t* cLgth = new uint16_t [ASize];
    uint16_t* cNum = new uint16_t [ASize];
    uint64_t* firstCodes = new uint64_t [ASize];
    for (uint16_t i = 0; i < ASize; i++){
        cLgth[i] = 0;
        cNum[i] = 0;
        firstCodes[i] = 0;
    }
    uint16_t maxLgth = 0;
    ReadCodebook(in, cLgth, cNum, &maxLgth);
    if (Err){
        error = InErr;
        InErr = "";
        Err = false;
        return 0;
    }
    firstCodes[0] = 1;
    SetCodes(cLgth, cNum, maxLgth, firstCodes);
    if (Err){
        error = InErr;
        InErr = "";
        Err = false;
        return 0;
    }
    uint64_t msgSz = 0;
    uint16_t sym = ReadSymb(in, cLgth, codes, firstCodes, maxLgth);
    while (sym != NEOF){
        if (Err){
            error = InErr;
            InErr = "";
            Err = false;
            return 0;
        }
        out.put(static_cast<char>(sym));
        msgSz++;
        sym = ReadSymb(in, cLgth, codes, firstCodes, maxLgth);
    }
    delete [] cLgth;
    delete [] cNum;
    delete [] firstCodes;
    return msgSz;
}


THDecoder::~THDecoder() {}
