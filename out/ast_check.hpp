// ast_check.hpp
#ifndef AST_CHECK_HPP
#define AST_CHECK_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include "ast_utils.hpp"
#include "ast.hpp"  // Added for full AstNode/TerminalNode definitions
#include "keywords.hpp"  // Added for parser::TokenType
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
#include "ast_utils.hpp"  // Для вспомогательных функций

namespace parser {
    enum class TokenType;
}

namespace ast {
    class AstNode;
    class TerminalNode;

    struct FunctionInfo {
        std::string return_type;
        std::vector<std::string> param_types;
        int line;
        int col;
        bool is_method = false;
        std::string class_name;
    };

    struct VarInfo {
        std::string type;
        int line;
        int col;
    };

    struct ClassInfo {
        std::unordered_map<std::string, std::string> fields;
        std::unordered_map<std::string, FunctionInfo> methods;
    };

    struct SemanticState {
        std::vector<std::unordered_map<std::string, VarInfo> > scopes;
        std::unordered_set<std::string> known_types;
        std::unordered_map<std::string, ClassInfo> classes;
        std::unordered_map<std::string, FunctionInfo> functions;
        std::vector<std::string> messages;

        std::string current_function_return_type;
        std::string current_class_context;
        int loop_depth = 0;

        void push_scope();

        void pop_scope();

        void declare_var(const std::string &name, const std::string &type, int line, int col);

        bool is_var_declared(const std::string &name) const;

        std::string get_var_type(const std::string &name);

        void add_type(const std::string &t);

        bool is_known_type(const std::string &t) const;

        void add_class(const std::string &name, const ClassInfo &info);

        ClassInfo *get_class_info(const std::string &name);

        void add_function(const std::string &name, const FunctionInfo &info);

        FunctionInfo *get_function_info(const std::string &name);

        void error(AstNode *node, const std::string &msg);

        void warning(AstNode *node, const std::string &msg);

        bool is_assignable(const std::string &target_type, const std::string &source_type) const;

        bool are_types_compatible(const std::string &type1, const std::string &type2) const;
    };

    std::string infer_expression_type(AstNode *expr, SemanticState &st);

    std::vector<std::string> collect_argument_types(AstNode *node, SemanticState &st);

    void semantic_collect_defs(AstNode *node, SemanticState &st);

    void semantic_check_and_validate(AstNode *node, SemanticState &st, AstNode *parent = nullptr);

    bool semantic_check(AstNode *root);
} // namespace ast

#endif // AST_CHECK_HPP
