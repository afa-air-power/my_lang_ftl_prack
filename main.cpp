#include <fstream>
#include <sstream>
#include <iostream>
#include "out/lexer.hpp"
#include "out/parser.hpp"
#include "out/ast.hpp"       // Для типа AstNode
#include "out/ast_utils.hpp" // Для semantic_check и optimize_ast
#include "poliz.hpp"       // Для класса poliz::Poliz

extern std::string current_file_path; // Глобальная переменная из ast_utils.cpp

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

    // Создаём лексер
    lexer::Lexer lex(source, "../out/system_reserved_identifiers.txt");

    // Запускаем синтаксический анализ
    ast::AstNode* root = nullptr;
    try {
        // Установка имени текущего файла для сообщений об ошибках
        current_file_path = path;

        // Предполагаем, что parser::parse() теперь возвращает корень AST
        root = parser::parse(lex, path);

        if (!root) {
            std::cerr << "❌ Parsing failed (AST root is null)." << std::endl;
            return 1;
        }

        std::cout << "\n=== Starting Semantic Checks ===\n";
        if (ast::semantic_check(root)) {
            std::cout << "✅ Semantic checks passed.\n";

            std::cout << "\n=== Starting AST Optimization ===\n";
            ast::optimize_ast(root);
            std::cout << "✅ AST optimization complete.\n";

            std::cout << "\n=== Starting POLIZ Generation ===\n";
            poliz::Poliz poliz_generator;
            poliz_generator.generate(root);
            poliz_generator.print();
            std::cout << "✅ POLIZ generation complete.\n";

        } else {
            std::cerr << "❌ Semantic checks failed. Stopping compilation.\n";
            // В случае ошибки root будет удален ниже, если не null
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ Exception during compilation: " << e.what() << std::endl;
        // Продолжаем выполнение, чтобы удалить root, если он был создан
    }

    if (root) delete root; // Очистка памяти AST
    return 0;
}