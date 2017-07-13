#include "HC.h"
#include "LZWcomp.h"
#include "CRC32.h"
#include <cstdint>
#include <string>
#include <vector>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem; 

enum ARGS {
    STDOUT     = 1,
    DECOMPRESS = 2,
    LIST       = 4,
    TEST       = 8,
    RECURSIVE = 16,
    STDIN     = 32
};

uint8_t CompDeg = 6;
uint8_t Opts = 0;
std::vector<std::string> MainArgs;
bool Err = false;
std::string Error = "";
const std::string Suf = ".arc";
const std::string ArcPref = {1, 59, 127};

void ParseArgs(int8_t argc, char** argv){
    for (int8_t i = 1; i < argc; i++){
        if (Err){
            return;
        }
        std::string arg = argv[i];
        if (arg == "-"){
            Opts |= STDIN;
        }
        else if (arg[0] == '-'){
            int16_t argSz = arg.length();
            for (int16_t j = 1; j < argSz; j++){
                char c = arg[j];
                switch(c){
                    case 'c':{
                        Opts |= STDOUT;
                        break;
                    }
                    case 'd':{
                        Opts |= DECOMPRESS;
                        break;
                    }
                    case 'l':{
                        Opts |= LIST;
                        break;
                    }
                    case 't':{
                        Opts |= TEST;
                        break;
                    }
                    case 'r':{
                        Opts |= RECURSIVE;
                        break;
                    }
                    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
                    case '9': {
                        CompDeg = c - '0';
                        break;
                    }
                    default:{
                        Err = true;
                        Error = "Error: invalid arguments.";
                        break;
                    }
                }
            }
        }
        else {
            MainArgs.push_back(arg);
        }
    }
    if ((Opts & (LIST | DECOMPRESS)) == (LIST | DECOMPRESS)){
        Opts ^= DECOMPRESS;
    }
    if ((Opts & (TEST | DECOMPRESS)) == (TEST | DECOMPRESS)){
        Opts ^= DECOMPRESS;
    }
    if ((Opts & (LIST | TEST)) == (LIST | TEST)){
        Opts ^= TEST;
    }
    if (MainArgs.empty() && !(Opts & STDIN)){
        Err = true;
        Error = "Error: file name is missing.";
        return;
    }
}

void WriteHeader(std::ostream& out, uint64_t origLgth, uint64_t encSz, uint32_t chkSum){
    out.write(ArcPref.c_str(), ArcPref.length());
    out.put(static_cast<char>(CompDeg));
    for (size_t i = 0; i < 8; i++){
        char byte = 0xff;
        byte = static_cast<char>(origLgth >> (56 - 8 * i)) & byte;
        out.put(byte);
    }
    for (size_t i = 0; i < 8; i++){
        char byte = 0xff;
        byte = static_cast<char>(encSz >> (56 - 8 * i)) & byte;
        out.put(byte);
    }
    for (size_t i = 0; i < 4; i++){
        char byte = 0xff;
        byte = static_cast<char>(chkSum >> (24 - 8 * i)) & byte;
        out.put(byte);
    }
}

void ReadGarbageHeader(std::ifstream& in, uint64_t* origLgth, uint64_t* encSz, uint32_t* chkSum){
    char c;
    uint8_t bt = 0;
    int8_t compSym = 0, prefSz = ArcPref.length();
    bool isHeader = false;
    while (!in.eof() && (compSym < prefSz)){
        in.get(c);
        bt = static_cast<uint8_t>(c);
        if (bt == ArcPref[compSym]){
            compSym++;
        }
        else if (compSym > 0){
            compSym--;
        }
    }
    if (in.eof()){
        Err = true;
        return;
    }
    uint64_t readOrLg = 0, readEncSz = 0;
    uint32_t readChkSum = 0;
    in.get(c);
    CompDeg = static_cast<uint8_t>(c);
    if (in.eof()){
        Err = true;
        Error = "Unexpected end of file.";
        return;
    }
    for (int8_t i = 0; i < 8; i++){
        in.get(c);
        if (in.eof()){
            Err = true;
            Error = "Unexpected end of file.";
            return;
        }
        bt = static_cast<uint8_t>(c);
        readOrLg |= (static_cast<uint64_t>(bt) << (56 - 8 * i));
    }
    for (int8_t i = 0; i < 8; i++){
        in.get(c);
        if (in.eof()){
            Err = true;
            Error = "Unexpected end of file.";
            return;
        }
        bt = static_cast<uint8_t>(c);
        readEncSz |= (static_cast<uint64_t>(bt) << (56 - 8 * i));
    }
    for (int8_t i = 0; i < 4; i++){
        in.get(c);
        if (in.eof()){
            Err = true;
            Error = "Unexpected end of file.";
            return;
        }
        bt = static_cast<uint8_t>(c);
        readChkSum |= (static_cast<uint64_t>(bt) << (24 - 8 * i));
    }
    *origLgth = readOrLg;
    *encSz = readEncSz;
    *chkSum = readChkSum;
}

void DecompressProc(std::istream& in, std::ostream& out){
    std::string tmpLZWNm = fs::temp_directory_path();
    std::string tmpHCNm = tmpLZWNm;
    tmpLZWNm += "/ArchiverTempLZW";
    tmpHCNm += "/ArchiverTempHC";
    std::ofstream tmpOut(tmpHCNm);
    THDecoder HDecoder;
    HDecoder.Decode(in, tmpOut, Error);
    tmpOut.close();
    if (!Error.empty()){
        Err = true;
        fs::remove(tmpHCNm);
        return;
    }
    std::ifstream tmpIn(tmpHCNm);
    TLZWDecoder LZWDecoder;
    LZWDecoder.Decode(tmpIn, out, Error, CompDeg);
    tmpIn.close();
    fs::remove(tmpHCNm);
    if (!Error.empty()){
        Err = true;
    }
}

void Decompress(int32_t argNo){
    std::string inNm, outNm;
    std::filebuf outFB;
    if (argNo == -1){
        inNm = fs::temp_directory_path();
        inNm += "/ArchiverStdInTmp";
        std::ofstream tmpOut(inNm);
        if (!tmpOut){
            Err = true;
            Error = "Can't create temp file.";
            return;
        }
        char c;
        while (std::cin.get(c)){
            tmpOut.put(c);
        }
        tmpOut.close();
    }
    else {
        fs::path fPath(MainArgs[argNo]);
        fs::file_status fStat = fs::status(fPath);
        if (fStat.type() == fs::file_type::directory){
            if (!(Opts & RECURSIVE)){
                Err = true;
                Error = "This is a directory.";
                return;
            }
            for (auto& p : fs::recursive_directory_iterator(MainArgs[argNo])){
                fPath = p;
                fStat = fs::status(fPath);
                if (fStat.type() != fs::file_type::directory){
                    MainArgs.push_back(fPath);
                }
            }
            return;
        }
        else {
            inNm = MainArgs[argNo];
            if (inNm.length() < Suf.length() || inNm.substr(inNm.length() - Suf.length()) != Suf){
                inNm += Suf;
            }
        }
    }
    std::ifstream in(inNm);
    if (!in){
        Err = true;
        if (argNo == -1){
            Error = "Can't use temp file.";
        }
        else {
            Error = "File can't be opened.";
        }
        return;
    }
    uint64_t origLgth = 0, encSz = 0;
    uint32_t chkSum = 0;
    outNm = inNm.substr(0, inNm.length() - Suf.length());
    outFB.open(outNm, std::ios_base::out);
    std::ostream kostyl(&outFB);
    std::ostream& out = (argNo == -1 || Opts & STDOUT) ? std::cout : kostyl;
    std::string tmpOutNm = fs::temp_directory_path();
    tmpOutNm += "/ArchiverTmpDecompress";
    std::ofstream crFile(tmpOutNm);
    crFile.close();
    ReadGarbageHeader(in, &origLgth, &encSz, &chkSum);
    while (!Err){
        std::ofstream tmpOut(tmpOutNm);
        DecompressProc(in, tmpOut);
        if (Err) {
            break;
        }
        uint64_t decLgth = 0;
        tmpOut.close();
        std::ifstream tmpIn(tmpOutNm);
        uint64_t decChkSum = CRC32(tmpIn, &decLgth);
        tmpIn.close();
        if (decChkSum != chkSum){
            Err = true;
            Error += "Checksum error. ";
        }
        if (decLgth != origLgth){
            Err = true;
            Error += "Length error. ";
        }
        tmpIn.open(tmpOutNm);
        char c;
        while (tmpIn.get(c)){
            out.put(c);
        }
        tmpIn.close();
        ReadGarbageHeader(in, &origLgth, &encSz, &chkSum);
    }
    in.close();
    if (Error.empty()){
        Err = false;
    }
    if (!Err || argNo == -1){
        fs::remove(inNm);
    }
    outFB.close();
    if (Err){
        fs::remove(outNm);
    }
    fs::remove(tmpOutNm);
    return;
}

void List(int32_t argNo){
    std::string inNm;
    if (argNo == -1){
        inNm = fs::temp_directory_path();
        inNm += "/ArchiverStdInTmp";
        std::ofstream tmpOut(inNm);
        if (!tmpOut){
            Err = true;
            Error = "Can't create temp file.";
            return;
        }
        char c;
        while (std::cin.get(c)){
            tmpOut.put(c);
        }
        tmpOut.close();
    }
    else {
        fs::path fPath(MainArgs[argNo]);
        fs::file_status fStat = fs::status(fPath);
        if (fStat.type() == fs::file_type::directory){
            if (!(Opts & RECURSIVE)){
                Err = true;
                Error = "This is a directory.";
                return;
            }
            for (auto& p : fs::recursive_directory_iterator(MainArgs[argNo])){
                fPath = p;
                fStat = fs::status(fPath);
                if (fStat.type() != fs::file_type::directory){
                    MainArgs.push_back(fPath);
                }
            }
            return;
        }
        else {
            inNm = MainArgs[argNo];
            if (inNm.length() < Suf.length() || inNm.substr(inNm.length() - Suf.length()) != Suf){
                inNm += Suf;
            }
        }
    }
    std::ifstream in(inNm);
    if (!in){
        Err = true;
        if (argNo == -1){
            Error = "Can't use temp file.";
        }
        else {
            Error = "File can't be opened.";
        }
        return;
    }
    uint64_t origLgth = 0, tmpLgth = 0, encSz = 0, tmpEncSz = 0;
    uint32_t chkSum = 0;
    ReadGarbageHeader(in, &tmpLgth, &tmpEncSz, &chkSum);
    while (!Err){
        origLgth += tmpLgth;
        encSz += tmpEncSz;
        in.ignore(tmpEncSz);
        if (in.eof()){
            Err = true;
        }
        ReadGarbageHeader(in, &tmpLgth, &tmpEncSz, &chkSum);
    }
    if (Error.empty()){
        if (origLgth == 0){
            Error = "Invalid file.";
        }
        else {
            Err = false;
        }
    }
    in.close();
    if (!Err){
        if (argNo == -1){
            std::cerr << "Standart input:\n";
        }
        else {
            std::cerr << inNm << ":\n";
        }
        std::cerr << "Uncompressed length: " << origLgth << '\n';
        std::cerr << "Compressed size: " << fs::file_size(fs::path(inNm)) << '\n';
        std::cerr << "Compression ratio: ";
        double compRatio;
        if (encSz <= origLgth){
            compRatio = static_cast<double>(encSz) / static_cast<double>(origLgth);
        }
        else {
            compRatio = -1.0 * (static_cast<double>(origLgth) / static_cast<double>(encSz));
        }
        std::cerr.precision(2);
        std::cerr << compRatio << '\n';
        std::cerr << "(Possible garbage is ignored.)" << std::endl;
    }
    if (argNo == -1){
        fs::remove(inNm);
    }
    return;
}

void Test(int32_t argNo){
    std::string inNm;
    if (argNo == -1){
        inNm = fs::temp_directory_path();
        inNm += "/ArchiverStdInTmp";
        std::ofstream tmpOut(inNm);
        if (!tmpOut){
            Err = true;
            Error = "Can't create temp file.";
            return;
        }
        char c;
        while (std::cin.get(c)){
            tmpOut.put(c);
        }
        tmpOut.close();
    }
    else {
        fs::path fPath(MainArgs[argNo]);
        fs::file_status fStat = fs::status(fPath);
        if (fStat.type() == fs::file_type::directory){
            if (!(Opts & RECURSIVE)){
                Err = true;
                Error = "This is a directory.";
                return;
            }
            for (auto& p : fs::recursive_directory_iterator(MainArgs[argNo])){
                fPath = p;
                fStat = fs::status(fPath);
                if (fStat.type() != fs::file_type::directory){
                    MainArgs.push_back(fPath);
                }
            }
            return;
        }
        else {
            inNm = MainArgs[argNo];
            if (inNm.length() < Suf.length() || inNm.substr(inNm.length() - Suf.length()) != Suf){
                inNm += Suf;
            }
        }
    }
    std::ifstream in(inNm);
    if (!in){
        Err = true;
        if (argNo == -1){
            Error = "Can't use temp file.";
        }
        else {
            Error = "File can't be opened.";
        }
        return;
    }
    uint64_t origLgth = 0, encSz = 0;
    uint32_t chkSum = 0;
    std::string tmpOutNm = fs::temp_directory_path();
    tmpOutNm += "/ArchiverTempTest";
    std::ofstream crFile(tmpOutNm);
    crFile.close();
    ReadGarbageHeader(in, &origLgth, &encSz, &chkSum);
    while (!Err){
        std::ofstream tmpOut(tmpOutNm);
        DecompressProc(in, tmpOut);
        if (Err) {
            break;
        }
        uint64_t decLgth = 0;
        tmpOut.close();
        std::ifstream tmpIn(tmpOutNm);
        uint64_t decChkSum = CRC32(tmpIn, &decLgth);
        tmpIn.close();
        if (decChkSum != chkSum){
            Err = true;
            Error += "Checksum error. ";
        }
        if (decLgth != origLgth){
            Err = true;
            Error += "Length error. ";
        }
        ReadGarbageHeader(in, &origLgth, &encSz, &chkSum);
    }
    if (Error.empty()){
        Err = false;
    }
    in.close();
    fs::remove(tmpOutNm);
    return;
}

void Compress(int32_t argNo){
    std::filebuf outFB;
    std::ifstream in;
    std::string inNm;
    if (argNo == -1){
        inNm = fs::temp_directory_path();
        inNm += "/ArchiverStdInTmp";
        std::ofstream tmpOut(inNm);
        if (!tmpOut){
            Err = true;
            Error = "Can't create temp file.";
            return;
        }
        char c;
        while (std::cin.get(c)){
            tmpOut.put(c);
        }
        tmpOut.close();
    }
    else {
        fs::path fPath(MainArgs[argNo]);
        fs::file_status fStat = fs::status(fPath);
        if (fStat.type() == fs::file_type::directory){
            if (!(Opts & RECURSIVE)){
                Err = true;
                Error = "This is a directory.";
                return;
            }
            for (auto& p : fs::recursive_directory_iterator(MainArgs[argNo])){
                fPath = p;
                fStat = fs::status(fPath);
                if (fStat.type() != fs::file_type::directory){
                    MainArgs.push_back(fPath);
                }
            }
            return;
        }
        else {
            inNm = MainArgs[argNo];
        }
    }
    in.open(inNm);
    if (!in){
        Err = true;
        if (argNo == -1){
            Error = "Can't use temp file.";
        }
        else {
            Error = "File can't be opened.";
        }
        return;
    }
    if (argNo != -1){
        outFB.open(MainArgs[argNo] + Suf, std::ios_base::out);
        if (!outFB.is_open()){
            Err = true;
            Error = "Output file can't be created.";
            return;
        }
    }
    std::ostream kostyl(&outFB);
    std::ostream& out = (argNo == -1 || Opts & STDOUT) ? std::cout : kostyl;
    std::string tmpFNmLZW = fs::temp_directory_path();
    std::string tmpFNmHC = tmpFNmLZW;
    tmpFNmLZW += "/ArchiverTempLZW";
    tmpFNmHC += "/ArchiverTempHC";
    std::ofstream tmpOut(tmpFNmLZW);
    if (!tmpOut){
        Err = true;
        Error = "Temp file can't be created.";
        return;
    }
    TLZWEncoder LZWEncoder;
    LZWEncoder.Encode(in, tmpOut, CompDeg);
    tmpOut.close();
    in.close();
    in.open(tmpFNmLZW);
    tmpOut.open(tmpFNmHC);
    if (!in || !tmpOut){
        Err = true;
        Error = "Can't use temp file.";
        return;
    }
    THEncoder HEncoder;
    uint64_t encSz = HEncoder.Encode(in, tmpOut, Error);
    in.close();
    tmpOut.close();
    fs::remove(tmpFNmLZW);
    in.open(inNm);
    if (!in){
        Err = true;
        Error = "Can't use temp file.";
        return;
    }
    uint64_t origLgth = 0;
    if (in.eof()) std::cout<< "hui\n";
    uint32_t ctrlSum = CRC32(in, &origLgth);
    in.close();
    WriteHeader(out, origLgth, encSz, ctrlSum);
    in.open(tmpFNmHC);
    if (!in){
        Err = true;
        Error = "Can't use temp file.";
        return;
    }
    char c;
    while (in.get(c)){
        out.put(c);
    }
    in.close();
    outFB.close();
    fs::remove(tmpFNmHC);
    if (!(Opts & STDOUT)){
        fs::remove(inNm);
    }
}


int main(int32_t argc, char** argv){
    ParseArgs(argc, argv);
    if (Err){
        std::cerr << Error << std::endl;
        return 1;
    }
    int32_t i = (Opts & STDIN) ? -1 : 0;
    for (; i < (int32_t)MainArgs.size(); i++){
        if (Opts & DECOMPRESS){
            Decompress(i);
        }
        else if (Opts & LIST){
            List(i);
        }
        else if (Opts & TEST){
            Test(i);
        }
        else {
            Compress(i);
        }
        if (Err){
            if (i == -1){
                std::cerr << "Stdin:\n";
            }
            else {
                std::cerr << MainArgs[i] << '\n';
            }
            std::cerr << Error << std::endl;
            Err = false;
            Error = "";
        }
    }
    return 0;
}
