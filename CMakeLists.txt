cmake_minimum_required(VERSION 3.20)
project(my_lang_ftl_prak LANGUAGES CXX)
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# --- Основной бинарь ---
add_executable(my_lang_ftl_prak
        main.cpp
        out/parser.cpp
)

target_include_directories(my_lang_ftl_prak PRIVATE out)

# --- Отдельная сборка автотестов ---
add_executable(my_lang_tests
        autotest/grammar_test.cpp
        autotest/parser_test.cpp
        out/parser.cpp
)
file(COPY ${CMAKE_SOURCE_DIR}/test_program.txt DESTINATION ${CMAKE_BINARY_DIR})

target_include_directories(my_lang_tests PRIVATE out)
target_link_libraries(my_lang_tests PRIVATE gtest_main)

include(GoogleTest)
gtest_discover_tests(my_lang_tests)
