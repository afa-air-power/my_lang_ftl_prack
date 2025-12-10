#pragma once
#include <string>

namespace parser {
    // =============================================================
    // Автоматически сгенерированные типы токенов
    // =============================================================

    enum class TokenType {
        KEYWORD,
        IDENTIFIER,
        NUMBER,
        STRING,
        SYMBOL,

        None,
        TOK_AFTER,
        TOK_AMP,
        TOK_AMPEQUAL,
        TOK_ANDAND,
        TOK_CARET,
        TOK_CARETEQUAL,
        TOK_CLASS,
        TOK_COLON,
        TOK_COMMA,
        TOK_DOT,
        TOK_DOUBLE,
        TOK_ELSE,
        TOK_EQEQ,
        TOK_EQUAL,
        TOK_EXCL,
        TOK_FLOAT,
        TOK_FOR,
        TOK_GEQ,
        TOK_GT,
        TOK_IF,
        TOK_INPUT,
        TOK_INT,
        TOK_LBRACE,
        TOK_LBRACKET,
        TOK_LEQ,
        TOK_LPAREN,
        TOK_LSHIFT,
        TOK_LSHIFTEQUAL,
        TOK_LT,
        TOK_MINUS,
        TOK_MINUSEQUAL,
        TOK_MINUSMINUS,
        TOK_NEQ,
        TOK_OR,
        TOK_PERCENT,
        TOK_PERCENTEQUAL,
        TOK_PLUS,
        TOK_PLUSEQUAL,
        TOK_PLUSPLUS,
        TOK_PRINT,
        TOK_QMARK,
        TOK_RBRACE,
        TOK_RBRACKET,
        TOK_RETURN,
        TOK_RPAREN,
        TOK_RSHIFT,
        TOK_RSHIFTEQUAL,
        TOK_SEMICOLON,
        TOK_SLASH,
        TOK_SLASHEQUAL,
        TOK_STAR,
        TOK_STAREQUAL,
        TOK_STRING,
        TOK_VECTOR,
        TOK_WHILE,
        END_OF_FILE
    };

    inline std::string token_to_string(TokenType t) {
        switch (t) {
            case TokenType::KEYWORD: return "KEYWORD";
            case TokenType::IDENTIFIER: return "IDENTIFIER";
            case TokenType::NUMBER: return "NUMBER";
            case TokenType::STRING: return "STRING";
            case TokenType::SYMBOL: return "SYMBOL";
            case TokenType::None: return "None";
            case TokenType::TOK_AFTER: return "TOK_AFTER";
            case TokenType::TOK_AMP: return "TOK_AMP";
            case TokenType::TOK_AMPEQUAL: return "TOK_AMPEQUAL";
            case TokenType::TOK_ANDAND: return "TOK_ANDAND";
            case TokenType::TOK_CARET: return "TOK_CARET";
            case TokenType::TOK_CARETEQUAL: return "TOK_CARETEQUAL";
            case TokenType::TOK_CLASS: return "TOK_CLASS";
            case TokenType::TOK_COLON: return "TOK_COLON";
            case TokenType::TOK_COMMA: return "TOK_COMMA";
            case TokenType::TOK_DOT: return "TOK_DOT";
            case TokenType::TOK_DOUBLE: return "TOK_DOUBLE";
            case TokenType::TOK_ELSE: return "TOK_ELSE";
            case TokenType::TOK_EQEQ: return "TOK_EQEQ";
            case TokenType::TOK_EQUAL: return "TOK_EQUAL";
            case TokenType::TOK_EXCL: return "TOK_EXCL";
            case TokenType::TOK_FLOAT: return "TOK_FLOAT";
            case TokenType::TOK_FOR: return "TOK_FOR";
            case TokenType::TOK_GEQ: return "TOK_GEQ";
            case TokenType::TOK_GT: return "TOK_GT";
            case TokenType::TOK_IF: return "TOK_IF";
            case TokenType::TOK_INPUT: return "TOK_INPUT";
            case TokenType::TOK_INT: return "TOK_INT";
            case TokenType::TOK_LBRACE: return "TOK_LBRACE";
            case TokenType::TOK_LBRACKET: return "TOK_LBRACKET";
            case TokenType::TOK_LEQ: return "TOK_LEQ";
            case TokenType::TOK_LPAREN: return "TOK_LPAREN";
            case TokenType::TOK_LSHIFT: return "TOK_LSHIFT";
            case TokenType::TOK_LSHIFTEQUAL: return "TOK_LSHIFTEQUAL";
            case TokenType::TOK_LT: return "TOK_LT";
            case TokenType::TOK_MINUS: return "TOK_MINUS";
            case TokenType::TOK_MINUSEQUAL: return "TOK_MINUSEQUAL";
            case TokenType::TOK_MINUSMINUS: return "TOK_MINUSMINUS";
            case TokenType::TOK_NEQ: return "TOK_NEQ";
            case TokenType::TOK_OR: return "TOK_OR";
            case TokenType::TOK_PERCENT: return "TOK_PERCENT";
            case TokenType::TOK_PERCENTEQUAL: return "TOK_PERCENTEQUAL";
            case TokenType::TOK_PLUS: return "TOK_PLUS";
            case TokenType::TOK_PLUSEQUAL: return "TOK_PLUSEQUAL";
            case TokenType::TOK_PLUSPLUS: return "TOK_PLUSPLUS";
            case TokenType::TOK_PRINT: return "TOK_PRINT";
            case TokenType::TOK_QMARK: return "TOK_QMARK";
            case TokenType::TOK_RBRACE: return "TOK_RBRACE";
            case TokenType::TOK_RBRACKET: return "TOK_RBRACKET";
            case TokenType::TOK_RETURN: return "TOK_RETURN";
            case TokenType::TOK_RPAREN: return "TOK_RPAREN";
            case TokenType::TOK_RSHIFT: return "TOK_RSHIFT";
            case TokenType::TOK_RSHIFTEQUAL: return "TOK_RSHIFTEQUAL";
            case TokenType::TOK_SEMICOLON: return "TOK_SEMICOLON";
            case TokenType::TOK_SLASH: return "TOK_SLASH";
            case TokenType::TOK_SLASHEQUAL: return "TOK_SLASHEQUAL";
            case TokenType::TOK_STAR: return "TOK_STAR";
            case TokenType::TOK_STAREQUAL: return "TOK_STAREQUAL";
            case TokenType::TOK_STRING: return "TOK_STRING";
            case TokenType::TOK_VECTOR: return "TOK_VECTOR";
            case TokenType::TOK_WHILE: return "TOK_WHILE";
            case TokenType::END_OF_FILE: return "EOF";
        }
        return "?";
    }
} // namespace parser
