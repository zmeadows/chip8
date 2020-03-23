#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp
                           // file
#include "catch.hpp"
#include "chip8.hpp"

using namespace chip8;

TEST_CASE("opcode_get_hex_digit_at", "[opcode]")
{
    REQUIRE(opcode_get_hex_digit_at<3>(0xF123) == 0x3);
    REQUIRE(opcode_get_hex_digit_at<2>(0xF123) == 0x2);
    REQUIRE(opcode_get_hex_digit_at<1>(0xF123) == 0x1);
    REQUIRE(opcode_get_hex_digit_at<0>(0xF123) == 0xF);

    REQUIRE(opcode_get_hex_digit_at<3>(0x0AB0) == 0x0);
    REQUIRE(opcode_get_hex_digit_at<2>(0x0AB0) == 0xB);
    REQUIRE(opcode_get_hex_digit_at<1>(0x0AB0) == 0xA);
    REQUIRE(opcode_get_hex_digit_at<0>(0x0AB0) == 0x0);

    REQUIRE(opcode_get_hex_digit_at<3>(0x00C0) == 0x0);
    REQUIRE(opcode_get_hex_digit_at<2>(0x00C0) == 0xC);
    REQUIRE(opcode_get_hex_digit_at<1>(0x00C0) == 0x0);
    REQUIRE(opcode_get_hex_digit_at<0>(0x00C0) == 0x0);

    REQUIRE(opcode_get_hex_digit_at<3>(0x0000) == 0x0);
    REQUIRE(opcode_get_hex_digit_at<2>(0x0000) == 0x0);
    REQUIRE(opcode_get_hex_digit_at<1>(0x0000) == 0x0);
    REQUIRE(opcode_get_hex_digit_at<0>(0x0000) == 0x0);

    REQUIRE(opcode_get_hex_digit_at<3>(0xFFFF) == 0xF);
    REQUIRE(opcode_get_hex_digit_at<2>(0xFFFF) == 0xF);
    REQUIRE(opcode_get_hex_digit_at<1>(0xFFFF) == 0xF);
    REQUIRE(opcode_get_hex_digit_at<0>(0xFFFF) == 0xF);
}
