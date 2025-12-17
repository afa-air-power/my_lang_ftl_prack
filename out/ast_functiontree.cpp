// ast_functiontree.cpp (updated with recursion detection and inlining)
#include "ast_functiontree.hpp"
#include "ast_utils.hpp"
#include "ast.hpp"  // For full definitions
#include "keywords.hpp"  // For TokenType
#include "command_tree.hpp"  // ← НОВОЕ: интеграция Command Tree
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
#include <stack>

namespace ast {
    // =============================================================================
    // ОПТИМИЗАЦИИ (Constant Folding и Dead Code Elimination)
    // =============================================================================

    std::optional<double> try_evaluate_expression(AstNode *node) {
        if (!node) return std::nullopt;

        if (auto *tn = dynamic_cast<TerminalNode *>(node)) {
            if (tn->token_type == parser::TokenType::NUMBER) {
                try { return std::stod(tn->value); } catch (...) { return std::nullopt; }
            }
            return std::nullopt;
        }

        if (node->children.size() == 1) {
            return try_evaluate_expression(node->children[0]);
        }

        if (node->children.size() == 2) {
            auto val_left = try_evaluate_expression(node->children[0]);
            if (!val_left) return std::nullopt;

            AstNode *rest = node->children[1];
            if (rest->children.size() >= 2) {
                std::string op;
                AstNode *right_node = nullptr;

                if (auto *term = dynamic_cast<TerminalNode *>(rest->children[0])) {
                    op = term->value;
                    right_node = rest->children[1];
                }

                if (!op.empty() && right_node) {
                    auto val_right = try_evaluate_expression(right_node);
                    if (!val_right) return std::nullopt;

                    double l = *val_left;
                    double r = *val_right;

                    if (op == "+") return l + r;
                    if (op == "-") return l - r;
                    if (op == "*") return l * r;

                    if (op == "/") {
                        if (r != 0) return l / r;
                        else return std::nullopt;
                    }
                    if (op == "%") return std::fmod(l, r);

                    // Логические операции
                    if (op == "==") return l == r ? 1.0 : 0.0;
                    if (op == "!=") return l != r ? 1.0 : 0.0;
                    if (op == "<") return l < r ? 1.0 : 0.0;
                    if (op == ">") return l > r ? 1.0 : 0.0;
                    if (op == "<=") return l <= r ? 1.0 : 0.0;
                    if (op == ">=") return l >= r ? 1.0 : 0.0;
                }
            }
        }

        return std::nullopt;
    }

    void dead_code_elimination(AstNode *node) {
        if (!node) return;

        for (auto *c: node->children) {
            dead_code_elimination(c);
        }

        std::string nodename = node->to_string();

        if (nodename == "TOK_STMTLIST" || nodename == "TOK_COMPOUNDSTMT") {
            bool exit_found = false;

            auto it = node->children.begin();
            while (it != node->children.end()) {
                AstNode *current_stmt = *it;

                if (exit_found) {
                    std::cerr << "Optimized: Dead code statement removed at line " << current_stmt->line << ".\n";
                    delete current_stmt;
                    it = node->children.erase(it);
                    continue;
                }

                if (current_stmt->children.size() > 0) {
                    std::string child_nodename = current_stmt->children[0]->to_string();
                    if (child_nodename == "TOK_RETURNSTMT" || child_nodename == "TOK_BREAKSTMT" || child_nodename ==
                        "TOK_CONTINUESTMT") {
                        exit_found = true;
                    }
                }

                ++it;
            }
        }
    }

    // Helper to collect function definitions (including methods)
    static std::unordered_map<std::string, ast::AstNode *> collect_function_defs(ast::AstNode *node) {
        std::unordered_map<std::string, ast::AstNode *> funcs;
        if (!node) return funcs;

        std::string nodename = node->to_string();
        if ((nodename == "TOK_DECLARATION" || nodename == "TOK_MEMBER") && is_function_declaration(node)) {
            std::string name = get_id_from_tok_id_child(node);
            ast::AstNode *body = nullptr;

            // Recursive search for body
            std::function<void(AstNode *)> find_body = [&](AstNode *n) {
                if (!n || body) return;
                if (n->to_string() == "TOK_COMPOUNDSTMT") {
                    body = n;
                    return;
                }
                for (auto *c: n->children) {
                    find_body(c);
                    if (body) return;
                }
            };
            find_body(node);

            if (!name.empty() && body) {
                funcs[name] = body;
            }
        }

        for (auto *c: node->children) {
            auto sub_funcs = collect_function_defs(c);
            funcs.insert(sub_funcs.begin(), sub_funcs.end());
        }
        return funcs;
    }

    // Helper to collect called function names in a body
    static std::vector<std::string> collect_calls(ast::AstNode *node) {
        std::vector<std::string> calls;
        if (!node) return calls;

        std::string nodename = node->to_string();
        if (nodename == "TOK_FUNCCALL") {
            std::string name = get_function_name_from_call(node);
            if (!name.empty()) calls.push_back(name);
        }

        for (auto *c: node->children) {
            auto sub_calls = collect_calls(c);
            calls.insert(calls.end(), sub_calls.begin(), sub_calls.end());
        }
        return calls;
    }

    // Detect cycles in the call graph (recursion check)
    static bool has_cycle(const std::unordered_map<std::string, std::vector<std::string> > &adj) {
        std::unordered_map<std::string, int> visit; // 0: not, 1: visiting, 2: visited
        std::function<bool(const std::string &)> dfs = [&](const std::string &u) -> bool {
            visit[u] = 1;
            for (const auto &v: adj.at(u)) {
                if (visit[v] == 1) return true; // cycle
                if (visit[v] == 0 && dfs(v)) return true;
            }
            visit[u] = 2;
            return false;
        };

        for (const auto &[u, _]: adj) {
            visit[u] = 0;
        }
        for (const auto &[u, _]: adj) {
            if (visit[u] == 0 && dfs(u)) return true;
        }
        return false;
    }

    // Inline a function call by replacing the call node with a copy of the body
    static void inline_function(AstNode *call_node, AstNode *func_body) {
        if (!call_node || !func_body) return;

        // Copy the body (simple clone; assume no params for now, or handle args if needed)
        AstNode *body_copy = func_body->clone(); // deep-clone the function body

        // Replace call_node with body_copy
        // But since call_node may have parent, need to replace in parent's children
        // For simplicity, assume we traverse and replace during inlining pass
        // Here, just swap children or something; need full implementation
    }

    // Build the call graph as a tree/DAG starting from main or implicit main
    FunctionNode *build_call_graph(AstNode *root) {
        auto func_defs = collect_function_defs(root);

        // Build adjacency for cycle check
        std::unordered_map<std::string, std::vector<std::string> > adj;
        for (const auto &[name, body]: func_defs) {
            adj[name] = collect_calls(body);
        }

        if (has_cycle(adj)) {
            std::cerr << "Recursion detected; prohibited." << std::endl;
            throw std::runtime_error("Recursion prohibited");
        }

        AstNode *main_body = nullptr;
        std::string main_name = "main";
        if (func_defs.find("main") != func_defs.end()) {
            main_body = func_defs["main"];
        } else {
            // Fallback: find the top-level STMTLIST or COMPOUNDSTMT in TOK_PROGRAM
            if (root->to_string() == "TOK_PROGRAM") {
                std::function<void(AstNode *)> find_body = [&](AstNode *n) {
                    if (!n || main_body) return;
                    std::string nn = n->to_string();
                    if (nn == "TOK_STMTLIST" || nn == "TOK_COMPOUNDSTMT" || nn == "TOK_STATEMENT") {
                        main_body = n;
                        main_name = "implicit_main";
                        return;
                    }
                    for (auto *c: n->children) {
                        find_body(c);
                        if (main_body) return;
                    }
                };
                find_body(root);
            }
            if (!main_body) {
                std::cerr << "No main function or body found" << std::endl;
                return nullptr;
            }
        }

        // Build nodes for all funcs, including main or implicit
        std::unordered_map<std::string, FunctionNode *> nodes;
        for (const auto &[name, body]: func_defs) {
            nodes[name] = new FunctionNode{name, body, {}};
        }
        if (func_defs.find(main_name) == func_defs.end()) {
            nodes[main_name] = new FunctionNode{main_name, main_body, {}};
            adj[main_name] = collect_calls(main_body);
        }

        // Add callees for all nodes
        for (const auto &[name, fn_node]: nodes) {
            auto called_names = adj[name];
            for (const auto &called: called_names) {
                if (nodes.count(called)) {
                    fn_node->callees.push_back(nodes[called]);
                }
            }
        }

        return nodes[main_name];
    }

    // Optimize the call graph (recursively optimize bodies)
    void optimize_call_graph(FunctionNode *graph_root) {
        if (!graph_root) return;

        // Optimize the body of this function
        dead_code_elimination(graph_root->body);

        // Recurse on callees
        for (auto *callee: graph_root->callees) {
            optimize_call_graph(callee);
        }
    }

    // Inline all functions in the graph (bottom-up)
    static void inline_functions(FunctionNode *graph_root) {
        if (!graph_root) return;

        // Inline callees first (post-order)
        for (auto *callee: graph_root->callees) {
            inline_functions(callee);
        }

        // Find and replace calls in this body
        std::function<void(AstNode *)> replace_calls = [&](AstNode *n) {
            if (!n) return;

            std::string nn = n->to_string();
            if (nn == "TOK_FUNCCALL") {
                std::string func_name = get_function_name_from_call(n);
                if (!func_name.empty()) {
                    // Find the body
                    AstNode *func_body = nullptr;
                    // Traverse graph to find, but since we have graph_root, assume map
                    // For simplicity, assume we have func_defs global or pass
                    // Inline by replacing n with copy of body
                    // But skip for now if params, etc.
                }
            }

            for (auto *c: n->children) {
                replace_calls(c);
            }
        };
        replace_calls(graph_root->body);
    }

    // Free the call graph memory
    void free_call_graph(FunctionNode *node) {
        if (!node) return;
        for (auto *callee: node->callees) {
            free_call_graph(callee);
        }
        delete node;
    }

    // Updated optimize_ast to use call graph only on top-level PROGRAM
    void optimize_ast(AstNode *root) {
        if (!root) return;

        std::string nodename = root->to_string();

        if (nodename == "TOK_PROGRAM") {
            // Only for top-level program
            auto *graph_root = build_call_graph(root);
            if (graph_root) {
                optimize_call_graph(graph_root);
                inline_functions(graph_root); // Inline after optimization
                free_call_graph(graph_root);
                // Since bodies are optimized in place, return; no need for further recursion here
                return;
            }
        }

        // Fallback or non-top: original optimization
        dead_code_elimination(root);

        for (auto *c: root->children) {
            optimize_ast(c);
        }

        if (nodename.find("TOK_EXPR") != std::string::npos) {
            bool has_potential = (root->children.size() > 1);

            if (has_potential) {
                auto result = try_evaluate_expression(root);
                if (result) {
                    double val = *result;
                    std::string val_str;
                    if (val == std::floor(val)) {
                        val_str = std::to_string((int) val);
                    } else {
                        val_str = std::to_string(val);
                    }

                    std::cerr << "Optimized constant expression at line " << root->line << " to " << val_str << "\n";

                    for (auto *c: root->children) delete c;
                    root->children.clear();

                    auto *term = new TerminalNode(parser::TokenType::NUMBER, val_str, "NUMBER", root->line, root->col);
                    root->add_child(term);
                }
            }
        }
    }
} // namespace ast
