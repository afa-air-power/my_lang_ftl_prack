#include "ast.hpp"
#include <iostream>
#include <iomanip>
extern std::string current_file_path;
namespace ast {

// =============================================================
// Базовый класс AstNode
// =============================================================

AstNode::~AstNode() {
    for (auto* child : children) {
        delete child;
    }
}

void AstNode::add_child(AstNode* child) {
    if (child) {
        children.push_back(child);
    }
}

void AstNode::print(int depth) const {
    std::cout << std::string(depth * 2, ' ') << to_string() << std::endl;
    for (const auto* child : children) {
        if (child) {
            child->print(depth + 1);
        }
    }
}

std::string AstNode::to_string() const {
    return "AstNode";
}

// =============================================================
// Терминальный узел
// =============================================================

// *** ИСПРАВЛЕНИЕ: Реализация конструктора с line/col ***
TerminalNode::TerminalNode(parser::TokenType tt, const std::string& val, const std::string& n, int l, int c)
    : AstNode(NodeType::TERMINAL), token_type(tt), value(val), name(n) {
    line = l;
    col = c;
}

void TerminalNode::print(int depth) const {
    std::cout << std::string(depth * 2, ' ')
              << "Terminal: " << parser::token_to_string(token_type)
              << " = '" << value << "' "
              << "(" << line << ":" << col << ")" << std::endl;
}

std::string TerminalNode::to_string() const {
    return "Terminal(" + parser::token_to_string(token_type) + ": '" + value + "')";
}

// =============================================================
// Специализированные узлы
// =============================================================

TOK_PROGRAMNode::TOK_PROGRAMNode() : AstNode(NodeType::TOK_PROGRAM) {}

std::string TOK_PROGRAMNode::to_string() const {
    return "TOK_PROGRAM";
}

TOK_IDNode::TOK_IDNode() : AstNode(NodeType::TOK_ID) {}

std::string TOK_IDNode::to_string() const {
    return "TOK_ID";
}

TOK_IDRESTNode::TOK_IDRESTNode() : AstNode(NodeType::TOK_IDREST) {}

std::string TOK_IDRESTNode::to_string() const {
    return "TOK_IDREST";
}

TOK_DECLARATIONLISTNode::TOK_DECLARATIONLISTNode() : AstNode(NodeType::TOK_DECLARATIONLIST) {}

std::string TOK_DECLARATIONLISTNode::to_string() const {
    return "TOK_DECLARATIONLIST";
}

TOK_DECLARATIONNode::TOK_DECLARATIONNode() : AstNode(NodeType::TOK_DECLARATION) {}

std::string TOK_DECLARATIONNode::to_string() const {
    return "TOK_DECLARATION";
}

TOK_DECLSUFFIXNode::TOK_DECLSUFFIXNode() : AstNode(NodeType::TOK_DECLSUFFIX) {}

std::string TOK_DECLSUFFIXNode::to_string() const {
    return "TOK_DECLSUFFIX";
}

TOK_CLASSDECLNode::TOK_CLASSDECLNode() : AstNode(NodeType::TOK_CLASSDECL) {}

std::string TOK_CLASSDECLNode::to_string() const {
    return "TOK_CLASSDECL";
}

TOK_MEMBERLISTNode::TOK_MEMBERLISTNode() : AstNode(NodeType::TOK_MEMBERLIST) {}

std::string TOK_MEMBERLISTNode::to_string() const {
    return "TOK_MEMBERLIST";
}

TOK_MEMBERNode::TOK_MEMBERNode() : AstNode(NodeType::TOK_MEMBER) {}

std::string TOK_MEMBERNode::to_string() const {
    return "TOK_MEMBER";
}

TOK_MEMBERSUFFIXNode::TOK_MEMBERSUFFIXNode() : AstNode(NodeType::TOK_MEMBERSUFFIX) {}

std::string TOK_MEMBERSUFFIXNode::to_string() const {
    return "TOK_MEMBERSUFFIX";
}

TOK_VARDECLRESTNode::TOK_VARDECLRESTNode() : AstNode(NodeType::TOK_VARDECLREST) {}

std::string TOK_VARDECLRESTNode::to_string() const {
    return "TOK_VARDECLREST";
}

TOK_TYPENode::TOK_TYPENode() : AstNode(NodeType::TOK_TYPE) {}

std::string TOK_TYPENode::to_string() const {
    return "TOK_TYPE";
}

TOK_PARAMLISTNode::TOK_PARAMLISTNode() : AstNode(NodeType::TOK_PARAMLIST) {}

std::string TOK_PARAMLISTNode::to_string() const {
    return "TOK_PARAMLIST";
}

TOK_PARAMLISTRESTNode::TOK_PARAMLISTRESTNode() : AstNode(NodeType::TOK_PARAMLISTREST) {}

std::string TOK_PARAMLISTRESTNode::to_string() const {
    return "TOK_PARAMLISTREST";
}

TOK_PARAMNode::TOK_PARAMNode() : AstNode(NodeType::TOK_PARAM) {}

std::string TOK_PARAMNode::to_string() const {
    return "TOK_PARAM";
}

TOK_COMPOUNDSTMTNode::TOK_COMPOUNDSTMTNode() : AstNode(NodeType::TOK_COMPOUNDSTMT) {}

std::string TOK_COMPOUNDSTMTNode::to_string() const {
    return "TOK_COMPOUNDSTMT";
}

TOK_STMTLISTNode::TOK_STMTLISTNode() : AstNode(NodeType::TOK_STMTLIST) {}

std::string TOK_STMTLISTNode::to_string() const {
    return "TOK_STMTLIST";
}

TOK_STATEMENTNode::TOK_STATEMENTNode() : AstNode(NodeType::TOK_STATEMENT) {}

std::string TOK_STATEMENTNode::to_string() const {
    return "TOK_STATEMENT";
}

TOK_FORSTMTNode::TOK_FORSTMTNode() : AstNode(NodeType::TOK_FORSTMT) {}

std::string TOK_FORSTMTNode::to_string() const {
    return "TOK_FORSTMT";
}

TOK_AFTERSTMTNode::TOK_AFTERSTMTNode() : AstNode(NodeType::TOK_AFTERSTMT) {}

std::string TOK_AFTERSTMTNode::to_string() const {
    return "TOK_AFTERSTMT";
}

TOK_LOOPSTMTNode::TOK_LOOPSTMTNode() : AstNode(NodeType::TOK_LOOPSTMT) {}

std::string TOK_LOOPSTMTNode::to_string() const {
    return "TOK_LOOPSTMT";
}

TOK_LOCALVARDECLNode::TOK_LOCALVARDECLNode() : AstNode(NodeType::TOK_LOCALVARDECL) {}

std::string TOK_LOCALVARDECLNode::to_string() const {
    return "TOK_LOCALVARDECL";
}

TOK_IFSTMTNode::TOK_IFSTMTNode() : AstNode(NodeType::TOK_IFSTMT) {}

std::string TOK_IFSTMTNode::to_string() const {
    return "TOK_IFSTMT";
}

TOK_ELSEPARTNode::TOK_ELSEPARTNode() : AstNode(NodeType::TOK_ELSEPART) {}

std::string TOK_ELSEPARTNode::to_string() const {
    return "TOK_ELSEPART";
}

TOK_WHILESTMTNode::TOK_WHILESTMTNode() : AstNode(NodeType::TOK_WHILESTMT) {}

std::string TOK_WHILESTMTNode::to_string() const {
    return "TOK_WHILESTMT";
}

TOK_RETURNSTMTNode::TOK_RETURNSTMTNode() : AstNode(NodeType::TOK_RETURNSTMT) {}

std::string TOK_RETURNSTMTNode::to_string() const {
    return "TOK_RETURNSTMT";
}

TOK_EXPRESSIONSTMTNode::TOK_EXPRESSIONSTMTNode() : AstNode(NodeType::TOK_EXPRESSIONSTMT) {}

std::string TOK_EXPRESSIONSTMTNode::to_string() const {
    return "TOK_EXPRESSIONSTMT";
}

TOK_EXPRESSIONNode::TOK_EXPRESSIONNode() : AstNode(NodeType::TOK_EXPRESSION) {}

std::string TOK_EXPRESSIONNode::to_string() const {
    return "TOK_EXPRESSION";
}

TOK_EXPR01Node::TOK_EXPR01Node() : AstNode(NodeType::TOK_EXPR01) {}

std::string TOK_EXPR01Node::to_string() const {
    return "TOK_EXPR01";
}

TOK_EXPR01RESTNode::TOK_EXPR01RESTNode() : AstNode(NodeType::TOK_EXPR01REST) {}

std::string TOK_EXPR01RESTNode::to_string() const {
    return "TOK_EXPR01REST";
}

TOK_EXPR02Node::TOK_EXPR02Node() : AstNode(NodeType::TOK_EXPR02) {}

std::string TOK_EXPR02Node::to_string() const {
    return "TOK_EXPR02";
}

TOK_EXPR02RESTNode::TOK_EXPR02RESTNode() : AstNode(NodeType::TOK_EXPR02REST) {}

std::string TOK_EXPR02RESTNode::to_string() const {
    return "TOK_EXPR02REST";
}

TOK_EQUALNode::TOK_EQUALNode() : AstNode(NodeType::TOK_EQUAL) {}

std::string TOK_EQUALNode::to_string() const {
    return "TOK_EQUAL";
}

TOK_ASSIGNOPNode::TOK_ASSIGNOPNode() : AstNode(NodeType::TOK_ASSIGNOP) {}

std::string TOK_ASSIGNOPNode::to_string() const {
    return "TOK_ASSIGNOP";
}

TOK_EXPR03Node::TOK_EXPR03Node() : AstNode(NodeType::TOK_EXPR03) {}

std::string TOK_EXPR03Node::to_string() const {
    return "TOK_EXPR03";
}

TOK_EXPR03RESTNode::TOK_EXPR03RESTNode() : AstNode(NodeType::TOK_EXPR03REST) {}

std::string TOK_EXPR03RESTNode::to_string() const {
    return "TOK_EXPR03REST";
}

TOK_EXPR04Node::TOK_EXPR04Node() : AstNode(NodeType::TOK_EXPR04) {}

std::string TOK_EXPR04Node::to_string() const {
    return "TOK_EXPR04";
}

TOK_EXPR04RESTNode::TOK_EXPR04RESTNode() : AstNode(NodeType::TOK_EXPR04REST) {}

std::string TOK_EXPR04RESTNode::to_string() const {
    return "TOK_EXPR04REST";
}

TOK_EXPR05Node::TOK_EXPR05Node() : AstNode(NodeType::TOK_EXPR05) {}

std::string TOK_EXPR05Node::to_string() const {
    return "TOK_EXPR05";
}

TOK_EXPR05RESTNode::TOK_EXPR05RESTNode() : AstNode(NodeType::TOK_EXPR05REST) {}

std::string TOK_EXPR05RESTNode::to_string() const {
    return "TOK_EXPR05REST";
}

TOK_EXPR06Node::TOK_EXPR06Node() : AstNode(NodeType::TOK_EXPR06) {}

std::string TOK_EXPR06Node::to_string() const {
    return "TOK_EXPR06";
}

TOK_EXPR06RESTNode::TOK_EXPR06RESTNode() : AstNode(NodeType::TOK_EXPR06REST) {}

std::string TOK_EXPR06RESTNode::to_string() const {
    return "TOK_EXPR06REST";
}

TOK_EXPR07Node::TOK_EXPR07Node() : AstNode(NodeType::TOK_EXPR07) {}

std::string TOK_EXPR07Node::to_string() const {
    return "TOK_EXPR07";
}

TOK_EXPR07RESTNode::TOK_EXPR07RESTNode() : AstNode(NodeType::TOK_EXPR07REST) {}

std::string TOK_EXPR07RESTNode::to_string() const {
    return "TOK_EXPR07REST";
}

TOK_EXPR08Node::TOK_EXPR08Node() : AstNode(NodeType::TOK_EXPR08) {}

std::string TOK_EXPR08Node::to_string() const {
    return "TOK_EXPR08";
}

TOK_EXPR08RESTNode::TOK_EXPR08RESTNode() : AstNode(NodeType::TOK_EXPR08REST) {}

std::string TOK_EXPR08RESTNode::to_string() const {
    return "TOK_EXPR08REST";
}

TOK_EXPR09Node::TOK_EXPR09Node() : AstNode(NodeType::TOK_EXPR09) {}

std::string TOK_EXPR09Node::to_string() const {
    return "TOK_EXPR09";
}

TOK_EXPR09RESTNode::TOK_EXPR09RESTNode() : AstNode(NodeType::TOK_EXPR09REST) {}

std::string TOK_EXPR09RESTNode::to_string() const {
    return "TOK_EXPR09REST";
}

TOK_EXPR10Node::TOK_EXPR10Node() : AstNode(NodeType::TOK_EXPR10) {}

std::string TOK_EXPR10Node::to_string() const {
    return "TOK_EXPR10";
}

TOK_EXPR10RESTNode::TOK_EXPR10RESTNode() : AstNode(NodeType::TOK_EXPR10REST) {}

std::string TOK_EXPR10RESTNode::to_string() const {
    return "TOK_EXPR10REST";
}

TOK_EXPR11Node::TOK_EXPR11Node() : AstNode(NodeType::TOK_EXPR11) {}

std::string TOK_EXPR11Node::to_string() const {
    return "TOK_EXPR11";
}

TOK_EXPR11RESTNode::TOK_EXPR11RESTNode() : AstNode(NodeType::TOK_EXPR11REST) {}

std::string TOK_EXPR11RESTNode::to_string() const {
    return "TOK_EXPR11REST";
}

TOK_EXPR12Node::TOK_EXPR12Node() : AstNode(NodeType::TOK_EXPR12) {}

std::string TOK_EXPR12Node::to_string() const {
    return "TOK_EXPR12";
}

TOK_EXPR12RESTNode::TOK_EXPR12RESTNode() : AstNode(NodeType::TOK_EXPR12REST) {}

std::string TOK_EXPR12RESTNode::to_string() const {
    return "TOK_EXPR12REST";
}

TOK_EXPR13Node::TOK_EXPR13Node() : AstNode(NodeType::TOK_EXPR13) {}

std::string TOK_EXPR13Node::to_string() const {
    return "TOK_EXPR13";
}

TOK_EXPR13RESTNode::TOK_EXPR13RESTNode() : AstNode(NodeType::TOK_EXPR13REST) {}

std::string TOK_EXPR13RESTNode::to_string() const {
    return "TOK_EXPR13REST";
}

TOK_EXPR14Node::TOK_EXPR14Node() : AstNode(NodeType::TOK_EXPR14) {}

std::string TOK_EXPR14Node::to_string() const {
    return "TOK_EXPR14";
}

TOK_UNARYOPNode::TOK_UNARYOPNode() : AstNode(NodeType::TOK_UNARYOP) {}

std::string TOK_UNARYOPNode::to_string() const {
    return "TOK_UNARYOP";
}

TOK_EXPR15Node::TOK_EXPR15Node() : AstNode(NodeType::TOK_EXPR15) {}

std::string TOK_EXPR15Node::to_string() const {
    return "TOK_EXPR15";
}

TOK_EXPR15RESTNode::TOK_EXPR15RESTNode() : AstNode(NodeType::TOK_EXPR15REST) {}

std::string TOK_EXPR15RESTNode::to_string() const {
    return "TOK_EXPR15REST";
}

TOK_EXPR16Node::TOK_EXPR16Node() : AstNode(NodeType::TOK_EXPR16) {}

std::string TOK_EXPR16Node::to_string() const {
    return "TOK_EXPR16";
}

TOK_ATOMNode::TOK_ATOMNode() : AstNode(NodeType::TOK_ATOM) {}

std::string TOK_ATOMNode::to_string() const {
    return "TOK_ATOM";
}

TOK_POSTFIXTAILNode::TOK_POSTFIXTAILNode() : AstNode(NodeType::TOK_POSTFIXTAIL) {}

std::string TOK_POSTFIXTAILNode::to_string() const {
    return "TOK_POSTFIXTAIL";
}

TOK_POSTFIX_ITEMNode::TOK_POSTFIX_ITEMNode() : AstNode(NodeType::TOK_POSTFIX_ITEM) {}

std::string TOK_POSTFIX_ITEMNode::to_string() const {
    return "TOK_POSTFIX_ITEM";
}

TOK_INONode::TOK_INONode() : AstNode(NodeType::TOK_INO) {}

std::string TOK_INONode::to_string() const {
    return "TOK_INO";
}

TOK_ARGLISTNode::TOK_ARGLISTNode() : AstNode(NodeType::TOK_ARGLIST) {}

std::string TOK_ARGLISTNode::to_string() const {
    return "TOK_ARGLIST";
}

TOK_ARGLISTRESTNode::TOK_ARGLISTRESTNode() : AstNode(NodeType::TOK_ARGLISTREST) {}

std::string TOK_ARGLISTRESTNode::to_string() const {
    return "TOK_ARGLISTREST";
}

TOK_LITERALNode::TOK_LITERALNode() : AstNode(NodeType::TOK_LITERAL) {}

std::string TOK_LITERALNode::to_string() const {
    return "TOK_LITERAL";
}

// =============================================================
// Утилиты
// =============================================================

void delete_tree(AstNode* root) {
    delete root;
}

void print_tree(AstNode* root, int depth) {
    if (root) {
        root->print(depth);
    }
}

} // namespace ast