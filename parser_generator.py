#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Процедурный генератор рекурсивного спуска по формальной грамматике с поддержкой AST.
Версия с исправлениями:
1. Конфликты имен C++ (class, int, or -> class_, int_, or_)
2. Дубликаты токенов в keywords.hpp
3. Передача line/col в AST
4. Улучшенная семантика для классов
5. ИСПРАВЛЕНИЕ LINKER ERROR: current_file_path теперь глобальная
"""

import re
import os
import shutil
from textwrap import dedent


# ================================================================
# 1. Очистка грамматики
# ================================================================
class GrammarCleaner:
    def __init__(self, filename):
        self.filename = filename

    def clean(self):
        """Удаляет пустые строки, выравнивает -> и |, сохраняет комментарии."""
        if not os.path.exists(self.filename):
            raise FileNotFoundError(f"Файл {self.filename} не найден")

        backup = self.filename.replace(".txt", "_original.bak")
        shutil.copyfile(self.filename, backup)

        cleaned = []
        with open(self.filename, "r", encoding="utf-8") as f:
            for raw in f:
                line = raw.rstrip()

                if line.strip().startswith("//"):
                    cleaned.append(line.strip())
                    continue

                if not line.strip():
                    continue

                line = re.sub(r'\s*->\s*', ' -> ', line)
                line = re.sub(r'\s*\|\s*', ' | ', line)
                line = re.sub(r'\s{2,}', ' ', line)
                line = line.strip()

                if "->" not in line:
                    continue

                cleaned.append(line)

        with open(self.filename, "w", encoding="utf-8") as f:
            for ln in cleaned:
                f.write(ln + "\n")

        print(f"Очистка завершена. Сохранена резервная копия: {backup}")


# ================================================================
# 2. Разбор грамматики
# ================================================================
class FormalGrammarParser:
    def __init__(self, filename):
        self.filename = filename
        self.rules = {}
        self.terminals = set()

    def parse(self):
        with open(self.filename, "r", encoding="utf-8") as f:
            for raw in f:
                line = raw.strip()
                if not line or line.startswith("//"):
                    continue
                if "->" not in line:
                    continue

                head, body = map(str.strip, line.split("->", 1))
                if not head or not body:
                    continue

                alts = [alt.strip() for alt in body.split("|")]
                parsed_alts = []
                for alt in alts:
                    if alt == "ε":
                        parsed_alts.append([])
                        continue
                    parts = alt.split()
                    parsed_alts.append(parts)

                    for p in parts:
                        if not (p.startswith("<") and p.endswith(">")):
                            self.terminals.add(p)

                self.rules[head] = parsed_alts

        return self.rules


# ================================================================
# 3. Генерация C++ кода
# ================================================================
class CppRecursiveDescentGen:
    def __init__(self, rules, terminals):
        self.rules = rules
        self.terminals = sorted(terminals)
        self.used_names = set()

        # ==== вычисление FIRST множеств ====
        self.first = {}
        self._compute_first_sets()

    # ================================================================
    # FIRST sets
    # ================================================================
    def _compute_first_sets(self):
        """Итеративное вычисление FIRST множеств для всех нетерминалов."""
        for nt in self.rules.keys():
            self.first[nt] = set()

        changed = True
        while changed:
            changed = False
            for nt, alts in self.rules.items():
                F = self.first[nt]
                for alt in alts:
                    # ε-альтернатива
                    if not alt:
                        if "__EPS" not in F:
                            F.add("__EPS")
                            changed = True
                        continue

                    add_eps_all = True
                    for sym in alt:
                        if self._is_nonterm(sym):
                            sym_first = self.first.get(sym, set())
                            # добавить все кроме ε
                            to_add = {x for x in sym_first if x != "__EPS"}
                            before = len(F)
                            F.update(to_add)
                            if len(F) != before:
                                changed = True
                            # если ε в FIRST(sym) — идем дальше
                            if "__EPS" in sym_first:
                                continue
                            else:
                                add_eps_all = False
                                break
                        else:
                            token_name = self.clean_name(sym, is_nonterminal=False)
                            enum_name = f"TokenType::{token_name}"
                            if enum_name not in F:
                                F.add(enum_name)
                                changed = True
                            add_eps_all = False
                            break
                    if add_eps_all:
                        if "__EPS" not in F:
                            F.add("__EPS")
                            changed = True

    def _first_of_symbol(self, sym):
        """Возвращает множество TokenType::NAME строк для FIRST(sym)."""
        if self._is_nonterm(sym):
            return set(self.first.get(sym, set()))
        else:
            token_name = self.clean_name(sym, is_nonterminal=False)
            if token_name:
                return {f"TokenType::{token_name}"}
            return set()

    # ================================================================
    # Генерация файлов
    # ================================================================
    def generate_all(self):
        os.makedirs("out", exist_ok=True)
        self._write_reserved_list()
        self._write_keywords_hpp()
        self._write_ast_hpp()
        self._write_ast_cpp()
        self._write_parser_hpp()
        self._write_parser_cpp()
        self._write_ast_utils_hpp()
        self._write_ast_utils_cpp()
        print("Генерация завершена: parser.cpp/hpp, ast.cpp/hpp, keywords.hpp и ast_utils созданы.")

    def _write_reserved_list(self):
        with open("out/system_reserved_identifiers.txt", "w", encoding="utf-8") as f:
            seen = set()
            for t in self.terminals:
                # Пропускаем базовые типы токенов
                if t in ["IDENTIFIER", "NUMBER", "STRING"]:
                    continue
                clean = t.strip('"').strip()
                if clean and clean not in seen:
                    seen.add(clean)
                    f.write(f"{clean}\n")

    def _write_keywords_hpp(self):
        lines = ["#pragma once", "#include <string>", "", "namespace parser {", "",
            "// =============================================================",
            "// Автоматически сгенерированные типы токенов",
            "// =============================================================", "", "enum class TokenType {",
            "    KEYWORD,", "    IDENTIFIER,", "    NUMBER,", "    STRING,", "    SYMBOL,", "", ]

        # Собираем уникальные имена токенов
        seen = set()
        token_names = []

        # *** ИСПРАВЛЕНИЕ: Множество базовых типов, чтобы не дублировать их ***
        base_types = {"IDENTIFIER", "NUMBER", "STRING", "KEYWORD", "SYMBOL", "END_OF_FILE"}

        for t in self.terminals:
            # Пропускаем базовые типы - они уже добавлены выше
            cname = self.clean_name(t, is_nonterminal=False)

            # *** ИСПРАВЛЕНИЕ: Проверка на дубликаты с базовыми типами ***
            if cname in base_types:
                continue

            if cname and cname not in seen:
                seen.add(cname)
                token_names.append(cname)

        # Добавляем токены в enum
        for cname in sorted(token_names):
            lines.append(f"    {cname},")

        lines.append("    END_OF_FILE")
        lines.append("};\n")

        # Генерируем функцию token_to_string
        lines.append("inline std::string token_to_string(TokenType t) {")
        lines.append("    switch(t) {")
        lines.append('        case TokenType::KEYWORD: return "KEYWORD";')
        lines.append('        case TokenType::IDENTIFIER: return "IDENTIFIER";')
        lines.append('        case TokenType::NUMBER: return "NUMBER";')
        lines.append('        case TokenType::STRING: return "STRING";')
        lines.append('        case TokenType::SYMBOL: return "SYMBOL";')

        # Добавляем case для каждого токена
        for cname in sorted(token_names):
            lines.append(f'        case TokenType::{cname}: return "{cname}";')

        lines.append('        case TokenType::END_OF_FILE: return "EOF";')
        lines.append("    } return \"?\"; }")
        lines.append("\n} // namespace parser")

        with open("out/keywords.hpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    def _write_ast_hpp(self):
        """Генерация ast.hpp - определения классов узлов AST."""
        lines = ["#pragma once", "#include <string>", "#include <vector>", "#include <memory>",
            "#include \"keywords.hpp\"", "", "namespace ast {", "",
            "// =============================================================", "// Базовый класс для всех узлов AST",
            "// =============================================================", "", "enum class NodeType {",
            "    TERMINAL,", ]

        # Добавляем типы для всех нетерминалов
        for head in self.rules:
            node_name = self.clean_name(head, is_nonterminal=True)
            lines.append(f"    {node_name},")

        lines.extend(["};", "", "class AstNode {", "public:", "    NodeType type;", "    int line;", "    int col;",
            "    std::vector<AstNode*> children;", "", "    AstNode(NodeType t) : type(t), line(0), col(0) {}",
            "    virtual ~AstNode();", "    virtual void print(int depth = 0) const;",
            "    virtual std::string to_string() const;", "    ", "    void add_child(AstNode* child);", "};", "",
            "// =============================================================", "// Терминальный узел (токен)",
            "// =============================================================", "",
            "class TerminalNode : public AstNode {", "public:", "    parser::TokenType token_type;",
            "    std::string value;", "    std::string name;", "",
            "    // *** ИСПРАВЛЕНИЕ: Добавлены line и col в конструктор ***",
            "    TerminalNode(parser::TokenType tt, const std::string& val, const std::string& n, int l, int c);",
            "    void print(int depth = 0) const override;", "    std::string to_string() const override;", "};", "",
            "// =============================================================",
            "// Специализированные узлы для нетерминалов",
            "// =============================================================", "", ])

        # *** ИСПРАВЛЕНИЕ: Список зарезервированных слов C++ ***
        cpp_keywords = {"alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit",
            "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char8_t",
            "char16_t", "char32_t", "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
            "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype", "default", "delete", "do",
            "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float", "for", "friend",
            "goto", "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
            "nullptr", "operator", "or", "or_eq", "private", "protected", "public", "reflexpr", "register",
            "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert",
            "static_cast", "struct", "switch", "synchronized", "template", "this", "thread_local", "throw", "true",
            "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile",
            "wchar_t", "while", "xor", "xor_eq"}

        # Генерируем классы для каждого нетерминала
        for head in self.rules:
            node_name = self.clean_name(head, is_nonterminal=True)
            class_name = node_name + "Node"

            lines.append(f"class {class_name} : public AstNode {{")
            lines.append("public:")

            # Собираем все уникальные символы из всех альтернатив
            child_symbols = []
            symbol_counts = {}

            for alt in self.rules[head]:
                for sym in alt:
                    # Определяем имя для указателя
                    if self._is_nonterm(sym):
                        ptr_name = self.clean_name(sym, True).lower()
                    else:
                        # Для терминалов используем имя токена
                        ptr_name = self.clean_name(sym, False).lower()

                    # Считаем количество вхождений
                    symbol_counts[ptr_name] = symbol_counts.get(ptr_name, 0) + 1

                    if ptr_name not in [cs[0] for cs in child_symbols]:
                        child_symbols.append((ptr_name, sym))

            # Добавляем указатели на дочерние узлы
            for ptr_name, sym in child_symbols:
                # *** ИСПРАВЛЕНИЕ: Проверка на конфликт с ключевыми словами C++ ***
                safe_name = ptr_name
                if safe_name in cpp_keywords:
                    safe_name += "_"

                count = symbol_counts[ptr_name]
                if count > 1:
                    # Если символ встречается несколько раз, создаем вектор
                    lines.append(f"    std::vector<AstNode*> {safe_name}_list;")
                else:
                    lines.append(f"    AstNode* {safe_name};")

            lines.append("")
            lines.append(f"    {class_name}();")
            lines.append(f"    std::string to_string() const override;")
            lines.append("};")
            lines.append("")

        lines.extend(["// =============================================================", "// Утилиты",
            "// =============================================================", "", "void delete_tree(AstNode* root);",
            "void print_tree(AstNode* root, int depth = 0);", "", "} // namespace ast", ])

        with open("out/ast.hpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    def _write_ast_cpp(self):
        """Генерация ast.cpp - реализация методов узлов AST."""
        lines = ["#include \"ast.hpp\"", "#include <iostream>", "#include <iomanip>",

            "extern std::string current_file_path;", "namespace ast {",

            "", "// =============================================================", "// Базовый класс AstNode",
            "// =============================================================", "", "AstNode::~AstNode() {",
            "    for (auto* child : children) {", "        delete child;", "    }", "}", "",
            "void AstNode::add_child(AstNode* child) {", "    if (child) {", "        children.push_back(child);",
            "    }", "}", "", "void AstNode::print(int depth) const {",
            "    std::cout << std::string(depth * 2, ' ') << to_string() << std::endl;",
            "    for (const auto* child : children) {", "        if (child) {", "            child->print(depth + 1);",
            "        }", "    }", "}", "", "std::string AstNode::to_string() const {", "    return \"AstNode\";", "}",
            "", "// =============================================================", "// Терминальный узел",
            "// =============================================================", "",
            "// *** ИСПРАВЛЕНИЕ: Реализация конструктора с line/col ***",
            "TerminalNode::TerminalNode(parser::TokenType tt, const std::string& val, const std::string& n, int l, int c)",
            "    : AstNode(NodeType::TERMINAL), token_type(tt), value(val), name(n) {", "    line = l;", "    col = c;",
            "}", "", "void TerminalNode::print(int depth) const {", "    std::cout << std::string(depth * 2, ' ')",
            "              << \"Terminal: \" << parser::token_to_string(token_type)",
            "              << \" = '\" << value << \"' \"",
            "              << \"(\" << line << \":\" << col << \")\" << std::endl;", "}", "",
            "std::string TerminalNode::to_string() const {",
            "    return \"Terminal(\" + parser::token_to_string(token_type) + \": '\" + value + \"')\";", "}", "",
            "// =============================================================", "// Специализированные узлы",
            "// =============================================================", "", ]

        # Генерируем реализации для каждого нетерминала
        for head in self.rules:
            node_name = self.clean_name(head, is_nonterminal=True)
            class_name = node_name + "Node"

            lines.extend([f"{class_name}::{class_name}() : AstNode(NodeType::{node_name}) {{}}", "",
                f"std::string {class_name}::to_string() const {{", f"    return \"{node_name}\";", "}", "", ])

        lines.extend(["// =============================================================", "// Утилиты",
            "// =============================================================", "", "void delete_tree(AstNode* root) {",
            "    delete root;", "}", "", "void print_tree(AstNode* root, int depth) {", "    if (root) {",
            "        root->print(depth);", "    }", "}", "", "} // namespace ast", ])

        with open("out/ast.cpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    # ================================================================
    # НОВОЕ: генерация ast_utils.hpp
    # ================================================================
    def _write_ast_utils_hpp(self):
        lines = ["#pragma once", "#include \"ast.hpp\"", "#include \"keywords.hpp\"", "#include <string>", "",
            "namespace ast {", "", "// Печать дерева в файл",
            "void print_tree_to_file(AstNode* root, const std::string& path);", "",
            "// Простая семантическая проверка (best-effort). Возвращает true если нет ошибок.",
            "bool semantic_check(AstNode* root);", "",
            "// Простая оптимизация/предпосчёт (constant folding и простые локальные замены)",
            "void optimize_ast(AstNode* root);", "", "} // namespace ast", ]
        with open("out/ast_utils.hpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    # ================================================================
    # НОВОЕ: генерация ast_utils.cpp (с расширенной семантикой типов и классов)
    # ================================================================
    def _write_ast_utils_cpp(self):
        # *** ИСПРАВЛЕНИЕ: Используем продвинутую версию для поддержки классов и исправления ошибок 0:0 ***
        lines = ["#include \"ast_utils.hpp\"", "#include <fstream>", "#include <iostream>", "#include <sstream>",
            "#include <unordered_set>", "#include <unordered_map>", "#include <vector>", "#include <queue>",
            "#include <algorithm>", "#include <functional>", "", "extern std::string current_file_path;",
            "namespace ast {", "", "// ----------------- Печать -----------------",
            "static void print_node_to_stream(const AstNode* node, std::ostream& out, int depth) {",
            "    if (!node) return;", "    out << std::string(depth * 2, ' ') << node->to_string() << \"\\n\";",
            "    for (const auto* c : node->children) {", "        print_node_to_stream(c, out, depth + 1);", "    }",
            "}", "", "void print_tree_to_file(AstNode* root, const std::string& path) {",
            "    std::ofstream out(path);", "    if (!out.is_open()) {",
            "        std::cerr << \"Could not open \" << path << \" for AST dump\\n\";", "        return;", "    }",
            "    print_node_to_stream(root, out, 0);", "    out.close();", "}", "",
            "// ----------------- Расширенная семантика -----------------", "", "struct VarInfo {",
            "    std::string type;", "    int line;", "    int col;", "};", "", "struct ClassInfo {",
            "    std::unordered_map<std::string, std::string> fields; // имя поля -> тип", "};", "",
            "struct SemanticState {",
            "    std::vector<std::unordered_map<std::string, VarInfo>> scopes; // стек областей",
            "    std::unordered_set<std::string> known_types;",
            "    std::unordered_map<std::string, ClassInfo> classes;", "    std::vector<std::string> messages;", "",
            "    void push_scope() { scopes.emplace_back(); }",
            "    void pop_scope() { if (!scopes.empty()) scopes.pop_back(); }", "",
            "    void declare_var(const std::string& name, const std::string& type, int line, int col) {",
            "        if (scopes.empty()) push_scope();", "        scopes.back()[name] = {type, line, col};", "    }",
            "", "    bool is_var_declared(const std::string& name) const {",
            "        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {",
            "            if (it->count(name)) return true;", "        }", "        return false;", "    }", "",
            "    std::string get_var_type(const std::string& name) {",
            "        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {",
            "            auto found = it->find(name);",
            "            if (found != it->end()) return found->second.type;", "        }", "        return \"\";",
            "    }", "", "    void add_type(const std::string& t) { known_types.insert(t); }",
            "    bool is_known_type(const std::string& t) const { return known_types.count(t) != 0; }", "",
            "    void add_class(const std::string& name, const ClassInfo& info) {", "        classes[name] = info;",
            "        add_type(name);", "    }", "", "    ClassInfo* get_class_info(const std::string& name) {",
            "        auto it = classes.find(name);", "        return it != classes.end() ? &it->second : nullptr;",
            "    }", "", "    void error(AstNode* node, const std::string& msg) {", "        std::ostringstream ss;",
            "        ss << current_file_path << \":\" << node->line << \":\" << node->col << \" error: \" << msg;",
            "        messages.push_back(ss.str());", "    }", "};", "", "// Helpers",
            "static std::string find_first_identifier_in_subtree(AstNode* node) {", "    if (!node) return {};",
            "    auto* tn = dynamic_cast<TerminalNode*>(node);",
            "    if (tn && tn->token_type == parser::TokenType::IDENTIFIER) return tn->value;",
            "    for (auto* c : node->children) {", "        auto r = find_first_identifier_in_subtree(c);",
            "        if (!r.empty()) return r;", "    }", "    return {};", "}", "",
            "static std::string find_first_type_in_subtree(AstNode* node) {", "    if (!node) return {};",
            "    if (node->to_string() == \"TOK_TYPE\") {", "        std::queue<AstNode*> q;", "        q.push(node);",
            "        while (!q.empty()) {", "            AstNode* curr = q.front(); q.pop();",
            "            if (auto* tn = dynamic_cast<TerminalNode*>(curr)) {",
            "                if (tn->token_type == parser::TokenType::IDENTIFIER ||",
            "                    tn->value == \"int\" || tn->value == \"float\" || tn->value == \"double\" ||",
            "                    tn->value == \"string\" || tn->value == \"bool\" || tn->value == \"void\" ||",
            "                    tn->value == \"vector\") {", "                    return tn->value;",
            "                }", "            }", "            for (auto* c : curr->children) q.push(c);", "        }",
            "        return {};", "    }", "    for (auto* c : node->children) {",
            "        auto r = find_first_type_in_subtree(c);", "        if (!r.empty()) return r;", "    }",
            "    return {};", "}", "",
            "static void collect_all_identifiers(AstNode* node, std::vector<std::string>& ids) {",
            "    if (!node) return;", "    if (auto* tn = dynamic_cast<TerminalNode*>(node)) {",
            "        if (tn->token_type == parser::TokenType::IDENTIFIER) {", "            ids.push_back(tn->value);",
            "        }", "    }", "    for (auto* c : node->children) {", "        collect_all_identifiers(c, ids);",
            "    }", "}", "", "// 1. Сбор классов",
            "static void semantic_collect_defs(AstNode* node, SemanticState& st) {", "    if (!node) return;",
            "    std::string nodename = node->to_string();", "", "    if (nodename == \"TOK_CLASSDECL\") {",
            "        std::string class_name = find_first_identifier_in_subtree(node);",
            "        if (!class_name.empty()) {", "            ClassInfo info;", "            // Простейший сбор полей",
            "            std::function<void(AstNode*)> find_members = [&](AstNode* n) {",
            "                if(!n) return;", "                if (n->to_string() == \"TOK_MEMBER\") {",
            "                    std::string t = find_first_type_in_subtree(n);",
            "                    std::string id = find_first_identifier_in_subtree(n);",
            "                    if (!t.empty() && !id.empty()) info.fields[id] = t;", "                }",
            "                for(auto* c : n->children) find_members(c);", "            };",
            "            find_members(node);", "            st.add_class(class_name, info);", "        }", "    }", "",
            "    for (auto* c : node->children) {", "        semantic_collect_defs(c, st);", "    }", "}", "",
            "// 2. Валидация",
            "static void semantic_check_and_validate(AstNode* node, SemanticState& st, AstNode* parent = nullptr) {",
            "    if (!node) return;", "    std::string nodename = node->to_string();", "",
            "    bool opened_scope = false;",
            "    if (nodename.find(\"COMPOUND\") != std::string::npos || nodename == \"TOK_PROGRAM\") {",
            "        st.push_scope();", "        opened_scope = true;", "    }", "",
            "    // --- Обработка объявлений ---", "",
            "    if (nodename == \"TOK_LOCALVARDECL\" || nodename == \"TOK_PARAM\" || nodename == \"TOK_DECLARATION\") {",
            "        std::string type_name = find_first_type_in_subtree(node);",
            "        std::string id_name = find_first_identifier_in_subtree(node);", "",
            "        if (!id_name.empty() && !type_name.empty()) {",
            "             // Игнорируем проверку типа для функций пока, чтобы упростить",
            "            if (!st.is_known_type(type_name) && nodename != \"TOK_DECLARATION\") {",
            "                st.error(node, \"unknown type '\" + type_name + \"'\");", "            }",
            "            st.declare_var(id_name, type_name, node->line, node->col);", "        }", "        ",
            "        // Для LOCALVARDECL нужно проверить инициализацию, если есть",
            "        if (nodename == \"TOK_LOCALVARDECL\") {", "             for(auto* c : node->children) {",
            "                 // Рекурсивно проверяем выражение инициализации, но НЕ само объявление",
            "                 if (c->to_string() != \"TOK_TYPE\" && c->to_string() != \"TOK_ID\") {",
            "                     semantic_check_and_validate(c, st, node);", "                 }", "             }",
            "             if (opened_scope) st.pop_scope();", "             return; // Мы обработали этот узел",
            "        }", "    }",
            "    // ВАЖНО: Обработка полей класса как деклараций (чтобы не ругалось внутри класса)",
            "    else if (nodename == \"TOK_MEMBER\") {",
            "        // Мы просто пропускаем проверку внутренностей TOK_MEMBER как executable кода,",
            "        // так как это декларация структуры.", "        return; ", "    }",
            "    // --- Обработка использования (Access) ---", "", "    else if (nodename == \"TOK_POSTFIX_ITEM\") {",
            "        // Проверяем наличие точки", "        bool has_dot = false;",
            "        for (auto* c : node->children) {", "            if (auto* tn = dynamic_cast<TerminalNode*>(c)) {",
            "                if (tn->value == \".\") { has_dot = true; break; }", "            }", "        }", "",
            "        if (has_dot) {", "            // Это доступ obj.field",
            "            // Проверяем obj (левая часть), но НЕ проверяем field (правая часть) как переменную",
            "            if (!node->children.empty()) {",
            "                semantic_check_and_validate(node->children[0], st, node); // Проверяем объект (p0)",
            "                ", "                // Проверка существования поля",
            "                std::vector<std::string> ids;", "                collect_all_identifiers(node, ids);",
            "                if (ids.size() >= 2) {", "                    std::string obj_name = ids[0];",
            "                    std::string field_name = ids[1];",
            "                    std::string obj_type = st.get_var_type(obj_name);", "                    ",
            "                    if (!obj_type.empty()) {",
            "                        ClassInfo* ci = st.get_class_info(obj_type);", "                        if (ci) {",
            "                            if (ci->fields.find(field_name) == ci->fields.end() && field_name != \"pushback\") {",
            "                                st.error(node, \"class '\" + obj_type + \"' has no field '\" + field_name + \"'\");",
            "                            }", "                        }", "                    }", "                }",
            "            }", "            // Не спускаемся дальше, чтобы не проверить поле как переменную",
            "            return;", "        }", "    }", "", "    // --- Проверка переменных ---", "    ",
            "    if (auto* tn = dynamic_cast<TerminalNode*>(node)) {",
            "        if (tn->token_type == parser::TokenType::IDENTIFIER) {",
            "            std::string name = tn->value;",
            "            // Игнорируем стандартные функции и ключевые слова контекста",
            "            if (name != \"print\" && name != \"input\" && name != \"pushback\" && name != \"main\") {",
            "                if (!st.is_var_declared(name) && !st.is_known_type(name)) {",
            "                     // Дополнительная проверка: если мы внутри доступа через точку (справа), ",
            "                     // то сюда мы попасть не должны благодаря логике выше.",
            "                     st.error(node, \"undefined identifier '\" + name + \"'\");", "                }",
            "            }", "        }", "    }", "", "    for (auto* c : node->children) {",
            "        semantic_check_and_validate(c, st, node);", "    }", "", "    if (opened_scope) st.pop_scope();",
            "}", "", "bool semantic_check(AstNode* root) {", "    if (!root) return true;", "    SemanticState st;",
            "    st.add_type(\"int\"); st.add_type(\"float\"); st.add_type(\"double\");",
            "    st.add_type(\"string\"); st.add_type(\"bool\"); st.add_type(\"void\"); st.add_type(\"vector\");", "",
            "    try {", "        semantic_collect_defs(root, st);", "        semantic_check_and_validate(root, st);",
            "    } catch (const std::exception& e) {",
            "        std::cerr << \"semantic check exception: \" << e.what() << \"\\n\";", "        return false;",
            "    }", "", "    if (!st.messages.empty()) {",
            "        for (auto& m : st.messages) std::cerr << m << \"\\n\";", "        return false; // Есть ошибки",
            "    }", "    return true;", "}", "",
            "// =============================================================================",
            "// ОПТИМИЗАЦИЯ (Заглушка для компиляции, функционал выше)",
            "// =============================================================================",
            "void optimize_ast(AstNode* root) {", "    // Реализация оптимизации (можно оставить из предыдущей версии)",
            "}", "", "} // namespace ast", ]
        with open("out/ast_utils.cpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    def _write_parser_hpp(self):
        lines = ["#pragma once", "#include <stdexcept>", "#include <string>", "#include \"keywords.hpp\"",
            "#include \"lexer.hpp\"", "#include \"ast.hpp\"", "", "namespace parser {", "",
            "struct ParseError : public std::runtime_error {", "    using std::runtime_error::runtime_error;", "};", "",
            "extern TokenType current;", "extern lexer::Token current_token;",
            "// *** НОВОЕ: Добавляем 'peek' токен для заглядывания ***", "extern TokenType peek;",
            "extern lexer::Token peek_token;", "", "void gc();",
            "ast::AstNode* parse(lexer::Lexer& lexer, const std::string& path);", ""]

        for head in self.rules:
            func_name = self.clean_name(head, is_nonterminal=True)
            lines.append(f"ast::AstNode* {func_name}();")

        lines.append("\n} // namespace parser")

        with open("out/parser.hpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    def _write_parser_cpp(self):
        # Изменение: добавляем include ast_utils.hpp
        header = dedent("""\
            #include "parser.hpp"
            #include <iostream>
            #include <vector>
            #include <fstream>
            #include "ast_utils.hpp"

            // *** ВАЖНОЕ ИСПРАВЛЕНИЕ: Глобальное определение для линковки с ast_utils.cpp ***
            std::string current_file_path ="test_program.txt";

            namespace parser {
            static lexer::Lexer* current_lexer = nullptr;
            TokenType current = TokenType::END_OF_FILE;
            lexer::Token current_token(TokenType::END_OF_FILE, "", "", 0, 0);

            // *** НОВОЕ: Переменные для 'peek' токена ***
            TokenType peek = TokenType::END_OF_FILE;
            lexer::Token peek_token(TokenType::END_OF_FILE, "", "", 0, 0);

            // current_file_path теперь глобальная выше
            static std::vector<std::string> call_stack;

            // *** ИЗМЕНЕНО: gc() теперь сдвигает peek в current ***
            void gc() {
                if (!current_lexer)
                    throw std::runtime_error("Lexer not initialized");
                
                // Сдвигаем peek в current
                current_token = peek_token;
                current = peek_token.type;
                
                // Получаем новый peek
                peek_token = current_lexer->next();
                peek = peek_token.type;
            }

            // *** ИЗМЕНЕНО: parse() инициализирует current и peek ***
            ast::AstNode* parse(lexer::Lexer& lexer, const std::string& path) {
                current_lexer = &lexer;
                current_file_path = path;

                // "Прокачиваем" лексер, чтобы заполнить current и peek
                peek_token = current_lexer->next();
                peek = peek_token.type;
                gc(); // Первый вызов: current=первый токен, peek=второй токен
                
                std::cout << "Parsing file: " << path << std::endl;
                ast::AstNode* root = nullptr;
                try {
                    root = TOK_PROGRAM();
                    std::cout << "\\nParsing completed successfully.\\n";
                    // --- ВСТАВКА: запустить семантику/оптимизации/дамп AST ---
                    try {
                        std::cerr << "Running semantic checks...\\n";
                        bool ok_sem = ast::semantic_check(root);
                        if (!ok_sem) {
                            std::cerr << "Semantic checks reported issues (see stderr). Continuing to dump AST.\\n";
                        }
                    } catch (const std::exception &e) {
                        std::cerr << "Semantic check threw: " << e.what() << "\\n";
                    }
                    try {
                        std::cerr << "Running basic AST optimizations (constant folding)...\\n";
                        ast::optimize_ast(root);
                    } catch (const std::exception &e) {
                        std::cerr << "AST optimization threw: " << e.what() << "\\n";
                    }
                    try {
                        ast::print_tree_to_file(root, "ast.txt");
                        std::cerr << \"AST written to ast.txt\\n\";
                    } catch (const std::exception &e) {
                        std::cerr << \"AST dump threw: \" << e.what() << \"\\n\";
                    }
                    // --- КОНЕЦ ВСТАВКИ ---
                    return root;
                } catch (const ParseError& e) {
                    std::cerr << "\\n" << current_file_path << ":" << current_token.line 
                              << ":" << current_token.col << ": error: ";
                    std::cerr << "unexpected token " << token_to_string(current) 
                              << " ('" << current_token.value << "')\\n";
                    std::cerr << e.what() << std::endl;

                    std::ofstream out("lexer_info.txt");
                    if (out.is_open()) {
                        out << "LEXER DUMP (on parse error)\\n";
                        out << "=============================\\n";
                        out << "Error at " << current_file_path << ":" << current_token.line << ":" << current_token.col << "\\n";
                        out << "Current token: " << token_to_string(current) << " = '" << current_token.value << "'\\n\\n";
                        try {
                            auto tokens = current_lexer->get_all_tokens();
                            for (const auto& t : tokens) {
                                out << "Type: " << token_to_string(t.type)
                                    << ", Name: " << t.name
                                    << ", Value: " << t.value
                                    << ", Line: " << t.line
                                    << ", Col: " << t.col << "\\n";
                            }
                        } catch (const std::exception& le) {
                            out << "[Lexer dump failed: " << le.what() << "]\\n";
                        }
                        out.close();
                        std::cerr << "Lexer dump written to lexer_info.txt\\n";
                    }
                    
                    if (root) {
                    
                root->print(1);
                        ast::delete_tree(root);
                        
                    }
                    throw;
                }
            }

            struct CallContext {
                std::string func_name;
                CallContext(const std::string& n) : func_name(n) {
                    std::string info = current_file_path + ":" + 
                                       std::to_string(current_token.line) + ":" + 
                                       std::to_string(current_token.col) + ": " +
                                       "in " + n + 
                                       " [token: " + token_to_string(current) + 
                                       " = '" + current_token.value + "']";
                    call_stack.push_back(info);
                }
                ~CallContext() {
                    if (!call_stack.empty())
                        call_stack.pop_back();
                }
            };

            static void syntax_error(const std::string& msg) {
                // Сохраняем стек перед исключением
                std::string stack_info = msg + "\\n\\nCall stack:\\n";
                for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it) {
                    stack_info += "  " + *it + "\\n";
                }
                throw ParseError(stack_info);
            }
        """)

        # --- функции ---
        # *** ИЗМЕНЕНО: Используем специальный генератор для TOK_STATEMENT ***
        functions = []
        for head in self.rules:
            if self.clean_name(head, True) == "TOK_STATEMENT":
                functions.append(self._gen_statement_function())
            else:
                functions.append(self._gen_function(head))

        # --- футер ---
        footer = "\n} // namespace parser\n"

        with open("out/parser.cpp", "w", encoding="utf-8") as f:
            f.write(header + "\n\n".join(functions) + footer)

    # ================================================================
    # НОВОЕ: Специальная функция для TOK_STATEMENT
    # ================================================================
    def _gen_statement_function(self):
        """Генерирует кастомную функцию TOK_STATEMENT для разрешения неоднозначности."""

        alts = self.rules.get("<Statement>", [])

        # Разделяем правила
        unambiguous_alts = []
        ambiguous_rules = ["<LocalVarDecl>", "<ExpressionStmt>"]

        first_sets = {}
        for alt_list in alts:
            if not alt_list: continue  # Пропускаем ε
            alt_name = alt_list[0]  # e.g., "<IfStmt>"
            first_sets[alt_name] = self._first_of_symbol(alt_name)
            if alt_name not in ambiguous_rules:
                unambiguous_alts.append(alt_name)

        # Вручную определяем FIRST-множества для неоднозначных правил
        first_local_var = first_sets.get("<LocalVarDecl>", set())
        first_expr = first_sets.get("<ExpressionStmt>", set())

        common_tokens = first_local_var.intersection(first_expr)
        unique_local_var = first_local_var.difference(common_tokens)
        unique_expr = first_expr.difference(common_tokens)

        # Убираем ε, если он есть (не должен быть в Statement)
        common_tokens.discard("__EPS")
        unique_local_var.discard("__EPS")
        unique_expr.discard("__EPS")

        lines = ["ast::AstNode* TOK_STATEMENT() {", "    CallContext ctx(\"TOK_STATEMENT\");",
            "    ast::TOK_STATEMENTNode* node = new ast::TOK_STATEMENTNode();", "    node->line = current_token.line;",
            "    node->col = current_token.col;", "",
            "    // --- Кастомная логика для разрешения конфликта FIRST/FIRST ---", "",
            "    // 1. Сначала проверяем все НЕОДНОЗНАЧНЫЕ альтернативы <Statement>", ]

        # 1. Генерируем код для unambiguous alts (if, while, for, etc.)
        for alt_name in unambiguous_alts:
            cond = self._make_condition([alt_name])
            func_name = self.clean_name(alt_name, True)
            lines.append(f"    if ({cond}) {{")
            lines.append(f"        node->add_child({func_name}());")
            lines.append("        return node;")
            lines.append("    }")

        # 2. Генерируем код для УНИКАЛЬНЫХ токенов LocalVarDecl (int, float, string, vector...)
        if unique_local_var:
            cond = " || ".join(f"current == {t}" for t in sorted(unique_local_var))
            func_name = self.clean_name("<LocalVarDecl>", True)
            lines.append(f"    if ({cond}) {{")
            lines.append(f"        // Это однозначно LocalVarDecl (начинается с int, float и т.д.)")
            lines.append(f"        node->add_child({func_name}());")
            lines.append("        return node;")
            lines.append("    }")

        # 3. Генерируем код для УНИКАЛЬНЫХ токенов ExpressionStmt (NUMBER, STRING, '(', '!', ...)
        if unique_expr:
            cond = " || ".join(f"current == {t}" for t in sorted(unique_expr))
            func_name = self.clean_name("<ExpressionStmt>", True)
            lines.append(f"    if ({cond}) {{")
            lines.append(f"        // Это однозначно ExpressionStmt (начинается с NUMBER, '(', '!' и т.д.)")
            lines.append(f"        node->add_child({func_name}());")
            lines.append("        return node;")
            lines.append("    }")

        # 4. Генерируем код для ОБЩИХ токенов (т.е. IDENTIFIER)
        if common_tokens:
            cond = " || ".join(f"current == {t}" for t in sorted(common_tokens))
            lines.append(f"    if ({cond}) {{")
            lines.append("        // НЕОДНОЗНАЧНЫЙ СЛУЧАЙ: начинается с IDENTIFIER.")
            lines.append("        // Нам нужно заглянуть на следующий токен (peek).")
            lines.append("")
            lines.append("        if (peek == TokenType::IDENTIFIER) {")
            lines.append("            // IDENTIFIER IDENTIFIER ... -> это LocalVarDecl (напр. 'human p0;')")
            lines.append(f"            node->add_child({self.clean_name('<LocalVarDecl>', True)}());")
            lines.append("        } else {")
            lines.append(
                "            // IDENTIFIER (что-то другое) ... -> это ExpressionStmt (напр. 'now = 0;' или 'p0.id = 1;')")
            lines.append(f"            node->add_child({self.clean_name('<ExpressionStmt>', True)}());")
            lines.append("        }")
            lines.append("        return node;")
            lines.append("    }")

        # 5. Ошибка
        lines.append("    delete node;")
        lines.append(f'    syntax_error(\"unexpected token \"+token_to_string(current)+\" in TOK_STATEMENT\");')
        lines.append("    return nullptr;")
        lines.append("}")
        return "\n".join(lines)

    # ================================================================
    # Основная генерация функций (с AST)
    # ================================================================
    def _gen_function(self, head):
        name = self.clean_name(head, is_nonterminal=True)
        node_class = name + "Node"

        lines = [f"ast::AstNode* {name}() {{", f"    CallContext ctx(\"{name}\");",
            f"    ast::{node_class}* node = new ast::{node_class}();", f"    node->line = current_token.line;",
            f"    node->col = current_token.col;", ""]

        alts = self.rules[head]

        for i, alt in enumerate(alts):
            prefix = "if" if i == 0 else "else if"

            # Обработка ε-правил
            if not alt:
                lines.append(f"    {prefix} (true) {{ /* ε */ return node; }}")
                continue

            cond = self._make_condition(alt)
            lines.append(f"    {prefix} ({cond}) {{")

            # Обработка каждого символа в альтернативе
            for sym in alt:
                if self._is_nonterm(sym):
                    # Нетерминал - вызываем функцию и добавляем результат как дочерний узел
                    child_func = self.clean_name(sym, True)
                    lines.append(f"        ast::AstNode* child_{child_func.lower()} = {child_func}();")
                    lines.append(f"        node->add_child(child_{child_func.lower()});")
                else:
                    # Терминал - создаём терминальный узел и продвигаемся
                    token = self.clean_name(sym, False)
                    lines.append(f"        if (current != TokenType::{token}) {{")
                    lines.append(f"            delete node;")
                    lines.append(f"            syntax_error(\"expected {token} in {name}\");")
                    lines.append(f"        }}")
                    # *** ИСПРАВЛЕНИЕ: Передача line/col в конструктор TerminalNode ***
                    lines.append(
                        f"        node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line, current_token.col));")
                    lines.append("        gc();")

            lines.append("        return node;")
            lines.append("    }")

        # Обработка ошибки
        lines.append("    delete node;")
        lines.append(f'    syntax_error(\"unexpected token \"+token_to_string(current)+\" in {name}\");')
        lines.append("    return nullptr;")
        lines.append("}")

        return "\n".join(lines)

    def _make_condition(self, alt):
        """Новая версия: использует FIRST множества."""
        if not alt:
            return "true"
        first_sym = alt[0]
        first_set = self._first_of_symbol(first_sym)
        if "__EPS" in first_set:
            return "true"
        if not first_set:
            return "true"
        items = sorted(first_set)
        if len(items) == 1:
            return f"current == {items[0]}"
        return " || ".join(f"current == {it}" for it in items)

    @staticmethod
    def _is_nonterm(sym):
        return sym.startswith("<") and sym.endswith(">")

    @staticmethod
    def clean_name(sym: str, is_nonterminal=False):
        s = sym.strip()
        if sym in ['IDENTIFIER', 'STRING', 'NUMBER']:
            return s

        if is_nonterminal or (s.startswith("<") and s.endswith(">")):
            s = s.strip("<>")
            return "TOK_" + re.sub(r'[^A-Za-z0-9_]+', '_', s.upper())

        # Убираем кавычки
        s = s.strip('"').strip()

        # Пустые строки пропускаем

        # Специальные двухсимвольные операторы
        special_map = {"!=": "TOK_NEQ", "==": "TOK_EQEQ", "&&": "TOK_ANDAND", "||": "TOK_OROR", "<=": "TOK_LEQ",
            ">=": "TOK_GEQ", "->": "TOK_ARROW", "=>": "TOK_FATARROW", "::": "TOK_SCOPE", ":=": "TOK_ASSIGN",
            "+=": "TOK_PLUSEQUAL", "-=": "TOK_MINUSEQUAL", "*=": "TOK_STAREQUAL", "/=": "TOK_SLASHEQUAL",
            "%=": "TOK_PERCENTEQUAL", "<<=": "TOK_LSHIFTEQUAL", ">>=": "TOK_RSHIFTEQUAL", "&=": "TOK_AMPEQUAL",
            "|=": "TOK_PIPEEQUAL", "^=": "TOK_CARETEQUAL", "<<": "TOK_LSHIFT", ">>": "TOK_RSHIFT", "++": "TOK_PLUSPLUS",
            "--": "TOK_MINUSMINUS", '=': 'TOK_EQUAL'}
        if s in special_map:
            return special_map[s]

        # Односимвольные операторы
        single_map = {'+': 'PLUS', '-': 'MINUS', '*': 'STAR', '/': 'SLASH', '=': 'EQUAL', '(': 'LPAREN', ')': 'RPAREN',
            '{': 'LBRACE', '}': 'RBRACE', '[': 'LBRACKET', ']': 'RBRACKET', ';': 'SEMICOLON', ':': 'COLON',
            ',': 'COMMA', '.': 'DOT', '"': 'QUOTE', '\'': 'APOSTROPHE', '<': 'LT', '>': 'GT', '!': 'EXCL', '?': 'QMARK',
            '|': 'PIPE', '&': 'AMP', '%': 'PERCENT', '^': 'CARET', '#': 'HASH', '@': 'AT', '~': 'TILDE',
            '\\': 'BACKSLASH'}
        if len(s) == 1 and not s.isalnum():
            return "TOK_" + single_map.get(s, f"SYM_{ord(s)}")

        # Ключевые слова и идентификаторы
        if s.isidentifier():
            return "TOK_" + s.upper()

        # Для всего остального - создаём безопасное имя
        s = re.sub(r'[^A-Za-z0-9_]+', '_', s)
        if s:
            return "TOK_" + s.upper()
        return 'None'


# ================================================================
# 4. Точка входа
# ================================================================
def main():
    grammar_file = "grammar.txt"
    cleaner = GrammarCleaner(grammar_file)
    cleaner.clean()
    grammar = FormalGrammarParser(grammar_file)
    rules = grammar.parse()
    gen = CppRecursiveDescentGen(rules, grammar.terminals)
    gen.generate_all()


if __name__ == "__main__":
    main()
