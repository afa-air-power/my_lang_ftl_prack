
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <iostream>

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

// ============================================================
// Бор / автомат Ахо-Корасика с хранением типа токена
// ============================================================
class AhoCorasick {
public:
    struct Node {
        std::unordered_map<char, int> next;
        int fail = 0;
        bool is_terminal = false;
        std::string word;
        parser::TokenType token_type = parser::TokenType::IDENTIFIER;
    };

    std::vector<Node> trie;

    AhoCorasick() { trie.emplace_back(); }

    void insert(const std::string& word, parser::TokenType t) {
        int v = 0;
        for (char ch : word) {
            if (!trie[v].next.count(ch)) {
                trie[v].next[ch] = (int)trie.size();
                trie.emplace_back();
            }
            v = trie[v].next[ch];
        }
        trie[v].is_terminal = true;
        trie[v].word = word;
        trie[v].token_type = t;
    }

    void build() {
        std::queue<int> q;
        for (auto& [ch, nxt] : trie[0].next)
            q.push(nxt);

        while (!q.empty()) {
            int v = q.front(); q.pop();
            for (auto& [ch, u] : trie[v].next) {
                int j = trie[v].fail;
                while (j && !trie[j].next.count(ch))
                    j = trie[j].fail;
                if (trie[j].next.count(ch))
                    j = trie[j].next[ch];
                trie[u].fail = j;
                if (trie[j].is_terminal && !trie[u].is_terminal) {
                    trie[u].is_terminal = true;
                    trie[u].word = trie[j].word;
                    trie[u].token_type = trie[j].token_type;
                }
                q.push(u);
            }
        }
    }

    bool match_exact(const std::string& word, parser::TokenType& out_type) const {
        int v = 0;
        for (char ch : word) {
            while (v && !trie[v].next.count(ch))
                v = trie[v].fail;
            if (trie[v].next.count(ch))
                v = trie[v].next.at(ch);
        }
        int j = v;
        while (j) {
            if (trie[j].is_terminal && trie[j].word == word) {
                out_type = trie[j].token_type;
                return true;
            }
            j = trie[j].fail;
        }
        return false;
    }

    void debug_print() const {
        std::cout << "🧭 Keyword trie built (" << trie.size() << " nodes):\\n";
        for (size_t i = 0; i < trie.size(); ++i) {
            const auto& n = trie[i];
            if (n.is_terminal) {
                std::cout << "   • [" << n.word << "] → "
                          << static_cast<int>(n.token_type) << std::endl;
            }
        }
    }
};

// ============================================================
// Лексер
// ============================================================
class Lexer {
public:
    Lexer(const std::string& text, const std::string& keywords_file)
        : source(text), pos(0), line(1), col(1)
    {
        load_keywords(keywords_file);
    }

    Token next() {
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

    std::vector<Token> tokenize() {
        tokens.clear();
        while (true) {
            Token t = next();
            tokens.push_back(t);
            if (t.type == parser::TokenType::END_OF_FILE)
                break;
        }
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
        if (!in.is_open()) {
            std::cerr << "⚠️ Could not open keyword file: " << file << std::endl;
            return;
        }

        std::string kw;
        while (std::getline(in, kw)) {
            if (!kw.empty()) {
                std::cout << "📘 Loading keyword: [" << kw << "]" << std::endl;
                parser::TokenType t = parser::TokenType::IDENTIFIER;
                if (kw == "int") t = parser::TokenType::TOK_INT;
                else if (kw == "float") t = parser::TokenType::TOK_FLOAT;
                else if (kw == "double") t = parser::TokenType::TOK_DOUBLE;
                else if (kw == "if") t = parser::TokenType::TOK_IF;
                else if (kw == "else") t = parser::TokenType::TOK_ELSE;
                else if (kw == "while") t = parser::TokenType::TOK_WHILE;
                else if (kw == "return") t = parser::TokenType::TOK_RETURN;
                else if (kw == "class") t = parser::TokenType::TOK_CLASS;
                automaton.insert(kw, t);
            }
        }
        automaton.build();
        automaton.debug_print();
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
        while (std::isalnum(peek_char()) || peek_char() == '_')
            get_char();

        std::string word = source.substr(start, pos - start);
        parser::TokenType t = parser::TokenType::IDENTIFIER;
        if (automaton.match_exact(word, t)) {
            std::cout << "🔹 Matched keyword: [" << word << "] → "
                      << static_cast<int>(t) << std::endl;
            return Token(t, word, word, line, start_col);
        }

        std::cout << "🟡 Identifier: [" << word << "]" << std::endl;
        return Token(parser::TokenType::IDENTIFIER, word, word, line, start_col);
    }

    Token read_number() {
        size_t start = pos, start_col = col;
        bool has_dot = false;

        while (std::isdigit(peek_char()) || (!has_dot && peek_char() == '.')) {
            if (peek_char() == '.') has_dot = true;
            get_char();
        }

        std::string val = source.substr(start, pos - start);
        std::cout << "🔢 Number: [" << val << "]" << std::endl;
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
        std::cout << "💬 String: [" << val << "]" << std::endl;
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
        std::cout << "⚙️ Symbol: [" << sym << "]" << std::endl;
        return Token(parser::TokenType::SYMBOL, sym, sym, line, start_col);
    }
};

} // namespace lexer
