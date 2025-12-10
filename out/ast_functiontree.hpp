// ast_functiontree.hpp (unchanged)
#ifndef AST_FUNCTIONTREE_HPP
#define AST_FUNCTIONTREE_HPP

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace parser {
    enum class TokenType;
}

namespace ast {
    class AstNode;
    class TerminalNode;

    struct FunctionNode {
        std::string name;
        AstNode *body; // The function body AST
        std::vector<FunctionNode *> callees; // Called functions
    };

    FunctionNode *build_call_graph(AstNode *root);

    void optimize_call_graph(FunctionNode *graph_root);

    void free_call_graph(FunctionNode *node); // To clean up

    std::optional<double> try_evaluate_expression(AstNode *node);

    void dead_code_elimination(AstNode *node);

    void optimize_ast(AstNode *root);
} // namespace ast

#endif // AST_FUNCTIONTREE_HPP
