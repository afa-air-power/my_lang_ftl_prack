#include <fstream>
#include <sstream>
#include <iostream>
#include "out/lexer.hpp"
#include "out/parser.hpp"

int main(int argc, char** argv) {
    std::string path = "test_program.txt";  // файл для теста
    if (argc > 1)
        path = argv[1];

    std::ifstream file;
    file.open(path);
    std::cout << path << std::endl;
    if (!file.is_open()) {
        std::cerr << "❌ Cannot open file: " << path << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Создаём лексер на основе содержимого файла lexer::Lexer lex(source, "out/system_reserved_identifiers.txt");
    lexer::Lexer lex(source, "out/system_reserved_identifiers.txt");


    // Запускаем синтаксический анализ
    try {
        parser::parse(lex, path);
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
