// ast_check.cpp
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
#include "ast.hpp"
#include "keywords.hpp"
#include <cmath>
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
#include <optional>

namespace ast {
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

        // Проверка совместимости типов для присваивания
        bool is_assignable(const std::string &target_type, const std::string &source_type) const {
            if (target_type == source_type) return true;
            if (target_type == "double" && (source_type == "int" || source_type == "float")) return true;
            if (target_type == "float" && source_type == "int") return true;

            return false;
        }

        // Проверка совместимости типов для операций
        bool are_types_compatible(const std::string &type1, const std::string &type2) const {
            if (type1 == type2) return true;

            // Разрешенные комбинации для числовых операций
            if ((type1 == "int" || type1 == "float" || type1 == "double") &&
                (type2 == "int" || type2 == "float" || type2 == "double")) {
                return true;
            }

            return false;
        }
    };

    static std::string infer_expression_type(AstNode *expr, SemanticState &st) {
        if (!expr) return "";
        std::string nodename = expr->to_string();

        // 1. Терминальные узлы
        if (auto *tn = dynamic_cast<TerminalNode *>(expr)) {
            if (tn->token_type == parser::TokenType::IDENTIFIER) {
                std::string type = st.get_var_type(tn->value);
                if (type.empty()) {
                    // Проверяем, не является ли это вызовом функции
                    FunctionInfo* finfo = st.get_function_info(tn->value);
                    if (finfo) {
                        return finfo->return_type;
                    }
                    st.error(expr, "use of undeclared identifier '" + tn->value + "'");
                }
                return type.empty() ? "unknown" : type;
            }
            if (tn->token_type == parser::TokenType::NUMBER) {
                if (tn->value.find('.') != std::string::npos) return "double";
                return "int";
            }
            if (tn->token_type == parser::TokenType::STRING) {
                if (tn->value.size() >= 2 && tn->value.front() == '\'' && tn->value.back() == '\'') {
                    return "int"; // char
                }
                return "string";
            }
            if (tn->token_type == parser::TokenType::KEYWORD) {
                if (tn->value == "true" || tn->value == "false") return "bool";
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
                            if (left_type != right_type) {
                                st.error(expr, "type mismatch in string operation: " + left_type + " " + op + " " + right_type);
                                return "unknown";
                            }
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
                        if (left_type != right_type && !st.are_types_compatible(left_type, right_type)) {
                            st.error(expr, "type mismatch in comparison: " + left_type + " " + op + " " + right_type);
                            return "unknown";
                        }
                        return "bool";
                    } else if (op == "&&" || op == "||") {
                        // Логические операторы работают с любыми типами (трактуются как bool)
                        // 0 = false, любое другое значение = true
                        return "int";  // Результат логической операции - число (0 или 1)
                    }
                }
                return left_type;
            }
        }

        // 3. Вызов функции
        if (nodename == "TOK_FUNCCALL") {
            std::string func_name = get_function_name_from_call(expr);
            if (!func_name.empty()) {
                FunctionInfo* finfo = st.get_function_info(func_name);
                if (finfo) {
                    return finfo->return_type;
                }
                st.error(expr, "undefined function '" + func_name + "'");
            }
            return "unknown";
        }

        // 4. Унарные операции
        if (nodename == "TOK_UNARYEXPR") {
            if (expr->children.size() >= 2) {
                TerminalNode* op_node = dynamic_cast<TerminalNode*>(expr->children[0]);
                std::string op = op_node ? op_node->value : "";
                std::string expr_type = infer_expression_type(expr->children[1], st);

                if (op == "!") {
                    // Логический NOT работает с любыми типами, возвращает int (0 или 1)
                    return "int";
                }
                if ((op == "-" || op == "+") && !(expr_type == "int" || expr_type == "double" || expr_type == "float")) {
                    st.error(expr, "unary " + op + " requires numeric operand");
                    return "unknown";
                }
                return expr_type;
            }
        }

        // Рекурсия
        for (auto* child : expr->children) {
            std::string child_type = infer_expression_type(child, st);
            if (!child_type.empty()) return child_type;
        }

        return "";
    }

    static std::vector<std::string> collect_argument_types(AstNode* node, SemanticState &st) {
        std::vector<std::string> result;
        if (!node) return result;

        std::function<void(AstNode*)> collect_args = [&](AstNode* n) {
            if (!n) return;

            // Находим узлы выражений-аргументов
            if (n->to_string().find("EXPR") != std::string::npos ||
                n->to_string() == "TOK_EXPRESSION" ||
                n->to_string() == "TOK_LITERAL" ||
                n->to_string() == "TOK_ATOM") {

                std::string arg_type = infer_expression_type(n, st);
                if (!arg_type.empty() && arg_type != "unknown") {
                    result.push_back(arg_type);
                }
            }

            for (auto* c : n->children) {
                collect_args(c);
            }
        };

        collect_args(node);
        return result;
    }

    static void semantic_collect_defs(AstNode *node, SemanticState &st) {
        if (!node) return;
        std::string nodename = node->to_string();

        if (nodename == "TOK_CLASSDECL") {
            std::string class_name = get_id_from_tok_id_child(node);

            if (!class_name.empty()) {
                ClassInfo info;
                std::function<void(AstNode *)> find_members = [&](AstNode *n) {
                    if (!n) return;
                    if (n->to_string() == "TOK_MEMBER") {
                        std::string type = find_first_type_in_subtree(n);
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


    static void semantic_check_and_validate(AstNode *node, SemanticState &st, AstNode *parent = nullptr) {
        if (!node) return;
        std::string nodename = node->to_string();

        // Управление областями видимости
        bool opened_scope = false;
        if (nodename == "TOK_COMPOUNDSTMT" || nodename == "TOK_PROGRAM") {
            st.push_scope();
            opened_scope = true;
        }

        // Управление контекстом функции
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

        // Объявление переменных
        if (nodename == "TOK_LOCALVARDECL" || nodename == "TOK_PARAM" || nodename == "TOK_DECLARATION") {
            std::string type_name = find_first_type_in_subtree(node);
            std::string id_name = get_id_from_tok_id_child(node);

            bool is_func = is_function_declaration(node);

            if (!id_name.empty() && !type_name.empty() && !is_func) {
                if (!st.is_known_type(type_name)) {
                    st.error(node, "unknown type '" + type_name + "'");
                }
                st.declare_var(id_name, type_name, node->line, node->col);
            }
        }

        // Проверка операторов присваивания
        if (nodename == "TOK_ASSIGN") {
            if (node->children.size() >= 2) {
                AstNode* left_expr = node->children[0];
                AstNode* right_expr = node->children[1];

                std::string left_type = infer_expression_type(left_expr, st);
                std::string right_type = infer_expression_type(right_expr, st);

                if (!left_type.empty() && !right_type.empty() && left_type != "unknown" && right_type != "unknown") {
                    if (!st.is_assignable(left_type, right_type)) {
                        st.error(node, "cannot assign " + right_type + " to " + left_type);
                    }
                }
            }
        }

        // Проверка вызовов функций
        if (nodename == "TOK_FUNCCALL") {
            std::string func_name = get_function_name_from_call(node);
            FunctionInfo* finfo = st.get_function_info(func_name);

            if (finfo) {
                std::vector<std::string> arg_types = collect_argument_types(node, st);

                // Проверка количества аргументов
                if (arg_types.size() != finfo->param_types.size()) {
                    st.error(node, "wrong number of arguments for '" + func_name + "': expected " +
                            std::to_string(finfo->param_types.size()) + ", got " +
                            std::to_string(arg_types.size()));
                } else {
                    // Проверка типов аргументов
                    for (size_t i = 0; i < arg_types.size(); ++i) {
                        if (!st.is_assignable(finfo->param_types[i], arg_types[i])) {
                            st.error(node, "argument " + std::to_string(i+1) +
                                    " type mismatch for '" + func_name + "': expected " +
                                    finfo->param_types[i] + ", got " + arg_types[i]);
                        }
                    }
                }
            } else if (!func_name.empty()) {
                st.error(node, "undefined function '" + func_name + "'");
            }
        }

        // Проверка условных операторов
        if (nodename == "TOK_IFSTMT" || nodename == "TOK_WHILESTMT") {
            AstNode* cond_expr = find_condition_expression(node);
            if (cond_expr) {
                std::string cond_type = infer_expression_type(cond_expr, st);
                // Любой тип выражения допускается в условии (трактуется как 0=false, non-0=true)
            }
        }

        // Проверка операторов цикла
        if (nodename == "TOK_FORSTMT") {
            st.loop_depth++;
        }

        // Проверка break/continue
        if (nodename == "TOK_BREAKSTMT" || nodename == "TOK_CONTINUESTMT") {
            if (st.loop_depth == 0) {
                st.error(node, nodename + " statement not within loop");
            }
        }

        // Проверка операторов return
        if (nodename == "TOK_RETURNSTMT") {
            if (st.current_function_return_type.empty()) {
                st.error(node, "return statement outside function");
            } else {
                AstNode* expr = nullptr;
                for (auto* c : node->children) {
                    if (c->to_string().find("EXPR") != std::string::npos) {
                        expr = c;
                        break;
                    }
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
                    else if (!st.is_assignable(expected_type, return_expr_type)) {
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

        // Проверка выражений (общая проверка типов)
        if (nodename.find("EXPR") != std::string::npos || nodename == "TOK_EXPRESSION") {
            std::string expr_type = infer_expression_type(node, st);
            // Вывод типа уже включает проверки, поэтому дополнительная проверка не нужна
        }

        // Рекурсивный обход
        for (auto* c : node->children) {
            semantic_check_and_validate(c, st, node);
        }

        // Восстановление состояния
        if (is_func_context) {
            st.current_function_return_type = saved_return_type;
            st.current_class_context = saved_class_context;
        }

        if (nodename == "TOK_FORSTMT") {
            st.loop_depth--;
        }

        if (opened_scope) st.pop_scope();
    }

    bool semantic_check(AstNode* root) {
        if (!root) return true;
        SemanticState st;

        // Добавление базовых типов
        st.add_type("int"); st.add_type("float"); st.add_type("double");
        st.add_type("string"); st.add_type("bool"); st.add_type("void"); st.add_type("vector");
        st.add_type("char");

        // Добавление стандартных функций
        st.add_function("main", {"int", {}, 0, 0});
        st.add_function("print", {"void", {"string"}, 0, 0});
        st.add_function("input", {"string", {}, 0, 0});
        st.add_function("strlen", {"int", {"string"}, 0, 0});
        st.add_function("strcat", {"string", {"string", "string"}, 0, 0});
        st.add_function("itoa", {"string", {"int"}, 0, 0});

        try {
            semantic_collect_defs(root, st);
            semantic_check_and_validate(root, st);
        } catch (const std::exception& e) {
            std::cerr << "semantic check exception: " << e.what() << "\n";
            return false;
        }

        if (!st.messages.empty()) {
            std::cerr << "Semantic checks reported " << st.messages.size() << " issue(s):\n";
            for (auto& m : st.messages) {
                std::cerr << m << "\n";
            }
            return false;
        }
        return true;
    }
} // namespace ast