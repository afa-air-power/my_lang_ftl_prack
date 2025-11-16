#pragma once
#include <string>
#include <vector>
#include <memory>
#include "keywords.hpp"

namespace ast {

// =============================================================
// Базовый класс для всех узлов AST
// =============================================================

enum class NodeType {
    TERMINAL,
    TOK_PROGRAM,
    TOK_ID,
    TOK_IDREST,
    TOK_DECLARATIONLIST,
    TOK_DECLARATION,
    TOK_DECLSUFFIX,
    TOK_CLASSDECL,
    TOK_MEMBERLIST,
    TOK_MEMBER,
    TOK_MEMBERSUFFIX,
    TOK_VARDECLREST,
    TOK_TYPE,
    TOK_PARAMLIST,
    TOK_PARAMLISTREST,
    TOK_PARAM,
    TOK_COMPOUNDSTMT,
    TOK_STMTLIST,
    TOK_STATEMENT,
    TOK_FORSTMT,
    TOK_LOCALVARDECL,
    TOK_IFSTMT,
    TOK_ELSEPART,
    TOK_WHILESTMT,
    TOK_RETURNSTMT,
    TOK_EXPRESSIONSTMT,
    TOK_EXPRESSION,
    TOK_EXPR01,
    TOK_EXPR01REST,
    TOK_EXPR02,
    TOK_EXPR02REST,
    TOK_EQUAL,
    TOK_ASSIGNOP,
    TOK_EXPR03,
    TOK_EXPR03REST,
    TOK_EXPR04,
    TOK_EXPR04REST,
    TOK_EXPR05,
    TOK_EXPR05REST,
    TOK_EXPR06,
    TOK_EXPR06REST,
    TOK_EXPR07,
    TOK_EXPR07REST,
    TOK_EXPR08,
    TOK_EXPR08REST,
    TOK_EXPR09,
    TOK_EXPR09REST,
    TOK_EXPR10,
    TOK_EXPR10REST,
    TOK_EXPR11,
    TOK_EXPR11REST,
    TOK_EXPR12,
    TOK_EXPR12REST,
    TOK_EXPR13,
    TOK_EXPR13REST,
    TOK_EXPR14,
    TOK_UNARYOP,
    TOK_EXPR15,
    TOK_EXPR15REST,
    TOK_EXPR16,
    TOK_ATOM,
    TOK_POSTFIXTAIL,
    TOK_POSTFIX_ITEM,
    TOK_INO,
    TOK_ARGLIST,
    TOK_ARGLISTREST,
    TOK_LITERAL,
};

class AstNode {
public:
    NodeType type;
    int line;
    int col;
    std::vector<AstNode*> children;

    AstNode(NodeType t) : type(t), line(0), col(0) {}
    virtual ~AstNode();
    virtual void print(int depth = 0) const;
    virtual std::string to_string() const;
    
    void add_child(AstNode* child);
};

// =============================================================
// Терминальный узел (токен)
// =============================================================

class TerminalNode : public AstNode {
public:
    parser::TokenType token_type;
    std::string value;
    std::string name;

    TerminalNode(parser::TokenType tt, const std::string& val, const std::string& n);
    void print(int depth = 0) const override;
    std::string to_string() const override;
};

// =============================================================
// Специализированные узлы для нетерминалов
// =============================================================

class TOK_PROGRAMNode : public AstNode {
public:
    AstNode* tok_declarationlist;

    TOK_PROGRAMNode();
    std::string to_string() const override;
};

class TOK_IDNode : public AstNode {
public:
    AstNode* identifier;
    AstNode* tok_idrest;

    TOK_IDNode();
    std::string to_string() const override;
};

class TOK_IDRESTNode : public AstNode {
public:
    AstNode* tok_dot;
    AstNode* tok_id;

    TOK_IDRESTNode();
    std::string to_string() const override;
};

class TOK_DECLARATIONLISTNode : public AstNode {
public:
    AstNode* tok_declaration;
    AstNode* tok_declarationlist;

    TOK_DECLARATIONLISTNode();
    std::string to_string() const override;
};

class TOK_DECLARATIONNode : public AstNode {
public:
    AstNode* tok_type;
    AstNode* tok_id;
    AstNode* tok_declsuffix;
    AstNode* tok_classdecl;

    TOK_DECLARATIONNode();
    std::string to_string() const override;
};

class TOK_DECLSUFFIXNode : public AstNode {
public:
    AstNode* tok_lparen;
    AstNode* tok_paramlist;
    AstNode* tok_rparen;
    AstNode* tok_compoundstmt;
    AstNode* tok_vardeclrest;
    AstNode* tok_semicolon;

    TOK_DECLSUFFIXNode();
    std::string to_string() const override;
};

class TOK_CLASSDECLNode : public AstNode {
public:
    AstNode* tok_class;
    AstNode* tok_id;
    AstNode* tok_lbrace;
    AstNode* tok_memberlist;
    AstNode* tok_rbrace;
    AstNode* tok_semicolon;

    TOK_CLASSDECLNode();
    std::string to_string() const override;
};

class TOK_MEMBERLISTNode : public AstNode {
public:
    AstNode* tok_member;
    AstNode* tok_memberlist;

    TOK_MEMBERLISTNode();
    std::string to_string() const override;
};

class TOK_MEMBERNode : public AstNode {
public:
    AstNode* tok_type;
    AstNode* tok_id;
    AstNode* tok_membersuffix;

    TOK_MEMBERNode();
    std::string to_string() const override;
};

class TOK_MEMBERSUFFIXNode : public AstNode {
public:
    AstNode* tok_lparen;
    AstNode* tok_paramlist;
    AstNode* tok_rparen;
    AstNode* tok_compoundstmt;
    AstNode* tok_semicolon;

    TOK_MEMBERSUFFIXNode();
    std::string to_string() const override;
};

class TOK_VARDECLRESTNode : public AstNode {
public:
    AstNode* tok_equal;
    AstNode* tok_expression;

    TOK_VARDECLRESTNode();
    std::string to_string() const override;
};

class TOK_TYPENode : public AstNode {
public:
    AstNode* tok_int;
    AstNode* tok_float;
    AstNode* tok_double;
    AstNode* tok_string;
    AstNode* tok_id;
    AstNode* tok_vector;
    AstNode* tok_lt;
    AstNode* tok_type;
    AstNode* tok_gt;

    TOK_TYPENode();
    std::string to_string() const override;
};

class TOK_PARAMLISTNode : public AstNode {
public:
    AstNode* tok_param;
    AstNode* tok_paramlistrest;

    TOK_PARAMLISTNode();
    std::string to_string() const override;
};

class TOK_PARAMLISTRESTNode : public AstNode {
public:
    AstNode* tok_comma;
    AstNode* tok_param;
    AstNode* tok_paramlistrest;

    TOK_PARAMLISTRESTNode();
    std::string to_string() const override;
};

class TOK_PARAMNode : public AstNode {
public:
    AstNode* tok_type;
    AstNode* tok_id;

    TOK_PARAMNode();
    std::string to_string() const override;
};

class TOK_COMPOUNDSTMTNode : public AstNode {
public:
    AstNode* tok_lbrace;
    AstNode* tok_stmtlist;
    AstNode* tok_rbrace;

    TOK_COMPOUNDSTMTNode();
    std::string to_string() const override;
};

class TOK_STMTLISTNode : public AstNode {
public:
    AstNode* tok_statement;
    AstNode* tok_stmtlist;

    TOK_STMTLISTNode();
    std::string to_string() const override;
};

class TOK_STATEMENTNode : public AstNode {
public:
    AstNode* tok_localvardecl;
    AstNode* tok_ifstmt;
    AstNode* tok_whilestmt;
    AstNode* tok_returnstmt;
    AstNode* tok_expressionstmt;
    AstNode* tok_compoundstmt;
    AstNode* tok_forstmt;

    TOK_STATEMENTNode();
    std::string to_string() const override;
};

class TOK_FORSTMTNode : public AstNode {
public:
    AstNode* tok_for;
    AstNode* tok_lparen;
    AstNode* tok_expression;
    AstNode* tok_rparen;
    AstNode* tok_statement;

    TOK_FORSTMTNode();
    std::string to_string() const override;
};

class TOK_LOCALVARDECLNode : public AstNode {
public:
    AstNode* tok_type;
    AstNode* tok_id;
    AstNode* tok_vardeclrest;
    AstNode* tok_semicolon;

    TOK_LOCALVARDECLNode();
    std::string to_string() const override;
};

class TOK_IFSTMTNode : public AstNode {
public:
    std::vector<AstNode*> tok_if_list;
    std::vector<AstNode*> tok_lparen_list;
    std::vector<AstNode*> tok__expression__list;
    std::vector<AstNode*> tok_expression_list;
    std::vector<AstNode*> tok_rparen_list;
    std::vector<AstNode*> tok_statement_list;
    AstNode* tok_elsepart;

    TOK_IFSTMTNode();
    std::string to_string() const override;
};

class TOK_ELSEPARTNode : public AstNode {
public:
    AstNode* tok_else;
    AstNode* tok_statement;

    TOK_ELSEPARTNode();
    std::string to_string() const override;
};

class TOK_WHILESTMTNode : public AstNode {
public:
    AstNode* tok_while;
    AstNode* tok_lparen;
    AstNode* tok_expression;
    AstNode* tok_rparen;
    AstNode* tok_statement;

    TOK_WHILESTMTNode();
    std::string to_string() const override;
};

class TOK_RETURNSTMTNode : public AstNode {
public:
    AstNode* tok_return;
    AstNode* tok_expression;
    AstNode* tok_semicolon;

    TOK_RETURNSTMTNode();
    std::string to_string() const override;
};

class TOK_EXPRESSIONSTMTNode : public AstNode {
public:
    AstNode* tok_expression;
    AstNode* tok_semicolon;

    TOK_EXPRESSIONSTMTNode();
    std::string to_string() const override;
};

class TOK_EXPRESSIONNode : public AstNode {
public:
    AstNode* tok_expr01;

    TOK_EXPRESSIONNode();
    std::string to_string() const override;
};

class TOK_EXPR01Node : public AstNode {
public:
    AstNode* tok_expr02;
    AstNode* tok_expr01rest;

    TOK_EXPR01Node();
    std::string to_string() const override;
};

class TOK_EXPR01RESTNode : public AstNode {
public:
    AstNode* tok_comma;
    AstNode* tok_expr02;
    AstNode* tok_expr01rest;

    TOK_EXPR01RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR02Node : public AstNode {
public:
    AstNode* tok_expr03;
    AstNode* tok_expr02rest;

    TOK_EXPR02Node();
    std::string to_string() const override;
};

class TOK_EXPR02RESTNode : public AstNode {
public:
    AstNode* tok_assignop;
    AstNode* tok_expr02;

    TOK_EXPR02RESTNode();
    std::string to_string() const override;
};

class TOK_EQUALNode : public AstNode {
public:
    AstNode* tok_equal;

    TOK_EQUALNode();
    std::string to_string() const override;
};

class TOK_ASSIGNOPNode : public AstNode {
public:
    std::vector<AstNode*> tok_equal_list;
    AstNode* tok_plusequal;
    AstNode* tok_minusequal;
    AstNode* tok_starequal;
    AstNode* tok_slashequal;
    AstNode* tok_percentequal;
    AstNode* tok_lshiftequal;
    AstNode* tok_rshiftequal;
    AstNode* tok_ampequal;
    AstNode* none;
    AstNode* tok_caretequal;

    TOK_ASSIGNOPNode();
    std::string to_string() const override;
};

class TOK_EXPR03Node : public AstNode {
public:
    AstNode* tok_expr04;
    AstNode* tok_expr03rest;

    TOK_EXPR03Node();
    std::string to_string() const override;
};

class TOK_EXPR03RESTNode : public AstNode {
public:
    AstNode* tok_qmark;
    AstNode* tok_expr02;
    AstNode* tok_colon;
    AstNode* tok_expr03;
    AstNode* tok_id;

    TOK_EXPR03RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR04Node : public AstNode {
public:
    AstNode* tok_expr05;
    AstNode* tok_expr04rest;

    TOK_EXPR04Node();
    std::string to_string() const override;
};

class TOK_EXPR04RESTNode : public AstNode {
public:
    AstNode* tok_or;
    AstNode* tok_expr05;
    AstNode* tok_expr04rest;

    TOK_EXPR04RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR05Node : public AstNode {
public:
    AstNode* tok_expr06;
    AstNode* tok_expr05rest;

    TOK_EXPR05Node();
    std::string to_string() const override;
};

class TOK_EXPR05RESTNode : public AstNode {
public:
    AstNode* tok_andand;
    AstNode* tok_expr06;
    AstNode* tok_expr05rest;

    TOK_EXPR05RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR06Node : public AstNode {
public:
    AstNode* tok_expr07;
    AstNode* tok_expr06rest;

    TOK_EXPR06Node();
    std::string to_string() const override;
};

class TOK_EXPR06RESTNode : public AstNode {
public:
    AstNode* tok_expr07;
    AstNode* tok_expr06rest;

    TOK_EXPR06RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR07Node : public AstNode {
public:
    AstNode* tok_expr08;
    AstNode* tok_expr07rest;

    TOK_EXPR07Node();
    std::string to_string() const override;
};

class TOK_EXPR07RESTNode : public AstNode {
public:
    AstNode* tok_caret;
    AstNode* tok_expr08;
    AstNode* tok_expr07rest;

    TOK_EXPR07RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR08Node : public AstNode {
public:
    AstNode* tok_expr09;
    AstNode* tok_expr08rest;

    TOK_EXPR08Node();
    std::string to_string() const override;
};

class TOK_EXPR08RESTNode : public AstNode {
public:
    AstNode* tok_amp;
    AstNode* tok_expr09;
    AstNode* tok_expr08rest;

    TOK_EXPR08RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR09Node : public AstNode {
public:
    AstNode* tok_expr10;
    AstNode* tok_expr09rest;

    TOK_EXPR09Node();
    std::string to_string() const override;
};

class TOK_EXPR09RESTNode : public AstNode {
public:
    AstNode* tok_eqeq;
    AstNode* tok_expr10;
    AstNode* tok_expr09rest;

    TOK_EXPR09RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR10Node : public AstNode {
public:
    AstNode* tok_expr11;
    AstNode* tok_expr10rest;

    TOK_EXPR10Node();
    std::string to_string() const override;
};

class TOK_EXPR10RESTNode : public AstNode {
public:
    AstNode* tok_lt;
    std::vector<AstNode*> tok_expr11_list;
    std::vector<AstNode*> tok_expr10rest_list;
    AstNode* tok_gt;

    TOK_EXPR10RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR11Node : public AstNode {
public:
    AstNode* tok_expr12;
    AstNode* tok_expr11rest;

    TOK_EXPR11Node();
    std::string to_string() const override;
};

class TOK_EXPR11RESTNode : public AstNode {
public:
    AstNode* tok_lshift;
    std::vector<AstNode*> tok_expr12_list;
    std::vector<AstNode*> tok_expr11rest_list;
    AstNode* tok_rshift;

    TOK_EXPR11RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR12Node : public AstNode {
public:
    AstNode* tok_expr13;
    AstNode* tok_expr12rest;

    TOK_EXPR12Node();
    std::string to_string() const override;
};

class TOK_EXPR12RESTNode : public AstNode {
public:
    AstNode* tok_plus;
    std::vector<AstNode*> tok_expr13_list;
    std::vector<AstNode*> tok_expr12rest_list;
    AstNode* tok_minus;

    TOK_EXPR12RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR13Node : public AstNode {
public:
    AstNode* tok_expr14;
    AstNode* tok_expr13rest;

    TOK_EXPR13Node();
    std::string to_string() const override;
};

class TOK_EXPR13RESTNode : public AstNode {
public:
    AstNode* tok_star;
    std::vector<AstNode*> tok_expr14_list;
    std::vector<AstNode*> tok_expr13rest_list;
    AstNode* tok_slash;
    AstNode* tok_percent;

    TOK_EXPR13RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR14Node : public AstNode {
public:
    AstNode* tok_unaryop;
    AstNode* tok_expr14;
    AstNode* tok_expr15;

    TOK_EXPR14Node();
    std::string to_string() const override;
};

class TOK_UNARYOPNode : public AstNode {
public:
    AstNode* tok_excl;
    AstNode* tok_minus;
    AstNode* tok_plusplus;
    AstNode* tok_minusminus;

    TOK_UNARYOPNode();
    std::string to_string() const override;
};

class TOK_EXPR15Node : public AstNode {
public:
    AstNode* tok_expr16;
    AstNode* tok_expr15rest;

    TOK_EXPR15Node();
    std::string to_string() const override;
};

class TOK_EXPR15RESTNode : public AstNode {
public:
    AstNode* tok_plusplus;
    AstNode* tok_minusminus;

    TOK_EXPR15RESTNode();
    std::string to_string() const override;
};

class TOK_EXPR16Node : public AstNode {
public:
    AstNode* tok_atom;
    AstNode* tok_postfixtail;

    TOK_EXPR16Node();
    std::string to_string() const override;
};

class TOK_ATOMNode : public AstNode {
public:
    AstNode* tok_id;
    AstNode* tok_literal;
    AstNode* tok_lparen;
    AstNode* tok_expression;
    AstNode* tok_rparen;
    AstNode* tok_ino;

    TOK_ATOMNode();
    std::string to_string() const override;
};

class TOK_POSTFIXTAILNode : public AstNode {
public:
    AstNode* tok_postfix_item;
    AstNode* tok_postfixtail;

    TOK_POSTFIXTAILNode();
    std::string to_string() const override;
};

class TOK_POSTFIX_ITEMNode : public AstNode {
public:
    AstNode* tok_dot;
    AstNode* tok_id;
    AstNode* tok_lbracket;
    AstNode* tok_expression;
    AstNode* tok_rbracket;
    AstNode* tok_lparen;
    AstNode* tok_arglist;
    AstNode* tok_rparen;

    TOK_POSTFIX_ITEMNode();
    std::string to_string() const override;
};

class TOK_INONode : public AstNode {
public:
    AstNode* tok_print;
    std::vector<AstNode*> tok_lparen_list;
    std::vector<AstNode*> tok_arglist_list;
    std::vector<AstNode*> tok_rparen_list;
    AstNode* tok_input;

    TOK_INONode();
    std::string to_string() const override;
};

class TOK_ARGLISTNode : public AstNode {
public:
    AstNode* tok_expression;
    AstNode* tok_arglistrest;

    TOK_ARGLISTNode();
    std::string to_string() const override;
};

class TOK_ARGLISTRESTNode : public AstNode {
public:
    AstNode* tok_comma;
    AstNode* tok_expression;
    AstNode* tok_arglistrest;

    TOK_ARGLISTRESTNode();
    std::string to_string() const override;
};

class TOK_LITERALNode : public AstNode {
public:
    AstNode* number;
    AstNode* string;

    TOK_LITERALNode();
    std::string to_string() const override;
};

// =============================================================
// Утилиты
// =============================================================

void delete_tree(AstNode* root);
void print_tree(AstNode* root, int depth = 0);

} // namespace ast