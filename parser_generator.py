#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Процедурный генератор рекурсивного спуска по формальной грамматике с поддержкой AST.
Создаёт:
  - out/parser.hpp
  - out/parser.cpp
  - out/keywords.hpp
  - out/ast.hpp
  - out/ast.cpp
  - out/system_reserved_identifiers.txt
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

        print(f" Очистка завершена. Сохранена резервная копия: {backup}")


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

        # ==== добавлено: вычисление FIRST множеств ====
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
        print("Генерация завершена: parser.cpp/hpp, ast.cpp/hpp и keywords.hpp созданы.")

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
        lines = [
            "#pragma once",
            "#include <string>",
            "",
            "namespace parser {",
            "",
            "// =============================================================",
            "// Автоматически сгенерированные типы токенов",
            "// =============================================================",
            "",
            "enum class TokenType {",
            "    KEYWORD,",
            "    IDENTIFIER,",
            "    NUMBER,",
            "    STRING,",
            "    SYMBOL,",
            "",
        ]

        # Собираем уникальные имена токенов
        seen = set()
        token_names = []

        for t in self.terminals:
            # Пропускаем базовые типы - они уже добавлены выше
            if t in ["IDENTIFIER", "NUMBER", "STRING"]:
                continue

            cname = self.clean_name(t, is_nonterminal=False)
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
        lines = [
            "#pragma once",
            "#include <string>",
            "#include <vector>",
            "#include <memory>",
            "#include \"keywords.hpp\"",
            "",
            "namespace ast {",
            "",
            "// =============================================================",
            "// Базовый класс для всех узлов AST",
            "// =============================================================",
            "",
            "enum class NodeType {",
            "    TERMINAL,",
        ]

        # Добавляем типы для всех нетерминалов
        for head in self.rules:
            node_name = self.clean_name(head, is_nonterminal=True)
            lines.append(f"    {node_name},")

        lines.extend([
            "};",
            "",
            "class AstNode {",
            "public:",
            "    NodeType type;",
            "    int line;",
            "    int col;",
            "    std::vector<AstNode*> children;",
            "",
            "    AstNode(NodeType t) : type(t), line(0), col(0) {}",
            "    virtual ~AstNode();",
            "    virtual void print(int depth = 0) const;",
            "    virtual std::string to_string() const;",
            "    ",
            "    void add_child(AstNode* child);",
            "};",
            "",
            "// =============================================================",
            "// Терминальный узел (токен)",
            "// =============================================================",
            "",
            "class TerminalNode : public AstNode {",
            "public:",
            "    parser::TokenType token_type;",
            "    std::string value;",
            "    std::string name;",
            "",
            "    TerminalNode(parser::TokenType tt, const std::string& val, const std::string& n);",
            "    void print(int depth = 0) const override;",
            "    std::string to_string() const override;",
            "};",
            "",
            "// =============================================================",
            "// Специализированные узлы для нетерминалов",
            "// =============================================================",
            "",
        ])

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
                count = symbol_counts[ptr_name]
                if count > 1:
                    # Если символ встречается несколько раз, создаем вектор
                    lines.append(f"    std::vector<AstNode*> {ptr_name}_list;")
                else:
                    lines.append(f"    AstNode* {ptr_name};")

            lines.append("")
            lines.append(f"    {class_name}();")
            lines.append(f"    std::string to_string() const override;")
            lines.append("};")
            lines.append("")

        lines.extend([
            "// =============================================================",
            "// Утилиты",
            "// =============================================================",
            "",
            "void delete_tree(AstNode* root);",
            "void print_tree(AstNode* root, int depth = 0);",
            "",
            "} // namespace ast",
        ])

        with open("out/ast.hpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    def _write_ast_cpp(self):
        """Генерация ast.cpp - реализация методов узлов AST."""
        lines = [
            "#include \"ast.hpp\"",
            "#include <iostream>",
            "#include <iomanip>",
            "",
            "namespace ast {",
            "",
            "// =============================================================",
            "// Базовый класс AstNode",
            "// =============================================================",
            "",
            "AstNode::~AstNode() {",
            "    for (auto* child : children) {",
            "        delete child;",
            "    }",
            "}",
            "",
            "void AstNode::add_child(AstNode* child) {",
            "    if (child) {",
            "        children.push_back(child);",
            "    }",
            "}",
            "",
            "void AstNode::print(int depth) const {",
            "    std::cout << std::string(depth * 2, ' ') << to_string() << std::endl;",
            "    for (const auto* child : children) {",
            "        if (child) {",
            "            child->print(depth + 1);",
            "        }",
            "    }",
            "}",
            "",
            "std::string AstNode::to_string() const {",
            "    return \"AstNode\";",
            "}",
            "",
            "// =============================================================",
            "// Терминальный узел",
            "// =============================================================",
            "",
            "TerminalNode::TerminalNode(parser::TokenType tt, const std::string& val, const std::string& n)",
            "    : AstNode(NodeType::TERMINAL), token_type(tt), value(val), name(n) {}",
            "",
            "void TerminalNode::print(int depth) const {",
            "    std::cout << std::string(depth * 2, ' ')",
            "              << \"Terminal: \" << parser::token_to_string(token_type)",
            "              << \" = '\" << value << \"'\" << std::endl;",
            "}",
            "",
            "std::string TerminalNode::to_string() const {",
            "    return \"Terminal(\" + parser::token_to_string(token_type) + \": '\" + value + \"')\";",
            "}",
            "",
            "// =============================================================",
            "// Специализированные узлы",
            "// =============================================================",
            "",
        ]

        # Генерируем реализации для каждого нетерминала
        for head in self.rules:
            node_name = self.clean_name(head, is_nonterminal=True)
            class_name = node_name + "Node"

            lines.extend([
                f"{class_name}::{class_name}() : AstNode(NodeType::{node_name}) {{}}",
                "",
                f"std::string {class_name}::to_string() const {{",
                f"    return \"{node_name}\";",
                "}",
                "",
            ])

        lines.extend([
            "// =============================================================",
            "// Утилиты",
            "// =============================================================",
            "",
            "void delete_tree(AstNode* root) {",
            "    delete root;",
            "}",
            "",
            "void print_tree(AstNode* root, int depth) {",
            "    if (root) {",
            "        root->print(depth);",
            "    }",
            "}",
            "",
            "} // namespace ast",
        ])

        with open("out/ast.cpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    def _write_parser_hpp(self):
        lines = [
            "#pragma once",
            "#include <stdexcept>",
            "#include <string>",
            "#include \"keywords.hpp\"",
            "#include \"lexer.hpp\"",
            "#include \"ast.hpp\"",
            "",
            "namespace parser {",
            "",
            "struct ParseError : public std::runtime_error {",
            "    using std::runtime_error::runtime_error;",
            "};",
            "",
            "extern TokenType current;",
            "extern lexer::Token current_token;",
            "void gc();",
            "ast::AstNode* parse(lexer::Lexer& lexer, const std::string& path);",
            ""
        ]

        for head in self.rules:
            func_name = self.clean_name(head, is_nonterminal=True)
            lines.append(f"ast::AstNode* {func_name}();")

        lines.append("\n} // namespace parser")

        with open("out/parser.hpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    def _write_parser_cpp(self):
        header = dedent("""\
            #include "parser.hpp"
            #include <iostream>
            #include <vector>
            #include <fstream>

            namespace parser {
            static lexer::Lexer* current_lexer = nullptr;
            TokenType current = TokenType::END_OF_FILE;
            lexer::Token current_token(TokenType::END_OF_FILE, "", "", 0, 0);
            static std::string current_file_path;

            static std::vector<std::string> call_stack;

            void gc() {
                if (!current_lexer)
                    throw std::runtime_error("Lexer not initialized");
                current_token = current_lexer->next();
                current = current_token.type;
            }

            ast::AstNode* parse(lexer::Lexer& lexer, const std::string& path) {
                current_lexer = &lexer;
                current_file_path = path;
                gc();
                
                std::cout << "Parsing file: " << path << std::endl;
                ast::AstNode* root = nullptr;
                try {
                    root = TOK_PROGRAM();
                    std::cout << "\\nParsing completed successfully.\\n";
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
        functions = [self._gen_function(head) for head in self.rules]

        # --- футер ---
        footer = "\n} // namespace parser\n"

        with open("out/parser.cpp", "w", encoding="utf-8") as f:
            f.write(header + "\n\n".join(functions) + footer)

    # ================================================================
    # Основная генерация функций (с AST)
    # ================================================================
    def _gen_function(self, head):
        name = self.clean_name(head, is_nonterminal=True)
        node_class = name + "Node"

        lines = [
            f"ast::AstNode* {name}() {{",
            f"    CallContext ctx(\"{name}\");",
            f"    ast::{node_class}* node = new ast::{node_class}();",
            f"    node->line = current_token.line;",
            f"    node->col = current_token.col;",
            ""
        ]

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
                    lines.append(f"        node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name));")
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
    def clean_name(sym:str, is_nonterminal=False):
        s = sym.strip()
        if sym in ['IDENTIFIER','STRING','NUMBER']:
            return s

        if is_nonterminal or (s.startswith("<") and s.endswith(">")):
            s = s.strip("<>")
            return "TOK_" + re.sub(r'[^A-Za-z0-9_]+', '_', s.upper())

        # Убираем кавычки
        s = s.strip('"').strip()

        # Пустые строки пропускаем


        # Специальные двухсимвольные операторы
        special_map = {
            "!=": "TOK_NEQ", "==": "TOK_EQEQ", "&&": "TOK_ANDAND", "||": "TOK_OROR",
            "<=": "TOK_LEQ", ">=": "TOK_GEQ", "->": "TOK_ARROW", "=>": "TOK_FATARROW",
            "::": "TOK_SCOPE", ":=": "TOK_ASSIGN", "+=": "TOK_PLUSEQUAL",
            "-=": "TOK_MINUSEQUAL", "*=": "TOK_STAREQUAL", "/=": "TOK_SLASHEQUAL",
            "%=": "TOK_PERCENTEQUAL", "<<=": "TOK_LSHIFTEQUAL", ">>=": "TOK_RSHIFTEQUAL",
            "&=": "TOK_AMPEQUAL", "|=": "TOK_PIPEEQUAL", "^=": "TOK_CARETEQUAL",
            "<<": "TOK_LSHIFT", ">>": "TOK_RSHIFT", "++": "TOK_PLUSPLUS", "--": "TOK_MINUSMINUS",
            '=': 'TOK_EQUAL'
        }
        if s in special_map:
            return special_map[s]

        # Односимвольные операторы
        single_map = {
            '+': 'PLUS', '-': 'MINUS', '*': 'STAR', '/': 'SLASH', '=': 'EQUAL',
            '(': 'LPAREN', ')': 'RPAREN', '{': 'LBRACE', '}': 'RBRACE',
            '[': 'LBRACKET', ']': 'RBRACKET', ';': 'SEMICOLON', ':': 'COLON',
            ',': 'COMMA', '.': 'DOT', '"': 'QUOTE', '\'': 'APOSTROPHE',
            '<': 'LT', '>': 'GT', '!': 'EXCL', '?': 'QMARK', '|': 'PIPE',
            '&': 'AMP', '%': 'PERCENT', '^': 'CARET', '#': 'HASH',
            '@': 'AT', '~': 'TILDE', '\\': 'BACKSLASH'
        }
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