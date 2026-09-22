#define __STRINGIFY(x) #x
#define TEST_STRINGIFY(x) __STRINGIFY(x)

#define TEST_VERSION_MAJOR 6
#define TEST_VERSION_MINOR 0
#define TEST_VERSION_BUGFIX 0

#define TEST_VERSION_STR                                       \
    TEST_STRINGIFY(TEST_VERSION_MAJOR)                         \
    "." TEST_STRINGIFY(TEST_VERSION_MINOR) "." TEST_STRINGIFY( \
        TEST_VERSION_BUGFIX)

#define TEST_CONCAT_1 \
    "ab"              \
    "cd"              \
    "ef"              \
    ""
#define TEST_CONCAT_2 1
#define TEST_CONCAT_3 1 2 3
#define TEST_CONCAT_4 "ab" 1 "cd"
#define TEST_CONCAT_5 \
    "ab\""            \
    "cd"
#define TEST_CONCAT_6 "ab\" \"cd"
