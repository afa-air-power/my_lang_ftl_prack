// ast_utils.hpp
#ifndef AST_UTILS_HPP
#define AST_UTILS_HPP

#include <string>
#include <vector>
#include <queue>
#include <ostream>

namespace ast {
    class AstNode;
    class TerminalNode;

    extern std::string current_file_path;

    void print_tree_to_file(AstNode *root, const std::string &path);

    // Вспомогательные функции поиска
    std::string find_first_identifier_in_subtree(AstNode *node);

    std::string get_id_from_tok_id_child(AstNode *parent);

    std::string find_first_type_in_subtree(AstNode *node);

    std::vector<std::string> collect_param_types(AstNode *param_list);

    bool is_function_declaration(AstNode *node);

    std::string get_function_name_from_call(AstNode *node);

    AstNode *find_condition_expression(AstNode *node);
} // namespace ast

#endif // AST_UTILS_HPP
