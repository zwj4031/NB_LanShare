// qrcode.cpp - QR code encoder ported from luaqrcode (BSD-3-Clause)
// Copyright (c) 2012-2020, Patrick Gundlach and contributors
// https://github.com/speedata/luaqrcode
#include "common.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace {

// capacity[version-1][ec_level-1]  (ec: 1=L 2=M 3=Q 4=H)
const int kCapacity[40][4] = {
    {  19,   16,   13,    9},{  34,   28,   22,   16},{  55,   44,   34,   26},{  80,   64,   48,   36},
    { 108,   86,   62,   46},{ 136,  108,   76,   60},{ 156,  124,   88,   66},{ 194,  154,  110,   86},
    { 232,  182,  132,  100},{ 274,  216,  154,  122},{ 324,  254,  180,  140},{ 370,  290,  206,  158},
    { 428,  334,  244,  180},{ 461,  365,  261,  197},{ 523,  415,  295,  223},{ 589,  453,  325,  253},
    { 647,  507,  367,  283},{ 721,  563,  397,  313},{ 795,  627,  445,  341},{ 861,  669,  485,  385},
    { 932,  714,  512,  406},{1006,  782,  568,  442},{1094,  860,  614,  464},{1174,  914,  664,  514},
    {1276, 1000,  718,  538},{1370, 1062,  754,  596},{1468, 1128,  808,  628},{1531, 1193,  871,  661},
    {1631, 1267,  911,  701},{1735, 1373,  985,  745},{1843, 1455, 1033,  793},{1955, 1541, 1115,  845},
    {2071, 1631, 1171,  901},{2191, 1725, 1231,  961},{2306, 1812, 1286,  986},{2434, 1914, 1354, 1054},
    {2566, 1992, 1426, 1096},{2702, 2102, 1502, 1142},{2812, 2216, 1582, 1222},{2956, 2334, 1666, 1276},
};

// EC block layout: flat (count,total,data) triples, up to 2 groups -> 6 ints.
const int kEcBlocks[40][4][6] = {
{{1,26,19},{1,26,16},{1,26,13},{1,26,9}},
{{1,44,34},{1,44,28},{1,44,22},{1,44,16}},
{{1,70,55},{1,70,44},{2,35,17},{2,35,13}},
{{1,100,80},{2,50,32},{2,50,24},{4,25,9}},
{{1,134,108},{2,67,43},{2,33,15,2,34,16},{2,33,11,2,34,12}},
{{2,86,68},{4,43,27},{4,43,19},{4,43,15}},
{{2,98,78},{4,49,31},{2,32,14,4,33,15},{4,39,13,1,40,14}},
{{2,121,97},{2,60,38,2,61,39},{4,40,18,2,41,19},{4,40,14,2,41,15}},
{{2,146,116},{3,58,36,2,59,37},{4,36,16,4,37,17},{4,36,12,4,37,13}},
{{2,86,68,2,87,69},{4,69,43,1,70,44},{6,43,19,2,44,20},{6,43,15,2,44,16}},
{{4,101,81},{1,80,50,4,81,51},{4,50,22,4,51,23},{3,36,12,8,37,13}},
{{2,116,92,2,117,93},{6,58,36,2,59,37},{4,46,20,6,47,21},{7,42,14,4,43,15}},
{{4,133,107},{8,59,37,1,60,38},{8,44,20,4,45,21},{12,33,11,4,34,12}},
{{3,145,115,1,146,116},{4,64,40,5,65,41},{11,36,16,5,37,17},{11,36,12,5,37,13}},
{{5,109,87,1,110,88},{5,65,41,5,66,42},{5,54,24,7,55,25},{11,36,12,7,37,13}},
{{5,122,98,1,123,99},{7,73,45,3,74,46},{15,43,19,2,44,20},{3,45,15,13,46,16}},
{{1,135,107,5,136,108},{10,74,46,1,75,47},{1,50,22,15,51,23},{2,42,14,17,43,15}},
{{5,150,120,1,151,121},{9,69,43,4,70,44},{17,50,22,1,51,23},{2,42,14,19,43,15}},
{{3,141,113,4,142,114},{3,70,44,11,71,45},{17,47,21,4,48,22},{9,39,13,16,40,14}},
{{3,135,107,5,136,108},{3,67,41,13,68,42},{15,54,24,5,55,25},{15,43,15,10,44,16}},
{{4,144,116,4,145,117},{17,68,42},{17,50,22,6,51,23},{19,46,16,6,47,17}},
{{2,139,111,7,140,112},{17,74,46},{7,54,24,16,55,25},{34,37,13}},
{{4,151,121,5,152,122},{4,75,47,14,76,48},{11,54,24,14,55,25},{16,45,15,14,46,16}},
{{6,147,117,4,148,118},{6,73,45,14,74,46},{11,54,24,16,55,25},{30,46,16,2,47,17}},
{{8,132,106,4,133,107},{8,75,47,13,76,48},{7,54,24,22,55,25},{22,45,15,13,46,16}},
{{10,142,114,2,143,115},{19,74,46,4,75,47},{28,50,22,6,51,23},{33,46,16,4,47,17}},
{{8,152,122,4,153,123},{22,73,45,3,74,46},{8,53,23,26,54,24},{12,45,15,28,46,16}},
{{3,147,117,10,148,118},{3,73,45,23,74,46},{4,54,24,31,55,25},{11,45,15,31,46,16}},
{{7,146,116,7,147,117},{21,73,45,7,74,46},{1,53,23,37,54,24},{19,45,15,26,46,16}},
{{5,145,115,10,146,116},{19,75,47,10,76,48},{15,54,24,25,55,25},{23,45,15,25,46,16}},
{{13,145,115,3,146,116},{2,74,46,29,75,47},{42,54,24,1,55,25},{23,45,15,28,46,16}},
{{17,145,115},{10,74,46,23,75,47},{10,54,24,35,55,25},{19,45,15,35,46,16}},
{{17,145,115,1,146,116},{14,74,46,21,75,47},{29,54,24,19,55,25},{11,45,15,46,46,16}},
{{13,145,115,6,146,116},{14,74,46,23,75,47},{44,54,24,7,55,25},{59,46,16,1,47,17}},
{{12,151,121,7,152,122},{12,75,47,26,76,48},{39,54,24,14,55,25},{22,45,15,41,46,16}},
{{6,151,121,14,152,122},{6,75,47,34,76,48},{46,54,24,10,55,25},{2,45,15,64,46,16}},
{{17,152,122,4,153,123},{29,74,46,14,75,47},{49,54,24,10,55,25},{24,45,15,46,46,16}},
{{4,152,122,18,153,123},{13,74,46,32,75,47},{48,54,24,14,55,25},{42,45,15,32,46,16}},
{{20,147,117,4,148,118},{40,75,47,7,76,48},{43,54,24,22,55,25},{10,45,15,67,46,16}},
{{19,148,118,6,149,119},{18,75,47,31,76,48},{34,54,24,34,55,25},{20,45,15,61,46,16}},
};

const int kRemainder[40] = {0,7,7,7,7,7,0,0,0,0,0,0,0,3,3,3,3,3,3,3,4,4,4,4,4,4,4,3,3,3,3,3,3,3,0,0,0,0,0,0};

// GF(256) exp/log tables for QR Reed-Solomon (primitive polynomial 0x11D).
int g_alphaInt[256];  // exponent -> field value ; [255] unused (0)
int g_intAlpha[256];  // field value -> exponent ; [0] = 256 (special)
struct GfInit {
    GfInit() {
        int x = 1;
        for (int i = 0; i < 255; ++i) {
            g_alphaInt[i] = x;
            g_intAlpha[x] = i;
            x <<= 1;
            if (x & 0x100) x ^= 0x11D;
        }
        g_alphaInt[255] = 0;
        g_intAlpha[0] = 256;
    }
};
GfInit g_gfInit;

const int kGp7[]  = {21,102,238,149,146,229,87,0};
const int kGp10[] = {45,32,94,64,70,118,61,46,67,251,0};
const int kGp13[] = {78,140,206,218,130,104,106,100,86,100,176,152,74,0};
const int kGp15[] = {105,99,5,124,140,237,58,58,51,37,202,91,61,183,8,0};
const int kGp16[] = {120,225,194,182,169,147,191,91,3,76,161,102,109,107,104,120,0};
const int kGp17[] = {136,163,243,39,150,99,24,147,214,206,123,239,43,78,206,139,43,0};
const int kGp18[] = {153,96,98,5,179,252,148,152,187,79,170,118,97,184,94,158,234,215,0};
const int kGp20[] = {190,188,212,212,164,156,239,83,225,221,180,202,187,26,163,61,50,79,60,17,0};
const int kGp22[] = {231,165,105,160,134,219,80,98,172,8,74,200,53,221,109,14,230,93,242,247,171,210,0};
const int kGp24[] = {21,227,96,87,232,117,0,111,218,228,226,192,152,169,180,159,126,251,117,211,48,135,121,229,0};
const int kGp26[] = {70,218,145,153,227,48,102,13,142,245,21,161,53,165,28,111,201,145,17,118,182,103,2,158,125,173,0};
const int kGp28[] = {123,9,37,242,119,212,195,42,87,245,43,21,201,232,27,205,147,195,190,110,180,108,234,224,104,200,223,168,0};
const int kGp30[] = {180,192,40,238,216,251,37,156,130,224,193,226,173,42,125,222,96,239,86,110,48,50,182,179,31,216,152,145,173,41,0};

int GpLen(int n) {
    switch (n) {
        case 7: return 8; case 10: return 11; case 13: return 14; case 15: return 16;
        case 16: return 17; case 17: return 18; case 18: return 19; case 20: return 21;
        case 22: return 23; case 24: return 25; case 26: return 27; case 28: return 29;
        case 30: return 31;
    }
    return 0;
}
const int* GpPtr(int n) {
    switch (n) {
        case 7: return kGp7; case 10: return kGp10; case 13: return kGp13; case 15: return kGp15;
        case 16: return kGp16; case 17: return kGp17; case 18: return kGp18; case 20: return kGp20;
        case 22: return kGp22; case 24: return kGp24; case 26: return kGp26; case 28: return kGp28;
        case 30: return kGp30;
    }
    return NULL;
}

std::string Binary(int x, int digits) {
    char h[32];
    sprintf(h, "%x", x);
    std::string s;
    for (size_t i = 0; h[i]; ++i) {
        int v = 0;
        char c = h[i];
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else v = 0;
        s += (v & 8) ? '1' : '0';
        s += (v & 4) ? '1' : '0';
        s += (v & 2) ? '1' : '0';
        s += (v & 1) ? '1' : '0';
    }
    size_t p = s.find_first_not_of('0');
    s = (p == std::string::npos) ? std::string() : s.substr(p);
    while ((int)s.size() < digits) s = "0" + s;
    return s;
}

int GetMode(const std::string& str) {
    if (str.empty()) return 4;
    bool numeric = true;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] < '0' || str[i] > '9') { numeric = false; break; }
    }
    if (numeric) return 1;
    static const char* alnum = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*./:+-";
    bool al = true;
    for (size_t i = 0; i < str.size(); ++i) {
        if (!strchr(alnum, str[i])) { al = false; break; }
    }
    if (al) return 2;
    return 4;
}

void GetVersionEcLevel(int len, int mode, int requested, int& out_version, int& out_ec) {
    int local_mode = mode;
    if (mode == 4) local_mode = 3;
    else if (mode == 8) local_mode = 4;

    const int tab[3][4] = {{10,9,8,8},{12,11,16,10},{14,13,16,12}};
    int minversion = 99;
    int maxec = requested;
    int minlv = 1, maxlv = 4;
    if (requested >= 1 && requested <= 4) { minlv = requested; maxlv = requested; }

    for (int ec = minlv; ec <= maxlv; ++ec) {
        for (int version = 1; version <= 40; ++version) {
            int bits = kCapacity[version - 1][ec - 1] * 8 - 4;
            int digits = 0;
            if (version < 10) digits = tab[0][local_mode - 1];
            else if (version < 27) digits = tab[1][local_mode - 1];
            else digits = tab[2][local_mode - 1];
            int modebits = bits - digits;
            int c = 0;
            if (local_mode == 1) c = (int)floor(modebits * 3.0 / 10.0);
            else if (local_mode == 2) c = (int)floor(modebits * 2.0 / 11.0);
            else if (local_mode == 3) c = (int)floor(modebits * 1.0 / 8.0);
            else c = (int)floor(modebits * 1.0 / 13.0);
            if (c >= len) {
                if (version <= minversion) { minversion = version; maxec = ec; }
                break;
            }
        }
    }
    out_version = minversion;
    out_ec = maxec;
}

std::string GetLength(const std::string& str, int version, int mode) {
    int i = mode;
    if (mode == 4) i = 3;
    else if (mode == 8) i = 4;
    const int tab[3][4] = {{10,9,8,8},{12,11,16,10},{14,13,16,12}};
    int digits;
    if (version < 10) digits = tab[0][i - 1];
    else if (version < 27) digits = tab[1][i - 1];
    else digits = tab[2][i - 1];
    return Binary((int)str.size(), digits);
}

// Indexed by (ascii_byte - 32). Covers chars 32..127.
const int kAsciiTbl[96] = {
    36,-1,-1,-1,37,38,-1,-1,-1,-1,39,40,-1,41,42,43,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,44,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
    25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

std::string EncodeNumeric(const std::string& str) {
    std::string out;
    for (size_t i = 0; i < str.size(); i += 3) {
        std::string a = str.substr(i, 3);
        int v = atoi(a.c_str());
        out += Binary(v, (int)a.size() * 3 + 1);
    }
    return out;
}

std::string EncodeAscii(const std::string& str) {
    std::string out;
    for (size_t i = 0; i < str.size(); i += 2) {
        std::string a = str.substr(i, 2);
        if (a.size() == 2) {
            int b1 = kAsciiTbl[(unsigned char)a[0] - 32];
            int b2 = kAsciiTbl[(unsigned char)a[1] - 32];
            out += Binary(b1 * 45 + b2, 11);
        } else {
            int v = kAsciiTbl[(unsigned char)a[0] - 32];
            out += Binary(v, 6);
        }
    }
    return out;
}

std::string EncodeBinary(const std::string& str) {
    std::string out;
    for (size_t i = 0; i < str.size(); ++i) {
        out += Binary((unsigned char)str[i], 8);
    }
    return out;
}

std::string EncodeData(const std::string& str, int mode) {
    if (mode == 1) return EncodeNumeric(str);
    if (mode == 2) return EncodeAscii(str);
    return EncodeBinary(str);
}

std::string AddPadData(int version, int ec, const std::string& data) {
    int cpty = kCapacity[version - 1][ec - 1] * 8;
    std::string buffer = data;
    int len = (int)buffer.size();
    int count_to_pad = std::min(4, cpty - len);
    if (count_to_pad > 0) { buffer += std::string(count_to_pad, '0'); len += count_to_pad; }
    if (len % 8 != 0) {
        int missing = 8 - len % 8;
        buffer += std::string(missing, '0');
        len += missing;
    }
    int remaining_bytes = (cpty - len) / 8;
    for (int i = 1; i <= remaining_bytes; ++i) {
        buffer += (i % 2 == 1) ? "11101100" : "00010001";
    }
    return buffer;
}

std::vector<int> ConvertBitstringToBytes(const std::string& data) {
    std::vector<int> msg;
    int n = (int)data.size() / 8;
    for (int i = 0; i < n; ++i) {
        int v = 0;
        for (int b = 0; b < 8; ++b) v = (v << 1) | (data[i * 8 + b] - '0');
        msg.push_back(v);
    }
    return msg;
}

std::vector<int> CalculateErrorCorrection(const std::vector<int>& mp, int num_ec) {
    int len_message = (int)mp.size();
    int highest_exponent = len_message + num_ec - 1;
    std::vector<int> mp_int(highest_exponent + 2, 0);
    std::vector<int> gp_alpha(highest_exponent + 1, 0);

    for (int i = 1; i <= len_message; ++i) {
        mp_int[highest_exponent - i + 1] = mp[i - 1];
    }
    for (int i = 1; i <= highest_exponent - len_message; ++i) mp_int[i] = 0;
    mp_int[0] = 0;

    const int* gp = GpPtr(num_ec);
    int gplen = GpLen(num_ec);

    while (highest_exponent >= num_ec) {
        // get_generator_polynomial_adjusted
        for (int i = 0; i <= highest_exponent; ++i) gp_alpha[i] = 0;
        for (int i = 0; i < gplen; ++i) {
            gp_alpha[highest_exponent - num_ec + i] = gp[i];
        }

        int exp = g_intAlpha[mp_int[highest_exponent]];
        for (int i = highest_exponent; i >= highest_exponent - num_ec; --i) {
            if (exp != 256) gp_alpha[i] = (gp_alpha[i] + exp) % 255;
            else gp_alpha[i] = 256;
        }
        for (int i = highest_exponent - num_ec - 1; i >= 0; --i) gp_alpha[i] = 256;

        for (int i = highest_exponent; i >= 0; --i) {
            int ai = (gp_alpha[i] == 256) ? 0 : g_alphaInt[gp_alpha[i]];
            mp_int[i] = ai ^ mp_int[i];
        }
        for (int i = highest_exponent; i >= num_ec; --i) {
            if (mp_int[i] == 0) highest_exponent = i - 1;
            else break;
        }
        if (highest_exponent < num_ec) break;
    }

    std::vector<int> ret;
    for (int i = highest_exponent; i >= 0; --i) ret.push_back(mp_int[i]);
    return ret;
}

std::string ArrangeCodewords(int version, int ec, const std::string& data) {
    std::vector<std::string> datablocks;
    std::vector<std::string> final_ecblocks;
    int pos = 0;
    const int* blk = kEcBlocks[version - 1][ec - 1];
    for (int g = 0; g < 2; ++g) {
        int count = blk[g * 3 + 0];
        int total = blk[g * 3 + 1];
        int dataCW = blk[g * 3 + 2];
        if (count == 0) continue;
        int size_ec = total - dataCW;
        for (int k = 0; k < count; ++k) {
            std::string datablock = data.substr(pos * 8, dataCW * 8);
            std::vector<int> mp = ConvertBitstringToBytes(datablock);
            std::vector<int> ecv = CalculateErrorCorrection(mp, size_ec);
            std::string ecstr;
            for (size_t x = 0; x < ecv.size(); ++x) ecstr += Binary(ecv[x], 8);
            datablocks.push_back(datablock);
            final_ecblocks.push_back(ecstr);
            pos += dataCW;
        }
    }

    std::string arranged;
    int maxBlockLen = 0;
    for (size_t i = 0; i < datablocks.size(); ++i)
        maxBlockLen = std::max(maxBlockLen, (int)datablocks[i].size());
    for (int p = 0; p < maxBlockLen; p += 8) {
        for (size_t i = 0; i < datablocks.size(); ++i) {
            if (p < (int)datablocks[i].size())
                arranged += datablocks[i].substr(p, 8);
        }
    }
    maxBlockLen = 0;
    for (size_t i = 0; i < final_ecblocks.size(); ++i)
        maxBlockLen = std::max(maxBlockLen, (int)final_ecblocks[i].size());
    for (int p = 0; p < maxBlockLen; p += 8) {
        for (size_t i = 0; i < final_ecblocks.size(); ++i) {
            if (p < (int)final_ecblocks[i].size())
                arranged += final_ecblocks[i].substr(p, 8);
        }
    }
    return arranged;
}

const int kAlignmentPattern[41][8] = {
    {0},{0},{6,18},{6,22},{6,26},{6,30},{6,34},
    {6,22,38},{6,24,42},{6,26,46},{6,28,50},{6,30,54},{6,32,58},{6,34,62},
    {6,26,46,66},{6,26,48,70},{6,26,50,74},{6,30,54,78},{6,30,56,82},{6,30,58,86},{6,34,62,90},
    {6,28,50,72,94},{6,26,50,74,98},{6,30,54,78,102},{6,28,54,80,106},{6,32,58,84,110},{6,30,58,86,114},{6,34,62,90,118},
    {6,26,50,74,98,122},{6,30,54,78,102,126},{6,26,52,78,104,130},{6,30,56,82,108,134},{6,34,60,86,112,138},{6,30,58,86,114,142},{6,34,62,90,118,146},
    {6,30,54,78,102,126,150},{6,24,50,76,102,128,154},{6,28,54,80,106,132,158},{6,32,58,84,110,136,162},{6,26,54,82,110,138,166},{6,30,58,86,114,142,170}
};

const char* kTypeInfo[4][8] = {
{"111011111000100","111001011110011","111110110101010","111100010011101","110011000101111","110001100011000","110110001000001","110100101110110"},
{"101010000010010","101000100100101","101111001111100","101101101001011","100010111111001","100000011001110","100111110010111","100101010100000"},
{"011010101011111","011000001101000","011111100110001","011101000000110","010010010110100","010000110000011","010111011011010","010101111101101"},
{"001011010001001","001001110111110","001110011100111","001100111010000","000011101100010","000001001010101","000110100001100","000100000111011"}
};

const char* kVersionInfo[34] = {
"001010010011111000","001111011010000100","100110010101100100","110010110010010100",
"011011111101110100","010001101110001100","111000100001101100","101100000110011100","000101001001111100",
"000111101101000010","101110100010100010","111010000101010010","010011001010110010","011001011001001010",
"110000010110101010","100100110001011010","001101111110111010","001000110111000100","100001111000100100",
"110101011111010110","011100010000110110","010110000011001110","111111001100101110","101011101011011110",
"000010100100111110","101010111001000001","000011110110100001","010111010001010001","111110011110110001",
"110100001101001001","011101000010101001","001001100101011001","100000101010111001","100101100011000101"
};

typedef std::vector<std::vector<int> > Matrix;

void AddPositionDetectionPatterns(Matrix& m) {
    int size = (int)m.size() - 1;
    for (int i = 1; i <= 8; ++i) {
        for (int j = 1; j <= 8; ++j) {
            m[i][j] = -2;
            m[size - 8 + i][j] = -2;
            m[i][size - 8 + j] = -2;
        }
    }
    for (int i = 1; i <= 7; ++i) {
        m[1][i] = 2; m[7][i] = 2; m[i][1] = 2; m[i][7] = 2;
        m[size][i] = 2; m[size - 6][i] = 2; m[size - i + 1][1] = 2; m[size - i + 1][7] = 2;
        m[1][size - i + 1] = 2; m[7][size - i + 1] = 2; m[i][size - 6] = 2; m[i][size] = 2;
    }
    for (int i = 1; i <= 3; ++i) {
        for (int j = 1; j <= 3; ++j) {
            m[2 + j][i + 2] = 2;
            m[size - j - 1][i + 2] = 2;
            m[2 + j][size - i - 1] = 2;
        }
    }
}

void AddTimingPattern(Matrix& m) {
    int size = (int)m.size() - 1;
    int line = 7;
    for (int i = 9; i <= size - 8; ++i) {
        m[i][line] = (i % 2 == 0) ? -2 : 2;
        m[line][i] = (i % 2 == 0) ? -2 : 2;
    }
}

void FillMatrixPosition(Matrix& m, const std::string& bitstr, int x, int y) {
    m[x][y] = (bitstr == "1") ? 2 : -2;
}

void AddVersionInformation(Matrix& m, int version) {
    if (version < 7) return;
    int size = (int)m.size() - 1;
    const char* bitstring = kVersionInfo[version - 7];
    int start_x, start_y;
    start_x = size - 10; start_y = 1;
    for (int i = 0; bitstring[i]; ++i) {
        std::string bit(1, bitstring[i]);
        int x = start_x + (i % 3);
        int y = start_y + (i / 3);
        FillMatrixPosition(m, bit, x, y);
    }
    start_x = 1; start_y = size - 10;
    for (int i = 0; bitstring[i]; ++i) {
        std::string bit(1, bitstring[i]);
        int x = start_x + (i / 3);
        int y = start_y + (i % 3);
        FillMatrixPosition(m, bit, x, y);
    }
}

void AddAlignmentPattern(Matrix& m) {
    int size = (int)m.size() - 1;
    int version = (size - 17) / 4;
    const int* ap = kAlignmentPattern[version];
    int n = 0;
    while (n < 8 && ap[n] != 0) n++;
    for (int x = 0; x < n; ++x) {
        for (int y = 0; y < n; ++y) {
            if ((x == 0 && y == 0) || (x == n - 1 && y == 0) || (x == 0 && y == n - 1)) continue;
            int pos_x = ap[x] + 1;
            int pos_y = ap[y] + 1;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    m[pos_x + dx][pos_y + dy] = (std::max(abs(dx), abs(dy)) % 2 == 0) ? 2 : -2;
                }
            }
        }
    }
}

void AddTypeInfoToMatrix(Matrix& m, int ec, int mask) {
    const char* ec_mask_type = kTypeInfo[ec - 1][mask];
    int size = (int)m.size() - 1;
    for (int i = 1; i <= 7; ++i) FillMatrixPosition(m, std::string(1, ec_mask_type[i - 1]), 9, size - i + 1);
    for (int i = 8; i <= 9; ++i) FillMatrixPosition(m, std::string(1, ec_mask_type[i - 1]), 9, 17 - i);
    for (int i = 10; i <= 15; ++i) FillMatrixPosition(m, std::string(1, ec_mask_type[i - 1]), 9, 16 - i);
    for (int i = 1; i <= 6; ++i) FillMatrixPosition(m, std::string(1, ec_mask_type[i - 1]), i, 9);
    FillMatrixPosition(m, std::string(1, ec_mask_type[6]), 8, 9);
    for (int i = 8; i <= 15; ++i) FillMatrixPosition(m, std::string(1, ec_mask_type[i - 1]), size - 15 + i, 9);
}

Matrix PrepareMatrixWithMask(int version, int ec, int mask) {
    int size = version * 4 + 17;
    Matrix m(size + 1, std::vector<int>(size + 1, 0));
    AddPositionDetectionPatterns(m);
    AddTimingPattern(m);
    AddVersionInformation(m, version);
    m[9][size - 7] = 2;
    AddAlignmentPattern(m);
    AddTypeInfoToMatrix(m, ec, mask);
    return m;
}

bool MaskFunc(int mask, int x, int y) {
    switch (mask) {
        case 0: return ((y + x) % 2) == 0;
        case 1: return (y % 2) == 0;
        case 2: return (x % 3) == 0;
        case 3: return ((y + x) % 3) == 0;
        case 4: return ((y % 4 - 1.5) * (x % 6 - 2.5)) > 0;
        case 5: return ((y * x) % 2 + (y * x) % 3) == 0;
        case 6: return (((y * x) % 3 + y * x) % 2) == 0;
        case 7: return (((y * x) % 3 + y + x) % 2) == 0;
        default: return false;
    }
}

int GetPixelWithMask(int mask, int x, int y, int dataBit) {
    bool invert = MaskFunc(mask, x - 1, y - 1);
    bool res = ((dataBit == 0) == invert);
    return res ? 1 : -1;
}

void AddDataToMatrix(Matrix& m, const std::string& data, int mask) {
    int size = (int)m.size() - 1;
    int ptr = 1;
    int x = size, y = size;
    int x_dir = -1, y_dir = -1;
    while (true) {
        if (m[x][y] == 0) {
            int bit = (ptr - 1 < (int)data.size()) ? (data[ptr - 1] - '0') : 0;
            m[x][y] = GetPixelWithMask(mask, x, y, bit);
            ptr++;
            if (ptr > (int)data.size() || x < 0) return;
        }
        x = x + x_dir;
        if (x_dir == 1) {
            y = y + y_dir;
            if (y < 1 || y > size) {
                x = x - 2;
                if (x == 7) x = 6;
                y = (y_dir == -1) ? 1 : size;
                y_dir = -y_dir;
            }
        }
        x_dir = -x_dir;
    }
}

int CalculatePenalty(const Matrix& m) {
    int penalty1 = 0, penalty2 = 0, penalty3 = 0;
    int size = (int)m.size() - 1;
    int number_of_dark_cells = 0;
    bool last_bit_blank = false, is_blank;

    for (int x = 1; x <= size; ++x) {
        int run = 0;
        last_bit_blank = false;
        for (int y = 1; y <= size; ++y) {
            if (m[x][y] > 0) { number_of_dark_cells++; is_blank = false; }
            else is_blank = true;
            if (y == 1) { run = 1; }
            else if (last_bit_blank == is_blank) run++;
            else {
                if (run >= 5) penalty1 += run - 2;
                run = 1;
            }
            last_bit_blank = is_blank;
        }
        if (run >= 5) penalty1 += run - 2;
    }
    for (int y = 1; y <= size; ++y) {
        int run = 0;
        last_bit_blank = false;
        for (int x = 1; x <= size; ++x) {
            is_blank = (m[x][y] < 0);
            if (x == 1) { run = 1; }
            else if (last_bit_blank == is_blank) run++;
            else {
                if (run >= 5) penalty1 += run - 2;
                run = 1;
            }
            last_bit_blank = is_blank;
        }
        if (run >= 5) penalty1 += run - 2;
    }

    for (int x = 1; x <= size; ++x) {
        for (int y = 1; y <= size; ++y) {
            if ((y < size - 1) && (x < size - 1) && (
                (m[x][y] < 0 && m[x + 1][y] < 0 && m[x][y + 1] < 0 && m[x + 1][y + 1] < 0) ||
                (m[x][y] > 0 && m[x + 1][y] > 0 && m[x][y + 1] > 0 && m[x + 1][y + 1] > 0)))
                penalty2 += 3;

            if (y + 6 < size &&
                m[x][y] > 0 && m[x][y + 1] < 0 && m[x][y + 2] > 0 && m[x][y + 3] > 0 &&
                m[x][y + 4] > 0 && m[x][y + 5] < 0 && m[x][y + 6] > 0 &&
                ((y + 10 < size && m[x][y + 7] < 0 && m[x][y + 8] < 0 && m[x][y + 9] < 0 && m[x][y + 10] < 0) ||
                 (y - 4 >= 1 && m[x][y - 1] < 0 && m[x][y - 2] < 0 && m[x][y - 3] < 0 && m[x][y - 4] < 0)))
                penalty3 += 40;

            if (x + 6 <= size &&
                m[x][y] > 0 && m[x + 1][y] < 0 && m[x + 2][y] > 0 && m[x + 3][y] > 0 &&
                m[x + 4][y] > 0 && m[x + 5][y] < 0 && m[x + 6][y] > 0 &&
                ((x + 10 <= size && m[x + 7][y] < 0 && m[x + 8][y] < 0 && m[x + 9][y] < 0 && m[x + 10][y] < 0) ||
                 (x - 4 >= 1 && m[x - 1][y] < 0 && m[x - 2][y] < 0 && m[x - 3][y] < 0 && m[x - 4][y] < 0)))
                penalty3 += 40;
        }
    }

    double dark_ratio = (double)number_of_dark_cells / (double)(size * size);
    int penalty4 = (int)floor(fabs(dark_ratio * 100.0 - 50.0)) * 2;
    return penalty1 + penalty2 + penalty3 + penalty4;
}

Matrix GetMatrixAndPenalty(int version, int ec, const std::string& data, int mask, int& penalty) {
    Matrix m = PrepareMatrixWithMask(version, ec, mask);
    AddDataToMatrix(m, data, mask);
    penalty = CalculatePenalty(m);
    return m;
}

Matrix GetMatrixWithLowestPenalty(int version, int ec, const std::string& data) {
    int min_penalty = 0;
    Matrix best = GetMatrixAndPenalty(version, ec, data, 0, min_penalty);
    for (int i = 1; i <= 7; ++i) {
        int penalty = 0;
        Matrix m = GetMatrixAndPenalty(version, ec, data, i, penalty);
        if (penalty < min_penalty) { best = m; min_penalty = penalty; }
    }
    return best;
}

} // namespace

bool QrEncode(const std::string& text, int ec_level, std::vector<std::vector<int> >& out) {
    int mode = GetMode(text);
    int version = 0, ec = 0;
    GetVersionEcLevel((int)text.size(), mode, ec_level, version, ec);
    if (version > 40 || version < 1) return false;

    std::string data_raw = Binary(mode, 4);
    data_raw += GetLength(text, version, mode);
    data_raw += EncodeData(text, mode);
    data_raw = AddPadData(version, ec, data_raw);
    std::string arranged = ArrangeCodewords(version, ec, data_raw);
    if (arranged.size() % 8 != 0) return false;
    arranged += std::string(kRemainder[version - 1], '0');

    Matrix m = GetMatrixWithLowestPenalty(version, ec, arranged);
    int size = version * 4 + 17;
    out.assign(size, std::vector<int>(size, 0));
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            out[y][x] = (m[x + 1][y + 1] > 0) ? 1 : 0;
        }
    }
    return true;
}

#ifdef QR_DEBUG
bool QrDebugArranged(const std::string& text, std::vector<unsigned char>& out) {
    int mode = GetMode(text);
    int version = 0, ec = 0;
    GetVersionEcLevel((int)text.size(), mode, 4, version, ec);
    std::string data = Binary(mode, 4);
    data += GetLength(text, version, mode);
    data += EncodeData(text, mode);
    data = AddPadData(version, ec, data);
    std::string arranged = ArrangeCodewords(version, ec, data);
    out.clear();
    for (size_t i = 0; i < arranged.size(); i += 8) {
        int v = 0;
        for (int b = 0; b < 8 && i + b < arranged.size(); ++b)
            v = (v << 1) | (arranged[i + b] - '0');
        out.push_back((unsigned char)v);
    }
    return true;
}
#endif
