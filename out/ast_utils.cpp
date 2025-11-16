#include "ast_utils.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <queue>
#include <algorithm>

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

// ----------------- Семаника с поддержкой пользовательских классов -----------------
struct SemanticState {
    std::vector<std::unordered_set<std::string>> scopes; // стек областей видимости для переменных
    std::unordered_set<std::string> known_types;        // базовые + пользовательские типы
    std::vector<std::string> messages;

    void push_scope() { scopes.emplace_back(); }
    void pop_scope() { if (!scopes.empty()) scopes.pop_back(); }
    void declare_var(const std::string& name) { if (scopes.empty()) push_scope(); scopes.back().insert(name); }
    bool is_var_declared(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) if (it->count(name)) return true;
        return false;
    }
    void add_type(const std::string& t) { known_types.insert(t); }
    bool is_known_type(const std::string& t) const { return known_types.count(t) != 0; }
    void warn(AstNode* node, const std::string& msg) {
 std::ostringstream ss;
ss <<"test_program.txt:" << node->line << ":" << node->col <<" " << msg;
  messages.push_back(ss.str());
}
};

// helper: найти первый терминальный идентификатор в поддереве
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

// *** ИСПРАВЛЕННАЯ ФУНКЦИЯ ***
// helper: найти первый тип в поддереве (базовый или пользовательский). Возвращает строку лексемы типа, если найдена.
static std::string find_first_type_in_subtree(AstNode* node) {
    if (!node) return {};

    // 1. Если сам узел - это TOK_TYPE, найдем первый IDENTIFIER или ключевое слово типа.
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
        return {}; // Ничего не нашли в TOK_TYPE
    }

    // 2. Если узел не TOK_TYPE, рекурсивно ищем TOK_TYPE в его дочерних узлах.
    for (auto* c : node->children) {
        auto r = find_first_type_in_subtree(c);
        if (!r.empty()) return r;
    }

    return {};
}

// Проход по дереву для сбора объявлений типов (классы) и объявления переменных (в глобальной области)
static void semantic_collect_defs(AstNode* node, SemanticState& st) {
    if (!node) return;
    std::string nodename = node->to_string();

    // Если это определение класса — распознаём по названию узла, содержащему 'CLASS' или 'CLASSDECL'
    if (nodename=="TOK_CLASSDECL") {
        auto cname = find_first_identifier_in_subtree(node);
        if (!cname.empty()) {
            st.add_type(cname);
        }
    }

    for (auto* c : node->children) semantic_collect_defs(c, st);
}

// *** ИСПРАВЛЕННАЯ ФУНКЦИЯ ***
// Второй проход: проверка объявлений переменных/параметров и использование идентификаторов
static void semantic_check_and_validate(AstNode* node, SemanticState& st) {
    if (!node) return;
    std::string nodename = node->to_string();

    // 1. Открыть новую область для CompoundStmt
    bool opened_scope = false;
    if (nodename.find("COMPOUND") != std::string::npos) {
        st.push_scope(); opened_scope = true;
    }

    // 2. Обработка деклараций
    if (nodename == "TOK_LOCALVARDECL") {
        std::string id_name;
        std::string type_name;

        // Ищем узел TOK_TYPE и узел TOK_ID *напрямую*
        AstNode* type_node = nullptr;
        AstNode* id_node = nullptr;

        for(auto* c : node->children) {
            if (c->to_string() == "TOK_TYPE") type_node = c;
            else if (c->to_string() == "TOK_ID") id_node = c;
        }

        if (type_node) {
            type_name = find_first_type_in_subtree(type_node);
        }
        if (id_node) {
            id_name = find_first_identifier_in_subtree(id_node);
        }

        if (id_name.empty()) {
            st.warn(node, "error: declaration missing identifier name");
        } else {
            if (type_name.empty()) {
                st.warn(node, std::string("error: declaration of '") + id_name + "' missing data type");
            } else {
                if (!st.is_known_type(type_name)) {
                     st.warn(node, std::string("error: declaration of '") + id_name + "' has unknown type '" + type_name + "'");
                }
            }
            st.declare_var(id_name); // Объявляем ПЕРЕМЕННУЮ
        }
        // Мы обработали эту ветку, НЕ НУЖНО спускаться рекурсивно для 'use'
    }
    else if (nodename == "TOK_MEMBER" || nodename == "TOK_PARAM" || nodename == "TOK_DECLARATION") {
        // ... (похожая, но, возможно, другая логика для других деклараций)
        // ... (пока оставим старую) ...
        auto id = find_first_identifier_in_subtree(node);
        auto t = find_first_type_in_subtree(node);
        if (!id.empty() && !t.empty()) st.declare_var(id);
    }
    else {
        // 3. Это не узел декларации. Ищем 'use' и спускаемся рекурсивно.
        if (auto* tn = dynamic_cast<TerminalNode*>(node)) {
            if (tn->token_type == parser::TokenType::IDENTIFIER) {
                std::string name = tn->value;
                if (!st.is_var_declared(name) && !st.is_known_type(name)) {
                    st.warn(node, std::string("error: identifier '") + name + "' used before declaration");
                }
            }
        }

        // 4. Рекурсивный обход
        for (auto* c : node->children) {
            semantic_check_and_validate(c, st);
        }
    }

    // 5. Закрыть область
    if (opened_scope) st.pop_scope();
}

bool semantic_check(AstNode* root) {
    if (!root) return true;
    SemanticState st;
    // добавить базовые типы
    st.add_type("int"); st.add_type("float"); st.add_type("double"); st.add_type("string"); st.add_type("bool"); st.add_type("void");st.add_type("vector");

    // --- Первый проход: собрать все декларации типов (например, классы) ---
    try {
        semantic_collect_defs(root, st);
    } catch (const std::exception &e) {
        std::cerr << "semantic collection threw: " << e.what() << "\n";
    }

    // --- Второй проход: проверить объявления и использования ---
    try {
        semantic_check_and_validate(root, st);
    } catch (const std::exception &e) {
        std::cerr << "semantic validation threw: " << e.what() << "\n";
    }

    if (!st.messages.empty()) {
        for (auto &m : st.messages) std::cerr << m << "\n";
        return false;
    }
    return true;
}

// ----------------- Простейшие оптимизации (constant folding) -----------------
static bool is_number_terminal(AstNode* n, double &outval) {
    if (!n) return false;
    auto* t = dynamic_cast<TerminalNode*>(n);
    if (!t) return false;
    if (t->token_type != parser::TokenType::NUMBER) return false;
    try { outval = std::stod(t->value); } catch(...) { return false; }
    return true;
}

static bool try_fold_in_children(std::vector<AstNode*>& ch) {
    // ищем паттерн: NUMBER OP NUMBER подряд и заменяем на один NUMBER
    for (size_t i = 0; i + 2 < ch.size(); ++i) {
        double a=0,b=0;
        if (!is_number_terminal(ch[i], a)) continue;
        auto* op = dynamic_cast<TerminalNode*>(ch[i+1]);
        if (!op) continue;
        if (!is_number_terminal(ch[i+2], b)) continue;
        // определи оператор: берем name или лексему
        std::string opname = op->name.empty() ? parser::token_to_string(op->token_type) : op->name;
        double res = 0; bool ok = true;
        if (opname == "+" || opname == "PLUS") res = a + b;
        else if (opname == "-" || opname == "MINUS") res = a - b;
        else if (opname == "*" || opname == "STAR") res = a * b;
        else if (opname == "/" || opname == "SLASH") { if (b==0) ok=false; else res = a / b; }
        else ok = false;
        if (!ok) continue;
        // заменить три узла на один терминальный NUMBER
        for (int k = 0; k < 3; ++k) { delete ch[i+k]; }
        ch[i] = new ast::TerminalNode(parser::TokenType::NUMBER, std::to_string(res), "NUMBER");
        ch.erase(ch.begin() + i + 1, ch.begin() + i + 3);
        return true; // один ход за раз (повторим в цикле)
    }
    return false;
}

static void optimize_node(AstNode* node) {
    if (!node) return;
    for (auto* c : node->children) optimize_node(c);
    // пробегаем по children и пытаемся сворачивать
    bool changed = true;
    while (changed) {
        changed = try_fold_in_children(node->children);
    }
}

void optimize_ast(AstNode* root) {
    if (!root) return;
    // несколько проходов для более глубокого свёртывания
    for (int i = 0; i < 3; ++i) optimize_node(root);
}

} // namespace ast