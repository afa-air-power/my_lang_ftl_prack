#pragma once
#include <stdexcept>
#include <string>
#include "keywords.hpp"
#include "lexer.hpp"
#include "ast.hpp"

namespace parser {

struct ParseError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

extern TokenType current;
extern lexer::Token current_token;
// *** НОВОЕ: Добавляем 'peek' токен для заглядывания ***
extern TokenType peek;
extern lexer::Token peek_token;

void gc();
ast::AstNode* parse(lexer::Lexer& lexer, const std::string& path);

ast::AstNode* TOK_PROGRAM();
ast::AstNode* TOK_ID();
ast::AstNode* TOK_IDREST();
ast::AstNode* TOK_DECLARATIONLIST();
ast::AstNode* TOK_DECLARATION();
ast::AstNode* TOK_DECLSUFFIX();
ast::AstNode* TOK_CLASSDECL();
ast::AstNode* TOK_MEMBERLIST();
ast::AstNode* TOK_MEMBER();
ast::AstNode* TOK_MEMBERSUFFIX();
ast::AstNode* TOK_VARDECLREST();
ast::AstNode* TOK_TYPE();
ast::AstNode* TOK_PARAMLIST();
ast::AstNode* TOK_PARAMLISTREST();
ast::AstNode* TOK_PARAM();
ast::AstNode* TOK_COMPOUNDSTMT();
ast::AstNode* TOK_STMTLIST();
ast::AstNode* TOK_STATEMENT();
ast::AstNode* TOK_FORSTMT();
ast::AstNode* TOK_AFTERSTMT();
ast::AstNode* TOK_LOOPSTMT();
ast::AstNode* TOK_LOCALVARDECL();
ast::AstNode* TOK_IFSTMT();
ast::AstNode* TOK_ELSEPART();
ast::AstNode* TOK_WHILESTMT();
ast::AstNode* TOK_RETURNSTMT();
ast::AstNode* TOK_EXPRESSIONSTMT();
ast::AstNode* TOK_EXPRESSION();
ast::AstNode* TOK_EXPR01();
ast::AstNode* TOK_EXPR01REST();
ast::AstNode* TOK_EXPR02();
ast::AstNode* TOK_EXPR02REST();
ast::AstNode* TOK_EQUAL();
ast::AstNode* TOK_ASSIGNOP();
ast::AstNode* TOK_EXPR03();
ast::AstNode* TOK_EXPR03REST();
ast::AstNode* TOK_EXPR04();
ast::AstNode* TOK_EXPR04REST();
ast::AstNode* TOK_EXPR05();
ast::AstNode* TOK_EXPR05REST();
ast::AstNode* TOK_EXPR06();
ast::AstNode* TOK_EXPR06REST();
ast::AstNode* TOK_EXPR07();
ast::AstNode* TOK_EXPR07REST();
ast::AstNode* TOK_EXPR08();
ast::AstNode* TOK_EXPR08REST();
ast::AstNode* TOK_EXPR09();
ast::AstNode* TOK_EXPR09REST();
ast::AstNode* TOK_EXPR10();
ast::AstNode* TOK_EXPR10REST();
ast::AstNode* TOK_EXPR11();
ast::AstNode* TOK_EXPR11REST();
ast::AstNode* TOK_EXPR12();
ast::AstNode* TOK_EXPR12REST();
ast::AstNode* TOK_EXPR13();
ast::AstNode* TOK_EXPR13REST();
ast::AstNode* TOK_EXPR14();
ast::AstNode* TOK_UNARYOP();
ast::AstNode* TOK_EXPR15();
ast::AstNode* TOK_EXPR15REST();
ast::AstNode* TOK_EXPR16();
ast::AstNode* TOK_ATOM();
ast::AstNode* TOK_POSTFIXTAIL();
ast::AstNode* TOK_POSTFIX_ITEM();
ast::AstNode* TOK_INO();
ast::AstNode* TOK_ARGLIST();
ast::AstNode* TOK_ARGLISTREST();
ast::AstNode* TOK_LITERAL();

} // namespace parser