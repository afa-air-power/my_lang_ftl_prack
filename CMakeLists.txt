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
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(googletest)

enable_testing()

# ============================================================
# ⚙️  Генерация парсера и лексера при каждой сборке
# ============================================================
if (WIN32)
    set(VENV_PYTHON "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
else()
    set(VENV_PYTHON "${CMAKE_SOURCE_DIR}/.venv/bin/python3")
endif()

add_custom_target(generate_parser_lexer ALL
        COMMAND ${CMAKE_COMMAND} -E echo "⚙️ Генерация синтаксического анализатора и лексера..."
        COMMAND ${CMAKE_COMMAND} -E env ${VENV_PYTHON} ${CMAKE_SOURCE_DIR}/parser_generator.py
        COMMAND ${CMAKE_COMMAND} -E env ${VENV_PYTHON} ${CMAKE_SOURCE_DIR}/lexer_generator.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Перегенерация парсера и лексера при каждой сборке"
        VERBATIM
)

add_custom_target(regen
        COMMAND ${CMAKE_COMMAND} -E echo "🔁 Принудительная регенерация парсера и лексера..."
        COMMAND ${CMAKE_COMMAND} -E env ${VENV_PYTHON} ${CMAKE_SOURCE_DIR}/parser_generator.py
        COMMAND ${CMAKE_COMMAND} -E env ${VENV_PYTHON} ${CMAKE_SOURCE_DIR}/lexer_generator.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# ============================================================
# 📄 Принудительное копирование test_program.txt
# ============================================================
add_custom_target(copy_test_program ALL
        COMMAND ${CMAKE_COMMAND} -E echo "📄 Копирование test_program.txt в директорию сборки..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/test_program.txt
        ${CMAKE_BINARY_DIR}/test_program.txt
        COMMENT "Копирование тестового файла при каждой сборке"
        VERBATIM
)

# ============================================================
#  Основной исполняемый файл
# ============================================================
add_executable(my_lang_ftl_prak
        main.cpp
        out/parser.cpp
        out/ast_utils.cpp
        out/ast.cpp
        out/poliz.cpp
        out/poliz.hpp
        out/runner.hpp
        out/runner.cpp
        out/ast_check.cpp
        out/ast_functiontree.cpp
        out/ast_functiontree.hpp
        out/ast_check.hpp
        out/command_tree.cpp
        out/command_tree.hpp
)

add_dependencies(my_lang_ftl_prak  copy_test_program)

target_include_directories(my_lang_ftl_prak PRIVATE
        ${CMAKE_SOURCE_DIR}/out
)

# ============================================================
#  Тесты
# ============================================================
add_executable(my_lang_tests
        out/parser.cpp
        out/ast.cpp
        out/ast.hpp
)

target_include_directories(my_lang_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/out
)

target_link_libraries(my_lang_tests PRIVATE gtest_main)

add_dependencies(my_lang_tests generate_parser_lexer copy_test_program)

include(GoogleTest)
gtest_discover_tests(my_lang_tests)
