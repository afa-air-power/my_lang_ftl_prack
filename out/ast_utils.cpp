#include "ast_utils.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>
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

// ----------------- Расширенная семантика с проверкой типов -----------------

struct VarInfo {
    std::string type;
    int line;
    int col;
};

struct FunctionInfo {
    std::string return_type;
    std::vector<std::string> param_types;
    int param_count;
};

struct ClassInfo {
    std::unordered_map<std::string, std::string> fields; // имя поля -> тип
    std::unordered_map<std::string, FunctionInfo> methods; // имя метода -> инфо
};

struct SemanticState {
    std::vector<std::unordered_map<std::string, VarInfo>> scopes; // стек областей с типами
    std::unordered_set<std::string> known_types; // базовые + пользовательские типы
    std::unordered_map<std::string, ClassInfo> classes; // информация о классах
    std::unordered_map<std::string, FunctionInfo> functions; // глобальные функции
    std::vector<std::string> messages;

    void push_scope() { scopes.emplace_back(); }
    void pop_scope() { if (!scopes.empty()) scopes.pop_back(); }

    void declare_var(const std::string& name, const std::string& type, int line, int col) {
        if (scopes.empty()) push_scope();
        scopes.back()[name] = {type, line, col};
    }

    VarInfo* get_var_info(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return &found->second;
        }
        return nullptr;
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
        ss << "test_program.txt:" << node->line << ":" << node->col << " error: " << msg;
        messages.push_back(ss.str());
    }

    void warn(AstNode* node, const std::string& msg) {
        std::ostringstream ss;
        ss << "test_program.txt:" << node->line << ":" << node->col << " warning: " << msg;
        messages.push_back(ss.str());
    }
};

// Helpers для извлечения информации из AST
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

// =============================================================================
// ПЕРВЫЙ ПРОХОД: Сбор определений классов и функций
// =============================================================================
static void semantic_collect_defs(AstNode* node, SemanticState& st) {
    if (!node) return;
    std::string nodename = node->to_string();

    // Сбор информации о классах
    if (nodename == "TOK_CLASSDECL") {
        std::string class_name = find_first_identifier_in_subtree(node);
        if (class_name.empty()) {
            st.error(node, "class declaration missing name");
            return;
        }

        ClassInfo info;

        // Ищем тело класса (TOK_MEMBERLIST)
        for (auto* child : node->children) {
            if (child->to_string() == "TOK_MEMBERLIST") {
                // Обрабатываем каждый член класса
                std::function<void(AstNode*)> process_members = [&](AstNode* member_list) {
                    if (!member_list) return;

                    for (auto* item : member_list->children) {
                        if (item->to_string() == "TOK_MEMBER") {
                            std::string member_type = find_first_type_in_subtree(item);
                            std::string member_name = find_first_identifier_in_subtree(item);

                            if (!member_name.empty() && !member_type.empty()) {
                                // Проверяем, это поле или метод
                                bool is_method = false;
                                for (auto* m : item->children) {
                                    if (m->to_string() == "TOK_MEMBERSUFFIX") {
                                        for (auto* s : m->children) {
                                            if (s->to_string() == "TOK_COMPOUNDSTMT") {
                                                is_method = true;
                                                break;
                                            }
                                        }
                                    }
                                }

                                if (is_method) {
                                    // Это метод - собираем параметры
                                    FunctionInfo func_info;
                                    func_info.return_type = member_type;
                                    func_info.param_count = 0;
                                    // TODO: собрать параметры
                                    info.methods[member_name] = func_info;
                                } else {
                                    // Это поле
                                    info.fields[member_name] = member_type;
                                }
                            }
                        }

                        // Рекурсивно обрабатываем вложенные списки
                        if (item->to_string() == "TOK_MEMBERLIST") {
                            process_members(item);
                        }
                    }
                };

                process_members(child);
            }
        }

        st.add_class(class_name, info);
        std::cerr << "Semantic: Registered class '" << class_name
                  << "' with " << info.fields.size() << " fields and "
                  << info.methods.size() << " methods\n";
    }

    // Сбор информации о глобальных функциях
    if (nodename == "TOK_DECLARATION") {
        bool is_function = false;
        std::string func_name;
        std::string return_type;

        for (auto* child : node->children) {
            if (child->to_string() == "TOK_DECLSUFFIX") {
                for (auto* s : child->children) {
                    if (s->to_string() == "TOK_COMPOUNDSTMT") {
                        is_function = true;
                        func_name = find_first_identifier_in_subtree(node);
                        return_type = find_first_type_in_subtree(node);
                        break;
                    }
                }
            }
        }

        if (is_function && !func_name.empty()) {
            FunctionInfo func_info;
            func_info.return_type = return_type;
            func_info.param_count = 0;
            // TODO: собрать параметры из TOK_PARAMLIST
            st.add_function(func_name, func_info);
            std::cerr << "Semantic: Registered function '" << func_name
                      << "' returning '" << return_type << "'\n";
        }
    }

    for (auto* c : node->children) {
        semantic_collect_defs(c, st);
    }
}

// =============================================================================
// ВТОРОЙ ПРОХОД: Проверка типов и использования
// =============================================================================

// Определение типа выражения
static std::string infer_expression_type(AstNode* node, SemanticState& st);

// Проверка, является ли узел частью доступа к полю (справа от точки)
static bool is_field_name_in_access(AstNode* node, AstNode* parent) {
    if (!parent) return false;

    // Проверяем, есть ли точка перед этим узлом
    for (size_t i = 0; i < parent->children.size(); ++i) {
        if (parent->children[i] == node && i > 0) {
            // Смотрим на предыдущий элемент
            if (auto* prev = dynamic_cast<TerminalNode*>(parent->children[i-1])) {
                if (prev->value == ".") {
                    return true; // Это имя поля после точки
                }
            }
        }
    }
    return false;
}

// Проверка доступа к полю: obj.field
static std::string check_member_access(AstNode* node, const std::string& obj_name,
                                       const std::string& field_name, SemanticState& st) {
    // Получаем тип объекта
    std::string obj_type = st.get_var_type(obj_name);
    if (obj_type.empty()) {
        st.error(node, "undefined variable '" + obj_name + "'");
        return "";
    }

    // Проверяем, что это класс
    ClassInfo* class_info = st.get_class_info(obj_type);
    if (!class_info) {
        st.error(node, "'" + obj_name + "' is not a class type");
        return "";
    }

    // Проверяем существование поля
    auto field_it = class_info->fields.find(field_name);
    if (field_it == class_info->fields.end()) {
        st.error(node, "class '" + obj_type + "' has no member named '" + field_name + "'");
        return "";
    }

    return field_it->second;
}

// Проверка вызова функции
static std::string check_function_call(AstNode* node, const std::string& func_name,
                                       int arg_count, SemanticState& st) {
    // Встроенные функции
    if (func_name == "print" || func_name == "input") {
        return "void";
    }

    // Проверяем пользовательские функции
    FunctionInfo* func_info = st.get_function_info(func_name);
    if (!func_info) {
        st.error(node, "undefined function '" + func_name + "'");
        return "";
    }

    // TODO: Проверка количества аргументов
    // if (arg_count != func_info->param_count) {
    //     st.error(node, "function '" + func_name + "' expects " +
    //              std::to_string(func_info->param_count) + " arguments, got " +
    //              std::to_string(arg_count));
    // }

    return func_info->return_type;
}

// Определение типа выражения (упрощенная версия)
static std::string infer_expression_type(AstNode* node, SemanticState& st) {
    if (!node) return "";

    // Терминалы
    if (auto* term = dynamic_cast<TerminalNode*>(node)) {
        if (term->token_type == parser::TokenType::NUMBER) {
            // Проверяем, есть ли точка
            if (term->value.find('.') != std::string::npos) {
                return "double";
            }
            return "int";
        }
        if (term->token_type == parser::TokenType::STRING) {
            return "string";
        }
        if (term->token_type == parser::TokenType::IDENTIFIER) {
            return st.get_var_type(term->value);
        }
    }

    std::string nodename = node->to_string();

    // Доступ к полю: obj.field
    if (nodename == "TOK_POSTFIX_ITEM") {
        std::vector<std::string> ids;
        collect_all_identifiers(node, ids);

        if (ids.size() >= 2) {
            // Это obj.field
            return check_member_access(node, ids[0], ids[1], st);
        }
    }

    // Вызов функции
    if (nodename == "TOK_INO" ||
        (nodename == "TOK_POSTFIX_ITEM" && node->children.size() > 0)) {
        std::string func_name = find_first_identifier_in_subtree(node);
        if (!func_name.empty()) {
            // Подсчитываем аргументы
            int arg_count = 0;
            // TODO: правильный подсчет аргументов
            return check_function_call(node, func_name, arg_count, st);
        }
    }

    // Бинарные операции - возвращаем тип операндов
    if (nodename.find("EXPR") != std::string::npos) {
        for (auto* child : node->children) {
            std::string t = infer_expression_type(child, st);
            if (!t.empty()) return t;
        }
    }

    // Рекурсивно ищем в детях
    for (auto* child : node->children) {
        std::string t = infer_expression_type(child, st);
        if (!t.empty()) return t;
    }

    return "";
}

static void semantic_check_and_validate(AstNode* node, SemanticState& st, AstNode* parent = nullptr) {
    if (!node) return;
    std::string nodename = node->to_string();

    bool opened_scope = false;
    if (nodename.find("COMPOUND") != std::string::npos || nodename == "TOK_PROGRAM") {
        st.push_scope();
        opened_scope = true;
    }

    // Объявление локальной переменной
    if (nodename == "TOK_LOCALVARDECL") {
        AstNode* type_node = nullptr;
        AstNode* id_node = nullptr;
        AstNode* init_expr = nullptr;

        for (auto* c : node->children) {
            if (c->to_string() == "TOK_TYPE") type_node = c;
            else if (c->to_string() == "TOK_ID") id_node = c;
            else if (c->to_string() == "TOK_VARDECLREST") {
                for (auto* v : c->children) {
                    if (v->to_string() == "TOK_EXPRESSION") {
                        init_expr = v;
                        break;
                    }
                }
            }
        }

        std::string type_name = find_first_type_in_subtree(type_node);
        std::string id_name = find_first_identifier_in_subtree(id_node);

        if (id_name.empty()) {
            st.error(node, "declaration missing identifier name");
        } else {
            if (type_name.empty()) {
                st.error(node, "declaration of '" + id_name + "' missing data type");
            } else {
                if (!st.is_known_type(type_name)) {
                    st.error(node, "declaration of '" + id_name + "' has unknown type '" + type_name + "'");
                }

                if (init_expr) {
                    std::string init_type = infer_expression_type(init_expr, st);
                    if (!init_type.empty() && init_type != type_name) {
                        if (!(type_name == "double" && init_type == "int")) {
                            st.error(node, "cannot initialize variable of type '" + type_name +
                                     "' with value of type '" + init_type + "'");
                        }
                    }
                }
            }
            st.declare_var(id_name, type_name, node->line, node->col);
        }

        if (init_expr) {
            semantic_check_and_validate(init_expr, st, node);
        }
    }
    else if (nodename == "TOK_PARAM") {
        std::string type_name = find_first_type_in_subtree(node);
        std::string id_name = find_first_identifier_in_subtree(node);

        if (!id_name.empty() && !type_name.empty()) {
            if (!st.is_known_type(type_name)) {
                st.error(node, "parameter '" + id_name + "' has unknown type '" + type_name + "'");
            }
            st.declare_var(id_name, type_name, node->line, node->col);
        }
    }
    else if (nodename == "TOK_DECLARATION") {
        std::string type_name = find_first_type_in_subtree(node);
        std::string id_name = find_first_identifier_in_subtree(node);

        if (!id_name.empty() && !type_name.empty()) {
            bool is_function = false;
            for (auto* c : node->children) {
                if (c->to_string() == "TOK_DECLSUFFIX") {
                    for (auto* s : c->children) {
                        if (s->to_string() == "TOK_COMPOUNDSTMT") {
                            is_function = true;
                            break;
                        }
                    }
                }
            }

            if (!is_function) {
                if (!st.is_known_type(type_name)) {
                    st.error(node, "variable '" + id_name + "' has unknown type '" + type_name + "'");
                }
                st.declare_var(id_name, type_name, node->line, node->col);
            }
        }

        for (auto* c : node->children) {
            semantic_check_and_validate(c, st, node);
        }
    }
    else if (nodename == "TOK_EXPR02REST" || nodename == "TOK_EXPRESSIONSTMT") {
        for (auto* c : node->children) {
            semantic_check_and_validate(c, st, node);
        }
    }
    else if (nodename == "TOK_POSTFIX_ITEM") {
        // Обработка доступа к полям и других постфиксных операций
        bool has_dot = false;
        for (auto* c : node->children) {
            if (auto* tn = dynamic_cast<TerminalNode*>(c)) {
                if (tn->value == ".") {
                    has_dot = true;
                    break;
                }
            }
        }

        if (has_dot) {
            // Это доступ к полю - проверяем только объект (первый идентификатор)
            // и само существование поля
            std::vector<std::string> ids;
            collect_all_identifiers(node, ids);

            if (ids.size() >= 2) {
                check_member_access(node, ids[0], ids[1], st);
            }

            // НЕ спускаемся рекурсивно, чтобы не проверять имя поля как переменную
        } else {
            // Другая операция - обрабатываем рекурсивно
            for (auto* c : node->children) {
                semantic_check_and_validate(c, st, node);
            }
        }
    }
    else {
        // Проверка использования идентификаторов
        // НО: пропускаем идентификаторы, которые являются именами полей после точки
        if (auto* tn = dynamic_cast<TerminalNode*>(node)) {
            if (tn->token_type == parser::TokenType::IDENTIFIER) {
                std::string name = tn->value;

                // Проверяем, не находимся ли мы после точки
                bool is_field_access = is_field_name_in_access(node, parent);

                if (!is_field_access && name != "print" && name != "input" && name != "pushback") {
                    if (!st.is_var_declared(name) && !st.is_known_type(name)) {
                        int line = tn->line > 0 ? tn->line : node->line;
                        int col = tn->col > 0 ? tn->col : node->col;

                        std::ostringstream ss;
                        ss << "test_program.txt:" << line << ":" << col
                           << " error: undefined identifier '" << name << "'";
                        st.messages.push_back(ss.str());
                    }
                }
            }
        }

        for (auto* c : node->children) {
            semantic_check_and_validate(c, st, node);
        }
    }

    if (opened_scope) st.pop_scope();
}

bool semantic_check(AstNode* root) {
    if (!root) return true;
    SemanticState st;
    st.add_type("int"); st.add_type("float"); st.add_type("double");
    st.add_type("string"); st.add_type("bool"); st.add_type("void"); st.add_type("vector");

    try {
        semantic_collect_defs(root, st);
    } catch (const std::exception& e) {
        std::cerr << "semantic collection threw: " << e.what() << "\n";
    }

    try {
        semantic_check_and_validate(root, st, nullptr);
    } catch (const std::exception& e) {
        std::cerr << "semantic validation threw: " << e.what() << "\n";
    }

    if (!st.messages.empty()) {
        for (auto& m : st.messages) std::cerr << m << "\n";
        return false;
    }
    return true;
}

// =============================================================================
// ОПТИМИЗАЦИИ AST
// =============================================================================

// Хелперы для работы с терминалами
static bool is_number_terminal(AstNode* n, double& outval) {
    if (!n) return false;
    auto* t = dynamic_cast<TerminalNode*>(n);
    if (!t) return false;
    if (t->token_type != parser::TokenType::NUMBER) return false;
    try { outval = std::stod(t->value); } catch (...) { return false; }
    return true;
}

static bool is_operator(AstNode* n, std::string& op) {
    if (!n) return false;
    auto* t = dynamic_cast<TerminalNode*>(n);
    if (!t) return false;
    op = t->value.empty() ? parser::token_to_string(t->token_type) : t->value;
    return true;
}

static AstNode* create_number_node(double val) {
    return new ast::TerminalNode(parser::TokenType::NUMBER, std::to_string(val), "NUMBER");
}

// Структура для отслеживания значений переменных
struct OptimizationContext {
    std::unordered_map<std::string, double> const_vars; // Константные переменные
    std::unordered_map<std::string, AstNode*> var_copies; // Для copy propagation
    std::unordered_set<std::string> modified_vars; // Переменные, которые были изменены
    int changes_made = 0;
};

// =============================================================================
// 1. CONSTANT FOLDING (Свёртка констант)
// =============================================================================
static bool try_constant_folding(std::vector<AstNode*>& children, OptimizationContext& ctx) {
    for (size_t i = 0; i + 2 < children.size(); ++i) {
        double a = 0, b = 0;
        std::string op;

        if (!is_number_terminal(children[i], a)) continue;
        if (!is_operator(children[i + 1], op)) continue;
        if (!is_number_terminal(children[i + 2], b)) continue;

        double result = 0;
        bool can_fold = true;

        // Бинарные операции
        if (op == "+" || op == "TOK_PLUS") result = a + b;
        else if (op == "-" || op == "TOK_MINUS") result = a - b;
        else if (op == "*" || op == "TOK_STAR") result = a * b;
        else if (op == "/" || op == "TOK_SLASH") {
            if (std::abs(b) < 1e-10) can_fold = false;
            else result = a / b;
        }
        else if (op == "%" || op == "TOK_PERCENT") {
            if (std::abs(b) < 1e-10) can_fold = false;
            else result = std::fmod(a, b);
        }
        else can_fold = false;

        if (!can_fold) continue;

        // Заменяем три узла на один
        for (int k = 0; k < 3; ++k) delete children[i + k];
        children[i] = create_number_node(result);
        children.erase(children.begin() + i + 1, children.begin() + i + 3);
        ctx.changes_made++;
        return true;
    }
    return false;
}

// =============================================================================
// 2. CONSTANT PROPAGATION (Распространение констант)
// =============================================================================
static void constant_propagation(AstNode* node, OptimizationContext& ctx) {
    if (!node) return;
    std::string nodename = node->to_string();

    // Ищем присваивания вида: var = const
    if (nodename == "TOK_LOCALVARDECL" || nodename == "TOK_EXPRESSIONSTMT") {
        // Проверяем паттерн: IDENTIFIER = NUMBER
        if (node->children.size() >= 3) {
            auto* id_node = dynamic_cast<TerminalNode*>(node->children[0]);
            auto* eq_node = node->children[1];
            double val;

            if (id_node && id_node->token_type == parser::TokenType::IDENTIFIER &&
                eq_node && eq_node->to_string().find("EQUAL") != std::string::npos &&
                is_number_terminal(node->children[2], val)) {

                ctx.const_vars[id_node->value] = val;
                ctx.changes_made++;
            }
        }
    }

    // Заменяем использование константных переменных на их значения
    for (size_t i = 0; i < node->children.size(); ++i) {
        if (auto* term = dynamic_cast<TerminalNode*>(node->children[i])) {
            if (term->token_type == parser::TokenType::IDENTIFIER) {
                auto it = ctx.const_vars.find(term->value);
                if (it != ctx.const_vars.end() && ctx.modified_vars.count(term->value) == 0) {
                    delete node->children[i];
                    node->children[i] = create_number_node(it->second);
                    ctx.changes_made++;
                }
            }
        }
    }

    for (auto* c : node->children) {
        constant_propagation(c, ctx);
    }
}

// =============================================================================
// 3. DEAD CODE ELIMINATION (Удаление мёртвого кода)
// =============================================================================
static bool is_always_false_condition(AstNode* node) {
    double val;
    if (is_number_terminal(node, val)) {
        return std::abs(val) < 1e-10; // 0 считается false
    }
    return false;
}

static bool dead_code_elimination(AstNode* node, OptimizationContext& ctx) {
    if (!node) return false;
    std::string nodename = node->to_string();

    // Удаляем if (false) блоки
    if (nodename == "TOK_IFSTMT") {
        // Ищем условие
        for (size_t i = 0; i < node->children.size(); ++i) {
            if (node->children[i]->to_string() == "TOK_EXPRESSION") {
                if (is_always_false_condition(node->children[i])) {
                    // Удаляем весь if блок
                    std::cerr << "Optimization: Removing dead if-block\n";
                    for (auto* c : node->children) delete c;
                    node->children.clear();
                    ctx.changes_made++;
                    return true;
                }
            }
        }
    }

    // Удаляем while (false) блоки
    if (nodename == "TOK_WHILESTMT") {
        for (size_t i = 0; i < node->children.size(); ++i) {
            if (node->children[i]->to_string() == "TOK_EXPRESSION") {
                if (is_always_false_condition(node->children[i])) {
                    std::cerr << "Optimization: Removing dead while-loop\n";
                    for (auto* c : node->children) delete c;
                    node->children.clear();
                    ctx.changes_made++;
                    return true;
                }
            }
        }
    }

    bool changed = false;
    for (auto* c : node->children) {
        if (dead_code_elimination(c, ctx)) changed = true;
    }
    return changed;
}

// =============================================================================
// 5. LOOP INVARIANT CODE MOTION (Вынос инвариантов из циклов)
// =============================================================================
// Упрощенная версия: просто детектируем потенциальные инварианты
static void detect_loop_invariants(AstNode* node) {
    if (!node) return;
    std::string nodename = node->to_string();

    if (nodename == "TOK_WHILESTMT" || nodename == "TOK_FORSTMT") {
        std::cerr << "Info: Loop detected at line " << node->line
                  << " - consider manual loop invariant optimization\n";
    }

    for (auto* c : node->children) {
        detect_loop_invariants(c);
    }
}

// =============================================================================
// 6. INLINE EXPANSION (Встраивание простых функций)
// =============================================================================
// Упрощенная версия: детектируем возможности для инлайнинга
static void detect_inline_opportunities(AstNode* node) {
    if (!node) return;
    std::string nodename = node->to_string();

    if (nodename == "TOK_DECLARATION") {
        // Ищем короткие функции (кандидаты на инлайнинг)
        bool is_function = false;
        for (auto* c : node->children) {
            if (c->to_string() == "TOK_COMPOUNDSTMT") {
                is_function = true;
                int stmt_count = 0;
                // Считаем количество операторов
                for (auto* stmt : c->children) {
                    if (stmt->to_string() == "TOK_STMTLIST") {
                        stmt_count++;
                    }
                }
                if (stmt_count <= 3) {
                    std::cerr << "Info: Small function at line " << node->line
                              << " - candidate for inlining\n";
                }
            }
        }
    }

    for (auto* c : node->children) {
        detect_inline_opportunities(c);
    }
}

// =============================================================================
// 7. ALGEBRAIC SIMPLIFICATION (Алгебраическое упрощение)
// =============================================================================
static bool algebraic_simplification(std::vector<AstNode*>& children, OptimizationContext& ctx) {
    for (size_t i = 0; i + 2 < children.size(); ++i) {
        double val;
        std::string op;

        if (!is_operator(children[i + 1], op)) continue;

        // x * 1 -> x или 1 * x -> x
        if ((op == "*" || op == "TOK_STAR") && is_number_terminal(children[i + 2], val) && std::abs(val - 1.0) < 1e-10) {
            delete children[i + 1];
            delete children[i + 2];
            children.erase(children.begin() + i + 1, children.begin() + i + 3);
            std::cerr << "Optimization: Simplified x * 1 -> x\n";
            ctx.changes_made++;
            return true;
        }
        if ((op == "*" || op == "TOK_STAR") && is_number_terminal(children[i], val) && std::abs(val - 1.0) < 1e-10) {
            delete children[i];
            delete children[i + 1];
            children.erase(children.begin() + i, children.begin() + i + 2);
            std::cerr << "Optimization: Simplified 1 * x -> x\n";
            ctx.changes_made++;
            return true;
        }

        // x + 0 -> x или 0 + x -> x
        if ((op == "+" || op == "TOK_PLUS") && is_number_terminal(children[i + 2], val) && std::abs(val) < 1e-10) {
            delete children[i + 1];
            delete children[i + 2];
            children.erase(children.begin() + i + 1, children.begin() + i + 3);
            std::cerr << "Optimization: Simplified x + 0 -> x\n";
            ctx.changes_made++;
            return true;
        }
        if ((op == "+" || op == "TOK_PLUS") && is_number_terminal(children[i], val) && std::abs(val) < 1e-10) {
            delete children[i];
            delete children[i + 1];
            children.erase(children.begin() + i, children.begin() + i + 2);
            std::cerr << "Optimization: Simplified 0 + x -> x\n";
            ctx.changes_made++;
            return true;
        }

        // x * 0 -> 0 или 0 * x -> 0
        if ((op == "*" || op == "TOK_STAR") && is_number_terminal(children[i + 2], val) && std::abs(val) < 1e-10) {
            delete children[i];
            delete children[i + 1];
            delete children[i + 2];
            children[i] = create_number_node(0.0);
            children.erase(children.begin() + i + 1, children.begin() + i + 3);
            std::cerr << "Optimization: Simplified x * 0 -> 0\n";
            ctx.changes_made++;
            return true;
        }

        // x - 0 -> x
        if ((op == "-" || op == "TOK_MINUS") && is_number_terminal(children[i + 2], val) && std::abs(val) < 1e-10) {
            delete children[i + 1];
            delete children[i + 2];
            children.erase(children.begin() + i + 1, children.begin() + i + 3);
            std::cerr << "Optimization: Simplified x - 0 -> x\n";
            ctx.changes_made++;
            return true;
        }

        // x / 1 -> x
        if ((op == "/" || op == "TOK_SLASH") && is_number_terminal(children[i + 2], val) && std::abs(val - 1.0) < 1e-10) {
            delete children[i + 1];
            delete children[i + 2];
            children.erase(children.begin() + i + 1, children.begin() + i + 3);
            std::cerr << "Optimization: Simplified x / 1 -> x\n";
            ctx.changes_made++;
            return true;
        }
    }
    return false;
}

// =============================================================================
// 12. BOOLEAN SIMPLIFICATION (Упрощение булевых выражений)
// =============================================================================
static bool boolean_simplification(AstNode* node, OptimizationContext& ctx) {
    if (!node) return false;
    std::string nodename = node->to_string();

    // Упрощаем "if (x == true)" -> "if (x)"
    if (nodename == "TOK_IFSTMT" || nodename == "TOK_WHILESTMT") {
        for (size_t i = 0; i < node->children.size(); ++i) {
            auto* expr = node->children[i];
            if (expr->to_string() == "TOK_EXPRESSION" && expr->children.size() >= 3) {
                // Ищем паттерн: var == true/1
                std::string op;
                double val;
                if (expr->children.size() >= 3 &&
                    is_operator(expr->children[1], op) &&
                    (op == "==" || op == "TOK_EQEQ") &&
                    is_number_terminal(expr->children[2], val) &&
                    std::abs(val - 1.0) < 1e-10) {

                    // Удаляем "== true" часть
                    delete expr->children[1];
                    delete expr->children[2];
                    expr->children.erase(expr->children.begin() + 1, expr->children.begin() + 3);
                    std::cerr << "Optimization: Simplified 'x == true' -> 'x'\n";
                    ctx.changes_made++;
                    return true;
                }
            }
        }
    }

    bool changed = false;
    for (auto* c : node->children) {
        if (boolean_simplification(c, ctx)) changed = true;
    }
    return changed;
}

// =============================================================================
// Главная функция оптимизации
// =============================================================================
static void optimize_node(AstNode* node, OptimizationContext& ctx) {
    if (!node) return;

    // Рекурсивно обрабатываем детей
    for (auto* c : node->children) {
        optimize_node(c, ctx);
    }

    // Применяем оптимизации к текущему узлу
    bool changed = true;
    int iterations = 0;
    while (changed && iterations < 10) {
        changed = false;
        iterations++;

        // 1. Constant Folding
        if (try_constant_folding(node->children, ctx)) changed = true;

        // 7. Algebraic Simplification
        if (algebraic_simplification(node->children, ctx)) changed = true;

        // 12. Boolean Simplification
        if (boolean_simplification(node, ctx)) changed = true;
    }
}

void optimize_ast(AstNode* root) {
    if (!root) return;

    std::cerr << "\n=== Starting AST Optimizations ===\n";
    OptimizationContext ctx;

    // Несколько проходов для более глубокой оптимизации
    for (int pass = 0; pass < 3; ++pass) {
        ctx.changes_made = 0;

        // 2. Constant Propagation
        constant_propagation(root, ctx);

        // 3. Dead Code Elimination
        dead_code_elimination(root, ctx);

        // 1, 7, 12. Другие оптимизации
        optimize_node(root, ctx);

        // 5. Loop Invariant Detection
        if (pass == 0) detect_loop_invariants(root);

        // 6. Inline Detection
        if (pass == 0) detect_inline_opportunities(root);

        std::cerr << "Pass " << (pass + 1) << ": " << ctx.changes_made << " optimizations applied\n";

        if (ctx.changes_made == 0) break;
    }

    std::cerr << "=== AST Optimizations Complete ===\n\n";
}

} // namespace ast