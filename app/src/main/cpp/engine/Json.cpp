#include "Json.h"
#include "Lang.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace vc {

namespace {

const Json kNull;

struct Parser {
    const std::string& s;
    size_t i = 0;
    std::string error;

    explicit Parser(const std::string& text) : s(text) {}

    bool fail(const std::string& msg) {
        if (!error.empty()) return false;
        int line = 1, col = 1;
        for (size_t k = 0; k < i && k < s.size(); ++k) {
            if (s[k] == '\n') { ++line; col = 1; }
            else if (((unsigned char)s[k] & 0xC0) != 0x80) ++col;
        }
        error = trf("řádek {0}, sloupec {1}: {2}", {std::to_string(line), std::to_string(col), tr(msg)});
        return false;
    }

    void skipSpace() {
        for (;;) {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
            // Allow // comments: handy when editing by hand on a phone.
            if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') {
                while (i < s.size() && s[i] != '\n') ++i;
                continue;
            }
            return;
        }
    }

    bool literal(const char* word) {
        size_t n = std::char_traits<char>::length(word);
        if (s.compare(i, n, word) != 0) return false;
        i += n;
        return true;
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | cp >> 6); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += (char)(0xE0 | cp >> 12); out += (char)(0x80 | (cp >> 6 & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | cp >> 18); out += (char)(0x80 | (cp >> 12 & 0x3F));
            out += (char)(0x80 | (cp >> 6 & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
        }
    }

    bool parseString(std::string& out) {
        ++i;  // opening quote
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c == '\n') { --i; return fail("neukončený řetězec (chybí \")"); }
            if (c != '\\') { out += c; continue; }
            if (i >= s.size()) break;
            char e = s[i++];
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
                    if (i + 4 > s.size()) return fail("neplatná sekvence \\u");
                    unsigned cp = (unsigned)std::strtoul(s.substr(i, 4).c_str(), nullptr, 16);
                    i += 4;
                    if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                        unsigned lo = (unsigned)std::strtoul(s.substr(i + 2, 4).c_str(), nullptr, 16);
                        i += 6;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: --i; return fail(std::string("neznámá escape sekvence \\") + e);
            }
        }
        return fail("neukončený řetězec (chybí \")");
    }

    bool parseValue(Json& out, int depth) {
        if (depth > 64) return fail("příliš hluboké vnoření");
        skipSpace();
        if (i >= s.size()) return fail("neočekávaný konec souboru");
        char c = s[i];
        if (c == '{') {
            ++i;
            out = Json::object();
            skipSpace();
            if (i < s.size() && s[i] == '}') { ++i; return true; }
            for (;;) {
                skipSpace();
                if (i >= s.size() || s[i] != '"') return fail("očekáván název klíče v uvozovkách");
                std::string key;
                if (!parseString(key)) return false;
                skipSpace();
                if (i >= s.size() || s[i] != ':') return fail(trf("za klíčem \"{0}\" chybí dvojtečka", {key}));
                ++i;
                Json v;
                if (!parseValue(v, depth + 1)) return false;
                out.set(key, std::move(v));
                skipSpace();
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    skipSpace();
                    if (i < s.size() && s[i] == '}') { ++i; return true; }  // trailing comma is forgiven
                    continue;
                }
                if (i < s.size() && s[i] == '}') { ++i; return true; }
                return fail("očekávána čárka nebo }");
            }
        }
        if (c == '[') {
            ++i;
            out = Json::array();
            skipSpace();
            if (i < s.size() && s[i] == ']') { ++i; return true; }
            for (;;) {
                Json v;
                if (!parseValue(v, depth + 1)) return false;
                out.push(std::move(v));
                skipSpace();
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    skipSpace();
                    if (i < s.size() && s[i] == ']') { ++i; return true; }
                    continue;
                }
                if (i < s.size() && s[i] == ']') { ++i; return true; }
                return fail("očekávána čárka nebo ]");
            }
        }
        if (c == '"') {
            std::string str;
            if (!parseString(str)) return false;
            out = Json::string(std::move(str));
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            const char* start = s.c_str() + i;
            char* end = nullptr;
            double v = std::strtod(start, &end);
            if (end == start) return fail("neplatné číslo");
            i += end - start;
            out = Json::number(v);
            return true;
        }
        if (literal("true")) { out = Json::fromBool(true); return true; }
        if (literal("false")) { out = Json::fromBool(false); return true; }
        if (literal("null")) { out = Json(); return true; }
        return fail(trf("neočekávaný znak '{0}'", {std::string(1, c)}));
    }
};

void escape(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    out += '"';
}

}  // namespace

const Json& Json::operator[](const std::string& key) const {
    auto it = fields_.find(key);
    return it == fields_.end() ? kNull : it->second;
}

const Json& Json::operator[](size_t i) const { return i < items_.size() ? items_[i] : kNull; }

void Json::set(const std::string& key, Json v) {
    if (!fields_.count(key)) keys_.push_back(key);
    fields_[key] = std::move(v);
}

bool Json::parse(const std::string& text, Json& out, std::string& error) {
    Parser p(text);
    Json v;
    if (!p.parseValue(v, 0)) {
        error = p.error;
        return false;
    }
    p.skipSpace();
    if (p.i != text.size()) {
        p.fail("za koncem JSON je ještě další text");
        error = p.error;
        return false;
    }
    out = std::move(v);
    error.clear();
    return true;
}

std::string Json::dump(int indent) const {
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

void Json::dumpTo(std::string& out, int indent, int depth) const {
    auto newline = [&](int d) {
        if (indent <= 0) return;
        out += '\n';
        out.append((size_t)(indent * d), ' ');
    };
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Number: {
            char buf[32];
            if (std::fabs(num_) < 1e15 && num_ == std::floor(num_)) std::snprintf(buf, sizeof buf, "%.0f", num_);
            else std::snprintf(buf, sizeof buf, "%.6g", num_);
            out += buf;
            break;
        }
        case Type::String: escape(out, str_); break;
        case Type::Array: {
            if (items_.empty()) { out += "[]"; break; }
            // Short arrays of plain values stay on one line, e.g. colors and patterns.
            bool flat = items_.size() <= 8;
            for (const Json& v : items_) flat = flat && (v.type_ != Type::Array && v.type_ != Type::Object);
            out += '[';
            for (size_t k = 0; k < items_.size(); ++k) {
                if (k) out += flat ? ", " : ",";
                if (!flat) newline(depth + 1);
                items_[k].dumpTo(out, indent, depth + 1);
            }
            if (!flat) newline(depth);
            out += ']';
            break;
        }
        case Type::Object: {
            if (keys_.empty()) { out += "{}"; break; }
            out += '{';
            for (size_t k = 0; k < keys_.size(); ++k) {
                if (k) out += ',';
                newline(depth + 1);
                escape(out, keys_[k]);
                out += indent > 0 ? ": " : ":";
                fields_.at(keys_[k]).dumpTo(out, indent, depth + 1);
            }
            newline(depth);
            out += '}';
            break;
        }
    }
}

}  // namespace vc
