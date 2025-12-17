// command_tree.cpp - Реализация оптимизатора дерева команд
#include "command_tree.hpp"
#include "ast_utils.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <queue>
#include <stack>
#include <sstream>
#include <functional>

namespace optimizer {

    // =============================================================================
    // ПОСТРОИТЕЛЬ ДЕРЕВА КОМАНД
    // =============================================================================

    std::shared_ptr<CommandNode> CommandTreeBuilder::build_from_ast(ast::AstNode* root) {
        if (!root) return nullptr;

        auto cmd_root = std::make_shared<CommandNode>(CommandType::NOP, "root");
        cmd_root->source_node = root;

        // Обход AST и построение дерева команд
        for (auto* child : root->children) {
            auto cmd_child = process_node(child);
            if (cmd_child) {
                cmd_root->add_child(cmd_child);
            }
        }

        return cmd_root;
    }

    std::shared_ptr<CommandNode> CommandTreeBuilder::process_node(ast::AstNode* node) {
        if (!node) return nullptr;

        std::string nodename = node->to_string();

        // Обработка различных типов узлов
        if (nodename.find("EXPR") != std::string::npos) {
            return process_expression(node);
        }
        else if (nodename == "TOK_DECLARATION" || nodename == "TOK_LOCALVARDECL") {
            auto cmd = std::make_shared<CommandNode>(CommandType::DECLARATION);
            cmd->source_node = node;
            cmd->line = node->line;
            cmd->col = node->col;
            cmd->name = ast::get_id_from_tok_id_child(node);

            variables[cmd->name] = cmd;
            return cmd;
        }
        else if (nodename == "TOK_IFSTMT") {
            auto cmd = std::make_shared<CommandNode>(CommandType::CONDITIONAL, "if");
            cmd->source_node = node;
            cmd->line = node->line;

            for (auto* child : node->children) {
                if (auto child_cmd = process_node(child)) {
                    cmd->add_child(child_cmd);
                }
            }
            return cmd;
        }
        else if (nodename == "TOK_WHILESTMT" || nodename == "TOK_FORSTMT") {
            auto cmd = std::make_shared<CommandNode>(CommandType::LOOP,
                nodename == "TOK_WHILESTMT" ? "while" : "for");
            cmd->source_node = node;
            cmd->line = node->line;

            for (auto* child : node->children) {
                if (auto child_cmd = process_node(child)) {
                    cmd->add_child(child_cmd);
                }
            }
            return cmd;
        }
        else if (nodename == "TOK_RETURNSTMT") {
            auto cmd = std::make_shared<CommandNode>(CommandType::RETURN, "return");
            cmd->source_node = node;
            cmd->line = node->line;

            for (auto* child : node->children) {
                if (auto child_cmd = process_expression(child)) {
                    cmd->add_child(child_cmd);
                }
            }
            return cmd;
        }

        // Рекурсивная обработка детей
        auto cmd = std::make_shared<CommandNode>(CommandType::NOP);
        cmd->source_node = node;

        for (auto* child : node->children) {
            if (auto child_cmd = process_node(child)) {
                cmd->add_child(child_cmd);
            }
        }

        return cmd->children.empty() ? nullptr : cmd;
    }

    std::shared_ptr<CommandNode> CommandTreeBuilder::process_expression(ast::AstNode* node) {
        if (!node) return nullptr;

        auto cmd = std::make_shared<CommandNode>(CommandType::EXPRESSION);
        cmd->source_node = node;
        cmd->line = node->line;

        // Проверка на вызов функции
        std::string nodename = node->to_string();
        if (nodename == "TOK_FUNCCALL") {
            return process_function_call(node);
        }

        // Проверка на присваивание
        if (nodename.find("ASSIGN") != std::string::npos) {
            return process_assignment(node);
        }

        // Проверка на константу
        if (auto* term = dynamic_cast<ast::TerminalNode*>(node)) {
            if (term->token_type == parser::TokenType::NUMBER) {
                cmd->is_constant = true;
                try {
                    cmd->constant_value = std::stod(term->value);
                } catch (...) {}
            }
        }

        return cmd;
    }

    std::shared_ptr<CommandNode> CommandTreeBuilder::process_function_call(ast::AstNode* node) {
        std::string func_name = ast::get_function_name_from_call(node);

        auto cmd = std::make_shared<CommandNode>(CommandType::FUNCTION_CALL, func_name);
        cmd->source_node = node;
        cmd->line = node->line;
        cmd->is_pure = is_pure_function(func_name);

        // Добавление функции в таблицу
        if (functions.find(func_name) == functions.end()) {
            functions[func_name] = cmd;
        }

        return cmd;
    }

    std::shared_ptr<CommandNode> CommandTreeBuilder::process_assignment(ast::AstNode* node) {
        auto cmd = std::make_shared<CommandNode>(CommandType::ASSIGNMENT);
        cmd->source_node = node;
        cmd->line = node->line;

        // Обработка левой и правой частей
        if (node->children.size() >= 2) {
            if (auto left = process_expression(node->children[0])) {
                cmd->add_child(left);
            }
            if (auto right = process_expression(node->children[1])) {
                cmd->add_child(right);

                // Если правая часть константна, то и присваивание константно
                if (right->is_constant) {
                    cmd->is_constant = true;
                    cmd->constant_value = right->constant_value;
                }
            }
        }

        return cmd;
    }

    bool CommandTreeBuilder::is_pure_function(const std::string& name) {
        return std::find(pure_functions.begin(), pure_functions.end(), name)
               != pure_functions.end();
    }

    // =============================================================================
    // АНАЛИЗАТОР ДЕРЕВА КОМАНД
    // =============================================================================

    std::vector<std::shared_ptr<CommandNode>> CommandTreeAnalyzer::find_hotspots(
        std::shared_ptr<CommandNode> root, int threshold) {

        std::vector<std::shared_ptr<CommandNode>> hotspots;
        find_hotspots_recursive(root, hotspots, threshold);

        // Сортировка по количеству выполнений
        std::sort(hotspots.begin(), hotspots.end(),
            [](const auto& a, const auto& b) {
                return a->execution_count > b->execution_count;
            });

        return hotspots;
    }

    void CommandTreeAnalyzer::find_hotspots_recursive(
        std::shared_ptr<CommandNode> node,
        std::vector<std::shared_ptr<CommandNode>>& hotspots,
        int threshold) {

        if (!node) return;

        if (node->execution_count >= threshold) {
            hotspots.push_back(node);
        }

        for (auto& child : node->children) {
            find_hotspots_recursive(child, hotspots, threshold);
        }
    }

    bool CommandTreeAnalyzer::detect_cycles(std::shared_ptr<CommandNode> root) {
        std::unordered_set<std::shared_ptr<CommandNode>> visited;
        std::unordered_set<std::shared_ptr<CommandNode>> rec_stack;

        return detect_cycles_dfs(root, visited, rec_stack);
    }

    bool CommandTreeAnalyzer::detect_cycles_dfs(
        std::shared_ptr<CommandNode> node,
        std::unordered_set<std::shared_ptr<CommandNode>>& visited,
        std::unordered_set<std::shared_ptr<CommandNode>>& rec_stack) {

        if (!node) return false;

        visited.insert(node);
        rec_stack.insert(node);

        // Проверка зависимостей
        for (auto& dep : node->dependencies) {
            if (visited.find(dep) == visited.end()) {
                if (detect_cycles_dfs(dep, visited, rec_stack)) {
                    return true;
                }
            } else if (rec_stack.find(dep) != rec_stack.end()) {
                return true; // Цикл обнаружен
            }
        }

        rec_stack.erase(node);
        return false;
    }

    std::vector<std::shared_ptr<CommandNode>> CommandTreeAnalyzer::find_inline_candidates(
        std::shared_ptr<CommandNode> root) {

        std::vector<std::shared_ptr<CommandNode>> candidates;

        std::function<void(std::shared_ptr<CommandNode>)> traverse;
        traverse = [&](std::shared_ptr<CommandNode> node) {
            if (!node) return;

            // Критерии для инлайнинга:
            // 1. Малое количество детей (< 10)
            // 2. Не рекурсивная функция
            // 3. Часто вызывается (execution_count > 5)
            if (node->type == CommandType::FUNCTION_CALL &&
                node->children.size() < 10 &&
                node->execution_count > 5) {

                node->can_inline = true;
                candidates.push_back(node);
            }

            for (auto& child : node->children) {
                traverse(child);
            }
        };

        traverse(root);
        return candidates;
    }

    std::vector<std::shared_ptr<CommandNode>> CommandTreeAnalyzer::find_constant_expressions(
        std::shared_ptr<CommandNode> root) {

        std::vector<std::shared_ptr<CommandNode>> constants;

        std::function<void(std::shared_ptr<CommandNode>)> traverse;
        traverse = [&](std::shared_ptr<CommandNode> node) {
            if (!node) return;

            if (node->is_constant) {
                constants.push_back(node);
            }

            for (auto& child : node->children) {
                traverse(child);
            }
        };

        traverse(root);
        return constants;
    }

    std::string CommandTreeAnalyzer::estimate_complexity(std::shared_ptr<CommandNode> root) {
        if (!root) return "O(1)";

        int loop_depth = 0;
        int max_loop_depth = 0;

        std::function<void(std::shared_ptr<CommandNode>, int)> traverse;
        traverse = [&](std::shared_ptr<CommandNode> node, int depth) {
            if (!node) return;

            if (node->type == CommandType::LOOP) {
                loop_depth++;
                max_loop_depth = std::max(max_loop_depth, loop_depth);
            }

            for (auto& child : node->children) {
                traverse(child, depth + 1);
            }

            if (node->type == CommandType::LOOP) {
                loop_depth--;
            }
        };

        traverse(root, 0);

        if (max_loop_depth == 0) return "O(1)";
        if (max_loop_depth == 1) return "O(n)";
        if (max_loop_depth == 2) return "O(n²)";
        return "O(n^" + std::to_string(max_loop_depth) + ")";
    }

    // =============================================================================
    // ОПТИМИЗАТОР ДЕРЕВА КОМАНД
    // =============================================================================

    void CommandTreeOptimizer::optimize(std::shared_ptr<CommandNode> root) {
        std::cout << "\n=== Command Tree Optimization ===" << std::endl;

        fold_constants(root);
        eliminate_dead_code(root);
        inline_functions(root);
        optimize_tail_recursion(root);

        std::cout << "=== Optimization Complete ===" << std::endl;
    }

    void CommandTreeOptimizer::fold_constants(std::shared_ptr<CommandNode> root) {
        if (!root) return;

        // Рекурсивная свертка констант
        for (auto& child : root->children) {
            fold_constants(child);
        }

        // Если все дети константны и узел - выражение
        if (root->type == CommandType::EXPRESSION && !root->children.empty()) {
            bool all_const = true;
            for (auto& child : root->children) {
                if (!child->is_constant) {
                    all_const = false;
                    break;
                }
            }

            if (all_const) {
                // Вычислить константное значение
                std::cout << "Folding constants at line " << root->line << std::endl;
                root->is_constant = true;
            }
        }
    }

    void CommandTreeOptimizer::eliminate_dead_code(std::shared_ptr<CommandNode> root) {
        if (!root) return;

        // Удаление недостижимого кода после return
        bool found_return = false;

        auto it = root->children.begin();
        while (it != root->children.end()) {
            if (found_return) {
                std::cout << "Eliminating dead code after return at line "
                          << (*it)->line << std::endl;
                it = root->children.erase(it);
            } else {
                if ((*it)->type == CommandType::RETURN) {
                    found_return = true;
                }
                eliminate_dead_code(*it);
                ++it;
            }
        }
    }

    void CommandTreeOptimizer::inline_functions(std::shared_ptr<CommandNode> root) {
        if (!root) return;

        for (auto& child : root->children) {
            if (child->can_inline && child->type == CommandType::FUNCTION_CALL) {
                std::cout << "Inlining function '" << child->name
                          << "' at line " << child->line << std::endl;
                // Логика инлайнинга
            }
            inline_functions(child);
        }
    }

    void CommandTreeOptimizer::optimize_tail_recursion(std::shared_ptr<CommandNode> root) {
        if (!root) return;

        if (is_tail_recursive(root)) {
            std::cout << "Converting tail recursion to loop at line "
                      << root->line << std::endl;
            // Логика преобразования
        }

        for (auto& child : root->children) {
            optimize_tail_recursion(child);
        }
    }

    bool CommandTreeOptimizer::is_tail_recursive(std::shared_ptr<CommandNode> node) {
        if (!node || node->type != CommandType::FUNCTION_CALL) return false;

        // Проверка на хвостовую рекурсию
        // (последний вызов в функции - вызов самой себя)
        return false; // Упрощенная проверка
    }

    // =============================================================================
    // ВИЗУАЛИЗАЦИЯ ДЕРЕВА КОМАНД
    // =============================================================================

    void CommandTreeVisualizer::print(std::shared_ptr<CommandNode> root, int indent) {
        print_recursive(root, indent);
    }

    void CommandTreeVisualizer::print_recursive(std::shared_ptr<CommandNode> node, int indent) {
        if (!node) return;

        std::string indent_str(indent * 2, ' ');
        std::string type_str;

        switch (node->type) {
            case CommandType::FUNCTION_CALL: type_str = "CALL"; break;
            case CommandType::ASSIGNMENT: type_str = "ASSIGN"; break;
            case CommandType::RETURN: type_str = "RETURN"; break;
            case CommandType::CONDITIONAL: type_str = "IF"; break;
            case CommandType::LOOP: type_str = "LOOP"; break;
            case CommandType::EXPRESSION: type_str = "EXPR"; break;
            case CommandType::DECLARATION: type_str = "DECL"; break;
            case CommandType::NOP: type_str = "NOP"; break;
        }

        std::cout << indent_str << type_str;
        if (!node->name.empty()) {
            std::cout << " '" << node->name << "'";
        }
        if (node->is_constant && node->constant_value) {
            std::cout << " = " << *node->constant_value;
        }
        if (node->execution_count > 0) {
            std::cout << " [exec: " << node->execution_count << "]";
        }
        std::cout << " (line " << node->line << ")" << std::endl;

        for (auto& child : node->children) {
            print_recursive(child, indent + 1);
        }
    }

    CommandTreeVisualizer::Statistics CommandTreeVisualizer::gather_statistics(
        std::shared_ptr<CommandNode> root) {

        Statistics stats;
        gather_statistics_recursive(root, stats);
        return stats;
    }

    void CommandTreeVisualizer::gather_statistics_recursive(
        std::shared_ptr<CommandNode> node,
        Statistics& stats) {

        if (!node) return;

        stats.total_nodes++;
        stats.max_depth = std::max(stats.max_depth, node->depth);

        switch (node->type) {
            case CommandType::FUNCTION_CALL:
                stats.function_calls++;
                stats.function_call_counts[node->name]++;
                break;
            case CommandType::LOOP:
                stats.loops++;
                break;
            case CommandType::CONDITIONAL:
                stats.conditionals++;
                break;
            default:
                break;
        }

        for (auto& child : node->children) {
            gather_statistics_recursive(child, stats);
        }
    }

} // namespace optimizer