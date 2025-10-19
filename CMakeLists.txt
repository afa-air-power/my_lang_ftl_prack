cmake_minimum_required(VERSION 3.20)
project(my_lang_ftl_prak LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- Fetch GoogleTest (for unit tests) ---
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# Optional: avoid timestamp warning by setting the option (newer CMake)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(googletest)

enable_testing()

# ============================================================
# ⚙️  Генерация парсера и лексера при каждой сборке
# ============================================================
# Псевдо-цель, которая всегда выполняется (ALL) — запускает генераторы
add_custom_target(generate_parser_lexer ALL
        COMMAND ${CMAKE_COMMAND} -E echo "⚙️  Генерация синтаксического анализатора и лексера..."
        COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/parser_generator.py
        COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/lexer_generator.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Перегенерация парсера и лексера при каждой сборке"
        VERBATIM
)

# (Опционально) Скопировать test_program.txt в binary dir, чтобы исполняемый находил его
file(COPY ${CMAKE_SOURCE_DIR}/test_program.txt DESTINATION ${CMAKE_BINARY_DIR})

# ============================================================
#  Основной исполняемый файл
# ============================================================
add_executable(my_lang_ftl_prak
        main.cpp
        out/parser.cpp    # generated file; generate_parser_lexer target will create it before build
)

# ensure the generation target runs before building the executable
add_dependencies(my_lang_ftl_prak generate_parser_lexer)

target_include_directories(my_lang_ftl_prak PRIVATE
        ${CMAKE_SOURCE_DIR}/out
)

# ============================================================
#  Тесты (отдельный таргет)
# ============================================================
add_executable(my_lang_tests
        autotest/grammar_test.cpp
        autotest/parser_test.cpp
        out/parser.cpp     # reuse generated parser implementation (ok for testing)
)

target_include_directories(my_lang_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/out
)

target_link_libraries(my_lang_tests PRIVATE gtest_main)

# make sure tests also depend on generation
add_dependencies(my_lang_tests generate_parser_lexer)

include(GoogleTest)
gtest_discover_tests(my_lang_tests)

# ============================================================
#  Удобный таргет для форсированного регенерирования (опционально)
# ============================================================
add_custom_target(regen
        COMMAND ${CMAKE_COMMAND} -E echo "Forcing regeneration of parser and lexer..."
        COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/parser_generator.py
        COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/lexer_generator.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Force regeneration of parser & lexer"
)
