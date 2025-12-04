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

extern std::string current_file_path;
namespace ast {

// ----------------- Печать -----------------
static void print_node_to_stream(const AstNode* node, std::ostream& out, int depth) {
    if (!node) return;
    out << std::string(depth * 2, ' ') << node->to_string() << "\n";
    for (const auto* c : node->children) {
        print_node_to_stream(c, out, depth + 1);
    }
}

void print_tree_to_file(AstNode* root, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "Could not open " << path << " for AST dump\n";
        return;
    }
    print_node_to_stream(root, out, 0);
    out.close();
}

// ----------------- Расширенная семантика -----------------

struct FunctionInfo {
    std::string return_type;
    std::vector<std::string> param_types;
    int line;
    int col;
    bool is_method = false; // метод класса
    std::string class_name; // для методов
};

struct VarInfo {
    std::string type;
    int line;
    int col;
};

struct ClassInfo {
    std::unordered_map<std::string, std::string> fields; // имя поля -> тип
    std::unordered_map<std::string, FunctionInfo> methods; // имя метода -> информация
};

struct SemanticState {
    std::vector<std::unordered_map<std::string, VarInfo>> scopes; // стек областей
    std::unordered_set<std::string> known_types;
    std::unordered_map<std::string, ClassInfo> classes;
    std::unordered_map<std::string, FunctionInfo> functions; // глобальные функции
    std::vector<std::string> messages;

    // Контекст текущей функции (для проверки return)
    std::string current_function_return_type;
    std::string current_class_context; // имя класса, если мы внутри метода

    void push_scope() { scopes.emplace_back(); }
    void pop_scope() { if (!scopes.empty()) scopes.pop_back(); }

    void declare_var(const std::string& name, const std::string& type, int line, int col) {
        if (scopes.empty()) push_scope();
        scopes.back()[name] = {type, line, col};
    }

    bool is_var_declared(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->count(name)) return true;
        }
        return false;
    }

    std::string get_var_type(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second.type;
        }
        return "";
    }

    void add_type(const std::string& t) { known_types.insert(t); }
    bool is_known_type(const std::string& t) const { return known_types.count(t) != 0; }

    void add_class(const std::string& name, const ClassInfo& info) {
        classes[name] = info;
        add_type(name);
    }

    ClassInfo* get_class_info(const std::string& name) {
        auto it = classes.find(name);
        return it != classes.end() ? &it->second : nullptr;
    }

    void add_function(const std::string& name, const FunctionInfo& info) {
        functions[name] = info;
    }

    FunctionInfo* get_function_info(const std::string& name) {
        auto it = functions.find(name);
        return it != functions.end() ? &it->second : nullptr;
    }

    void error(AstNode* node, const std::string& msg) {
        std::ostringstream ss;
        ss << current_file_path << ":" << node->line << ":" << node->col << " error: " << msg;
        messages.push_back(ss.str());
    }

    void warning(AstNode* node, const std::string& msg) {
        std::ostringstream ss;
        ss << current_file_path << ":" << node->line << ":" << node->col << " warning: " << msg;
        messages.push_back(ss.str());
    }
};

// Helpers
static std::string find_first_identifier_in_subtree(AstNode* node) {
    if (!node) return {};
    auto* tn = dynamic_cast<TerminalNode*>(node);
    if (tn && tn->token_type == parser::TokenType::IDENTIFIER) return tn->value;
    for (auto* c : node->children) {
        auto r = find_first_identifier_in_subtree(c);
        if (!r.empty()) return r;
    }
    return {};
}

static std::string find_first_type_in_subtree(AstNode* node) {
    if (!node) return {};
    if (node->to_string() == "TOK_TYPE") {
        std::queue<AstNode*> q;
        q.push(node);
        while (!q.empty()) {
            AstNode* curr = q.front(); q.pop();
            if (auto* tn = dynamic_cast<TerminalNode*>(curr)) {
                if (tn->token_type == parser::TokenType::IDENTIFIER ||
                    tn->value == "int" || tn->value == "float" || tn->value == "double" ||
                    tn->value == "string" || tn->value == "bool" || tn->value == "void" ||
                    tn->value == "vector") {
                    return tn->value;
                }
            }
            for (auto* c : curr->children) q.push(c);
        }
        return {};
    }
    for (auto* c : node->children) {
        auto r = find_first_type_in_subtree(c);
        if (!r.empty()) return r;
    }
    return {};
}

static void collect_all_identifiers(AstNode* node, std::vector<std::string>& ids) {
    if (!node) return;
    if (auto* tn = dynamic_cast<TerminalNode*>(node)) {
        if (tn->token_type == parser::TokenType::IDENTIFIER) {
            ids.push_back(tn->value);
        }
    }
    for (auto* c : node->children) {
        collect_all_identifiers(c, ids);
    }
}

// Сбор параметров функции
static std::vector<std::string> collect_param_types(AstNode* param_list) {
    std::vector<std::string> result;
    if (!param_list) return result;

    std::function<void(AstNode*)> collect = [&](AstNode* n) {
        if (!n) return;
        if (n->to_string() == "TOK_PARAM") {
            std::string type = find_first_type_in_subtree(n);
            if (!type.empty()) result.push_back(type);
        }
        for (auto* c : n->children) {
            collect(c);
        }
    };

    collect(param_list);
    return result;
}

// Проверка, является ли узел функцией (есть параметры)
static bool is_function_declaration(AstNode* node) {
    if (!node) return false;
    for (auto* c : node->children) {
        if (c->to_string() == "TOK_PARAMLIST") return true;
    }
    return false;
}

// 1. Сбор определений классов и функций
static void semantic_collect_defs(AstNode* node, SemanticState& st) {
    if (!node) return;
    std::string nodename = node->to_string();

    // Сбор классов
    if (nodename == "TOK_CLASSDECL") {
        std::string class_name = find_first_identifier_in_subtree(node);
        if (!class_name.empty()) {
            ClassInfo info;

            // Собираем поля и методы
            std::function<void(AstNode*)> find_members = [&](AstNode* n) {
                if(!n) return;
                if (n->to_string() == "TOK_MEMBER") {
                    std::string type = find_first_type_in_subtree(n);
                    std::string id = find_first_identifier_in_subtree(n);

                    if (!type.empty() && !id.empty()) {
                        // Проверяем, метод это или поле
                        if (is_function_declaration(n)) {
                            // Это метод
                            FunctionInfo finfo;
                            finfo.return_type = type;
                            finfo.is_method = true;
                            finfo.class_name = class_name;
                            finfo.line = n->line;
                            finfo.col = n->col;

                            // Собираем параметры
                            for (auto* child : n->children) {
                                if (child->to_string() == "TOK_PARAMLIST") {
                                    finfo.param_types = collect_param_types(child);
                                    break;
                                }
                            }

                            info.methods[id] = finfo;
                        } else {
                            // Это поле
                            info.fields[id] = type;
                        }
                    }
                }
                for(auto* c : n->children) find_members(c);
            };
            find_members(node);
            st.add_class(class_name, info);
        }
    }

    // Сбор глобальных функций
    if (nodename == "TOK_DECLARATION") {
        std::string type_name = find_first_type_in_subtree(node);
        std::string func_name = find_first_identifier_in_subtree(node);

        if (!type_name.empty() && !func_name.empty() && is_function_declaration(node)) {
            FunctionInfo finfo;
            finfo.return_type = type_name;
            finfo.line = node->line;
            finfo.col = node->col;

            // Собираем параметры
            for (auto* child : node->children) {
                if (child->to_string() == "TOK_DECLSUFFIX") {
                    for (auto* sc : child->children) {
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

    for (auto* c : node->children) {
        semantic_collect_defs(c, st);
    }
}

// Вывод типа выражения (упрощенная версия)
static std::string infer_expression_type(AstNode* expr, SemanticState& st) {
    if (!expr) return "";

    std::string nodename = expr->to_string();

    // Терминал - переменная или литерал
    if (auto* tn = dynamic_cast<TerminalNode*>(expr)) {
        if (tn->token_type == parser::TokenType::IDENTIFIER) {
            return st.get_var_type(tn->value);
        }
        if (tn->token_type == parser::TokenType::NUMBER) {
            // Упрощение: считаем все числа int (можно улучшить)
            return "int";
        }
        if (tn->token_type == parser::TokenType::STRING) {
            return "string";
        }
    }

    // Доступ к полю obj.field
    if (nodename == "TOK_POSTFIX_ITEM") {
        std::vector<std::string> ids;
        collect_all_identifiers(expr, ids);

        if (ids.size() >= 2) {
            std::string obj_name = ids[0];
            std::string field_name = ids[1];
            std::string obj_type = st.get_var_type(obj_name);

            if (!obj_type.empty()) {
                ClassInfo* ci = st.get_class_info(obj_type);
                if (ci) {
                    auto field_it = ci->fields.find(field_name);
                    if (field_it != ci->fields.end()) {
                        return field_it->second;
                    }
                }
            }
        }
    }

    // Рекурсивный спуск для сложных выражений
    for (auto* c : expr->children) {
        std::string child_type = infer_expression_type(c, st);
        if (!child_type.empty()) return child_type;
    }

    return "";
}

// 2. Валидация
static void semantic_check_and_validate(AstNode* node, SemanticState& st, AstNode* parent = nullptr) {
    if (!node) return;
    std::string nodename = node->to_string();

    bool opened_scope = false;
    if (nodename.find("COMPOUND") != std::string::npos || nodename == "TOK_PROGRAM") {
        st.push_scope();
        opened_scope = true;
    }

    // --- Вход в функцию/метод (установка контекста return) ---
    bool is_func_context = false;
    std::string saved_return_type;
    std::string saved_class_context;

    if (nodename == "TOK_DECLARATION" || nodename == "TOK_MEMBER") {
        std::string func_name = find_first_identifier_in_subtree(node);
        std::string return_type = find_first_type_in_subtree(node);

        if (is_function_declaration(node)) {
            is_func_context = true;
            saved_return_type = st.current_function_return_type;
            saved_class_context = st.current_class_context;

            st.current_function_return_type = return_type;

            // Если это метод класса
            if (nodename == "TOK_MEMBER" && parent) {
                // Ищем имя класса выше по дереву
                AstNode* class_node = parent;
                while (class_node && class_node->to_string() != "TOK_CLASSDECL") {
                    class_node = nullptr; // упрощение, нужен указатель на родителя
                    break;
                }
                // В упрощенной версии используем saved контекст
            }
        }
    }

    // --- Обработка объявлений переменных ---
    if (nodename == "TOK_LOCALVARDECL" || nodename == "TOK_PARAM" || nodename == "TOK_DECLARATION") {
        std::string type_name = find_first_type_in_subtree(node);
        std::string id_name = find_first_identifier_in_subtree(node);

        // Для TOK_DECLARATION проверяем, не функция ли это
        bool is_func = is_function_declaration(node);

        if (!id_name.empty() && !type_name.empty() && !is_func) {
            if (!st.is_known_type(type_name)) {
                st.error(node, "unknown type '" + type_name + "'");
            }
            st.declare_var(id_name, type_name, node->line, node->col);
        }

        // Для LOCALVARDECL проверяем инициализацию
        if (nodename == "TOK_LOCALVARDECL") {
            for(auto* c : node->children) {
                if (c->to_string() != "TOK_TYPE" && c->to_string() != "TOK_ID") {
                    semantic_check_and_validate(c, st, node);
                }
            }
            if (opened_scope) st.pop_scope();
            return;
        }

        // Для TOK_DECLARATION (глобальные переменные) тоже обрабатываем инициализацию
        if (nodename == "TOK_DECLARATION" && !is_func) {
            for(auto* c : node->children) {
                if (c->to_string() != "TOK_TYPE" && c->to_string() != "TOK_ID") {
                    semantic_check_and_validate(c, st, node);
                }
            }
            if (opened_scope) st.pop_scope();
            return;
        }
    }

    // --- Проверка return statement ---
    if (nodename == "TOK_RETURNSTMT") {
        if (st.current_function_return_type.empty()) {
            // Отладка
            std::cerr << "[DEBUG] Return at line " << node->line
                      << ", current_function_return_type is empty\n";
            st.error(node, "return statement outside function");
        } else {
            // Проверяем тип возвращаемого значения
            AstNode* expr = nullptr;
            for (auto* c : node->children) {
                if (c->to_string().find("EXPR") != std::string::npos) {
                    expr = c;
                    break;
                }
            }

            if (expr) {
                std::string return_expr_type = infer_expression_type(expr, st);

                if (!return_expr_type.empty() && return_expr_type != st.current_function_return_type) {
                    if (st.current_function_return_type != "void") {
                        st.warning(node, "return type mismatch: expected '" +
                                 st.current_function_return_type + "', got '" +
                                 return_expr_type + "'");
                    }
                }
            } else if (st.current_function_return_type != "void") {
                st.warning(node, "non-void function should return a value");
            }
        }
    }

    // --- Обработка доступа к полям/методам ---
    if (nodename == "TOK_POSTFIX_ITEM") {
        bool has_dot = false;
        for (auto* c : node->children) {
            if (auto* tn = dynamic_cast<TerminalNode*>(c)) {
                if (tn->value == ".") { has_dot = true; break; }
            }
        }

        if (has_dot) {
            std::vector<std::string> ids;
            collect_all_identifiers(node, ids);

            if (ids.size() >= 2) {
                std::string obj_name = ids[0];
                std::string member_name = ids[1];
                std::string obj_type = st.get_var_type(obj_name);

                if (!obj_type.empty()) {
                    ClassInfo* ci = st.get_class_info(obj_type);
                    if (ci) {
                        // Проверяем наличие поля или метода
                        bool found = false;

                        if (ci->fields.find(member_name) != ci->fields.end()) {
                            found = true;
                        } else if (ci->methods.find(member_name) != ci->methods.end()) {
                            found = true;
                        } else if (member_name == "pushback") {
                            // Специальный метод для vector
                            found = true;
                        }

                        if (!found) {
                            st.error(node, "class '" + obj_type + "' has no member '" + member_name + "'");
                        }
                    }
                } else {
                    st.error(node, "undefined object '" + obj_name + "'");
                }
            }

            // Проверяем объект, но не поле
            if (!node->children.empty()) {
                semantic_check_and_validate(node->children[0], st, node);
            }
            if (opened_scope) st.pop_scope();
            if (is_func_context) {
                st.current_function_return_type = saved_return_type;
                st.current_class_context = saved_class_context;
            }
            return;
        }
    }

    // --- Проверка использования переменных ---
    if (auto* tn = dynamic_cast<TerminalNode*>(node)) {
        if (tn->token_type == parser::TokenType::IDENTIFIER) {
            std::string name = tn->value;

            // Проверяем контекст: не является ли это объявлением?
            bool is_declaration_context = false;
            if (parent) {
                std::string parent_name = parent->to_string();
                if (parent_name == "TOK_TYPE" || parent_name == "TOK_ID") {
                    // Это может быть часть объявления типа или ID
                    AstNode* grandparent = nullptr; // нужен для полной проверки
                    // Упрощение: если parent это TOK_ID, а его parent - TOK_LOCALVARDECL/TOK_DECLARATION
                    // то это объявление
                    is_declaration_context = true;
                }
            }

            // Игнорируем стандартные функции
            if (name != "print" && name != "input" && name != "pushback" && name != "main") {
                if (!is_declaration_context &&
                    !st.is_var_declared(name) &&
                    !st.is_known_type(name) &&
                    !st.get_function_info(name)) {
                    st.error(node, "undefined identifier '" + name + "'");
                }
            }
        }
    }

    // Рекурсивный обход (только если не обработали как функцию выше)
    for (auto* c : node->children) {
        semantic_check_and_validate(c, st, node);
    }

    if (opened_scope) st.pop_scope();
}

bool semantic_check(AstNode* root) {
    if (!root) return true;
    SemanticState st;

    // Встроенные типы
    st.add_type("int"); st.add_type("float"); st.add_type("double");
    st.add_type("string"); st.add_type("bool"); st.add_type("void");
    st.add_type("vector");

    try {
        semantic_collect_defs(root, st);
        semantic_check_and_validate(root, st);
    } catch (const std::exception& e) {
        std::cerr << "semantic check exception: " << e.what() << "\n";
        return false;
    }

    if (!st.messages.empty()) {
        for (auto& m : st.messages) std::cerr << m << "\n";
        return false;
    }
    return true;
}

// =============================================================================
// ОПТИМИЗАЦИЯ (Простая constant folding)
// =============================================================================
void optimize_ast(AstNode* root) {
    // Можно оставить заглушку или добавить базовую оптимизацию
    if (!root) return;

    // Пример: свертка константных выражений 2+3 -> 5
    // Оставим как есть для компиляции
}

} // namespace ast