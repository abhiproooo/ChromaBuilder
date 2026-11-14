#include "json.h"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace chroma::json {
namespace {

const Value& nullValue() {
    static const Value value;
    return value;
}

class Parser {
public:
    Parser(std::string_view text, std::string* error)
        : text_(text), error_(error) {}

    std::optional<Value> parseDocument() {
        skipWhitespace();
        auto value = parseValue(0);
        if (!value) {
            return std::nullopt;
        }
        skipWhitespace();
        if (position_ != text_.size()) {
            fail("trailing characters after JSON document");
            return std::nullopt;
        }
        return value;
    }

private:
    static constexpr int kMaxDepth = 256;

    void fail(const std::string& message) {
        if (error_ != nullptr && error_->empty()) {
            *error_ = message + " at byte " + std::to_string(position_);
        }
    }

    char peek() const {
        return position_ < text_.size() ? text_[position_] : '\0';
    }

    char take() {
        return position_ < text_.size() ? text_[position_++] : '\0';
    }

    bool consume(char expected) {
        if (peek() != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    void skipWhitespace() {
        while (position_ < text_.size()) {
            const char c = text_[position_];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                break;
            }
            ++position_;
        }
    }

    std::optional<Value> parseValue(int depth) {
        if (depth > kMaxDepth) {
            fail("JSON nesting too deep");
            return std::nullopt;
        }
        skipWhitespace();
        switch (peek()) {
            case '{':
                return parseObject(depth + 1);
            case '[':
                return parseArray(depth + 1);
            case '"': {
                std::string text;
                if (!parseString(text)) {
                    return std::nullopt;
                }
                return Value(std::move(text));
            }
            case 't':
                if (text_.substr(position_, 4) == "true") {
                    position_ += 4;
                    return Value(true);
                }
                break;
            case 'f':
                if (text_.substr(position_, 5) == "false") {
                    position_ += 5;
                    return Value(false);
                }
                break;
            case 'n':
                if (text_.substr(position_, 4) == "null") {
                    position_ += 4;
                    return Value();
                }
                break;
            default:
                break;
        }

        if (peek() == '-' || (peek() >= '0' && peek() <= '9')) {
            return parseNumber();
        }
        fail("unexpected token");
        return std::nullopt;
    }

    std::optional<Value> parseObject(int depth) {
        if (!consume('{')) {
            fail("expected '{'");
            return std::nullopt;
        }
        Value::Object object;
        skipWhitespace();
        if (consume('}')) {
            return Value(std::move(object));
        }
        for (;;) {
            skipWhitespace();
            std::string key;
            if (!parseString(key)) {
                fail("expected object key");
                return std::nullopt;
            }
            skipWhitespace();
            if (!consume(':')) {
                fail("expected ':' after object key");
                return std::nullopt;
            }
            auto value = parseValue(depth);
            if (!value) {
                return std::nullopt;
            }
            object.emplace_back(std::move(key), std::move(*value));
            skipWhitespace();
            if (consume(',')) {
                continue;
            }
            if (consume('}')) {
                return Value(std::move(object));
            }
            fail("expected ',' or '}' in object");
            return std::nullopt;
        }
    }

    std::optional<Value> parseArray(int depth) {
        if (!consume('[')) {
            fail("expected '['");
            return std::nullopt;
        }
        Value::Array array;
        skipWhitespace();
        if (consume(']')) {
            return Value(std::move(array));
        }
        for (;;) {
            auto value = parseValue(depth);
            if (!value) {
                return std::nullopt;
            }
            array.push_back(std::move(*value));
            skipWhitespace();
            if (consume(',')) {
                continue;
            }
            if (consume(']')) {
                return Value(std::move(array));
            }
            fail("expected ',' or ']' in array");
            return std::nullopt;
        }
    }

    bool parseString(std::string& out) {
        if (!consume('"')) {
            fail("expected string");
            return false;
        }
        out.clear();
        while (position_ < text_.size()) {
            const unsigned char c = static_cast<unsigned char>(take());
            if (c == '"') {
                return true;
            }
            if (c == '\\') {
                if (position_ >= text_.size()) {
                    fail("unterminated escape sequence");
                    return false;
                }
                const char escaped = take();
                switch (escaped) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        unsigned codepoint = 0;
                        if (!parseHex4(codepoint)) {
                            return false;
                        }
                        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                            // Combine a UTF-16 surrogate pair.
                            if (position_ + 1 >= text_.size() ||
                                text_[position_] != '\\' ||
                                text_[position_ + 1] != 'u') {
                                fail("unpaired high surrogate");
                                return false;
                            }
                            position_ += 2;
                            unsigned low = 0;
                            if (!parseHex4(low)) {
                                return false;
                            }
                            if (low < 0xDC00 || low > 0xDFFF) {
                                fail("invalid low surrogate");
                                return false;
                            }
                            codepoint = 0x10000 +
                                        ((codepoint - 0xD800) << 10) +
                                        (low - 0xDC00);
                        } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                            fail("unpaired low surrogate");
                            return false;
                        }
                        appendUtf8(out, codepoint);
                        break;
                    }
                    default:
                        fail("invalid escape sequence");
                        return false;
                }
                continue;
            }
            if (c < 0x20) {
                fail("control character in string");
                return false;
            }
            out.push_back(static_cast<char>(c));
        }
        fail("unterminated string");
        return false;
    }

    bool parseHex4(unsigned& codepoint) {
        codepoint = 0;
        for (int i = 0; i < 4; ++i) {
            if (position_ >= text_.size()) {
                fail("truncated unicode escape");
                return false;
            }
            const char c = text_[position_++];
            unsigned digit = 0;
            if (c >= '0' && c <= '9') {
                digit = static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                digit = static_cast<unsigned>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                digit = static_cast<unsigned>(c - 'A' + 10);
            } else {
                fail("invalid unicode escape");
                return false;
            }
            codepoint = (codepoint << 4) | digit;
        }
        return true;
    }

    static void appendUtf8(std::string& out, unsigned codepoint) {
        if (codepoint <= 0x7F) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    std::optional<Value> parseNumber() {
        const std::size_t begin = position_;
        if (peek() == '-') {
            ++position_;
        }
        if (peek() == '0') {
            ++position_;
        } else if (peek() >= '1' && peek() <= '9') {
            while (peek() >= '0' && peek() <= '9') {
                ++position_;
            }
        } else {
            fail("invalid number");
            return std::nullopt;
        }
        if (peek() == '.') {
            ++position_;
            if (!(peek() >= '0' && peek() <= '9')) {
                fail("expected digit after decimal point");
                return std::nullopt;
            }
            while (peek() >= '0' && peek() <= '9') {
                ++position_;
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            ++position_;
            if (peek() == '+' || peek() == '-') {
                ++position_;
            }
            if (!(peek() >= '0' && peek() <= '9')) {
                fail("expected exponent digit");
                return std::nullopt;
            }
            while (peek() >= '0' && peek() <= '9') {
                ++position_;
            }
        }

        const std::string token(text_.substr(begin, position_ - begin));
        char* end = nullptr;
        const double number = std::strtod(token.c_str(), &end);
        if (end == nullptr || *end != '\0' || !std::isfinite(number)) {
            fail("number out of range");
            return std::nullopt;
        }
        return Value(number);
    }

    std::string_view text_;
    std::string* error_;
    std::size_t position_ = 0;
};

}  // namespace

Value::Value(bool value) : storage_(value) {}
Value::Value(double value) : storage_(value) {}
Value::Value(std::string value) : storage_(std::move(value)) {}
Value::Value(Array value) : storage_(std::move(value)) {}
Value::Value(Object value) : storage_(std::move(value)) {}

Value::Type Value::type() const {
    switch (storage_.index()) {
        case 0: return Type::Null;
        case 1: return Type::Bool;
        case 2: return Type::Number;
        case 3: return Type::String;
        case 4: return Type::Array;
        case 5: return Type::Object;
        default: return Type::Null;
    }
}

bool Value::isNull() const { return type() == Type::Null; }
bool Value::isBool() const { return type() == Type::Bool; }
bool Value::isNumber() const { return type() == Type::Number; }
bool Value::isString() const { return type() == Type::String; }
bool Value::isArray() const { return type() == Type::Array; }
bool Value::isObject() const { return type() == Type::Object; }

bool Value::asBool(bool fallback) const {
    const auto* value = std::get_if<bool>(&storage_);
    return value != nullptr ? *value : fallback;
}

double Value::asNumber(double fallback) const {
    const auto* value = std::get_if<double>(&storage_);
    return value != nullptr ? *value : fallback;
}

int Value::asInt(int fallback) const {
    if (!isNumber()) {
        return fallback;
    }
    const double number = asNumber();
    if (number < static_cast<double>(std::numeric_limits<int>::min()) ||
        number > static_cast<double>(std::numeric_limits<int>::max())) {
        return fallback;
    }
    return static_cast<int>(std::lround(number));
}

float Value::asFloat(float fallback) const {
    return isNumber() ? static_cast<float>(asNumber()) : fallback;
}

const std::string& Value::asString() const {
    static const std::string empty;
    const auto* value = std::get_if<std::string>(&storage_);
    return value != nullptr ? *value : empty;
}

const Value* Value::find(std::string_view key) const {
    const auto* object = std::get_if<Object>(&storage_);
    if (object == nullptr) {
        return nullptr;
    }
    for (const auto& entry : *object) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

bool Value::has(std::string_view key) const {
    return find(key) != nullptr;
}

const Value& Value::at(std::string_view key) const {
    const Value* value = find(key);
    return value != nullptr ? *value : nullValue();
}

const Value& Value::at(std::size_t index) const {
    const auto* array = std::get_if<Array>(&storage_);
    if (array == nullptr || index >= array->size()) {
        return nullValue();
    }
    return (*array)[index];
}

std::size_t Value::size() const {
    if (const auto* array = std::get_if<Array>(&storage_)) {
        return array->size();
    }
    if (const auto* object = std::get_if<Object>(&storage_)) {
        return object->size();
    }
    return 0;
}

const Value::Array& Value::array() const {
    static const Array empty;
    const auto* array = std::get_if<Array>(&storage_);
    return array != nullptr ? *array : empty;
}

const Value::Object& Value::object() const {
    static const Object empty;
    const auto* object = std::get_if<Object>(&storage_);
    return object != nullptr ? *object : empty;
}

bool Value::getBool(std::string_view key, bool fallback) const {
    return at(key).asBool(fallback);
}

int Value::getInt(std::string_view key, int fallback) const {
    return at(key).asInt(fallback);
}

float Value::getFloat(std::string_view key, float fallback) const {
    return at(key).asFloat(fallback);
}

std::string Value::getString(std::string_view key, std::string fallback) const {
    const Value& value = at(key);
    return value.isString() ? value.asString() : std::move(fallback);
}

std::optional<Value> Value::parse(std::string_view text, std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
    Parser parser(text, error);
    return parser.parseDocument();
}

}  // namespace chroma::json
