#pragma once
#include "ast.hpp"
#include "keywords.hpp"
#include <string>

// Global current file path used by parser/semantic components
extern std::string current_file_path;

namespace ast {

// Печать дерева в файл
void print_tree_to_file(AstNode* root, const std::string& path);

// Простая семантическая проверка (best-effort). Возвращает true если нет ошибок.
bool semantic_check(AstNode* root);

// Простая оптимизация/предосчет (constant folding и простые локальные замены)
void optimize_ast(AstNode* root);

// Helper declarations used by ast_check.cpp / optimizer
std::string find_first_identifier_in_subtree(AstNode* node);
std::string get_id_from_tok_id_child(AstNode* parent);
std::string find_first_type_in_subtree(AstNode* node);
std::vector<std::string> collect_param_types(AstNode* param_list);
bool is_function_declaration(AstNode* node);
std::string get_function_name_from_call(AstNode* node);
AstNode* find_condition_expression(AstNode* node);

} // namespace ast