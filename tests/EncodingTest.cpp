#include "catch_helper.hpp"

import std;
import cli11;
import test_helper;

static const std::string abcd_str = "abcd";
static const std::wstring abcd_wstr = L"abcd";

static const std::array<uint8_t, 13> egypt_utf8_codeunits {
    {0xF0, 0x93, 0x82, 0x80, 0xF0, 0x93, 0x82, 0x80, 0xF0, 0x93, 0x82, 0x80, 0x00}};
static const std::string egypt_str(reinterpret_cast<const char *>(egypt_utf8_codeunits.data()));

#ifdef _WIN32
static const std::array<uint16_t, 7> egypt_utf16_codeunits {{0xD80C, 0xDC80, 0xD80C, 0xDC80, 0xD80C, 0xDC80, 0x0000}};
static const std::wstring egypt_wstr(reinterpret_cast<const wchar_t *>(egypt_utf16_codeunits.data()));
#else
static const std::array<uint32_t, 4> egypt_utf32_codeunits {{0x00013080, 0x00013080, 0x00013080, 0x00000000}};
static const std::wstring egypt_wstr(reinterpret_cast<const wchar_t *>(egypt_utf32_codeunits.data()));
#endif

static const std::array<uint8_t, 51> hello_utf8_codeunits {
    {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x48, 0x61, 0x6c, 0x6c, 0xc3, 0xb3, 0x20, 0xd0, 0x9f, 0xd1, 0x80,
     0xd0, 0xb8, 0xd0, 0xb2, 0xd0, 0xb5, 0xd1, 0x82, 0x20, 0xe4, 0xbd, 0xa0, 0xe5, 0xa5, 0xbd, 0x20, 0xf0,
     0x9f, 0x91, 0xa9, 0xe2, 0x80, 0x8d, 0xf0, 0x9f, 0x9a, 0x80, 0xe2, 0x9d, 0xa4, 0xef, 0xb8, 0x8f, 0x00}};
static const std::string hello_str(reinterpret_cast<const char *>(hello_utf8_codeunits.data()));

#ifdef _WIN32
static const std::array<uint16_t, 30> hello_utf16_codeunits {
    {0x0048, 0x0065, 0x006c, 0x006c, 0x006f, 0x0020, 0x0048, 0x0061, 0x006c, 0x006c,
     0x00f3, 0x0020, 0x041f, 0x0440, 0x0438, 0x0432, 0x0435, 0x0442, 0x0020, 0x4f60,
     0x597d, 0x0020, 0xd83d, 0xdc69, 0x200d, 0xd83d, 0xde80, 0x2764, 0xfe0f, 0x0000}};
static const std::wstring hello_wstr(reinterpret_cast<const wchar_t *>(hello_utf16_codeunits.data()));
#else
static const std::array<uint32_t, 28> hello_utf32_codeunits {
    {0x00000048, 0x00000065, 0x0000006c, 0x0000006c, 0x0000006f, 0x00000020, 0x00000048,
     0x00000061, 0x0000006c, 0x0000006c, 0x000000f3, 0x00000020, 0x0000041f, 0x00000440,
     0x00000438, 0x00000432, 0x00000435, 0x00000442, 0x00000020, 0x00004f60, 0x0000597d,
     0x00000020, 0x0001f469, 0x0000200d, 0x0001f680, 0x00002764, 0x0000fe0f, 0x00000000}};
static const std::wstring hello_wstr(reinterpret_cast<const wchar_t *>(hello_utf32_codeunits.data()));
#endif

TEST_CASE("Encoding: Widen", "[unicode]")
{
    using CLI::widen;
    CHECK(abcd_wstr == widen(abcd_str));
    CHECK(egypt_wstr == widen(egypt_str));
    CHECK(hello_wstr == widen(hello_str));
    CHECK(hello_wstr == widen(hello_str.c_str()));
    CHECK(hello_wstr == widen(hello_str.c_str(), hello_str.size()));
    CHECK(hello_wstr == widen(std::string_view {hello_str}));
}

TEST_CASE("Encoding: Narrow", "[unicode]")
{
    using CLI::narrow;
    CHECK(abcd_str == narrow(abcd_wstr));
    CHECK(egypt_str == narrow(egypt_wstr));
    CHECK(hello_str == narrow(hello_wstr));
    CHECK(hello_str == narrow(hello_wstr.c_str()));
    CHECK(hello_str == narrow(hello_wstr.c_str(), hello_wstr.size()));
    CHECK(hello_str == narrow(std::wstring_view {hello_wstr}));
}

TEST_CASE("Encoding: to_path roundtrip", "[unicode]")
{
    using std::filesystem::path;
#ifdef _WIN32
    std::wstring native_str = CLI::widen(hello_str);
#else
    std::string native_str = hello_str;
#endif
    CHECK(CLI::to_path(hello_str).native() == native_str);
}
