cmake_minimum_required(VERSION 3.16)
project(my_lang_ftl_prak)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Автогенерация парсера и лексера ===
add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/out/parser.cpp
        ${CMAKE_SOURCE_DIR}/out/parser.hpp
        ${CMAKE_SOURCE_DIR}/out/keywords.hpp
        ${CMAKE_SOURCE_DIR}/out/lexer.hpp
        COMMAND ${CMAKE_COMMAND} -E echo "⚙️  Генерация синтаксического анализатора и лексера..."
        COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/parser_generator.py
        COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/lexer_generator.py
        DEPENDS ${CMAKE_SOURCE_DIR}/grammar.txt
        ${CMAKE_SOURCE_DIR}/parser_generator.py
        ${CMAKE_SOURCE_DIR}/lexer_generator.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running parser and lexer generators"
)

add_executable(my_lang_ftl_prak
        main.cpp
        ${CMAKE_SOURCE_DIR}/out/lexer.hpp
        ${CMAKE_SOURCE_DIR}/out/parser.cpp
)

target_include_directories(my_lang_ftl_prak PRIVATE
        ${CMAKE_SOURCE_DIR}/out
)
