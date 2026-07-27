// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "catch_helper.hpp"
import std;
import cli11;
import test_helper;

class NotStreamable
{
};

class Streamable
{
};

std::ostream &operator<<(std::ostream &out, const Streamable &)
{
    return out << "Streamable";
}

TEST_CASE("TypeTools: Streaming", "[helpers]")
{

    CHECK(cli::detail::to_string(NotStreamable {}).empty());

    CHECK("Streamable" == cli::detail::to_string(Streamable {}));

    CHECK("5" == cli::detail::to_string(5));

    CHECK(std::string("string") == cli::detail::to_string("string"));
    CHECK(std::string("string") == cli::detail::to_string(std::string("string")));

    enum class t1 : std::uint8_t
    {
        enum1,
        enum2
    };
    CHECK(cli::detail::to_string(t1::enum1) == "0");
}

TEST_CASE("TypeTools: tuple", "[helpers]")
{
    CHECK_FALSE(cli::detail::tuple_like<int>);
    CHECK_FALSE(cli::detail::tuple_like<std::vector<double>>);
    auto v = cli::detail::tuple_like<std::tuple<double, int>>;
    CHECK(v);
    v = cli::detail::tuple_like<std::tuple<double, double, double>>;
    CHECK(v);
}

TEST_CASE("TypeTools: tuple_to_string", "[helpers]")
{
    std::pair<double, std::string> p1 {0.999, "kWh"};
    CHECK(cli::detail::to_string(p1) == "[0.999,kWh]");

    const std::tuple<std::string> t1 {"kWh"};
    CHECK(cli::detail::to_string(t1) == "kWh");

    const std::tuple<double> td {0.999};
    CHECK(cli::detail::to_string(td) == "0.999");
}

TEST_CASE("TypeTools: type_size", "[helpers]")
{
    auto V = cli::detail::type_count_v<int>;
    CHECK(1 == V);
    V = cli::detail::type_count_v<void>;
    CHECK(0 == V);
    V = cli::detail::type_count_v<std::vector<double>>;
    CHECK(1 == V);
    V = cli::detail::type_count_v<std::tuple<double, int>>;
    CHECK(2 == V);
    V = cli::detail::type_count_v<std::tuple<std::string, double, int>>;
    CHECK(3 == V);
    V = cli::detail::type_count_v<std::array<std::string, 5>>;
    CHECK(5 == V);
    V = cli::detail::type_count_v<std::vector<std::pair<std::string, double>>>;
    CHECK(2 == V);
    V = cli::detail::type_count_v<std::tuple<std::pair<std::string, double>>>;
    CHECK(2 == V);
    V = cli::detail::type_count_v<std::tuple<int, std::pair<std::string, double>>>;
    CHECK(3 == V);
    V = cli::detail::type_count_v<std::tuple<std::pair<int, double>, std::pair<std::string, double>>>;
    CHECK(4 == V);
    // maps
    V = cli::detail::type_count_v<std::map<int, std::pair<int, double>>>;
    CHECK(3 == V);
    // three level tuples
    V = cli::detail::type_count_v<std::tuple<int, std::pair<int, std::tuple<int, double, std::string>>>>;
    CHECK(5 == V);
    V = cli::detail::type_count_v<std::pair<int, std::vector<int>>>;
    CHECK(cli::detail::expected_max_vector_size <= V);
    V = cli::detail::type_count_v<std::vector<std::vector<int>>>;
    CHECK(cli::detail::expected_max_vector_size == V);
}

TEST_CASE("TypeTools: type_size_min", "[helpers]")
{
    auto V = cli::detail::type_count_min_v<int>;
    CHECK(1 == V);
    V = cli::detail::type_count_min_v<void>;
    CHECK(0 == V);
    V = cli::detail::type_count_min_v<std::vector<double>>;
    CHECK(1 == V);
    V = cli::detail::type_count_min_v<std::tuple<double, int>>;
    CHECK(2 == V);
    V = cli::detail::type_count_min_v<std::tuple<std::string, double, int>>;
    CHECK(3 == V);
    V = cli::detail::type_count_min_v<std::array<std::string, 5>>;
    CHECK(5 == V);
    V = cli::detail::type_count_min_v<std::vector<std::pair<std::string, double>>>;
    CHECK(2 == V);
    V = cli::detail::type_count_min_v<std::tuple<std::pair<std::string, double>>>;
    CHECK(2 == V);
    V = cli::detail::type_count_min_v<std::tuple<int, std::pair<std::string, double>>>;
    CHECK(3 == V);
    V = cli::detail::type_count_min_v<std::tuple<std::pair<int, double>, std::pair<std::string, double>>>;
    CHECK(4 == V);
    // maps
    V = cli::detail::type_count_min_v<std::map<int, std::pair<int, double>>>;
    CHECK(3 == V);
    // three level tuples
    V = cli::detail::type_count_min_v<std::tuple<int, std::pair<int, std::tuple<int, double, std::string>>>>;
    CHECK(5 == V);
    V = cli::detail::type_count_min_v<std::pair<int, std::vector<int>>>;
    CHECK(2 == V);
    V = cli::detail::type_count_min_v<std::vector<std::vector<int>>>;
    CHECK(1 == V);
    V = cli::detail::type_count_min_v<std::vector<std::vector<std::pair<int, int>>>>;
    CHECK(2 == V);
}

TEST_CASE("TypeTools: expected_count", "[helpers]")
{
    auto V = cli::detail::expected_count_v<int>;
    CHECK(1 == V);
    V = cli::detail::expected_count_v<void>;
    CHECK(0 == V);
    V = cli::detail::expected_count_v<std::vector<double>>;
    CHECK(cli::detail::expected_max_vector_size == V);
    V = cli::detail::expected_count_v<std::tuple<double, int>>;
    CHECK(1 == V);
    V = cli::detail::expected_count_v<std::tuple<std::string, double, int>>;
    CHECK(1 == V);
    V = cli::detail::expected_count_v<std::array<std::string, 5>>;
    CHECK(1 == V);
    V = cli::detail::expected_count_v<std::vector<std::pair<std::string, double>>>;
    CHECK(cli::detail::expected_max_vector_size == V);
}

TEST_CASE("Split: SimpleByToken", "[helpers]")
{
    auto out = cli::detail::split("one.two.three", '.');
    REQUIRE(out.size() == 3u);
    CHECK(out.at(0) == "one");
    CHECK(out.at(1) == "two");
    CHECK(out.at(2) == "three");
}

TEST_CASE("Split: Single", "[helpers]")
{
    auto out = cli::detail::split("one", '.');
    REQUIRE(out.size() == 1u);
    CHECK(out.at(0) == "one");
}

TEST_CASE("Split: Empty", "[helpers]")
{
    auto out = cli::detail::split("", '.');
    REQUIRE(out.size() == 1u);
    CHECK(out.at(0).empty());
}

TEST_CASE("String: InvalidName", "[helpers]")
{
    CHECK(cli::detail::valid_name_string("valid"));
    CHECK_FALSE(cli::detail::valid_name_string("-invalid"));
    CHECK(cli::detail::valid_name_string("va-li-d"));
    CHECK_FALSE(cli::detail::valid_name_string("valid{}"));
    CHECK(cli::detail::valid_name_string("_valid"));
    CHECK(cli::detail::valid_name_string("/valid"));
    CHECK(cli::detail::valid_name_string("vali?d"));
    CHECK(cli::detail::valid_name_string("@@@@"));
    CHECK(cli::detail::valid_name_string("b@d2?"));
    CHECK(cli::detail::valid_name_string("2vali?d"));
    CHECK_FALSE(cli::detail::valid_name_string("!valid"));
    CHECK_FALSE(cli::detail::valid_name_string("!va\nlid"));
}

TEST_CASE("StringTools: Modify", "[helpers]")
{
    int cnt {0};
    std::string newString = cli::detail::find_and_modify("======", "=", [&cnt](std::string &str, std::size_t index) {
        if ((++cnt) % 2 == 0)
        {
            str[index] = ':';
        }
        return index + 1;
    });
    CHECK("=:=:=:" == newString);
}

TEST_CASE("StringTools: Modify2", "[helpers]")
{
    std::string newString =
        cli::detail::find_and_modify("this is a string test", "is", [](std::string &str, std::size_t index) {
            if ((index > 1) && (str[index - 1] != ' '))
            {
                str[index] = 'a';
                str[index + 1] = 't';
            }
            return index + 1;
        });
    CHECK("that is a string test" == newString);
}

TEST_CASE("StringTools: Modify3", "[helpers]")
{
    // this picks up 3 sets of 3 after the 'b' then collapses the new first set
    std::string newString = cli::detail::find_and_modify("baaaaaaaaaa", "aaa", [](std::string &str, std::size_t index) {
        str.erase(index, 3);
        str.insert(str.begin(), 'a');
        return 0u;
    });
    CHECK("aba" == newString);
}

TEST_CASE("StringTools: flagValues", "[helpers]")
{
    errno = 0;
    CHECK(-1 == cli::detail::to_flag_value("0"));
    CHECK(errno == 0);
    CHECK(1 == cli::detail::to_flag_value("t"));
    CHECK(1 == cli::detail::to_flag_value("1"));
    CHECK(6 == cli::detail::to_flag_value("6"));
    CHECK(-6 == cli::detail::to_flag_value("-6"));
    CHECK(-1 == cli::detail::to_flag_value("false"));
    CHECK(1 == cli::detail::to_flag_value("YES"));
    errno = 0;
    cli::detail::to_flag_value("frog");
    CHECK(errno == EINVAL);
    errno = 0;
    cli::detail::to_flag_value("q");
    CHECK(errno == EINVAL);
    errno = 0;
    cli::detail::to_flag_value(
        "77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777");
    CHECK(errno == ERANGE);
    errno = 0;
    CHECK(-1 == cli::detail::to_flag_value("NO"));
    CHECK(475555233 == cli::detail::to_flag_value("475555233"));
}

TEST_CASE("StringTools: Validation", "[helpers]")
{
    CHECK(cli::detail::isalpha(""));
    CHECK(cli::detail::isalpha("a"));
    CHECK(cli::detail::isalpha("abcd"));
    CHECK_FALSE(cli::detail::isalpha("_"));
    CHECK_FALSE(cli::detail::isalpha("2"));
    CHECK_FALSE(cli::detail::isalpha("test test"));
    CHECK_FALSE(cli::detail::isalpha("test "));
    CHECK_FALSE(cli::detail::isalpha(" test"));
    CHECK_FALSE(cli::detail::isalpha("test2"));
}

TEST_CASE("StringTools: binaryEscapeConversion", "[helpers]")
{
    std::string testString("string1");
    std::string estring = cli::detail::binary_escape_string(testString);
    CHECK(testString == estring);
    CHECK_FALSE(cli::detail::is_binary_escaped_string(estring));

    std::string testString2("\nstring1\n");
    estring = cli::detail::binary_escape_string(testString2);
    CHECK_FALSE(testString == estring);
    CHECK(cli::detail::is_binary_escaped_string(estring));
    std::string rstring = cli::detail::extract_binary_string(estring);
    CHECK(rstring == testString2);

    cli::detail::remove_quotes(estring);
    CHECK(cli::detail::is_binary_escaped_string(estring));
    std::string rstringrq = cli::detail::extract_binary_string(estring);
    CHECK(rstringrq == testString2);

    testString2.push_back(0);
    testString2.push_back(static_cast<char>(197));
    testString2.push_back(78);
    testString2.push_back(static_cast<char>(-34));

    rstring = cli::detail::extract_binary_string(cli::detail::binary_escape_string(testString2));
    CHECK(rstring == testString2);

    testString2.push_back('b');
    testString2.push_back('G');

    rstring = cli::detail::extract_binary_string(cli::detail::binary_escape_string(testString2));
    CHECK(rstring == testString2);
    auto rstring2 = cli::detail::extract_binary_string(rstring);
    CHECK(rstring == rstring2);
}

TEST_CASE("StringTools: binaryEscapeConversion2", "[helpers]")
{
    std::string testString;
    testString.push_back(0);
    testString.push_back(0);
    testString.push_back(0);
    testString.push_back(56);
    testString.push_back(static_cast<char>(-112));
    testString.push_back(static_cast<char>(-112));
    testString.push_back(39);
    testString.push_back(97);
    std::string estring = cli::detail::binary_escape_string(testString);
    CHECK(cli::detail::is_binary_escaped_string(estring));
    std::string rstring = cli::detail::extract_binary_string(estring);
    CHECK(rstring == testString);
}

TEST_CASE("StringTools: binaryEscapeConversion_withX", "[helpers]")
{
    std::string testString("hippy\\x35mm\\XF3_helpX26fox19");
    testString.push_back(0);
    testString.push_back(0);
    testString.push_back(0);
    testString.push_back(56);
    testString.push_back(static_cast<char>(-112));
    testString.push_back(static_cast<char>(-112));
    testString.push_back(39);
    testString.push_back(97);
    std::string estring = cli::detail::binary_escape_string(testString);
    CHECK(cli::detail::is_binary_escaped_string(estring));
    std::string rstring = cli::detail::extract_binary_string(estring);
    CHECK(rstring == testString);
}

TEST_CASE("StringTools: binaryEscapeConversion_withBrackets", "[helpers]")
{

    std::string vstr = R"raw('B"([\xb0\x0a\xb0/\xb0\xb0\xb0\xb0\xb0\xb0\xb0\xb0\xb0\xb0\xb0\xb0\xb0])"')raw";
    std::string testString("[");
    testString.push_back(static_cast<char>(-80));
    testString.push_back('\n');
    testString.push_back(static_cast<char>(-80));
    testString.push_back('/');
    for (int ii = 0; ii < 13; ++ii)
    {
        testString.push_back(static_cast<char>(-80));
    }
    testString.push_back(']');

    std::string estring = cli::detail::binary_escape_string(testString);
    CHECK(cli::detail::is_binary_escaped_string(estring));
    CHECK(estring == vstr);
    std::string rstring = cli::detail::extract_binary_string(estring);
    CHECK(rstring == testString);
}

TEST_CASE("StringTools: binaryStrings", "[helpers]")
{
    std::string rstring = "B\"()\"";
    CHECK(cli::detail::extract_binary_string(rstring).empty());

    rstring = "B\"(\\x35\\xa7)\"";
    CHECK(cli::detail::is_binary_escaped_string(rstring));
    auto result = cli::detail::extract_binary_string(rstring);
    CHECK(result[0] == static_cast<char>(0x35));
    CHECK(result[1] == static_cast<char>(0xa7));

    rstring = "'B\"(\\x3e\\xf7)\"'";
    CHECK(cli::detail::is_binary_escaped_string(rstring));
    result = cli::detail::extract_binary_string(rstring);
    CHECK(result[0] == static_cast<char>(0x3e));
    CHECK(result[1] == static_cast<char>(0xf7));

    rstring = "B\"(\\x3E\\xf7)\"";
    result = cli::detail::extract_binary_string(rstring);
    CHECK(result[0] == static_cast<char>(0x3e));
    CHECK(result[1] == static_cast<char>(0xf7));

    rstring = "B\"(\\X3E\\XF7)\"";
    result = cli::detail::extract_binary_string(rstring);
    CHECK(result[0] == static_cast<char>(0x3e));
    CHECK(result[1] == static_cast<char>(0xf7));

    rstring = "B\"(\\XME\\XK7)\"";
    result = cli::detail::extract_binary_string(rstring);
    CHECK(result == "\\XME\\XK7");

    rstring = "B\"(\\XEM\\X7K)\"";
    result = cli::detail::extract_binary_string(rstring);
    CHECK(result == "\\XEM\\X7K");
}

TEST_CASE("StringTools: escapeConversion", "[helpers]")
{
    CHECK(cli::detail::remove_escaped_characters("test\\\"") == "test\"");
    CHECK(cli::detail::remove_escaped_characters("test\\\\") == "test\\");
    CHECK(cli::detail::remove_escaped_characters("test\\b") == "test\b");
    CHECK(cli::detail::remove_escaped_characters("test\\t") == "test\t");
    CHECK(cli::detail::remove_escaped_characters("test\\n\\r\\t\\f") == "test\n\r\t\f");
    CHECK(cli::detail::remove_escaped_characters("test\\r") == "test\r");
    CHECK(cli::detail::remove_escaped_characters("test\\f") == "test\f");
    std::string zstring = "test";
    zstring.push_back('\0');
    zstring.append("test\n");
    CHECK(cli::detail::remove_escaped_characters("test\\0test\\n") == zstring);

    CHECK_THROWS_AS(cli::detail::remove_escaped_characters("test\\m_bad"), std::invalid_argument);
    CHECK_THROWS_AS(cli::detail::remove_escaped_characters("test\\"), std::invalid_argument);
}

TEST_CASE("StringTools: quotedString", "[helpers]")
{

    std::string rstring = "'B\"(\\x35\\xa7)\"'";
    std::string s2 {rstring};
    cli::detail::process_quoted_string(s2);
    CHECK(s2[0] == static_cast<char>(0x35));
    CHECK(s2[1] == static_cast<char>(0xa7));
    s2 = rstring;
    cli::detail::remove_quotes(s2);
    cli::detail::process_quoted_string(s2);
    CHECK(s2[0] == static_cast<char>(0x35));
    CHECK(s2[1] == static_cast<char>(0xa7));

    std::string qbase = R"("this\nis\na\nfour\tline test")";
    std::string qresult = "this\nis\na\nfour\tline test";

    std::string q1 = qbase;

    // test remove quotes and escape processing
    cli::detail::process_quoted_string(q1);
    CHECK(q1 == qresult);

    std::string q2 = qbase;
    q2.front() = '\'';
    q2.pop_back();
    q2.push_back('\'');
    std::string qliteral = qbase.substr(1);
    qliteral.pop_back();

    // test remove quotes for literal string
    CHECK(cli::detail::process_quoted_string(q2));
    CHECK(q2 == qliteral);

    std::string q3 = qbase;
    q3.front() = '`';
    q3.pop_back();
    q3.push_back('`');

    // test remove quotes for literal string
    CHECK(cli::detail::process_quoted_string(q3));
    CHECK(q3 == qliteral);

    std::string q4 = qbase;
    q4.front() = '|';
    q4.pop_back();
    q4.push_back('|');

    // check that it doesn't process
    CHECK_FALSE(cli::detail::process_quoted_string(q4));
    // test custom string quote character
    CHECK(cli::detail::process_quoted_string(q4, '|'));
    CHECK(q4 == qresult);

    std::string q5 = qbase;
    q5.front() = '?';
    q5.pop_back();
    q5.push_back('?');

    // test custom literal quote character
    CHECK(cli::detail::process_quoted_string(q5, '|', '?'));
    CHECK(q5 == qliteral);

    q3 = qbase;
    q3.front() = '`';
    q3.pop_back();
    q3.push_back('`');

    // test that '`' still works regardless of the other specified characters
    CHECK(cli::detail::process_quoted_string(q3));
    CHECK(q3 == qliteral);
}

TEST_CASE("StringTools: unicode_literals", "[helpers]")
{

    CHECK(cli::detail::remove_escaped_characters("test\\u03C0\\u00e9") == from_u8string(u8"test\u03C0\u00E9"));
    CHECK(cli::detail::remove_escaped_characters("test\\u73C0\\u0057") == from_u8string(u8"test\u73C0\u0057"));

    CHECK(cli::detail::remove_escaped_characters("test\\U0001F600\\u00E9") == from_u8string(u8"test\U0001F600\u00E9"));

    CHECK_THROWS_AS(cli::detail::remove_escaped_characters("test\\U0001M600\\u00E9"), std::invalid_argument);
    CHECK_THROWS_AS(cli::detail::remove_escaped_characters("test\\U0001E600\\u00M9"), std::invalid_argument);
    CHECK_THROWS_AS(cli::detail::remove_escaped_characters("test\\U0001E600\\uD8E9"), std::invalid_argument);

    CHECK_THROWS_AS(cli::detail::remove_escaped_characters("test\\U0001E600\\uD8"), std::invalid_argument);
    CHECK_THROWS_AS(cli::detail::remove_escaped_characters("test\\U0001E60"), std::invalid_argument);
}

TEST_CASE("StringTools: close_sequence", "[helpers]")
{
    CHECK(cli::detail::close_sequence("[test]", 0, ']') == 5U);
    CHECK(cli::detail::close_sequence("[\"test]\"]", 0, ']') == 8U);
    CHECK(cli::detail::close_sequence("[\"test]\"],[t2]", 0, ']') == 8U);
    CHECK(cli::detail::close_sequence("[\"test]\"],[t2]", 10, ']') == 13U);
    CHECK(cli::detail::close_sequence("{\"test]\"],[t2]", 0, '}') == 14U);
    CHECK(cli::detail::close_sequence("[(),(),{},\"]]52{}\",[],[54],[[],[],()]]", 0, ']') == 37U);
}

TEST_CASE("Trim: Various", "[helpers]")
{
    std::string s1 {"  sdlfkj sdflk sd s  "};
    std::string a1 {"sdlfkj sdflk sd s"};
    cli::detail::trim(s1);
    CHECK(s1 == a1);

    std::string s2 {" a \t"};
    cli::detail::trim(s2);
    CHECK(s2 == "a");

    std::string s3 {" a \n"};
    cli::detail::trim(s3);
    CHECK(s3 == "a");

    std::string s4 {" a b "};
    CHECK(cli::detail::trim(s4) == "a b");
}

TEST_CASE("Trim: VariousFilters", "[helpers]")
{
    std::string s1 {"  sdlfkj sdflk sd s  "};
    std::string a1 {"sdlfkj sdflk sd s"};
    cli::detail::trim(s1, " ");
    CHECK(s1 == a1);

    std::string s2 {" a \t"};
    cli::detail::trim(s2, " ");
    CHECK(s2 == "a \t");

    std::string s3 {"abdavda"};
    cli::detail::trim(s3, "a");
    CHECK(s3 == "bdavd");

    std::string s4 {"abcabcabc"};
    CHECK(cli::detail::trim(s4, "ab") == "cabcabc");
}

TEST_CASE("Trim: TrimCopy", "[helpers]")
{
    std::string orig {" cabc  "};
    std::string trimmed = cli::detail::trim_copy(orig);
    CHECK(trimmed == "cabc");
    CHECK(trimmed != orig);
    cli::detail::trim(orig);
    CHECK(orig == trimmed);

    orig = "abcabcabc";
    trimmed = cli::detail::trim_copy(orig, "ab");
    CHECK(trimmed == "cabcabc");
    CHECK(trimmed != orig);
    cli::detail::trim(orig, "ab");
    CHECK(orig == trimmed);
}

TEST_CASE("Validators: FileExists", "[helpers]")
{
    std::string myfile {"TestFileNotUsed.txt"};
    CHECK_FALSE(cli::existing_file(myfile).empty());
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a')); // create file
    CHECK(ok);
    CHECK(cli::existing_file(myfile).empty());

    std::remove(myfile.c_str());
    CHECK_FALSE(cli::existing_file(myfile).empty());
}

TEST_CASE("Validators: FileNotExists", "[helpers]")
{
    std::string myfile {"TestFileNotUsed.txt"};
    CHECK(cli::nonexistent_path(myfile).empty());
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a')); // create file
    CHECK(ok);
    CHECK_FALSE(cli::nonexistent_path(myfile).empty());

    std::remove(myfile.c_str());
    CHECK(cli::nonexistent_path(myfile).empty());
}

TEST_CASE("Validators: FilePathModifier", "[helpers]")
{
    std::string myfile {"../TestFileNotUsed_1.txt"};
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a')); // create file
    CHECK(ok);
    std::string filename = "TestFileNotUsed_1.txt";
    cli::file_on_default_path_t defPath("../");
    CHECK(defPath(filename).empty());
    CHECK(filename == myfile);
    std::string filename2 = "nonexistingfile.csv";
    CHECK_FALSE(defPath(filename2).empty());
    // check it didn't modify the string
    CHECK(filename2 == "nonexistingfile.csv");
    CHECK(defPath(filename).empty());
    std::remove(myfile.c_str());
    CHECK_FALSE(defPath(myfile).empty());
    // now test the no error version
    cli::file_on_default_path_t defPathNoFail("../", false);
    CHECK(defPathNoFail(filename2).empty());
    CHECK(filename2 == "nonexistingfile.csv");
}

TEST_CASE("Validators: FileIsDir", "[helpers]")
{
    std::string mydir {"../tests"};
    CHECK(!cli::existing_file(mydir).empty());
}

TEST_CASE("Validators: DirectoryExists", "[helpers]")
{
    std::string mydir {"tests"};
    CHECK(cli::existing_directory(mydir).empty());
}

TEST_CASE("Validators: DirectoryNotExists", "[helpers]")
{
    std::string mydir {"nondirectory"};
    CHECK(!cli::existing_directory(mydir).empty());
}

TEST_CASE("Validators: DirectoryIsFile", "[helpers]")
{
    std::string myfile {"TestFileNotUsed.txt"};
    CHECK(cli::nonexistent_path(myfile).empty());
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a')); // create file
    CHECK(ok);
    CHECK_FALSE(cli::existing_directory(myfile).empty());

    std::remove(myfile.c_str());
    CHECK(cli::nonexistent_path(myfile).empty());
}

TEST_CASE("Validators: PathExistsDir", "[helpers]")
{
    std::string mydir {"tests"};
    CHECK(cli::existing_path(mydir).empty());
}

TEST_CASE("Validators: PathExistsFile", "[helpers]")
{
    std::string myfile {"TestFileNotUsed.txt"};
    CHECK_FALSE(cli::existing_path(myfile).empty());
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a')); // create file
    CHECK(ok);
    CHECK(cli::existing_path(myfile).empty());

    std::remove(myfile.c_str());
    CHECK_FALSE(cli::existing_path(myfile).empty());
}

TEST_CASE("Validators: PathNotExistsDir", "[helpers]")
{
    std::string mydir {"nonpath"};
    CHECK(!cli::existing_path(mydir).empty());
}

TEST_CASE("Validators: PositiveValidator", "[helpers]")
{
    std::string num = "1.1.1.1";
    CHECK_FALSE(cli::positive_number(num).empty());
    num = "1";
    CHECK(cli::positive_number(num).empty());
    num = "10000";
    CHECK(cli::positive_number(num).empty());
    num = "0";
    CHECK_FALSE(cli::positive_number(num).empty());
    num = "+0.5";
    CHECK(cli::positive_number(num).empty());
    num = "-1";
    CHECK_FALSE(cli::positive_number(num).empty());
    num = "-1.5";
    CHECK_FALSE(cli::positive_number(num).empty());
    num = "a";
    CHECK_FALSE(cli::positive_number(num).empty());
}

TEST_CASE("Validators: NonNegativeValidator", "[helpers]")
{
    std::string num = "1.1.1.1";
    CHECK_FALSE(cli::non_negative_number(num).empty());
    num = "1";
    CHECK(cli::non_negative_number(num).empty());
    num = "10000";
    CHECK(cli::non_negative_number(num).empty());
    num = "0";
    CHECK(cli::non_negative_number(num).empty());
    num = "+0.5";
    CHECK(cli::non_negative_number(num).empty());
    num = "-1";
    CHECK_FALSE(cli::non_negative_number(num).empty());
    num = "-1.5";
    CHECK_FALSE(cli::non_negative_number(num).empty());
    num = "a";
    CHECK_FALSE(cli::non_negative_number(num).empty());
}

TEST_CASE("Validators: CombinedAndRange", "[helpers]")
{
    auto crange = cli::range_t(0, 12) & cli::range_t(4, 16);
    CHECK(crange("4").empty());
    CHECK(crange("12").empty());
    CHECK(crange("7").empty());

    CHECK_FALSE(crange("-2").empty());
    CHECK_FALSE(crange("2").empty());
    CHECK_FALSE(crange("15").empty());
    CHECK_FALSE(crange("16").empty());
    CHECK_FALSE(crange("18").empty());
}

TEST_CASE("Validators: CombinedOrRange", "[helpers]")
{
    auto crange = cli::range_t(0, 4) | cli::range_t(8, 12);

    CHECK_FALSE(crange("-2").empty());
    CHECK(crange("2").empty());
    CHECK_FALSE(crange("5").empty());
    CHECK(crange("8").empty());
    CHECK(crange("12").empty());
    CHECK_FALSE(crange("16").empty());
}

TEST_CASE("Validators: CombinedPaths", "[helpers]")
{
    std::string myfile {"TestFileNotUsed.txt"};
    CHECK_FALSE(cli::existing_file(myfile).empty());
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a')); // create file
    CHECK(ok);

    std::string dir {"tests"};
    std::string notpath {"nondirectory"};

    auto path_or_dir = cli::existing_path | cli::existing_directory;
    CHECK(path_or_dir(dir).empty());
    CHECK(path_or_dir(myfile).empty());
    CHECK_FALSE(path_or_dir(notpath).empty());

    auto file_or_dir = cli::existing_file | cli::existing_directory;
    CHECK(file_or_dir(dir).empty());
    CHECK(file_or_dir(myfile).empty());
    CHECK_FALSE(file_or_dir(notpath).empty());

    auto path_and_dir = cli::existing_path & cli::existing_directory;
    CHECK(path_and_dir(dir).empty());
    CHECK_FALSE(path_and_dir(myfile).empty());
    CHECK_FALSE(path_and_dir(notpath).empty());

    auto path_and_file = cli::existing_file & cli::existing_directory;
    CHECK_FALSE(path_and_file(dir).empty());
    CHECK_FALSE(path_and_file(myfile).empty());
    CHECK_FALSE(path_and_file(notpath).empty());

    std::remove(myfile.c_str());
    CHECK_FALSE(cli::existing_file(myfile).empty());
}

TEST_CASE("Validators: ProgramNameSplit", "[helpers]")
{
    TempFile myfile {"program_name1.exe"};
    {
        std::ofstream out {myfile};
        out << "useless string doesn't matter" << '\n';
    }
    auto res =
        cli::detail::split_program_name(std::string("./") + std::string(myfile) + " this is a bunch of extra stuff  ");
    CHECK(std::string("./") + std::string(myfile) == res.name);
    CHECK("this is a bunch of extra stuff" == res.arguments);

    TempFile myfile2 {"program name1.exe"};
    {
        std::ofstream out {myfile2};
        out << "useless string doesn't matter" << '\n';
    }
    res = cli::detail::split_program_name(std::string("   ") + std::string("./") + std::string(myfile2) +
                                          "      this is a bunch of extra stuff  ");
    CHECK(std::string("./") + std::string(myfile2) == res.name);
    CHECK("this is a bunch of extra stuff" == res.arguments);

    res = cli::detail::split_program_name("./program_name    this is a bunch of extra stuff  ");
    CHECK("./program_name" == res.name);
    CHECK("this is a bunch of extra stuff" == res.arguments);

    res = cli::detail::split_program_name(std::string("  ./") + std::string(myfile) + "    ");
    CHECK(std::string("./") + std::string(myfile) == res.name);
    CHECK(res.arguments.empty());

    res = cli::detail::split_program_name("'odd_program_name.exe --arg --arg2=5");
    CHECK("'odd_program_name.exe" == res.name);
    CHECK_FALSE(res.arguments.empty());
}

TEST_CASE("CheckedMultiply: Int", "[helpers]")
{
    int a {10};
    int b {-20};
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(-200 == a);

    a = 0;
    b = -20;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(0 == a);

    a = 20;
    b = 0;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(0 == a);

    a = (std::numeric_limits<int>::max)();
    b = 1;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<int>::max)() == a);

    a = (std::numeric_limits<int>::max)();
    b = 2;
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<int>::max)() == a);

    a = (std::numeric_limits<int>::max)();
    b = -1;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(-(std::numeric_limits<int>::max)() == a);

    a = (std::numeric_limits<int>::max)();
    b = (std::numeric_limits<int>::max)();
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<int>::max)() == a);

    a = (std::numeric_limits<int>::min)();
    b = (std::numeric_limits<int>::max)();
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<int>::min)() == a);

    a = (std::numeric_limits<int>::min)();
    b = 1;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<int>::min)() == a);

    a = (std::numeric_limits<int>::min)();
    b = -1;
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<int>::min)() == a);

    b = (std::numeric_limits<int>::min)();
    a = -1;
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE(-1 == a);

    a = (std::numeric_limits<int>::min)() / 100;
    b = 99;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<int>::min)() / 100 * 99 == a);

    a = (std::numeric_limits<int>::min)() / 100;
    b = -101;
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<int>::min)() / 100 == a);
    a = 2;
    b = (std::numeric_limits<int>::min)() / 2;
    REQUIRE(cli::detail::checked_multiply(a, b));
    a = (std::numeric_limits<int>::min)() / 2;
    b = 2;
    REQUIRE(cli::detail::checked_multiply(a, b));

    a = 4;
    b = (std::numeric_limits<int>::min)() / 4;
    REQUIRE(cli::detail::checked_multiply(a, b));

    a = 48;
    b = (std::numeric_limits<int>::min)() / 48;
    REQUIRE(cli::detail::checked_multiply(a, b));
}

TEST_CASE("CheckedMultiply: SizeT", "[helpers]")
{
    std::size_t a = 10;
    std::size_t b = 20;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(200u == a);

    a = 0u;
    b = 20u;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(0u == a);

    a = 20u;
    b = 0u;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(0u == a);

    a = (std::numeric_limits<std::size_t>::max)();
    b = 1u;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<std::size_t>::max)() == a);

    a = (std::numeric_limits<std::size_t>::max)();
    b = 2u;
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<std::size_t>::max)() == a);

    a = (std::numeric_limits<std::size_t>::max)();
    b = (std::numeric_limits<std::size_t>::max)();
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<std::size_t>::max)() == a);

    a = (std::numeric_limits<std::size_t>::max)() / 100;
    b = 99u;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<std::size_t>::max)() / 100u * 99u == a);
}

TEST_CASE("CheckedMultiply: Float", "[helpers]")
{
    float a {10.0F};
    float b {20.0F};
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(200 == Approx(a));

    a = 0.0F;
    b = 20.0F;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(0 == Approx(a));

    a = INFINITY;
    b = 20.0F;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(INFINITY == Approx(a));

    a = 2.0F;
    b = -INFINITY;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(-INFINITY == Approx(a));

    a = (std::numeric_limits<float>::max)() / 100.0F;
    b = 1.0F;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<float>::max)() / 100.0F == Approx(a));

    a = (std::numeric_limits<float>::max)() / 100.0F;
    b = 99.0F;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<float>::max)() / 100.0F * 99.0F == Approx(a));

    a = (std::numeric_limits<float>::max)() / 100.0F;
    b = 101;
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<float>::max)() / 100.0F == Approx(a));

    a = (std::numeric_limits<float>::max)() / 100.0F;
    b = -99;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<float>::max)() / 100.0F * -99.0F == Approx(a));

    a = (std::numeric_limits<float>::max)() / 100.0F;
    b = -101;
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<float>::max)() / 100.0F == Approx(a));
}

TEST_CASE("CheckedMultiply: Double", "[helpers]")
{
    double a {10.0F};
    double b {20.0F};
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(200 == Approx(a));

    a = 0;
    b = 20;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(0 == Approx(a));

    a = std::numeric_limits<double>::infinity();
    b = 20;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(std::numeric_limits<double>::infinity() == Approx(a));

    a = 2;
    b = -std::numeric_limits<double>::infinity();
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE(-std::numeric_limits<double>::infinity() == Approx(a));

    a = (std::numeric_limits<double>::max)() / 100;
    b = 1;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<double>::max)() / 100 == Approx(a));

    a = (std::numeric_limits<double>::max)() / 100;
    b = 99;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<double>::max)() / 100 * 99 == Approx(a));

    a = (std::numeric_limits<double>::max)() / 100;
    b = 101;
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<double>::max)() / 100 == Approx(a));

    a = (std::numeric_limits<double>::max)() / 100;
    b = -99;
    REQUIRE(cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<double>::max)() / 100 * -99 == Approx(a));

    a = (std::numeric_limits<double>::max)() / 100;
    b = -101;
    REQUIRE(!cli::detail::checked_multiply(a, b));
    REQUIRE((std::numeric_limits<double>::max)() / 100 == Approx(a));
}

// Yes, this is testing an app_helper :)
TEST_CASE("AppHelper: TempfileCreated", "[helpers]")
{
    std::string name = "TestFileNotUsed.txt";
    {
        TempFile myfile {name};

        CHECK_FALSE(cli::existing_file(myfile).empty());

        bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a')); // create file
        CHECK(ok);
        CHECK(cli::existing_file(name).empty());
        CHECK_THROWS_AS([&]() { TempFile otherfile(name); }(), std::runtime_error);
    }
    CHECK_FALSE(cli::existing_file(name).empty());
}

TEST_CASE("AppHelper: TempfileNotCreated", "[helpers]")
{
    std::string name = "TestFileNotUsed.txt";
    {
        TempFile myfile {name};

        CHECK_FALSE(cli::existing_file(myfile).empty());
    }
    CHECK_FALSE(cli::existing_file(name).empty());
}

TEST_CASE("AppHelper: Ofstream", "[helpers]")
{

    std::string name = "TestFileNotUsed.txt";
    {
        TempFile myfile(name);

        {
            std::ofstream out {myfile};
            out << "this is output" << '\n';
        }

        CHECK(cli::existing_file(myfile).empty());
    }
    CHECK_FALSE(cli::existing_file(name).empty());
}

TEST_CASE("Split: StringList", "[helpers]")
{

    std::vector<std::string> results {"a", "long", "--lone", "-q"};
    CHECK(cli::detail::split_names("a,long,--lone,-q") == results);
    CHECK(cli::detail::split_names(" a, long, --lone, -q") == results);
    CHECK(cli::detail::split_names(" a , long , --lone , -q ") == results);
    CHECK(cli::detail::split_names("   a  ,  long  ,  --lone  ,    -q  ") == results);

    CHECK(cli::detail::split_names("one") == std::vector<std::string>({"one"}));
}

TEST_CASE("RegEx: Shorts", "[helpers]")
{
    auto res = cli::detail::split_short("-a");
    REQUIRE(res.has_value());
    CHECK(res->name == "a");
    CHECK(res->value.empty());

    res = cli::detail::split_short("-B");
    REQUIRE(res.has_value());
    CHECK(res->name == "B");
    CHECK(res->value.empty());

    res = cli::detail::split_short("-cc");
    REQUIRE(res.has_value());
    CHECK(res->name == "c");
    CHECK(res->value == "c");

    res = cli::detail::split_short("-simple");
    REQUIRE(res.has_value());
    CHECK(res->name == "s");
    CHECK(res->value == "imple");

    CHECK_FALSE(cli::detail::split_short("--a").has_value());
    CHECK_FALSE(cli::detail::split_short("--thing").has_value());
    CHECK_FALSE(cli::detail::split_short("--").has_value());
    CHECK_FALSE(cli::detail::split_short("something").has_value());
    CHECK_FALSE(cli::detail::split_short("s").has_value());
}

TEST_CASE("RegEx: Longs", "[helpers]")
{
    auto res = cli::detail::split_long("--a");
    REQUIRE(res.has_value());
    CHECK(res->name == "a");
    CHECK(res->value.empty());

    res = cli::detail::split_long("--thing");
    REQUIRE(res.has_value());
    CHECK(res->name == "thing");
    CHECK(res->value.empty());

    res = cli::detail::split_long("--some=thing");
    REQUIRE(res.has_value());
    CHECK(res->name == "some");
    CHECK(res->value == "thing");

    CHECK_FALSE(cli::detail::split_long("-a").has_value());
    CHECK_FALSE(cli::detail::split_long("-things").has_value());
    CHECK_FALSE(cli::detail::split_long("Q").has_value());
    CHECK_FALSE(cli::detail::split_long("--").has_value());
}

TEST_CASE("RegEx: SplittingNew", "[helpers]")
{
    cli::detail::option_names_t names;

    CHECK_NOTHROW(names = cli::detail::get_names({"--long", "-s", "-q", "--also-long"}));
    CHECK(names.long_names == std::vector<std::string>({"long", "also-long"}));
    CHECK(names.short_names == std::vector<std::string>({"s", "q"}));
    CHECK(names.positional_name.empty());

    names = cli::detail::get_names({"--long", "", "-s", "-q", "", "--also-long"});
    CHECK(names.long_names == std::vector<std::string>({"long", "also-long"}));
    CHECK(names.short_names == std::vector<std::string>({"s", "q"}));

    CHECK_THROWS_AS([&]() { names = cli::detail::get_names({"-"}); }(), cli::bad_name_string_t);
    CHECK_THROWS_AS([&]() { names = cli::detail::get_names({"--"}); }(), cli::bad_name_string_t);
    CHECK_THROWS_AS([&]() { names = cli::detail::get_names({"-hi"}); }(), cli::bad_name_string_t);
    CHECK_THROWS_AS([&]() { names = cli::detail::get_names({"---hi"}); }(), cli::bad_name_string_t);
    CHECK_THROWS_AS([&]() { names = cli::detail::get_names({"one", "two"}); }(), cli::bad_name_string_t);
}

TEST_CASE("String: ToLower", "[helpers]")
{
    CHECK("one and two" == cli::detail::to_lower("one And TWO"));
}

TEST_CASE("Join: Forward", "[helpers]")
{
    std::vector<std::string> val {{"one", "two", "three"}};
    CHECK(cli::detail::join(val) == "one,two,three");
    CHECK(cli::detail::join(val, ";") == "one;two;three");
}

TEST_CASE("Join: Backward", "[helpers]")
{
    std::vector<std::string> val {{"three", "two", "one"}};
    CHECK(cli::detail::rjoin(val) == "one,two,three");
    CHECK(cli::detail::rjoin(val, ";") == "one;two;three");
}

TEST_CASE("SplitUp: Simple", "[helpers]")
{
    std::vector<std::string> oput = {"one", "\"two three\""};
    std::string orig {R"(one "two three")"};
    std::vector<std::string> result = cli::detail::split_up(orig);
    CHECK(result == oput);
}

TEST_CASE("SplitUp: SimpleDifferentQuotes", "[helpers]")
{
    std::vector<std::string> oput = {"one", "`two three`"};
    std::string orig {R"(one `two three`)"};
    std::vector<std::string> result = cli::detail::split_up(orig);
    CHECK(result == oput);
}

TEST_CASE("SplitUp: SimpleMissingQuotes", "[helpers]")
{
    std::vector<std::string> oput = {"one", "`two three"};
    std::string orig {R"(one `two three)"};
    std::vector<std::string> result = cli::detail::split_up(orig);
    CHECK(result == oput);
}

TEST_CASE("SplitUp: SimpleMissingQuotesEscaped", "[helpers]")
{
    std::vector<std::string> oput = {"one", R"("two three\"")"};
    std::string orig {R"(one "two three\"")"};
    std::vector<std::string> result = cli::detail::split_up(orig);
    CHECK(result == oput);
}

TEST_CASE("SplitUp: SimpleDifferentQuotes2", "[helpers]")
{
    std::vector<std::string> oput = {"one", "'two three'"};
    std::string orig {R"(one 'two three')"};
    std::vector<std::string> result = cli::detail::split_up(orig);
    CHECK(result == oput);
}

TEST_CASE("SplitUp: Bracket1", "[helpers]")
{
    std::vector<std::string> oput = {"one", "[two, three]"};
    std::string orig {"one, [two, three]"};
    std::vector<std::string> result = cli::detail::split_up(orig, ',');
    CHECK(result == oput);
}

TEST_CASE("SplitUp: Bracket2", "[helpers]")
{
    std::vector<std::string> oput = {"one", "<two, three>"};
    std::string orig {"one, <two, three>"};
    std::vector<std::string> result = cli::detail::split_up(orig, ',');
    CHECK(result == oput);
}

TEST_CASE("SplitUp: Bracket3", "[helpers]")
{
    std::vector<std::string> oput = {"one", "(two, three)"};
    std::string orig {"one, (two, three)"};
    std::vector<std::string> result = cli::detail::split_up(orig, ',');
    CHECK(result == oput);
}

TEST_CASE("SplitUp: Bracket4", "[helpers]")
{
    std::vector<std::string> oput = {"one", "{two, three}"};
    std::string orig {"one, {two, three}"};
    std::vector<std::string> result = cli::detail::split_up(orig, ',');
    CHECK(result == oput);
}

TEST_CASE("SplitUp: Comment", "[helpers]")
{
    std::vector<std::string> oput = {R"(["quote1", "#"])"};
    std::string orig {R"(["quote1", "#"])"};
    std::vector<std::string> result = cli::detail::split_up(orig, '#');
    CHECK(result == oput);
}

TEST_CASE("SplitUp: Layered", "[helpers]")
{
    std::vector<std::string> output = {R"("one 'two three'")"};
    std::string orig {R"("one 'two three'")"};
    std::vector<std::string> result = cli::detail::split_up(orig);
    CHECK(result == output);
}

TEST_CASE("SplitUp: Spaces", "[helpers]")
{
    std::vector<std::string> oput = {"one", "\"  two three\""};
    std::string orig {R"(  one  "  two three" )"};
    std::vector<std::string> result = cli::detail::split_up(orig);
    CHECK(result == oput);
}

TEST_CASE("SplitUp: BadStrings", "[helpers]")
{
    std::vector<std::string> oput = {"one", "\"  two three"};
    std::string orig {R"(  one  "  two three )"};
    std::vector<std::string> result = cli::detail::split_up(orig);
    CHECK(result == oput);

    oput = {"one", "'  two three"};
    orig = R"(  one  '  two three )";
    result = cli::detail::split_up(orig);
    CHECK(result == oput);
}

TEST_CASE("Types: TypeName", "[helpers]")
{
    std::string int_name = cli::detail::type_name<int>();
    CHECK(int_name == "INT");

    std::string int2_name = cli::detail::type_name<std::int16_t>();
    CHECK(int2_name == "INT");

    std::string uint_name = cli::detail::type_name<unsigned char>();
    CHECK(uint_name == "UINT");

    std::string float_name = cli::detail::type_name<double>();
    CHECK(float_name == "FLOAT");

    std::string char_name = cli::detail::type_name<char>();
    CHECK(char_name == "CHAR");

    std::string vector_name = cli::detail::type_name<std::vector<int>>();
    CHECK(vector_name == "INT");

    vector_name = cli::detail::type_name<std::vector<double>>();
    CHECK(vector_name == "FLOAT");

    static_assert(cli::detail::classify_object_v<std::pair<int, std::string>> ==
                      cli::detail::object_category_t::tuple_value,
                  "pair<int,string> does not read like a tuple");

    static_assert(cli::detail::classify_object_v<std::tuple<std::string, double>> ==
                      cli::detail::object_category_t::tuple_value,
                  "tuple<string,double> does not read like a tuple");

    std::string pair_name = cli::detail::type_name<std::vector<std::pair<int, std::string>>>();
    CHECK(pair_name == "[INT,TEXT]");

    vector_name = cli::detail::type_name<std::vector<std::vector<unsigned char>>>();
    CHECK(vector_name == "UINT");

    auto vclass = cli::detail::classify_object_v<std::vector<std::vector<unsigned char>>>;
    CHECK(cli::detail::object_category_t::container_value == vclass);

    auto tclass = cli::detail::classify_object_v<std::tuple<double>>;
    CHECK(cli::detail::object_category_t::number_constructible == tclass);

    std::string tuple_name = cli::detail::type_name<std::tuple<double>>();
    CHECK(tuple_name == "FLOAT");

    static_assert(cli::detail::classify_object_v<std::tuple<int, std::string>> ==
                      cli::detail::object_category_t::tuple_value,
                  "tuple<int,string> does not read like a tuple");
    tuple_name = cli::detail::type_name<std::tuple<int, std::string>>();
    CHECK(tuple_name == "[INT,TEXT]");

    tuple_name = cli::detail::type_name<std::tuple<const int, std::string>>();
    CHECK(tuple_name == "[INT,TEXT]");

    tuple_name = cli::detail::type_name<const std::tuple<int, std::string>>();
    CHECK(tuple_name == "[INT,TEXT]");

    tuple_name = cli::detail::type_name<std::tuple<std::string, double>>();
    CHECK(tuple_name == "[TEXT,FLOAT]");

    tuple_name = cli::detail::type_name<const std::tuple<std::string, double>>();
    CHECK(tuple_name == "[TEXT,FLOAT]");

    tuple_name = cli::detail::type_name<std::tuple<int, std::string, double>>();
    CHECK(tuple_name == "[INT,TEXT,FLOAT]");

    tuple_name = cli::detail::type_name<std::tuple<int, std::string, double, unsigned int>>();
    CHECK(tuple_name == "[INT,TEXT,FLOAT,UINT]");

    tuple_name = cli::detail::type_name<std::tuple<int, std::string, double, unsigned int, std::string>>();
    CHECK(tuple_name == "[INT,TEXT,FLOAT,UINT,TEXT]");

    tuple_name = cli::detail::type_name<std::array<int, 10>>();
    CHECK(tuple_name == "[INT,INT,INT,INT,INT,INT,INT,INT,INT,INT]");

    std::string text_name = cli::detail::type_name<std::string>();
    CHECK(text_name == "TEXT");

    std::string text2_name = cli::detail::type_name<char *>();
    CHECK(text2_name == "TEXT");

    enum class test : std::uint8_t
    {
        test1,
        test2,
        test3
    };
    std::string enum_name = cli::detail::type_name<test>();
    CHECK(enum_name == "ENUM");

    vclass = cli::detail::classify_object_v<std::tuple<test>>;
    CHECK(cli::detail::object_category_t::tuple_value == vclass);
    static_assert(cli::detail::classify_object_v<std::tuple<test>> == cli::detail::object_category_t::tuple_value,
                  "tuple<test> does not classify as a tuple");
    std::string enum_name2 = cli::detail::type_name<std::tuple<test>>();
    CHECK(enum_name2 == "ENUM");
    std::string umapName = cli::detail::type_name<std::unordered_map<int, std::tuple<std::string, double>>>();
    CHECK(umapName == "[INT,[TEXT,FLOAT]]");

    // On older compilers, this may show up as other/TEXT
    vclass = cli::detail::classify_object_v<std::atomic<int>>;
    CHECK((cli::detail::object_category_t::wrapper_value == vclass || cli::detail::object_category_t::other == vclass));

    std::string atomic_name = cli::detail::type_name<std::atomic<int>>();
    CHECK((atomic_name == "INT" || atomic_name == "TEXT"));
}

TEST_CASE("Types: TypeNameStrings", "[helpers]")
{
    auto sclass = cli::detail::classify_object_v<std::string>;
    CHECK(cli::detail::object_category_t::string_assignable == sclass);

    auto wsclass = cli::detail::classify_object_v<std::wstring>;
    CHECK(cli::detail::object_category_t::wstring_assignable == wsclass);

#if defined CLI11_HAS_FILESYSTEM && CLI11_HAS_FILESYSTEM > 0 && defined(_MSC_VER)
    auto fspclass = cli::detail::classify_object_v<std::filesystem::path>;
    CHECK(cli::detail::object_category_t::wstring_assignable == fspclass);
#endif
}

TEST_CASE("Types: OverflowSmall", "[helpers]")
{
    signed char x = 0;
    auto strmax = std::to_string((std::numeric_limits<signed char>::max)() + 1);
    CHECK_FALSE(cli::detail::lexical_cast(strmax, x));

    unsigned char y = 0;
    strmax = std::to_string((std::numeric_limits<unsigned char>::max)() + 1);
    CHECK_FALSE(cli::detail::lexical_cast(strmax, y));
}

TEST_CASE("Types: LexicalCastInt", "[helpers]")
{
    std::string signed_input = "-912";
    int x_signed = 0;
    CHECK(cli::detail::lexical_cast(signed_input, x_signed));
    CHECK(x_signed == -912);

    std::string unsigned_input = "912";
    unsigned int x_unsigned = 0;
    CHECK(cli::detail::lexical_cast(unsigned_input, x_unsigned));
    CHECK(x_unsigned == (unsigned int)912);

    CHECK_FALSE(cli::detail::lexical_cast(signed_input, x_unsigned));

    unsigned char y = 0;
    std::string overflow_input = std::to_string((std::numeric_limits<uint64_t>::max)()) + "0";
    CHECK_FALSE(cli::detail::lexical_cast(overflow_input, y));

    char y_signed = 0;
    CHECK_FALSE(cli::detail::lexical_cast(overflow_input, y_signed));

    std::string bad_input = "hello";
    CHECK_FALSE(cli::detail::lexical_cast(bad_input, y));

    std::string extra_input = "912i";
    CHECK_FALSE(cli::detail::lexical_cast(extra_input, y));

    extra_input = "true";
    CHECK(cli::detail::lexical_cast(extra_input, x_signed));
    CHECK(x_signed != 0);

    std::string empty_input {};
    CHECK_FALSE(cli::detail::lexical_cast(empty_input, x_signed));
    CHECK_FALSE(cli::detail::lexical_cast(empty_input, x_unsigned));
    CHECK_FALSE(cli::detail::lexical_cast(empty_input, y_signed));
}

TEST_CASE("Types: LexicalCastDouble", "[helpers]")
{
    std::string input = "9.12";
    long double x = NAN;
    CHECK(cli::detail::lexical_cast(input, x));
    CHECK((float)x == Approx((float)9.12));

    std::string bad_input = "hello";
    CHECK_FALSE(cli::detail::lexical_cast(bad_input, x));

    std::string overflow_input = "1" + std::to_string((std::numeric_limits<long double>::max)());
    CHECK(cli::detail::lexical_cast(overflow_input, x));
    CHECK_FALSE(std::isfinite(x));

    std::string extra_input = "9.12i";
    CHECK_FALSE(cli::detail::lexical_cast(extra_input, x));

    std::string empty_input {};
    CHECK_FALSE(cli::detail::lexical_cast(empty_input, x));
}

TEST_CASE("Types: LexicalCastBool", "[helpers]")
{
    std::string input = "false";
    bool x = false;
    CHECK(cli::detail::lexical_cast(input, x));
    CHECK_FALSE(x);

    std::string bad_input = "happy";
    CHECK_FALSE(cli::detail::lexical_cast(bad_input, x));

    std::string input_true = "EnaBLE";
    CHECK(cli::detail::lexical_cast(input_true, x));
    CHECK(x);
}

TEST_CASE("Types: LexicalCastString", "[helpers]")
{
    std::string input = "one";
    std::string output;
    cli::detail::lexical_cast(input, output);
    CHECK(output == input);
}

TEST_CASE("Types: LexicalCastParsable", "[helpers]")
{
    std::string input = "(4.2,7.3)";
    std::string fail_input = "4.2,7.3";
    std::string extra_input = "(4.2,7.3)e";

    std::complex<double> output;
    CHECK(cli::detail::lexical_cast(input, output));
    CHECK(4.2 == Approx(output.real()));
    CHECK(7.3 == Approx(output.imag()));

    CHECK(cli::detail::lexical_cast("2.456", output));
    CHECK(2.456 == Approx(output.real()));
    CHECK(0.0 == Approx(output.imag()));

    CHECK_FALSE(cli::detail::lexical_cast(fail_input, output));
    CHECK_FALSE(cli::detail::lexical_cast(extra_input, output));
}

TEST_CASE("Types: LexicalCastEnum", "[helpers]")
{
    enum t1 : signed char
    {
        v1 = 5,
        v3 = 7,
        v5 = -9
    };

    t1 output = v1;
    CHECK(cli::detail::lexical_cast("-9", output));
    CHECK(v5 == output);

    CHECK_FALSE(cli::detail::lexical_cast("invalid", output));
    enum class t2 : std::uint64_t
    {
        enum1 = 65,
        enum2 = 45667,
        enum3 = 9999999999999
    };
    t2 output2 {t2::enum2};
    CHECK(cli::detail::lexical_cast("65", output2));
    CHECK(t2::enum1 == output2);

    CHECK_FALSE(cli::detail::lexical_cast("invalid", output2));

    CHECK(cli::detail::lexical_cast("9999999999999", output2));
    CHECK(t2::enum3 == output2);
}

TEST_CASE("Types: LexicalConversionDouble", "[helpers]")
{
    cli::results_t input = {"9.12"};
    long double x {0.0};
    bool res = cli::detail::lexical_conversion<long double, double>(input, x);
    CHECK(res);
    CHECK((float)x == Approx((float)9.12));

    cli::results_t bad_input = {"hello"};
    res = cli::detail::lexical_conversion<long double, double>(bad_input, x);
    CHECK_FALSE(res);
}

TEST_CASE("Types: LexicalConversionDoubleTuple", "[helpers]")
{
    cli::results_t input = {"9.12"};
    std::tuple<double> x {0.0};
    bool res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK(res);
    CHECK(std::get<0>(x) == Approx(9.12));

    cli::results_t bad_input = {"hello"};
    res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(bad_input, x);
    CHECK_FALSE(res);
}

TEST_CASE("Types: LexicalConversionVectorDouble", "[helpers]")
{
    cli::results_t input = {"9.12", "10.79", "-3.54"};
    std::vector<double> x;
    bool res = cli::detail::lexical_conversion<std::vector<double>, double>(input, x);
    CHECK(res);
    CHECK(3u == x.size());
    CHECK(-3.54 == Approx(x[2]));

    res = cli::detail::lexical_conversion<std::vector<double>, std::vector<double>>(input, x);
    CHECK(res);
    CHECK(3u == x.size());
    CHECK(-3.54 == Approx(x[2]));
}

TEST_CASE("Types: LexicalConversionEmptyVectorDouble", "[helpers]")
{
    cli::results_t input = {};
    std::vector<double> x;
    bool res = cli::detail::lexical_conversion<std::vector<double>, std::vector<double>>(input, x);
    CHECK(res);
    CHECK(x.empty());
}

static_assert(!cli::detail::tuple_like<std::vector<double>>, "vector should not be like a tuple");
static_assert(cli::detail::tuple_like<std::pair<double, double>>, "pair of double should be like a tuple");
static_assert(cli::detail::tuple_like<std::array<double, 4>>, "std::array<double,4> should be like a tuple");
static_assert(cli::detail::tuple_like<std::array<int, 10>>, "std::array<int,10> should be like a tuple");
static_assert(!cli::detail::tuple_like<std::string>, "std::string should not be like a tuple");
static_assert(!cli::detail::tuple_like<double>, "double should not be like a tuple");
static_assert(cli::detail::tuple_like<std::tuple<double, int, double>>, "tuple should look like a tuple");
static_assert(!cli::detail::tuple_like<std::complex<double>>, "std::complex should not be like a tuple");

TEST_CASE("Types: LexicalConversionTuple2", "[helpers]")
{
    cli::results_t input = {"9.12", "19"};

    std::tuple<double, int> x {0.0, 0};
    static_assert(cli::detail::tuple_like<decltype(x)>, "tuple type must have is_tuple_like trait to be true");
    bool res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK(res);
    CHECK(19 == std::get<1>(x));
    CHECK(9.12 == Approx(std::get<0>(x)));

    input = {"19", "9.12"};
    res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK_FALSE(res);
}

TEST_CASE("Types: LexicalConversionTuple3", "[helpers]")
{
    cli::results_t input = {"9.12", "19", "hippo"};
    std::tuple<double, int, std::string> x;
    bool res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK(res);
    CHECK(19 == std::get<1>(x));
    CHECK(9.12 == Approx(std::get<0>(x)));
    CHECK("hippo" == std::get<2>(x));

    input = {"19", "9.12"};
    res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK_FALSE(res);
}

TEST_CASE("Types: LexicalConversionTuple4", "[helpers]")
{
    cli::results_t input = {"9.12", "19", "18.6", "5.87"};
    std::array<double, 4> x;
    auto tsize = cli::detail::type_count_v<decltype(x)>;
    CHECK(tsize == 4);
    bool res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK(res);
    CHECK(19 == Approx(std::get<1>(x)));
    CHECK(9.12 == Approx(x[0]));
    CHECK(18.6 == Approx(x[2]));
    CHECK(5.87 == Approx(x[3]));

    input = {"19", "9.12", "hippo"};
    res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK_FALSE(res);
}

TEST_CASE("Types: LexicalConversionTuple5", "[helpers]")
{
    cli::results_t input = {"9", "19", "18", "5", "235235"};
    std::array<unsigned int, 5> x;
    bool res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK(res);
    CHECK(19u == std::get<1>(x));
    CHECK(9u == x[0]);
    CHECK(18u == x[2]);
    CHECK(5u == x[3]);
    CHECK(235235u == x[4]);

    input = {"19", "9.12", "hippo"};
    res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK_FALSE(res);
}

TEST_CASE("Types: LexicalConversionTuple10", "[helpers]")
{
    cli::results_t input = {"9", "19", "18", "5", "235235", "9", "19", "18", "5", "235235"};
    std::array<unsigned int, 10> x;
    bool res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK(res);
    CHECK(19u == std::get<1>(x));
    CHECK(9u == x[0]);
    CHECK(18u == x[2]);
    CHECK(5u == x[3]);
    CHECK(235235u == x[4]);
    CHECK(235235u == x[9]);
    input[3] = "hippo";
    res = cli::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    CHECK_FALSE(res);
}

TEST_CASE("Types: LexicalConversionTuple10XC", "[helpers]")
{
    cli::results_t input = {"9", "19", "18", "5", "235235", "9", "19", "18", "5", "235235"};
    std::array<double, 10> x;
    bool res = cli::detail::lexical_conversion<decltype(x), std::array<unsigned int, 10>>(input, x);

    CHECK(res);
    CHECK(19.0 == std::get<1>(x));
    CHECK(9.0 == x[0]);
    CHECK(18.0 == x[2]);
    CHECK(5.0 == x[3]);
    CHECK(235235.0 == x[4]);
    CHECK(235235.0 == x[9]);
    input[3] = "19.7";
    res = cli::detail::lexical_conversion<decltype(x), std::array<unsigned int, 10>>(input, x);
    CHECK_FALSE(res);
}

TEST_CASE("Types: LexicalConversionComplex", "[helpers]")
{
    cli::results_t input = {"5.1", "3.5"};
    std::complex<double> x;
    bool res = cli::detail::lexical_conversion<std::complex<double>, std::array<double, 2>>(input, x);
    CHECK(res);
    CHECK(5.1 == x.real());
    CHECK(3.5 == x.imag());
}

static_assert(cli::detail::wrapper_like<std::vector<double>>, "vector double should be a wrapper");
static_assert(cli::detail::wrapper_like<std::vector<std::string>>, "vector string should be a wrapper");
static_assert(cli::detail::wrapper_like<std::string>, "string should be a wrapper");
static_assert(!cli::detail::wrapper_like<double>, "double should not be a wrapper");

static_assert(cli::detail::mutable_container<std::vector<double>>, "vector class should be a container");
static_assert(cli::detail::mutable_container<std::vector<std::string>>, "vector class should be a container");
static_assert(!cli::detail::mutable_container<std::string>, "string should be a container");
static_assert(!cli::detail::mutable_container<double>, "double should not be a container");
static_assert(!cli::detail::mutable_container<std::array<double, 5>>, "array should not be a container");

static_assert(cli::detail::mutable_container<std::vector<int>>, "vector int should be a container");

static_assert(cli::detail::readable_container<std::vector<int> &>, "vector int & should be a readable container");
static_assert(cli::detail::readable_container<const std::vector<int>>,
              "const vector int should be a readable container");
static_assert(cli::detail::readable_container<const std::vector<int> &>,
              "const vector int & should be a readable container");

TEST_CASE("FixNewLines: BasicCheck", "[helpers]")
{
    std::string input = "one\ntwo";
    std::string output = "one\n; two";
    std::string result = cli::detail::fix_newlines("; ", input);
    CHECK(output == result);
}

TEST_CASE("FixNewLines: EdgesCheck", "[helpers]")
{
    std::string input = "\none\ntwo\n";
    std::string output = "\n; one\n; two\n; ";
    std::string result = cli::detail::fix_newlines("; ", input);
    CHECK(output == result);
}

TEST_CASE("String: environment", "[helpers]")
{
    put_env("TEST1", "TESTS");

    auto value = cli::detail::get_environment_value("TEST1");
    CHECK(value == "TESTS");
    unset_env("TEST1");

    value = cli::detail::get_environment_value("TEST2");
    CHECK(value.empty());
}
