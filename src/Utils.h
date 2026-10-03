#pragma once
// Utils.h - helpers: money handling, SHA-256 hashing, timestamps, safe input.

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace util
{

// Money is stored in paisa (1 Rs = 100 paisa) as an integer.
// This avoids the rounding errors that double causes.
using Money = long long;

// Thrown when stdin is closed (Ctrl+D / Ctrl+Z / end of piped input)
struct InputClosed
{
};

// ---------------------------------------------------------------- strings

inline std::string trim(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline std::string toLower(std::string s)
{
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s)
    {
        if (c == delim)
        {
            out.push_back(cur);
            cur.clear();
        }
        else
        {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

// '|' is our file separator, so it must never appear inside a field.
inline std::string sanitizeField(std::string s)
{
    for (auto& c : s)
        if (c == '|' || c == '\n' || c == '\r') c = ' ';
    return trim(s);
}

inline bool allDigits(const std::string& s)
{
    if (s.empty()) return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

// ---------------------------------------------------------------- time

// "YYYY-MM-DD HH:MM:SS" in local time
inline std::string timestamp()
{
    std::time_t t = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &lt);
    return buf;
}

// ---------------------------------------------------------------- money

// Accepts "500", "500.5", "500.50", ".5". Rejects negatives, letters, >2 decimals.
inline bool parseMoney(const std::string& text, Money& out)
{
    std::string s = trim(text);
    if (s.empty()) return false;

    size_t dot = s.find('.');
    std::string ip = s.substr(0, dot);
    std::string fp = (dot == std::string::npos) ? "" : s.substr(dot + 1);

    if (ip.empty() && fp.empty()) return false;
    if (ip.size() > 12 || fp.size() > 2) return false;
    for (char c : ip)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    for (char c : fp)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;

    while (fp.size() < 2) fp += '0';
    Money whole = ip.empty() ? 0 : std::stoll(ip);
    out = whole * 100 + std::stoll(fp);
    return true;
}

// 123456 -> "Rs. 1,234.56"
inline std::string formatMoney(Money m)
{
    bool neg = m < 0;
    if (neg) m = -m;

    std::string whole = std::to_string(m / 100);
    std::string withCommas;
    int count = 0;
    for (int i = static_cast<int>(whole.size()) - 1; i >= 0; --i)
    {
        withCommas.insert(withCommas.begin(), whole[i]);
        if (++count % 3 == 0 && i != 0) withCommas.insert(withCommas.begin(), ',');
    }

    int paisa = static_cast<int>(m % 100);
    std::string frac = (paisa < 10 ? "0" : "") + std::to_string(paisa);
    return std::string("Rs. ") + (neg ? "-" : "") + withCommas + "." + frac;
}

// ---------------------------------------------------------------- hashing

inline std::string sha256(const std::string& msg)
{
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    std::vector<uint8_t> data(msg.begin(), msg.end());
    uint64_t bitLen = static_cast<uint64_t>(msg.size()) * 8;
    data.push_back(0x80);
    while (data.size() % 64 != 56) data.push_back(0);
    for (int i = 7; i >= 0; --i) data.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xff));

    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };

    for (size_t off = 0; off < data.size(); off += 64)
    {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
        {
            w[i] = (static_cast<uint32_t>(data[off + 4 * i]) << 24) |
                   (static_cast<uint32_t>(data[off + 4 * i + 1]) << 16) |
                   (static_cast<uint32_t>(data[off + 4 * i + 2]) << 8) |
                   (static_cast<uint32_t>(data[off + 4 * i + 3]));
        }
        for (int i = 16; i < 64; ++i)
        {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i)
        {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    static const char* hex = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 8; ++i)
        for (int s = 28; s >= 0; s -= 4) out += hex[(h[i] >> s) & 0xf];
    return out;
}

inline std::string randomSalt()
{
    std::random_device rd;
    std::mt19937_64 gen((static_cast<uint64_t>(rd()) << 32) ^ rd());
    static const char* hex = "0123456789abcdef";
    std::string s;
    for (int i = 0; i < 16; ++i) s += hex[gen() % 16];
    return s;
}

// PINs / passwords are never stored in plain text: we store sha256(salt + secret).
inline std::string hashSecret(const std::string& salt, const std::string& secret)
{
    return sha256(salt + secret);
}

// ---------------------------------------------------------------- input
// Everything is read with getline, so the old "cin >> then getline" newline
// bug can never happen.

inline std::string readLine(const std::string& prompt)
{
    std::cout << prompt;
    std::string s;
    if (!std::getline(std::cin, s)) throw InputClosed();
    return trim(s);
}

inline int readInt(const std::string& prompt, int lo = INT_MIN, int hi = INT_MAX)
{
    while (true)
    {
        std::string s = readLine(prompt);
        char* end = nullptr;
        long v = std::strtol(s.c_str(), &end, 10);
        if (!s.empty() && *end == '\0' && v >= lo && v <= hi) return static_cast<int>(v);

        if (lo == INT_MIN && hi == INT_MAX)
            std::cout << "Invalid input. Please enter a number.\n";
        else
            std::cout << "Invalid input. Please enter a number between " << lo << " and " << hi << ".\n";
    }
}

inline Money readAmount(const std::string& prompt, bool allowZero = false)
{
    while (true)
    {
        Money m;
        if (!parseMoney(readLine(prompt), m))
        {
            std::cout << "Invalid amount. Use digits only, max 2 decimals (e.g. 1500 or 1500.50).\n";
            continue;
        }
        if (m == 0 && !allowZero)
        {
            std::cout << "Amount must be greater than 0.\n";
            continue;
        }
        return m;
    }
}

inline std::string readPin(const std::string& prompt)
{
    while (true)
    {
        std::string s = readLine(prompt);
        if (s.size() == 4 && allDigits(s)) return s;
        std::cout << "PIN must be exactly 4 digits.\n";
    }
}

inline std::string readName(const std::string& prompt)
{
    while (true)
    {
        std::string s = sanitizeField(readLine(prompt));
        if (s.empty())
        {
            std::cout << "Name cannot be empty.\n";
            continue;
        }
        if (s.size() > 40)
        {
            std::cout << "Name is too long (max 40 characters).\n";
            continue;
        }
        return s;
    }
}

}  // namespace util
