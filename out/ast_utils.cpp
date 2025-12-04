#include "ast_utils.hpp"
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

// Предполагаем, что эти узлы объявлены в ast.hpp
namespace ast {
    class TOK_ATOMNode;
    class TOK_LITERALNode;
}

extern std::string current_file_path;

namespace ast {
    // =============================================================
    // Вспомогательные структуры и утилиты (без изменений)
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

    // ----------------- Расширенная семантика (Структуры) -----------------

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

        void push_scope() { scopes.emplace_back(); }
        void pop_scope() { if (!scopes.empty()) scopes.pop_back(); }

        void declare_var(const std::string &name, const std::string &type, int line, int col) {
            if (scopes.empty()) push_scope();
            if (scopes.back().count(name)) {
                // Ошибка переопределения в текущем скоупе
                std::ostringstream ss;
                ss << current_file_path << ":" << line << ":" << col << " error: redeclaration of '" << name << "'";
                messages.push_back(ss.str());
                return;
            }
            scopes.back()[name] = {type, line, col};
        }

        bool is_var_declared(const std::string &name) const {
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
                if (it->count(name)) return true;
            }
            return false;
        }

        std::string get_var_type(const std::string &name) {
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
                auto found = it->find(name);
                if (found != it->end()) return found->second.type;
            }
            return "";
        }

        void add_type(const std::string &t) { known_types.insert(t); }
        bool is_known_type(const std::string &t) const { return known_types.count(t) != 0; }

        void add_class(const std::string &name, const ClassInfo &info) {
            classes[name] = info;
            add_type(name);
        }

        ClassInfo *get_class_info(const std::string &name) {
            auto it = classes.find(name);
            return it != classes.end() ? &it->second : nullptr;
        }

        void add_function(const std::string &name, const FunctionInfo &info) {
            functions[name] = info;
        }

        FunctionInfo *get_function_info(const std::string &name) {
            auto it = functions.find(name);
            return it != functions.end() ? &it->second : nullptr;
        }

        void error(AstNode *node, const std::string &msg) {
            std::ostringstream ss;
            ss << current_file_path << ":" << node->line << ":" << node->col << " error: " << msg;
            messages.push_back(ss.str());
        }

        void warning(AstNode *node, const std::string &msg) {
            std::ostringstream ss;
            ss << current_file_path << ":" << node->line << ":" << node->col << " warning: " << msg;
            messages.push_back(ss.str());
        }
    };

    // ----------------- Вспомогательные функции поиска -----------------

    static std::string find_first_identifier_in_subtree(AstNode *node) {
        if (!node) return {};
        auto *tn = dynamic_cast<TerminalNode *>(node);
        if (tn && tn->token_type == parser::TokenType::IDENTIFIER) return tn->value;
        for (auto *c: node->children) {
            auto r = find_first_identifier_in_subtree(c);
            if (!r.empty()) return r;
        }
        return {};
    }

    // *** НОВАЯ ФУНКЦИЯ: Поиск ID только в узле TOK_ID ***
    static std::string get_id_from_tok_id_child(AstNode* parent) {
        if (!parent) return "";
        for (auto* c : parent->children) {
            if (c->to_string() == "TOK_ID") {
                return find_first_identifier_in_subtree(c);
            }
        }
        return "";
    }

    static std::string find_first_type_in_subtree(AstNode *node) {
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

    static std::vector<std::string> collect_param_types(AstNode *param_list) {
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

    static bool is_function_declaration(AstNode *node) {
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

    static void semantic_collect_defs(AstNode *node, SemanticState &st) {
        if (!node) return;
        std::string nodename = node->to_string();

        if (nodename == "TOK_CLASSDECL") {
            // Исправлено: имя класса берем из TOK_ID
            std::string class_name = get_id_from_tok_id_child(node);

            if (!class_name.empty()) {
                ClassInfo info;
                std::function<void(AstNode *)> find_members = [&](AstNode *n) {
                    if (!n) return;
                    if (n->to_string() == "TOK_MEMBER") {
                        std::string type = find_first_type_in_subtree(n);
                        // Исправлено: имя члена берем из TOK_ID
                        std::string id = get_id_from_tok_id_child(n);

                        if (!type.empty() && !id.empty()) {
                            if (is_function_declaration(n)) {
                                FunctionInfo finfo;
                                finfo.return_type = type;
                                finfo.is_method = true;
                                finfo.class_name = class_name;
                                finfo.line = n->line;
                                finfo.col = n->col;
                                for (auto *child: n->children) {
                                    if (child->to_string() == "TOK_MEMBERSUFFIX") {
                                        for (auto *sub: child->children) {
                                            if (sub->to_string() == "TOK_PARAMLIST") {
                                                finfo.param_types = collect_param_types(sub);
                                                break;
                                            }
                                        }
                                    }
                                }
                                info.methods[id] = finfo;
                            } else {
                                info.fields[id] = type;
                            }
                        }
                    }
                    for (auto *c: n->children) find_members(c);
                };
                find_members(node);
                st.add_class(class_name, info);
            }
        }

        if (nodename == "TOK_DECLARATION") {
            // Исправлено: имя функции/переменной берем из TOK_ID
            std::string type_name = find_first_type_in_subtree(node);
            std::string func_name = get_id_from_tok_id_child(node);

            if (!type_name.empty() && !func_name.empty() && is_function_declaration(node)) {
                FunctionInfo finfo;
                finfo.return_type = type_name;
                finfo.line = node->line;
                finfo.col = node->col;

                for (auto *child: node->children) {
                    if (child->to_string() == "TOK_DECLSUFFIX") {
                        for (auto *sc: child->children) {
                            if (sc->to_string() == "TOK_PARAMLIST") {
                                finfo.param_types = collect_param_types(sc);
                                break;
                            }
                        }
                        break;
                    }
                }
                st.add_function(func_name, finfo);
            }
        }

        for (auto *c: node->children) {
            semantic_collect_defs(c, st);
        }
    }

    // --- Вывод типа выражения ---
    static std::string infer_expression_type(AstNode *expr, SemanticState &st) {
        if (!expr) return "";
        std::string nodename = expr->to_string();

        // 1. Терминальные узлы
        if (auto *tn = dynamic_cast<TerminalNode *>(expr)) {
            if (tn->token_type == parser::TokenType::IDENTIFIER) {
                std::string type = st.get_var_type(tn->value);
                if (type.empty()) st.error(expr, "use of undeclared identifier '" + tn->value + "'");
                return type.empty() ? "unknown" : type;
            }
            if (tn->token_type == parser::TokenType::NUMBER) {
                if (tn->value.find('.') != std::string::npos) return "double";
                return "int";
            }
            if (tn->token_type == parser::TokenType::STRING) {
                if (tn->value.size() >= 2 && tn->value.front() == '\'' && tn->value.back() == '\'') {
                    return "int";
                }
                return "string";
            }
            return "";
        }

        // 2. Узлы выражений
        if (nodename.find("EXPR") != std::string::npos || nodename == "TOK_EXPRESSION") {
            if (expr->children.size() == 1) {
                return infer_expression_type(expr->children[0], st);
            } else if (expr->children.size() >= 2) {
                std::string left_type = infer_expression_type(expr->children[0], st);
                AstNode* rest = expr->children[1];
                if (rest->to_string().find("REST") != std::string::npos && rest->children.size() >= 2) {
                    TerminalNode* op_node = dynamic_cast<TerminalNode*>(rest->children[0]);
                    std::string op = op_node ? op_node->value : "";
                    std::string right_type = infer_expression_type(rest->children[1], st);

                    if (op.empty()) return left_type;

                    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
                        if (left_type == "string" || right_type == "string") {
                            if (op != "+") {
                                st.error(expr, "invalid operator '" + op + "' for string operands");
                                return "unknown";
                            }
                            return "string";
                        }
                        if ((left_type == "int" || left_type == "double" || left_type == "float") &&
                            (right_type == "int" || right_type == "double" || right_type == "float")) {
                            if (left_type == "double" || right_type == "double") return "double";
                            return "int";
                        }
                        st.error(expr, "invalid operands to '" + op + "': " + left_type + " and " + right_type);
                        return "unknown";
                    } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
                        if (left_type != right_type) {
                            st.error(expr, "type mismatch in comparison: " + left_type + " " + op + " " + right_type);
                            return "unknown";
                        }
                        return "bool";
                    }
                }
                return left_type; // Fallback
            }
        }

        // Рекурсия
        for (auto* child : expr->children) {
            std::string child_type = infer_expression_type(child, st);
            if (!child_type.empty()) return child_type;
        }

        return "";
    }

    static void semantic_check_and_validate(AstNode *node, SemanticState &st, AstNode *parent = nullptr) {
        if (!node) return;
        std::string nodename = node->to_string();

        bool opened_scope = false;
        if (nodename == "TOK_COMPOUNDSTMT" || nodename == "TOK_PROGRAM") {
            st.push_scope();
            opened_scope = true;
        }

        bool is_func_context = false;
        std::string saved_return_type;
        std::string saved_class_context;

        if (nodename == "TOK_DECLARATION" || nodename == "TOK_MEMBER") {
            if (is_function_declaration(node)) {
                std::string return_type = find_first_type_in_subtree(node);
                is_func_context = true;
                saved_return_type = st.current_function_return_type;
                saved_class_context = st.current_class_context;
                st.current_function_return_type = return_type;

                if (!opened_scope) {
                    st.push_scope();
                    opened_scope = true;
                }
            }
        }

        if (nodename == "TOK_LOCALVARDECL" || nodename == "TOK_PARAM" || nodename == "TOK_DECLARATION") {
            std::string type_name = find_first_type_in_subtree(node);

            // *** ИСПРАВЛЕНИЕ: Ищем идентификатор строго в дочернем узле TOK_ID ***
            // Это предотвращает нахождение имени типа как имени переменной, если тип - пользовательский класс.
            std::string id_name = get_id_from_tok_id_child(node);

            bool is_func = is_function_declaration(node);

            if (!id_name.empty() && !type_name.empty() && !is_func) {
                if (!st.is_known_type(type_name)) {
                    st.error(node, "unknown type '" + type_name + "'");
                }
                st.declare_var(id_name, type_name, node->line, node->col);
            }
        }

        if (nodename == "TOK_RETURNSTMT") {
            if (st.current_function_return_type.empty()) {
                st.error(node, "return statement outside function");
            } else {
                AstNode* expr = nullptr;
                for (auto* c : node->children) {
                    if (c->to_string().find("EXPR") != std::string::npos) { expr = c; break; }
                }

                std::string return_expr_type = expr ? infer_expression_type(expr, st) : "void";
                std::string expected_type = st.current_function_return_type;

                if (expected_type == "void") {
                    if (return_expr_type != "void" && !return_expr_type.empty() && return_expr_type != "unknown") {
                        st.error(node, "void function cannot return a value");
                    }
                } else {
                    if (return_expr_type.empty() || return_expr_type == "void" || return_expr_type == "unknown") {
                        st.error(node, "non-void function must return a value");
                    }
                    else if (return_expr_type != expected_type) {
                        if ((expected_type == "int" && return_expr_type == "double") ||
                            (expected_type == "double" && return_expr_type == "int")) {
                            st.warning(node, "implicit conversion from '" + return_expr_type + "' to '" + expected_type + "'");
                        }
                        else {
                            st.error(node, "return type mismatch: expected '" + expected_type + "', got '" + return_expr_type + "'");
                        }
                    }
                }
            }
        }

        // Рекурсивный обход
        for (auto* c : node->children) {
            semantic_check_and_validate(c, st, node);
        }

        if (is_func_context) {
            st.current_function_return_type = saved_return_type;
            st.current_class_context = saved_class_context;
        }

        if (opened_scope) st.pop_scope();
    }

    bool semantic_check(AstNode* root) {
        if (!root) return true;
        SemanticState st;
        st.add_type("int"); st.add_type("float"); st.add_type("double");
        st.add_type("string"); st.add_type("bool"); st.add_type("void"); st.add_type("vector");

        st.add_function("main", {"int", {}, 0, 0});
        st.add_function("print", {"void", {"string"}, 0, 0});
        st.add_function("input", {"string", {}, 0, 0});

        try {
            semantic_collect_defs(root, st);
            semantic_check_and_validate(root, st);
        } catch (const std::exception& e) {
            std::cerr << "semantic check exception: " << e.what() << "\n";
            return false;
        }

        if (!st.messages.empty()) {
            std::cerr << "Semantic checks reported issues (see stderr).\n";
            for (auto& m : st.messages) std::cerr << m << "\n";
            return false;
        }
        return true;
    }

    // =============================================================================
    // ОПТИМИЗАЦИИ (Constant Folding и Dead Code Elimination)
    // =============================================================================

    static std::optional<double> try_evaluate_expression(AstNode *node) {
        if (!node) return std::nullopt;

        if (auto* tn = dynamic_cast<TerminalNode*>(node)) {
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

            AstNode* rest = node->children[1];
            if (rest->children.size() >= 2) {
                std::string op;
                AstNode* right_node = nullptr;

                if (auto* term = dynamic_cast<TerminalNode*>(rest->children[0])) {
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
                }
            }
        }

        return std::nullopt;
    }

    static void dead_code_elimination(AstNode* node) {
        return ;
        if (!node) return;

        for (auto* c : node->children) {
            dead_code_elimination(c);
        }

        std::string nodename = node->to_string();

        if (nodename == "TOK_STMTLIST" || nodename == "TOK_COMPOUNDSTMT") {
            bool exit_found = false;

            auto it = node->children.begin();
            while (it != node->children.end()) {
                AstNode* current_stmt = *it;

                if (exit_found) {
                    std::cerr << "Optimized: Dead code statement removed at line " << current_stmt->line << ".\n";
                    delete current_stmt;
                    it = node->children.erase(it);
                    continue;
                }

                if (current_stmt->children.size() > 0) {
                    std::string child_nodename = current_stmt->children[0]->to_string();
                    if (child_nodename == "TOK_RETURNSTMT" || child_nodename == "TOK_BREAKSTMT" || child_nodename == "TOK_CONTINUESTMT") {
                        exit_found = true;
                    }
                }

                ++it;
            }
        }
    }


    void optimize_ast(AstNode* root) {
        if (!root) return;

        dead_code_elimination(root);

        for (auto* c : root->children) {
            optimize_ast(c);
        }

        std::string nodename = root->to_string();
        if (nodename.find("TOK_EXPR") != std::string::npos) {

            bool has_potential = (root->children.size() > 1);

            if (has_potential) {
                auto result = try_evaluate_expression(root);
                if (result) {
                    double val = *result;
                    std::string val_str;
                    if (val == std::floor(val)) {
                        val_str = std::to_string((int)val);
                    } else {
                        val_str = std::to_string(val);
                    }

                    std::cerr << "Optimized constant expression at line " << root->line << " to " << val_str << "\n";

                    for(auto* c : root->children) delete c;
                    root->children.clear();

                    auto* term = new TerminalNode(parser::TokenType::NUMBER, val_str, "NUMBER", root->line, root->col);
                    root->add_child(term);
                }
            }
        }
    }
} // namespace ast