#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Процедурный генератор рекурсивного спуска по формальной грамматике.
Создаёт:
  - out/parser.hpp
  - out/parser.cpp
  - out/keywords.hpp
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

        print(f"🧹 Очистка завершена. Сохранена резервная копия: {backup}")


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
            return {f"TokenType::{token_name}"}

    # ================================================================
    # Генерация файлов
    # ================================================================
    def generate_all(self):
        os.makedirs("out", exist_ok=True)
        self._write_reserved_list()
        self._write_keywords_hpp()
        self._write_parser_hpp()
        self._write_parser_cpp()
        print("✅ Генерация завершена: parser.cpp/hpp и keywords.hpp созданы.")

    def _write_reserved_list(self):
        with open("out/system_reserved_identifiers.txt", "w", encoding="utf-8") as f:
            for t in self.terminals:
                # Пропускаем базовые типы токенов - они уже есть в TokenType
                if t in ["IDENTIFIER", "NUMBER", "STRING"]:
                    continue
                f.write(f"{t.strip('"')}\n")

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

        seen = set()
        for t in self.terminals:
            # Пропускаем базовые типы - они уже добавлены выше
            if t in ["IDENTIFIER", "NUMBER", "STRING"]:
                continue
            cname = self.clean_name(t, is_nonterminal=False)
            if cname in seen:
                continue
            seen.add(cname)
            lines.append(f"    {cname},")

        lines.append("    END_OF_FILE")
        lines.append("};\n")

        lines.append("inline std::string token_to_string(TokenType t) {")
        lines.append("    switch(t) {")
        lines.append('        case TokenType::KEYWORD: return "KEYWORD";')
        lines.append('        case TokenType::IDENTIFIER: return "IDENTIFIER";')
        lines.append('        case TokenType::NUMBER: return "NUMBER";')
        lines.append('        case TokenType::STRING: return "STRING";')
        lines.append('        case TokenType::SYMBOL: return "SYMBOL";')

        for t in self.terminals:
            if t in ["IDENTIFIER", "NUMBER", "STRING"]:
                continue
            cname = self.clean_name(t, is_nonterminal=False)
            lines.append(f'        case TokenType::{cname}: return "{cname}";')

        lines.append('        case TokenType::END_OF_FILE: return "EOF";')
        lines.append("    } return \"?\"; }")
        lines.append("\n} // namespace parser")

        with open("out/keywords.hpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    def _write_parser_hpp(self):
        lines = [
            "#pragma once",
            "#include <stdexcept>",
            "#include <string>",
            "#include \"keywords.hpp\"",
            "#include \"lexer.hpp\"",
            "",
            "namespace parser {",
            "",
            "struct ParseError : public std::runtime_error {",
            "    using std::runtime_error::runtime_error;",
            "};",
            "",
            "extern TokenType current;",
            "void gc();",
            "void parse(lexer::Lexer& lexer, const std::string& path);",
            ""
        ]

        for head in self.rules:
            lines.append(f"void {self.clean_name(head, is_nonterminal=True)}();")

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

            static std::vector<std::string> call_stack;

            static void debug_token(const std::string& where) {
                std::cout << "🔎 [" << where << "] current token: "
                          << token_to_string(current) << std::endl;
            }

            void gc() {
                if (!current_lexer)
                    throw std::runtime_error("Lexer not initialized");
                current = current_lexer->next().type;
                std::cout << "➡️ gc(): now current = " << token_to_string(current) << std::endl;
            }

           void parse(lexer::Lexer& lexer, const std::string& path) {
    current_lexer = &lexer;
    gc();
    
    std::cout << "📘 Parsing file: " << path << std::endl;
    try {
        std::ofstream out("lexer_info.txt");
        if (out.is_open()) {
            out << "LEXER DUMP (on parse error)\\n";
            out << "=============================\\n";
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
            std::cerr << "📝 Lexer dump written to lexer_info.txt\\n";
        }
        TOK_PROGRAM();
        std::cout << "\\n Parsing completed successfully.\\n";
    } catch (const ParseError& e) {
        std::cerr << "\\n Parse failed: " << e.what() << std::endl;
        std::cerr << "Call stack (on error):\\n";
        for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it)
            std::cerr << "  • " << *it << std::endl;

        std::ofstream out("lexer_info.txt");
        if (out.is_open()) {
            out << "LEXER DUMP (on parse error)\\n";
            out << "=============================\\n";
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
            std::cerr << " Lexer dump written to lexer_info.txt\\n";
        }
    }
}
            

            struct CallContext {
                std::string name;
                CallContext(const std::string& n) : name(n) {
                    std::string information_=token_to_string(current);
                    call_stack.push_back(n );
                    std::cout << "\\n➡️ Enter <" << n << ">" << std::endl;
                    debug_token(n);
                }
                ~CallContext() {
                    std::cout << "⬅️ Leave <" << name << ">\\n";
                    call_stack.pop_back();
                }
            };

            static void syntax_error(const std::string& msg) {
                std::cerr << "\\n❌ Syntax error: " << msg << "\\n";
                std::cerr << "Call stack:" << std::endl;
                for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it)
                    std::cerr << "  in <" << *it << ">" << std::endl;
                throw ParseError(msg);
            }
        """)

        # --- функции ---
        functions = [self._gen_function(head) for head in self.rules]

        # --- футер ---
        footer = "\n} // namespace parser\n"

        with open("out/parser.cpp", "w", encoding="utf-8") as f:
            f.write(header + "\n\n".join(functions) + footer)

    # ================================================================
    # Основная генерация функций
    # ================================================================
    def _gen_function(self, head):
        name = self.clean_name(head, is_nonterminal=True)
        lines = [f"void {name}() {{", f"    CallContext ctx(\"{name}\");"]
        alts = self.rules[head]

        for i, alt in enumerate(alts):
            prefix = "if" if i == 0 else "else if"
            if not alt:
                lines.append(f"    {prefix} (true) {{ /* ε */ return; }}")
                continue

            cond = self._make_condition(alt)
            lines.append(f"    {prefix} ({cond}) {{")
            for sym in alt:
                if self._is_nonterm(sym):
                    lines.append(f"        {self.clean_name(sym, True)}();")
                else:
                    token = self.clean_name(sym, False)

                    lines.append(f"        if (current != TokenType::{token}) "
                                 f"syntax_error(\"expected {token} in {name}\");")
                    lines.append("        gc();")
            lines.append("        return; }")

        lines.append(f'    syntax_error(\"unexpected token "+token_to_string(current)+"  in {name}\");')
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
    def clean_name(sym, is_nonterminal=False):
        s = sym.strip()
        if sym in ['IDENTIFIER','STRING','NUMBER']: return s
        if is_nonterminal or (s.startswith("<") and s.endswith(">")):
            s = s.strip("<>")
            return "TOK_" + re.sub(r'[^A-Za-z0-9_]+', '_', s.upper())

        s = s.strip('"')
        special_map = {
            "!=": "TOK_NEQ", "==": "TOK_EQEQ", "&&": "TOK_ANDAND", "||": "TOK_OROR",
            "<=": "TOK_LEQ", ">=": "TOK_GEQ", "->": "TOK_ARROW", "=>": "TOK_FATARROW",
            "::": "TOK_SCOPE", ":=": "TOK_ASSIGN", "-=": "TOK_MINUSEQUAL",
            "*=": "TOK_STAREQUAL", "/=": "TOK_SLASHEQUAL"
        }
        if s in special_map:
            return special_map[s]

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

        if not s.isidentifier():
            code = "_".join(str(ord(ch)) for ch in s)
            return f"TOK_REGEX_{code}"

        s = re.sub(r'[^A-Za-z0-9_]+', '_', s)
        return "TOK_" + s.upper()


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
