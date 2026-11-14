#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace chroma::json {

// Minimal immutable JSON DOM. Objects preserve insertion order, which keeps
// DragonBones display order stable and makes debugging builds deterministic.
class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::vector<std::pair<std::string, Value>>;
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    explicit Value(bool value);
    explicit Value(double value);
    explicit Value(std::string value);
    explicit Value(Array value);
    explicit Value(Object value);

    Type type() const;
    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0.0) const;
    int asInt(int fallback = 0) const;
    float asFloat(float fallback = 0.0f) const;
    const std::string& asString() const;

    // Object access. Missing keys return nullptr / a shared null value.
    const Value* find(std::string_view key) const;
    bool has(std::string_view key) const;
    const Value& at(std::string_view key) const;
    const Value& at(std::size_t index) const;

    std::size_t size() const;
    const Array& array() const;
    const Object& object() const;

    bool getBool(std::string_view key, bool fallback = false) const;
    int getInt(std::string_view key, int fallback = 0) const;
    float getFloat(std::string_view key, float fallback = 0.0f) const;
    std::string getString(std::string_view key, std::string fallback = {}) const;

    // Returns nullopt on malformed input; error receives an offset-qualified
    // message when provided.
    static std::optional<Value> parse(std::string_view text, std::string* error = nullptr);

private:
    using Storage =
        std::variant<std::monostate, bool, double, std::string, Array, Object>;
    Storage storage_{};
};

}  // namespace chroma::json
