#pragma once
#include "ast.hpp"
#include "keywords.hpp"
#include <string>

namespace ast {

// Печать дерева в файл
void print_tree_to_file(AstNode* root, const std::string& path);

// Простая семантическая проверка (best-effort). Возвращает true если нет ошибок.
bool semantic_check(AstNode* root);

// Простая оптимизация/предпосчёт (constant folding и простые локальные замены)
void optimize_ast(AstNode* root);

} // namespace ast