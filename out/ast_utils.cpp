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

struct VarInfo {
    std::string type;
    int line;
    int col;
};

struct ClassInfo {
    std::unordered_map<std::string, std::string> fields; // имя поля -> тип
};

struct SemanticState {
    std::vector<std::unordered_map<std::string, VarInfo>> scopes; // стек областей
    std::unordered_set<std::string> known_types;
    std::unordered_map<std::string, ClassInfo> classes;
    std::vector<std::string> messages;

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

    void error(AstNode* node, const std::string& msg) {
        std::ostringstream ss;
        ss << current_file_path << ":" << node->line << ":" << node->col << " error: " << msg;
        messages.push_back(ss.str());
    }
};

// Helpers
std::string find_first_identifier_in_subtree(AstNode* node) {
    if (!node) return {};
    auto* tn = dynamic_cast<TerminalNode*>(node);
    if (tn && tn->token_type == parser::TokenType::IDENTIFIER) return tn->value;
    for (auto* c : node->children) {
        auto r = find_first_identifier_in_subtree(c);
        if (!r.empty()) return r;
    }
    return {};
}

std::string find_first_type_in_subtree(AstNode* node) {
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

// Additional helper implementations (externally linked)
std::string get_id_from_tok_id_child(AstNode* parent) {
    if (!parent) return "";
    for (auto* c: parent->children) {
        if (c && c->to_string() == "TOK_ID") {
            return find_first_identifier_in_subtree(c);
        }
    }
    return "";
}

std::vector<std::string> collect_param_types(AstNode* param_list) {
    std::vector<std::string> result;
    if (!param_list) return result;
    std::function<void(AstNode*)> collect = [&](AstNode* n) {
        if (!n) return;
        if (n->to_string() == "TOK_PARAM") {
            std::string t = find_first_type_in_subtree(n);
            if (!t.empty()) result.push_back(t);
        }
        for (auto* c: n->children) collect(c);
    };
    collect(param_list);
    return result;
}

bool is_function_declaration(AstNode* node) {
    if (!node) return false;
    for (auto* c: node->children) {
        if (!c) continue;
        if (c->to_string() == "TOK_PARAMLIST") return true;
        if (c->to_string() == "TOK_DECLSUFFIX" || c->to_string() == "TOK_MEMBERSUFFIX") {
            for (auto* sub: c->children) {
                if (sub && sub->to_string() == "TOK_PARAMLIST") return true;
            }
        }
    }
    return false;
}

std::string get_function_name_from_call(AstNode* node) {
    if (!node) return "";
    for (auto* c: node->children) {
        if (c && c->to_string() == "TOK_ID") {
            return find_first_identifier_in_subtree(c);
        }
    }
    return "";
}

AstNode* find_condition_expression(AstNode* node) {
    if (!node) return nullptr;
    std::queue<AstNode*> q;
    q.push(node);
    while (!q.empty()) {
        AstNode* cur = q.front(); q.pop();
        if (!cur) continue;
        std::string name = cur->to_string();
        if (name.find("EXPR") != std::string::npos || name == "TOK_EXPRESSION") return cur;
        for (auto* c: cur->children) q.push(c);
    }
    return nullptr;
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

// 1. Сбор классов
static void semantic_collect_defs(AstNode* node, SemanticState& st) {
    if (!node) return;
    std::string nodename = node->to_string();

    if (nodename == "TOK_CLASSDECL") {
        std::string class_name = find_first_identifier_in_subtree(node);
        if (!class_name.empty()) {
            ClassInfo info;
            // Простейший сбор полей
            std::function<void(AstNode*)> find_members = [&](AstNode* n) {
                if(!n) return;
                if (n->to_string() == "TOK_MEMBER") {
                    std::string t = find_first_type_in_subtree(n);
                    std::string id = find_first_identifier_in_subtree(n);
                    if (!t.empty() && !id.empty()) info.fields[id] = t;
                }
                for(auto* c : n->children) find_members(c);
            };
            find_members(node);
            st.add_class(class_name, info);
        }
    }

    for (auto* c : node->children) {
        semantic_collect_defs(c, st);
    }
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

    // --- Обработка объявлений ---

    if (nodename == "TOK_LOCALVARDECL" || nodename == "TOK_PARAM" || nodename == "TOK_DECLARATION") {
        std::string type_name = find_first_type_in_subtree(node);
        std::string id_name = find_first_identifier_in_subtree(node);

        if (!id_name.empty() && !type_name.empty()) {
             // Игнорируем проверку типа для функций пока, чтобы упростить
            if (!st.is_known_type(type_name) && nodename != "TOK_DECLARATION") {
                st.error(node, "unknown type '" + type_name + "'");
            }
            st.declare_var(id_name, type_name, node->line, node->col);
        }
        
        // Для LOCALVARDECL нужно проверить инициализацию, если есть
        if (nodename == "TOK_LOCALVARDECL") {
             for(auto* c : node->children) {
                 // Рекурсивно проверяем выражение инициализации, но НЕ само объявление
                 if (c->to_string() != "TOK_TYPE" && c->to_string() != "TOK_ID") {
                     semantic_check_and_validate(c, st, node);
                 }
             }
             if (opened_scope) st.pop_scope();
             return; // Мы обработали этот узел
        }
    }
    // ВАЖНО: Обработка полей класса как деклараций (чтобы не ругалось внутри класса)
    else if (nodename == "TOK_MEMBER") {
        // Мы просто пропускаем проверку внутренностей TOK_MEMBER как executable кода,
        // так как это декларация структуры.
        return; 
    }
    // --- Обработка использования (Access) ---

    else if (nodename == "TOK_POSTFIX_ITEM") {
        // Проверяем наличие точки
        bool has_dot = false;
        for (auto* c : node->children) {
            if (auto* tn = dynamic_cast<TerminalNode*>(c)) {
                if (tn->value == ".") { has_dot = true; break; }
            }
        }

        if (has_dot) {
            // Это доступ obj.field
            // Проверяем obj (левая часть), но НЕ проверяем field (правая часть) как переменную
            if (!node->children.empty()) {
                semantic_check_and_validate(node->children[0], st, node); // Проверяем объект (p0)
                
                // Проверка существования поля
                std::vector<std::string> ids;
                collect_all_identifiers(node, ids);
                if (ids.size() >= 2) {
                    std::string obj_name = ids[0];
                    std::string field_name = ids[1];
                    std::string obj_type = st.get_var_type(obj_name);
                    
                    if (!obj_type.empty()) {
                        ClassInfo* ci = st.get_class_info(obj_type);
                        if (ci) {
                            if (ci->fields.find(field_name) == ci->fields.end() && field_name != "pushback") {
                                st.error(node, "class '" + obj_type + "' has no field '" + field_name + "'");
                            }
                        }
                    }
                }
            }
            // Не спускаемся дальше, чтобы не проверить поле как переменную
            return;
        }
    }

    // --- Проверка переменных ---
    
    if (auto* tn = dynamic_cast<TerminalNode*>(node)) {
        if (tn->token_type == parser::TokenType::IDENTIFIER) {
            std::string name = tn->value;
            // Игнорируем стандартные функции и ключевые слова контекста
            if (name != "print" && name != "input" && name != "pushback" && name != "main") {
                if (!st.is_var_declared(name) && !st.is_known_type(name)) {
                     // Дополнительная проверка: если мы внутри доступа через точку (справа), 
                     // то сюда мы попасть не должны благодаря логике выше.
                     st.error(node, "undefined identifier '" + name + "'");
                }
            }
        }
    }

    for (auto* c : node->children) {
        semantic_check_and_validate(c, st, node);
    }

    if (opened_scope) st.pop_scope();
}

// helpers only in this translation unit; main semantic/optimize implementations
// are provided in out/ast_check.cpp and out/ast_functiontree.cpp respectively.

} // namespace ast
