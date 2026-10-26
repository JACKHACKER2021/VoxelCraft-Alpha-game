#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vc {

// Small JSON value with a parser that reports line/column of errors and a
// pretty printer, used for biome, dimension and recipe files in packs.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    static Json fromBool(bool b) { Json j; j.type_ = Type::Bool; j.bool_ = b; return j; }
    static Json number(double n) { Json j; j.type_ = Type::Number; j.num_ = n; return j; }
    static Json string(std::string s) { Json j; j.type_ = Type::String; j.str_ = std::move(s); return j; }
    static Json array() { Json j; j.type_ = Type::Array; return j; }
    static Json object() { Json j; j.type_ = Type::Object; return j; }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isObject() const { return type_ == Type::Object; }
    bool isArray() const { return type_ == Type::Array; }
    bool isString() const { return type_ == Type::String; }
    bool isNumber() const { return type_ == Type::Number; }

    // Lenient accessors: wrong type or missing key gives the default.
    double num(double def = 0) const { return type_ == Type::Number ? num_ : def; }
    bool boolean(bool def = false) const { return type_ == Type::Bool ? bool_ : def; }
    const std::string& str() const { return str_; }
    std::string str(const std::string& def) const { return type_ == Type::String ? str_ : def; }

    const Json& operator[](const std::string& key) const;
    const Json& operator[](size_t i) const;
    size_t size() const { return type_ == Type::Array ? items_.size() : type_ == Type::Object ? keys_.size() : 0; }
    bool has(const std::string& key) const { return fields_.count(key) > 0; }
    const std::vector<std::string>& keys() const { return keys_; }  // in file order

    void push(Json v) { items_.push_back(std::move(v)); }
    void set(const std::string& key, Json v);

    // Returns false and fills `error` ("řádek 3, sloupec 7: ...") on bad input.
    static bool parse(const std::string& text, Json& out, std::string& error);
    std::string dump(int indent = 2) const;

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0;
    std::string str_;
    std::vector<Json> items_;
    std::map<std::string, Json> fields_;
    std::vector<std::string> keys_;
    void dumpTo(std::string& out, int indent, int depth) const;
};

}  // namespace vc
