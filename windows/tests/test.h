#pragma once
// A whole test runner in one header, without dependencies. TEST registers a test under a readable
// name, CHECK and CHECK_EQ report a failure and carry on, and the runner (test_main.cpp) returns
// non-zero when anything failed, which is all CTest needs.
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace test {

struct Case {
    const char* name;  // UTF-8
    void (*body)();
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Registration {
    Registration(const char* name, void (*body)()) { registry().push_back({name, body}); }
};

inline std::string utf8(std::wstring_view text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        char32_t c = text[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (text[++i] - 0xDC00);
        }
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

// How a value looks in a failure message. Control characters are spelled out.
inline std::string show(std::wstring_view text) {
    std::wstring escaped;
    for (wchar_t c : text) {
        if (c == L'\n') escaped += L"\\n";
        else if (c == L'\r') escaped += L"\\r";
        else if (c == L'\t') escaped += L"\\t";
        else if (c < 0x20) escaped += L"\\x" + std::to_wstring(static_cast<int>(c));
        else escaped += c;
    }
    return "\"" + utf8(escaped) + "\"";
}
inline std::string show(const std::wstring& text) { return show(std::wstring_view(text)); }
inline std::string show(const wchar_t* text) { return show(std::wstring_view(text)); }
inline std::string show(std::string_view text) { return "\"" + std::string(text) + "\""; }
inline std::string show(const std::string& text) { return show(std::string_view(text)); }
inline std::string show(const char* text) { return show(std::string_view(text)); }
inline std::string show(bool value) { return value ? "true" : "false"; }
template <class T>
std::string show(const T& value) {
    if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<long long>(value));
    } else if constexpr (std::is_arithmetic_v<T>) {
        return std::to_string(value);
    } else {
        return "<value>";
    }
}

inline void fail(const char* file, int line, const std::string& message) {
    ++failures();
    std::printf("    %s:%d: %s\n", file, line, message.c_str());
}

}  // namespace test

#define TEST_CAT2(a, b) a##b
#define TEST_CAT(a, b) TEST_CAT2(a, b)
#define TEST(name)                                                                                   \
    static void TEST_CAT(test_body_, __LINE__)();                                                    \
    static ::test::Registration TEST_CAT(test_registration_, __LINE__)(name, &TEST_CAT(test_body_, __LINE__)); \
    static void TEST_CAT(test_body_, __LINE__)()

#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) ::test::fail(__FILE__, __LINE__, "CHECK(" #condition ")"); \
    } while (0)

#define CHECK_EQ(actual, expected)                                                                        \
    do {                                                                                                  \
        const auto& test_actual = (actual);                                                               \
        const auto& test_expected = (expected);                                                           \
        if (!(test_actual == test_expected)) {                                                            \
            ::test::fail(__FILE__, __LINE__,                                                              \
                         "CHECK_EQ(" #actual ", " #expected "): " + ::test::show(test_actual) + " != " +   \
                             ::test::show(test_expected));                                                \
        }                                                                                                 \
    } while (0)
