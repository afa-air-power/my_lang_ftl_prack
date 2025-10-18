#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Процедурный генератор C++ лексера
Создаёт out/lexer.hpp, совместимый с parser::TokenType
"""

import os
from textwrap import dedent

OUT_DIR = "out"
OUT_FILE = os.path.join(OUT_DIR, "lexer.hpp")

def ensure_out_dir():
    os.makedirs(OUT_DIR, exist_ok=True)


def generate_lexer():
    header = dedent(r'''
    #pragma once
    #include <string>
    #include <vector>
    #include <unordered_map>
    #include <queue>
    #include <fstream>
    #include <sstream>
    #include <cctype>
    #include <algorithm>

    #include "keywords.hpp"

    namespace lexer {

    struct Token {
        parser::TokenType type;
        std::string name;
        std::string value;
        size_t line;
        size_t col;
        Token(parser::TokenType t, std::string n, std::string v, size_t l, size_t c)
            : type(t), name(std::move(n)), value(std::move(v)), line(l), col(c) {}
    };

    class AhoCorasick {
    public:
        struct Node {
            std::unordered_map<char, int> next;
            int fail = 0;
            std::string out;
            bool is_terminal = false;
        };

        std::vector<Node> trie;

        AhoCorasick() { trie.emplace_back(); }

        void insert(const std::string& word) {
            int v = 0;
            for (char ch : word) {
                if (!trie[v].next.count(ch)) {
                    trie[v].next[ch] = (int)trie.size();
                    trie.emplace_back();
                }
                v = trie[v].next[ch];
            }
            trie[v].is_terminal = true;
            trie[v].out = word;
        }

        void build() {
            std::queue<int> q;
            for (auto& p : trie[0].next)
                q.push(p.second);

            while (!q.empty()) {
                int v = q.front(); q.pop();
                for (auto& p : trie[v].next) {
                    char ch = p.first;
                    int u = p.second;
                    int j = trie[v].fail;
                    while (j && !trie[j].next.count(ch))
                        j = trie[j].fail;
                    if (trie[j].next.count(ch))
                        j = trie[j].next[ch];
                    trie[u].fail = j;
                    if (trie[j].is_terminal && trie[u].out.empty())
                        trie[u].out = trie[j].out;
                    q.push(u);
                }
            }
        }

        bool is_keyword(const std::string& word) const {
            int v = 0;
            for (char ch : word) {
                while (v && !trie[v].next.count(ch))
                    v = trie[v].fail;
                if (trie[v].next.count(ch))
                    v = trie[v].next.at(ch);
            }
            int j = v;
            while (j) {
                if (trie[j].is_terminal && trie[j].out == word)
                    return true;
                j = trie[j].fail;
            }
            return false;
        }
    };

    class Lexer {
    public:
        Lexer(const std::string& text, const std::string& keywords_file)
            : source(text), pos(0), line(1), col(1)
        {
            load_keywords(keywords_file);
        }

        Token next() {
            if (pos >= source.size())
                return Token(parser::TokenType::END_OF_FILE, "EOF", "", line, col);

            skip_ws_comments();

            if (pos >= source.size())
                return Token(parser::TokenType::END_OF_FILE, "EOF", "", line, col);

            char ch = peek_char();

            if (std::isalpha(ch) || ch == '_')
                return read_identifier_or_keyword();
            if (std::isdigit(ch))
                return read_number();
            if (ch == '"' || ch == '\'')
                return read_string();
            return read_symbol();
        }

        Token peek() const {
            if (pos < tokens.size())
                return tokens[pos];
            return Token(parser::TokenType::END_OF_FILE, "EOF", "", line, col);
        }

        std::vector<Token> tokenize() {
            tokens.clear();
            while (pos < source.size()) {
                Token t = next();
                if (t.type == parser::TokenType::END_OF_FILE) break;
                tokens.push_back(t);
            }
            tokens.push_back(Token(parser::TokenType::END_OF_FILE, "EOF", "", line, col));
            return tokens;
        }

    private:
        std::string source;
        std::vector<Token> tokens;
        size_t pos;
        size_t line, col;
        AhoCorasick automaton;

        void load_keywords(const std::string& file) {
            std::ifstream in(file);
            std::string kw;
            while (std::getline(in, kw)) {
                if (!kw.empty())
                    automaton.insert(kw);
            }
            automaton.build();
        }

        char peek_char() const { return pos < source.size() ? source[pos] : '\0'; }

        char get_char() {
            char c = peek_char();
            if (c == '\n') { line++; col = 1; }
            else col++;
            pos++;
            return c;
        }

        void skip_ws_comments() {
            while (pos < source.size()) {
                if (std::isspace(peek_char())) { get_char(); continue; }
                if (peek_char() == '/' && pos + 1 < source.size()) {
                    if (source[pos + 1] == '/') {
                        while (pos < source.size() && get_char() != '\n');
                        continue;
                    } else if (source[pos + 1] == '*') {
                        pos += 2;
                        while (pos + 1 < source.size() &&
                              !(source[pos] == '*' && source[pos + 1] == '/'))
                            get_char();
                        pos += 2;
                        continue;
                    }
                }
                break;
            }
        }

        Token read_identifier_or_keyword() {
            size_t start = pos, start_col = col;
            while (std::isalnum(peek_char()) || peek_char() == '_') get_char();
            std::string word = source.substr(start, pos - start);
            parser::TokenType t = automaton.is_keyword(word)
                ? parser::TokenType::KEYWORD
                : parser::TokenType::IDENTIFIER;
            return Token(t, word, word, line, start_col);
        }

        Token read_number() {
            size_t start = pos, start_col = col;
            bool has_dot = false;
            while (std::isdigit(peek_char()) || (!has_dot && peek_char() == '.')) {
                if (peek_char() == '.') has_dot = true;
                get_char();
            }
            std::string val = source.substr(start, pos - start);
            return Token(parser::TokenType::NUMBER, val, val, line, start_col);
        }

        Token read_string() {
            char quote = get_char();
            size_t start_col = col;
            std::string val;
            while (pos < source.size() && peek_char() != quote) {
                if (peek_char() == '\\') val += get_char();
                val += get_char();
            }
            get_char();
            return Token(parser::TokenType::STRING, val, val, line, start_col);
        }

        Token read_symbol() {
            size_t start_col = col;
            std::string sym(1, get_char());
            if (pos < source.size()) {
                std::string two = sym + peek_char();
                static const std::vector<std::string> ops = {
                    "==","!=",">=","<=","&&","||","++","--","->"
                };
                if (std::find(ops.begin(), ops.end(), two) != ops.end()) {
                    get_char();
                    sym = two;
                }
            }
            return Token(parser::TokenType::SYMBOL, sym, sym, line, start_col);
        }
    };

    } // namespace lexer
    ''')

    ensure_out_dir()
    with open(OUT_FILE, "w", encoding="utf-8") as f:
        f.write(header)
    print(f"✅ Лексер успешно сгенерирован: {OUT_FILE}")


if __name__ == "__main__":
    generate_lexer()
