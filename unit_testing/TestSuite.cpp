#include <gtest/gtest.h>
#include "TimeParser.h"

// Test suite: TimeParserTest
TEST(TimeParserTest, TestCaseCorrectTime) {

    // Note that this test fails on purpose!!

    // Test with correct time string
    char time_test[] = "141205";
    ASSERT_EQ(time_parse(time_test),0);

}
TEST(TimeParserTest, TestCaseNullTime) {

    char *time_test = NULL;
    ASSERT_EQ(time_parse(time_test),TIME_NULL_ERROR);

}
TEST(TimeParserTest, TestCaseIncorrectLength) {

    char time_test[] = "12345";
    ASSERT_EQ(time_parse(time_test),TIME_LEN_ERROR);

}
TEST(TimeParserTest, TestCaseIncorrectArray) {

    char time_test[] = "12a205";
    ASSERT_EQ(time_parse(time_test),TIME_ARRAY_ERROR);

}


// https://google.github.io/googletest/reference/testing.html
// https://google.github.io/googletest/reference/assertions.html
