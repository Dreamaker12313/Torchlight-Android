#include "core/runic_strings.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tl::core {

std::string value_as_string(int32_t value) {
    char text[16];
    std::snprintf(text, sizeof text, "%" PRId32, value);
    return text;
}

std::string value_as_string(uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof text, "%" PRIu32, value);
    return text;
}

std::string value_as_string(int64_t value) {
    char text[24];
    std::snprintf(text, sizeof text, "%" PRId64, value);
    return text;
}

std::string value_as_string(float value) {
    // The console's C runtime spells what is not a number its own way: by the
    // float's bits, its sign kept, and the one negative quiet NaN with no
    // payload (the "indefinite" its arithmetic makes) apart.
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    const std::string sign = (bits >> 31) != 0 ? "-" : "";
    if ((bits & 0x7f800000u) == 0x7f800000u) {
        const uint32_t payload = bits & 0x7fffffu;
        if (payload == 0) return sign + "1.#INF";
        // A signalling NaN: the runtime's spelling. Not verified: a signalling
        // NaN cannot be passed in unchanged.
        if ((payload & 0x400000u) == 0) return sign + "1.#SNAN";
        if (bits == 0xffc00000u) return "-1.#IND";
        return sign + "1.#QNAN";
    }
    if (value == 0.0f) return sign + "0";
    // `%g`, six significant digits, rounded from the exact decimal with ties away from zero
    // (581.3125 is 581.313), exponent at least three digits (`1e+006`).
    char exact[160];
    std::snprintf(exact, sizeof exact, "%.110e", std::fabs(static_cast<double>(value)));
    std::string digits;
    const char* c = exact;
    for (; *c != 'e'; ++c)
        if (*c != '.') digits += *c;
    int exponent = std::atoi(c + 1);
    std::string kept = digits.substr(0, 6);
    if (digits[6] >= '5') {
        int i = 5;
        for (; i >= 0 && kept[static_cast<size_t>(i)] == '9'; --i)
            kept[static_cast<size_t>(i)] = '0';
        if (i >= 0) {
            ++kept[static_cast<size_t>(i)];
        } else {
            kept = "100000";
            ++exponent;
        }
    }
    std::string out = sign;
    if (exponent < -4 || exponent >= 6) {
        std::string fraction = kept.substr(1);
        while (!fraction.empty() && fraction.back() == '0') fraction.pop_back();
        out += kept.substr(0, 1);
        if (!fraction.empty()) out += "." + fraction;
        char tail[16];
        std::snprintf(tail, sizeof tail, "e%c%03d", exponent < 0 ? '-' : '+',
                      exponent < 0 ? -exponent : exponent);
        return out + tail;
    }
    std::string whole, fraction;
    if (exponent >= 0) {
        whole = kept.substr(0, static_cast<size_t>(exponent) + 1);
        fraction = kept.substr(static_cast<size_t>(exponent) + 1);
    } else {
        whole = "0";
        fraction = std::string(static_cast<size_t>(-exponent - 1), '0') + kept;
    }
    while (!fraction.empty() && fraction.back() == '0') fraction.pop_back();
    out += whole;
    if (!fraction.empty()) out += "." + fraction;
    return out;
}

std::string replace_wstring(std::string text, const std::string& find, const std::string& with) {
    // DIVERGENCE: an empty `find`, or a `with` holding `find`, hangs the console; the port
    // replaces each occurrence once, left to right.
    if (find.empty() || with.find(find) != std::string::npos) {
        if (find.empty()) return text;
        size_t at = 0;
        while ((at = text.find(find, at)) != std::string::npos) {
            text.replace(at, find.size(), with);
            at += with.size();
        }
        return text;
    }
    size_t at;
    while ((at = text.find(find, 0)) != std::string::npos) text.replace(at, find.size(), with);
    return text;
}

}  // namespace tl::core
