// SPDX-License-Identifier: MIT — part of frame-apps by sasaken1102r, shipped under the host app's MIT license
// Reading the fetched list: the rules for each field, a small strict JSON reader, SHA-256 and the PNG size check.
// Nothing here trusts the network: every app must pass every rule, or it is dropped.
#include "frame_apps.h"

#include <cstdint>
#include <cstdlib>
#include <set>
#include <utility>

namespace frame_apps {

namespace {

bool isLower(char c) { return c >= 'a' && c <= 'z'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }

/** @return true if every character is a-z, 0-9 or '-', the first isn't '-' (never read as an option), and the length
 *  is in [1, max] */
bool lowerDigitDash(const std::string& s, size_t max) {
    if (s.empty() || s.size() > max || s[0] == '-') return false;
    for (char c : s) {
        if (!isLower(c) && !isDigit(c) && c != '-') return false;
    }
    return true;
}

/** A JSON value (only what apps.json needs). */
struct JValue {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<JValue> array;
    std::vector<std::pair<std::string, JValue>> object;

    /** @return the member, or nullptr (the first one if the name repeats) */
    const JValue* get(const std::string& name) const {
        for (const auto& member : object) {
            if (member.first == name) return &member.second;
        }
        return nullptr;
    }
};

/** Strict JSON (RFC 8259) reader with a depth limit. */
class JsonReader {
public:
    explicit JsonReader(const std::string& text) : s_(text) {}

    /** @return true if the whole text is one value */
    bool read(JValue& out) {
        skipSpace();
        if (!value(out, 0)) return false;
        skipSpace();
        return i_ == s_.size();
    }

private:
    const std::string& s_;
    size_t i_ = 0;

    void skipSpace() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }

    bool literal(const char* word) {
        size_t n = 0;
        while (word[n] != '\0') ++n;
        if (s_.compare(i_, n, word) != 0) return false;
        i_ += n;
        return true;
    }

    bool value(JValue& out, int depth) {
        if (depth > 16 || i_ >= s_.size()) return false;
        const char c = s_[i_];
        if (c == '{') return object(out, depth);
        if (c == '[') return array(out, depth);
        if (c == '"') {
            out.type = JValue::Type::String;
            return string(out.string);
        }
        if (c == 't' || c == 'f') {
            out.type = JValue::Type::Bool;
            out.boolean = c == 't';
            return literal(c == 't' ? "true" : "false");
        }
        if (c == 'n') {
            out.type = JValue::Type::Null;
            return literal("null");
        }
        return number(out);
    }

    bool number(JValue& out) {
        const size_t start = i_;
        if (i_ < s_.size() && s_[i_] == '-') ++i_;
        if (i_ >= s_.size() || !isDigit(s_[i_])) return false;
        if (s_[i_] == '0') {
            ++i_;
        } else {
            while (i_ < s_.size() && isDigit(s_[i_])) ++i_;
        }
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            if (i_ >= s_.size() || !isDigit(s_[i_])) return false;
            while (i_ < s_.size() && isDigit(s_[i_])) ++i_;
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
            if (i_ >= s_.size() || !isDigit(s_[i_])) return false;
            while (i_ < s_.size() && isDigit(s_[i_])) ++i_;
        }
        out.type = JValue::Type::Number;
        out.number = std::strtod(s_.substr(start, i_ - start).c_str(), nullptr);
        return true;
    }

    static void putUtf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(uint32_t& v) {
        if (i_ + 4 > s_.size()) return false;
        v = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s_[i_++];
            v <<= 4;
            if (isDigit(c)) {
                v |= static_cast<uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                v |= static_cast<uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                v |= static_cast<uint32_t>(c - 'A' + 10);
            } else {
                return false;
            }
        }
        return true;
    }

    bool string(std::string& out) {
        ++i_;  // "
        out.clear();
        while (i_ < s_.size()) {
            const unsigned char c = static_cast<unsigned char>(s_[i_]);
            if (c == '"') {
                ++i_;
                return true;
            }
            if (c < 0x20) return false;  // raw control characters aren't allowed in JSON strings
            if (c != '\\') {
                out += static_cast<char>(c);
                ++i_;
                continue;
            }
            if (++i_ >= s_.size()) return false;
            const char e = s_[i_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        uint32_t low = 0;
                        if (i_ + 2 > s_.size() || s_[i_] != '\\' || s_[i_ + 1] != 'u') return false;
                        i_ += 2;
                        if (!hex4(low) || low < 0xDC00 || low > 0xDFFF) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return false;
                    }
                    putUtf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool array(JValue& out, int depth) {
        out.type = JValue::Type::Array;
        ++i_;  // [
        skipSpace();
        if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            return true;
        }
        for (;;) {
            JValue item;
            skipSpace();
            if (!value(item, depth + 1)) return false;
            out.array.push_back(std::move(item));
            skipSpace();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') {
                ++i_;
                return true;
            }
            return false;
        }
    }

    bool object(JValue& out, int depth) {
        out.type = JValue::Type::Object;
        ++i_;  // {
        skipSpace();
        if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            return true;
        }
        for (;;) {
            skipSpace();
            if (i_ >= s_.size() || s_[i_] != '"') return false;
            std::string name;
            if (!string(name)) return false;
            skipSpace();
            if (i_ >= s_.size() || s_[i_] != ':') return false;
            ++i_;
            skipSpace();
            JValue v;
            if (!value(v, depth + 1)) return false;
            out.object.emplace_back(std::move(name), std::move(v));
            skipSpace();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') {
                ++i_;
                return true;
            }
            return false;
        }
    }
};

/** SHA-256 (FIPS 180-4). */
class Sha256 {
public:
    std::string hex(const std::string& data) {
        uint64_t bits = static_cast<uint64_t>(data.size()) * 8;
        std::string msg = data;
        msg += static_cast<char>(0x80);
        while (msg.size() % 64 != 56) msg += '\0';
        for (int k = 7; k >= 0; --k) msg += static_cast<char>((bits >> (k * 8)) & 0xFF);
        for (size_t off = 0; off < msg.size(); off += 64) block(reinterpret_cast<const unsigned char*>(msg.data() + off));
        static const char* digits = "0123456789abcdef";
        std::string out;
        for (uint32_t v : h_) {
            for (int k = 28; k >= 0; k -= 4) out += digits[(v >> k) & 0xF];
        }
        return out;
    }

private:
    uint32_t h_[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void block(const unsigned char* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int t = 0; t < 16; ++t) {
            w[t] = (uint32_t(p[t * 4]) << 24) | (uint32_t(p[t * 4 + 1]) << 16) | (uint32_t(p[t * 4 + 2]) << 8) |
                   uint32_t(p[t * 4 + 3]);
        }
        for (int t = 16; t < 64; ++t) {
            const uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
            const uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
            w[t] = w[t - 16] + s0 + w[t - 7] + s1;
        }
        uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (int t = 0; t < 64; ++t) {
            const uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[t] + w[t];
            const uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
        h_[4] += e;
        h_[5] += f;
        h_[6] += g;
        h_[7] += h;
    }
};

}  // namespace

bool isValidKey(const std::string& key) {
    if (key.empty() || key.size() > 16) return false;
    for (char c : key) {
        if (!isLower(c)) return false;
    }
    return true;
}

bool isValidName(const std::string& name) {
    return lowerDigitDash(name, 40);
}

bool isValidMono(const std::string& mono) {
    if (mono.empty() || mono.size() > 4) return false;
    for (char c : mono) {
        if (!isLower(c) && !isDigit(c) && !(c >= 'A' && c <= 'Z')) return false;
    }
    return true;
}

bool isValidUnit(const std::string& unit) {
    const std::string suffix = ".service";
    if (unit.size() <= suffix.size() || unit.compare(unit.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
    return lowerDigitDash(unit.substr(0, unit.size() - suffix.size()), 40);
}

bool isValidMainFile(const std::string& path) {
    if (path.empty() || path.size() > 120) return false;
    size_t start = 0;
    for (;;) {
        const size_t slash = path.find('/', start);
        const std::string part = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (part.empty() || part == "." || part == "..") return false;  // also refuses a leading / and //
        for (char c : part) {
            if (!isLower(c) && !isDigit(c) && !(c >= 'A' && c <= 'Z') && c != '.' && c != '_' && c != '-') return false;
        }
        if (slash == std::string::npos) return true;
        start = slash + 1;
    }
}

bool isValidDesc(const std::string& text) {
    size_t count = 0;
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        uint32_t cp = 0;
        size_t n = 0;
        if (c < 0x80) {
            cp = c;
            n = 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            n = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            n = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            n = 4;
        } else {
            return false;
        }
        if (i + n > text.size()) return false;
        for (size_t k = 1; k < n; ++k) {
            const unsigned char cc = static_cast<unsigned char>(text[i + k]);
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3F);
        }
        const uint32_t minimum[5] = {0, 0, 0x80, 0x800, 0x10000};
        if (cp < minimum[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;  // overlong / surrogate
        if (cp < 0x20) return false;  // control characters (also new lines)
        i += n;
        ++count;
    }
    return count >= 1 && count <= 120;
}

bool isValidIconName(const std::string& icon) {
    const std::string suffix = "-128.png";
    if (icon.size() <= suffix.size() || icon.compare(icon.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
    return lowerDigitDash(icon.substr(0, icon.size() - suffix.size()), 40);
}

bool isValidSha256(const std::string& hex) {
    if (hex.size() != 64) return false;
    for (char c : hex) {
        if (!isDigit(c) && !(c >= 'a' && c <= 'f')) return false;
    }
    return true;
}

bool isValidApp(const AppInfo& app) {
    return isValidName(app.name) && isValidKey(app.key) && isValidMono(app.mono) && isValidDesc(app.descJa) &&
           isValidDesc(app.descEn) && isValidUnit(app.unit) && isValidMainFile(app.mainFile) &&
           isValidIconName(app.icon) && isValidSha256(app.iconSha256);
}

CatalogParse parseCatalogJson(const std::string& text) {
    CatalogParse result;
    if (text.size() > kMaxCatalogBytes) {
        result.error = "the list is larger than 64 KB";
        return result;
    }
    JValue root;
    if (!JsonReader(text).read(root)) {
        result.error = "the list isn't valid JSON";
        return result;
    }
    const JValue* version = root.type == JValue::Type::Object ? root.get("version") : nullptr;
    const JValue* apps = root.type == JValue::Type::Object ? root.get("apps") : nullptr;
    if (version == nullptr || version->type != JValue::Type::Number || version->number != 1) {
        result.error = "the list isn't version 1";
        return result;
    }
    if (apps == nullptr || apps->type != JValue::Type::Array) {
        result.error = "the list has no apps array";
        return result;
    }
    if (apps->array.size() > kMaxApps) {
        result.error = "the list has more than 64 apps";
        return result;
    }
    std::set<std::string> names, keys;
    for (const JValue& item : apps->array) {
        AppInfo app;
        bool fieldsOk = item.type == JValue::Type::Object;
        const std::pair<const char*, std::string*> fields[] = {
            {"name", &app.name},       {"key", &app.key},        {"mono", &app.mono},
            {"desc_ja", &app.descJa},  {"desc_en", &app.descEn}, {"unit", &app.unit},
            {"main_file", &app.mainFile}, {"icon", &app.icon},   {"icon_sha256", &app.iconSha256},
        };
        for (const auto& field : fields) {
            const JValue* v = fieldsOk ? item.get(field.first) : nullptr;
            if (v == nullptr || v->type != JValue::Type::String) {
                fieldsOk = false;
                break;
            }
            *field.second = v->string;
        }
        if (!fieldsOk || !isValidApp(app) || names.count(app.name) != 0 || keys.count(app.key) != 0) {
            ++result.dropped;
            continue;
        }
        names.insert(app.name);
        keys.insert(app.key);
        result.apps.push_back(std::move(app));
    }
    if (result.apps.empty()) {
        result.error = "no usable app in the list";
        return result;
    }
    result.ok = true;
    return result;
}

std::string sha256Hex(const std::string& bytes) {
    return Sha256().hex(bytes);
}

bool isPng128(const std::string& bytes) {
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (bytes.size() < 24 || bytes.size() > kMaxIconBytes) return false;
    for (int k = 0; k < 8; ++k) {
        if (static_cast<unsigned char>(bytes[k]) != signature[k]) return false;
    }
    if (bytes.compare(12, 4, "IHDR") != 0) return false;
    const auto be32 = [&](size_t at) {
        return (uint32_t(static_cast<unsigned char>(bytes[at])) << 24) | (uint32_t(static_cast<unsigned char>(bytes[at + 1])) << 16) |
               (uint32_t(static_cast<unsigned char>(bytes[at + 2])) << 8) | uint32_t(static_cast<unsigned char>(bytes[at + 3]));
    };
    return be32(16) == 128 && be32(20) == 128;
}

}  // namespace frame_apps
