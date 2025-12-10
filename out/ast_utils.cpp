// ast_utils.cpp
#include "ast_utils.hpp"
#include "ast.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include "ast_utils.hpp"
#include "ast.hpp"  // For full definitions
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <queue>
#include <algorithm>
#include <functional>
#include <cmath>
#include <optional>
#include <vector>
#include <queue>
#include <algorithm>
#include <functional>
#include <cmath>
#include <optional>
#include "ast_utils.hpp"
#include "ast.hpp"
#include <fstream>
// ... (other includes)

// Add this definition:
std::string ast::current_file_path;

// Предполагаем, что эти узлы объявлены в ast.hpp
namespace ast {
    class TOK_ATOMNode;
    class TOK_LITERALNode;
}

extern std::string current_file_path;

namespace ast {
    // =============================================================
    // Вспомогательные структуры и утилиты
    // =============================================================

    // ----------------- Печать -----------------
    static void print_node_to_stream(const AstNode *node, std::ostream &out, int depth) {
        if (!node) return;
        out << std::string(depth * 2, ' ') << node->to_string() << "\n";
        for (const auto *c: node->children) {
            print_node_to_stream(c, out, depth + 1);
        }
    }

    void print_tree_to_file(AstNode *root, const std::string &path) {
        std::ofstream out(path);
        if (!out.is_open()) {
            std::cerr << "Could not open " << path << " for AST dump\n";
            return;
        }
        print_node_to_stream(root, out, 0);
        out.close();
    }

    // ----------------- Вспомогательные функции поиска -----------------

    std::string find_first_identifier_in_subtree(AstNode *node) {
        if (!node) return {};
        auto *tn = dynamic_cast<TerminalNode *>(node);
        if (tn && tn->token_type == parser::TokenType::IDENTIFIER) return tn->value;
        for (auto *c: node->children) {
            auto r = find_first_identifier_in_subtree(c);
            if (!r.empty()) return r;
        }
        return {};
    }

    struct FunctionInfo {
        std::string return_type;
        std::vector<std::string> param_types;
        int line;
        int col;
        bool is_method = false;
        std::string class_name;
    };

    std::string get_id_from_tok_id_child(AstNode *parent) {
        if (!parent) return "";
        for (auto *c: parent->children) {
            if (c->to_string() == "TOK_ID") {
                return find_first_identifier_in_subtree(c);
            }
        }
        return "";
    }

    std::string find_first_type_in_subtree(AstNode *node) {
        if (!node) return {};
        if (node->to_string() == "TOK_TYPE") {
            std::queue<AstNode *> q;
            q.push(node);
            while (!q.empty()) {
                AstNode *curr = q.front();
                q.pop();
                if (auto *tn = dynamic_cast<TerminalNode *>(curr)) {
                    if (tn->token_type == parser::TokenType::IDENTIFIER ||
                        tn->value == "int" || tn->value == "float" || tn->value == "double" ||
                        tn->value == "string" || tn->value == "bool" || tn->value == "void" ||
                        tn->value == "vector") {
                        return tn->value;
                    }
                }
                for (auto *c: curr->children) q.push(c);
            }
            return {};
        }
        for (auto *c: node->children) {
            auto r = find_first_type_in_subtree(c);
            if (!r.empty()) return r;
        }
        return {};
    }

    std::vector<std::string> collect_param_types(AstNode *param_list) {
        std::vector<std::string> result;
        if (!param_list) return result;

        std::function<void(AstNode *)> collect = [&](AstNode *n) {
            if (!n) return;
            if (n->to_string() == "TOK_PARAM") {
                std::string type = find_first_type_in_subtree(n);
                if (!type.empty()) result.push_back(type);
            }
            for (auto *c: n->children) {
                collect(c);
            }
        };

        collect(param_list);
        return result;
    }

    bool is_function_declaration(AstNode *node) {
        if (!node) return false;
        for (auto *c: node->children) {
            if (c->to_string() == "TOK_PARAMLIST") return true;

            if (c->to_string() == "TOK_DECLSUFFIX" || c->to_string() == "TOK_MEMBERSUFFIX") {
                for (auto *sub: c->children) {
                    if (sub->to_string() == "TOK_PARAMLIST") return true;
                }
            }
        }
        return false;
    }

    // НОВЫЕ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    std::string get_function_name_from_call(AstNode *node) {
        if (!node) return "";
        for (auto *c: node->children) {
            if (c->to_string() == "TOK_ID") {
                return find_first_identifier_in_subtree(c);
            }
        }
        return "";
    }

    AstNode *find_condition_expression(AstNode *node) {
        if (!node) return nullptr;

        // Ищем выражение условия в if/while
        std::queue<AstNode *> q;
        q.push(node);

        while (!q.empty()) {
            AstNode *current = q.front();
            q.pop();

            if (current->to_string().find("EXPR") != std::string::npos ||
                current->to_string() == "TOK_EXPRESSION") {
                return current;
            }

            for (auto *c: current->children) {
                q.push(c);
            }
        }

        return nullptr;
    }
} // namespace ast
