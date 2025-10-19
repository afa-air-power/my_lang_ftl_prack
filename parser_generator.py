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

    def generate_all(self):
        os.makedirs("out", exist_ok=True)
        self._write_reserved_list()
        self._write_keywords_hpp()
        self._write_parser_hpp()
        self._write_parser_cpp()
        print("✅ Генерация завершена: parser.cpp/hpp и keywords.hpp созданы.")

    # ---------- system_reserved_identifiers.txt ----------
    def _write_reserved_list(self):
        with open("out/system_reserved_identifiers.txt", "w", encoding="utf-8") as f:
            for t in self.terminals:
                f.write(f"{t}\n")

    # ---------- keywords.hpp ----------
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
            "enum class TokenType {"
            "    KEYWORD,",
            "    IDENTIFIER,",
            "    NUMBER,",
            "    STRING,",
            "    SYMBOL,",
            "",

        ]

        seen = set()
        for t in self.terminals:
            cname = self.clean_name(t, is_nonterminal=False)
            if cname in seen:
                continue
            seen.add(cname)
            lines.append(f"    {cname},")

        lines.append("    END_OF_FILE")
        lines.append("};\n")

        # Строковое представление
        lines.append("inline std::string token_to_string(TokenType t) {")
        lines.append("    switch(t) {")

        for t in self.terminals:
            cname = self.clean_name(t, is_nonterminal=False)
            escaped = t.replace('"', '\\"')
            lines.append(f'        case TokenType::{cname}: return "{escaped}";')

        lines.append('        case TokenType::END_OF_FILE: return "EOF";')
        lines.append("    } return \"?\"; }")
        lines.append("\n} // namespace parser")

        with open("out/keywords.hpp", "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    # ---------- parser.hpp ----------
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

    # ---------- parser.cpp ----------
    def _write_parser_cpp(self):
        header = dedent("""\
            #include "parser.hpp"
            #include <iostream>
            #include <vector>

            namespace parser {
            static lexer::Lexer* current_lexer = nullptr;
            TokenType current = TokenType::END_OF_FILE;

            void gc() {
                if (!current_lexer)
                    throw std::runtime_error("Lexer not initialized");
                current = current_lexer->next().type;
            }

            void parse(lexer::Lexer& lexer, const std::string& path) {
                current_lexer = &lexer;
                gc();
                std::cout << "Parsing file: " << path << std::endl;
                try {
                    TOK_PROGRAM();
                    std::cout << "✅ Parsing completed successfully." << std::endl;
                } catch (const ParseError& e) {
                    std::cerr << "❌ Parse failed: " << e.what() << std::endl;
                }
            }

            static std::vector<std::string> call_stack;

            struct CallContext {
                std::string name;
                CallContext(const std::string& n) : name(n) {
                    call_stack.push_back(n);
                }
                ~CallContext() { call_stack.pop_back(); }
            };

            static void syntax_error(const std::string& msg) {
                std::cerr << "❌ Syntax error: " << msg << "\\n";
                std::cerr << "Call stack:" << std::endl;
                for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it)
                    std::cerr << "  in <" << *it << ">" << std::endl;
                throw ParseError(msg);
            }
        """)

        functions = [self._gen_function(head) for head in self.rules]
        footer = "\n} // namespace parser\n"

        with open("out/parser.cpp", "w", encoding="utf-8") as f:
            f.write(header + "\n\n".join(functions) + footer)

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
                    lines.append(f"        {self.clean_name(sym, is_nonterminal=True)}();")
                else:
                    token = self.clean_name(sym, is_nonterminal=False)
                    lines.append(
                        f"        if (current != TokenType::{token}) "
                        f"syntax_error(\"expected {token} in {name}\");"
                    )
                    lines.append("        gc();")
            lines.append("        return; }")

        lines.append(f"    syntax_error(\"unexpected token in {name}\");")
        lines.append("}")
        return "\n".join(lines)

    def _make_condition(self, alt):
        for sym in alt:
            if not self._is_nonterm(sym):
                return f"current == TokenType::{self.clean_name(sym, is_nonterminal=False)}"
        return "true"

    @staticmethod
    def _is_nonterm(sym):
        return sym.startswith("<") and sym.endswith(">")

    # ---------- clean_name ----------
    @staticmethod
    def clean_name(sym, is_nonterminal=False):
        s = sym.strip()

        # если это нетерминал — правило грамматики
        if is_nonterminal or (s.startswith("<") and s.endswith(">")):
            s = s.strip("<>")
            return "TOK_" + re.sub(r'[^A-Za-z0-9_]+', '_', s.upper())

        # терминал
        s = s.strip('"')

        special_map = {
            "!=": "TOK_NEQ", "==": "TOK_EQEQ", "&&": "TOK_ANDAND", "||": "TOK_OROR",
            "<=": "TOK_LEQ", ">=": "TOK_GEQ", "->": "TOK_ARROW", "=>": "TOK_FATARROW",
            "::": "TOK_SCOPE", ":=": "TOK_ASSIGN"
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
            '@': 'AT', '$': 'DOLLAR', '~': 'TILDE', '\\': 'BACKSLASH'
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
