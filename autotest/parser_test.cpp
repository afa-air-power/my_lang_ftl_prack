#include <gtest/gtest.h>
#include "../out/parser.hpp"
#include "../out/lexer.hpp"
#include <fstream>

TEST(ParserTest, CanParseSimpleProgram) {
    const std::string testFile = "test_input.lang";
    const std::string keywordsFile = "out/system_reserved_identifiers.txt";

    // создаём тестовую программу
    std::ofstream f(testFile);
    f << "int main() { int x = 5; return x; }";
    f.close();

    // создаём лексер с двумя аргументами (текст + ключевые слова)
    lexer::Lexer lexer(testFile, keywordsFile);

    // проверяем, что парсер не выбрасывает исключений
    EXPECT_NO_THROW({
        parser::parse(lexer, testFile);
    }) << "Parser failed on simple input";

    // очищаем тестовый файл
    std::remove(testFile.c_str());
}
