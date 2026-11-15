#include <fstream>
#include <sstream>
#include <iostream>
#include "out/lexer.hpp"
#include "out/parser.hpp"
#include "out/ast.hpp"
#include "out/ast_utils.hpp"
#include "out/poliz.hpp"
#include "out/runner.hpp"
#include "out/ast_check.hpp"

extern std::string current_file_path;

int main(int argc, char **argv) {
    std::string path = "test_program.txt"; // файл для теста
    if (argc > 1)
        path = argv[1];
    current_file_path = path;

    std::ifstream file;
    file.open(current_file_path);
    std::cout << path << std::endl;
    if (!file.is_open()) {
        std::cerr << "❌ Cannot open file: " << path << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Создаём лексер - попытаемся найти файл с ключевыми словами
    std::string keywords_path = "system_reserved_identifiers.txt";
    std::ifstream kw_test(keywords_path);
    if (!kw_test.is_open()) {
        keywords_path = "../system_reserved_identifiers.txt";
    }
    kw_test.close();
    lexer::Lexer lex(source, keywords_path);

    // Запускаем синтаксический анализ
    ast::AstNode *root = nullptr;
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

            std::cout << "\n=== Starting Program Interpretation ===\n";
            runner::Interpreter interp(poliz_generator.get_code());
            interp.run();
            std::cout << "✅ Program interpretation complete.\n";

        } else {
            std::cerr << "❌ Semantic checks failed. Stopping compilation.\n";
            // В случае ошибки root будет удален ниже, если не null
        }
    } catch (const std::exception &e) {
        std::cerr << "❌ Exception during compilation: " << e.what() << std::endl;
        // Продолжаем выполнение, чтобы удалить root, если он был создан
    }

    delete root; // Очистка памяти AST
    return 0;
}
