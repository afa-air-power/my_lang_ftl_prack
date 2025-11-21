#include <gtest/gtest.h>
#include <sstream>
#include "../out/lexer.hpp"
#include "../out/parser.hpp"
#include "../out/ast.hpp"
#include "../out/ast_utils.hpp"

// Helper для создания временного лексера и парсера
class SemanticTestHelper {
public:
    static ast::AstNode* parse_code(const std::string& code) {
        lexer::Lexer lex(code, "../out/system_reserved_identifiers.txt");

        // Перенаправляем stderr для подавления вывода
        std::streambuf* old_cerr = std::cerr.rdbuf();
        std::ostringstream null_stream;
        std::cerr.rdbuf(null_stream.rdbuf());

        ast::AstNode* root = nullptr;
        try {
            root = parser::parse(lex, "test.txt");
        } catch (...) {
            std::cerr.rdbuf(old_cerr);
            return nullptr;
        }

        std::cerr.rdbuf(old_cerr);
        return root;
    }

    static bool check_semantics(const std::string& code) {
        ast::AstNode* root = parse_code(code);
        if (!root) return false;

        bool result = ast::semantic_check(root);
        ast::delete_tree(root);
        return result;
    }

    static std::string get_semantic_errors(const std::string& code) {
        std::streambuf* old_cerr = std::cerr.rdbuf();
        std::ostringstream error_stream;
        std::cerr.rdbuf(error_stream.rdbuf());

        check_semantics(code);

        std::cerr.rdbuf(old_cerr);
        return error_stream.str();
    }
};

// =============================================================================
// Тесты для базовых проверок
// =============================================================================

TEST(SemanticTest, SimpleVariableDeclaration) {
    std::string code = R"(
        int main() {
            int x;
            x = 5;
            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

TEST(SemanticTest, UndefinedVariable) {
    std::string code = R"(
        int main() {
            x = 5;
            return 0;
        }
    )";

    EXPECT_FALSE(SemanticTestHelper::check_semantics(code));

    std::string errors = SemanticTestHelper::get_semantic_errors(code);
    EXPECT_TRUE(errors.find("undefined identifier 'x'") != std::string::npos);
}

TEST(SemanticTest, UnknownType) {
    std::string code = R"(
        int main() {
            UnknownType var;
            return 0;
        }
    )";

    EXPECT_FALSE(SemanticTestHelper::check_semantics(code));

    std::string errors = SemanticTestHelper::get_semantic_errors(code);
    EXPECT_TRUE(errors.find("unknown type 'UnknownType'") != std::string::npos);
}

TEST(SemanticTest, ValidClassDeclaration) {
    std::string code = R"(
        class Point {
            int x;
            int y;
        };

        int main() {
            Point p;
            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

// =============================================================================
// Тесты для доступа к полям класса
// =============================================================================

TEST(SemanticTest, ValidMemberAccess) {
    std::string code = R"(
        class Point {
            int x;
            int y;
        };

        int main() {
            Point p;
            p.x = 10;
            p.y = 20;
            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

TEST(SemanticTest, InvalidMemberAccess) {
    std::string code = R"(
        class Point {
            int x;
            int y;
        };

        int main() {
            Point p;
            p.z = 10;
            return 0;
        }
    )";

    EXPECT_FALSE(SemanticTestHelper::check_semantics(code));

    std::string errors = SemanticTestHelper::get_semantic_errors(code);
    EXPECT_TRUE(errors.find("has no member named 'z'") != std::string::npos);
}

TEST(SemanticTest, MemberAccessOnNonClassType) {
    std::string code = R"(
        int main() {
            int x;
            x.y = 5;
            return 0;
        }
    )";

    EXPECT_FALSE(SemanticTestHelper::check_semantics(code));
}

TEST(SemanticTest, ComplexClassWithVectors) {
    std::string code = R"(
        class Person {
            int id;
            string name;
            vector<int> scores;
        };

        int main() {
            Person p;
            p.id = 1;
            p.name = "Alice";
            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

// =============================================================================
// Тесты для областей видимости
// =============================================================================

TEST(SemanticTest, NestedScopes) {
    std::string code = R"(
        int main() {
            int x;
            x = 5;
            {
                int y;
                y = x;
            }
            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

TEST(SemanticTest, VariableOutOfScope) {
    std::string code = R"(
        int main() {
            {
                int x;
                x = 5;
            }
            int y;
            y = x;
            return 0;
        }
    )";

    EXPECT_FALSE(SemanticTestHelper::check_semantics(code));
}

TEST(SemanticTest, ShadowingVariable) {
    std::string code = R"(
        int main() {
            int x;
            x = 5;
            {
                int x;
                x = 10;
            }
            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

// =============================================================================
// Тесты для проверки типов
// =============================================================================

TEST(SemanticTest, TypeCompatibilityIntToDouble) {
    std::string code = R"(
        int main() {
            double x;
            x = 5;
            return 0;
        }
    )";

    // int -> double разрешено
    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

TEST(SemanticTest, TypeIncompatibilityStringToInt) {
    std::string code = R"(
        int main() {
            int x;
            x = "hello";
            return 0;
        }
    )";

    EXPECT_FALSE(SemanticTestHelper::check_semantics(code));
}

// =============================================================================
// Тесты для функций
// =============================================================================

TEST(SemanticTest, FunctionDeclaration) {
    std::string code = R"(
        int add(int a, int b) {
            return a + b;
        }

        int main() {
            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

TEST(SemanticTest, FunctionParameters) {
    std::string code = R"(
        int multiply(int a, int b) {
            int result;
            result = a * b;
            return result;
        }

        int main() {
            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

// =============================================================================
// Тесты для сложных сценариев
// =============================================================================

TEST(SemanticTest, ComplexHumanClassExample) {
    std::string code = R"(
        class human {
            int id;
            string name;
            int hp;
            double stress;
            vector<int> friends;
        };

        int main() {
            human p0;
            human p1;

            p0.id = 0;
            p0.name = "Alice";
            p0.hp = 100;
            p0.stress = 25.5;

            p1.id = 1;
            p1.name = "Bob";

            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

TEST(SemanticTest, MultipleClassesInteraction) {
    std::string code = R"(
        class Point {
            int x;
            int y;
        };

        class Rectangle {
            Point topLeft;
            Point bottomRight;
        };

        int main() {
            Point p;
            Rectangle r;

            p.x = 10;
            p.y = 20;

            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

// =============================================================================
// Тесты для глобальных переменных
// =============================================================================

TEST(SemanticTest, GlobalVariable) {
    std::string code = R"(
        int global_var;

        int main() {
            global_var = 42;
            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

TEST(SemanticTest, GlobalAndLocalVariable) {
    std::string code = R"(
        int count;

        int main() {
            int count;
            count = 10;
            return 0;
        }
    )";

    EXPECT_TRUE(SemanticTestHelper::check_semantics(code));
}

// =============================================================================
// Негативные тесты (должны провалиться)
// =============================================================================

TEST(SemanticTest, UseBeforeDeclaration) {
    std::string code = R"(
        int main() {
            y = 10;
            int y;
            return 0;
        }
    )";

    EXPECT_FALSE(SemanticTestHelper::check_semantics(code));
}

TEST(SemanticTest, AccessFieldOfUndeclaredVariable) {
    std::string code = R"(
        class Point {
            int x;
        };

        int main() {
            p.x = 10;
            return 0;
        }
    )";

    EXPECT_FALSE(SemanticTestHelper::check_semantics(code));
}

// =============================================================================
// Главная функция для запуска тестов
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}