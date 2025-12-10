#include "parser.hpp"
#include <iostream>
#include <vector>
#include <fstream>
#include "ast_utils.hpp"
#include "parser.hpp"
#include "ast_check.hpp"  // Added for semantic_check
#include "ast_functiontree.hpp"  // Added for optimize_ast
#include <iostream>
#include <vector>
#include <fstream>
#include "ast_utils.hpp"
// *** ВАЖНОЕ ИСПРАВЛЕНИЕ: Глобальное определение для линковки с ast_utils.cpp ***
std::string current_file_path = "test_program.txt";

namespace parser {
    static lexer::Lexer *current_lexer = nullptr;
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
    ast::AstNode *parse(lexer::Lexer &lexer, const std::string &path) {
        current_lexer = &lexer;
        current_file_path = path;

        // "Прокачиваем" лексер, чтобы заполнить current и peek
        peek_token = current_lexer->next();
        peek = peek_token.type;
        gc(); // Первый вызов: current=первый токен, peek=второй токен

        std::cout << "Parsing file: " << path << std::endl;
        ast::AstNode *root = nullptr;
        try {
            root = TOK_PROGRAM();
            std::cout << "\nParsing completed successfully.\n";
            // --- ВСТАВКА: запустить семантику/оптимизации/дамп AST ---
            try {
                std::cerr << "Running semantic checks...\n";
                bool ok_sem = ast::semantic_check(root);
                if (!ok_sem) {
                    std::cerr << "Semantic checks reported issues (see stderr). Continuing to dump AST.\n";
                }
            } catch (const std::exception &e) {
                std::cerr << "Semantic check threw: " << e.what() << "\n";
            }
            try {
                std::cerr << "Running basic AST optimizations (constant folding)...\n";
                ast::optimize_ast(root);
            } catch (const std::exception &e) {
                std::cerr << "AST optimization threw: " << e.what() << "\n";
            }
            try {
                ast::print_tree_to_file(root, "ast.txt");
                std::cerr << "AST written to ast.txt\n";
            } catch (const std::exception &e) {
                std::cerr << "AST dump threw: " << e.what() << "\n";
            }
            // --- КОНЕЦ ВСТАВКИ ---
            return root;
        } catch (const ParseError &e) {
            std::cerr << "\n" << current_file_path << ":" << current_token.line
                    << ":" << current_token.col << ": error: ";
            std::cerr << "unexpected token " << token_to_string(current)
                    << " ('" << current_token.value << "')\n";
            std::cerr << e.what() << std::endl;

            std::ofstream out("lexer_info.txt");
            if (out.is_open()) {
                out << "LEXER DUMP (on parse error)\n";
                out << "=============================\n";
                out << "Error at " << current_file_path << ":" << current_token.line << ":" << current_token.col <<
                        "\n";
                out << "Current token: " << token_to_string(current) << " = '" << current_token.value << "'\n\n";
                try {
                    auto tokens = current_lexer->get_all_tokens();
                    for (const auto &t: tokens) {
                        out << "Type: " << token_to_string(t.type)
                                << ", Name: " << t.name
                                << ", Value: " << t.value
                                << ", Line: " << t.line
                                << ", Col: " << t.col << "\n";
                    }
                } catch (const std::exception &le) {
                    out << "[Lexer dump failed: " << le.what() << "]\n";
                }
                out.close();
                std::cerr << "Lexer dump written to lexer_info.txt\n";
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

        CallContext(const std::string &n) : func_name(n) {
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

    static void syntax_error(const std::string &msg) {
        // Сохраняем стек перед исключением
        std::string stack_info = msg + "\n\nCall stack:\n";
        for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it) {
            stack_info += "  " + *it + "\n";
        }
        throw ParseError(stack_info);
    }

    ast::AstNode *TOK_PROGRAM() {
        CallContext ctx("TOK_PROGRAM");
        ast::TOK_PROGRAMNode *node = new ast::TOK_PROGRAMNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (true) {
            ast::AstNode *child_tok_declarationlist = TOK_DECLARATIONLIST();
            node->add_child(child_tok_declarationlist);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_PROGRAM");
        return nullptr;
    }

    ast::AstNode *TOK_ID() {
        CallContext ctx("TOK_ID");
        ast::TOK_IDNode *node = new ast::TOK_IDNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER) {
            if (current != TokenType::IDENTIFIER) {
                delete node;
                syntax_error("expected IDENTIFIER in TOK_ID");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_idrest = TOK_IDREST();
            node->add_child(child_tok_idrest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_ID");
        return nullptr;
    }

    ast::AstNode *TOK_IDREST() {
        CallContext ctx("TOK_IDREST");
        ast::TOK_IDRESTNode *node = new ast::TOK_IDRESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_DOT) {
            if (current != TokenType::TOK_DOT) {
                delete node;
                syntax_error("expected TOK_DOT in TOK_IDREST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_id = TOK_ID();
            node->add_child(child_tok_id);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_IDREST");
        return nullptr;
    }

    ast::AstNode *TOK_DECLARATIONLIST() {
        CallContext ctx("TOK_DECLARATIONLIST");
        ast::TOK_DECLARATIONLISTNode *node = new ast::TOK_DECLARATIONLISTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::TOK_CLASS || current == TokenType::TOK_DOUBLE ||
            current == TokenType::TOK_FLOAT || current == TokenType::TOK_INT || current == TokenType::TOK_STRING ||
            current == TokenType::TOK_VECTOR) {
            ast::AstNode *child_tok_declaration = TOK_DECLARATION();
            node->add_child(child_tok_declaration);
            ast::AstNode *child_tok_declarationlist = TOK_DECLARATIONLIST();
            node->add_child(child_tok_declarationlist);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_DECLARATIONLIST");
        return nullptr;
    }

    ast::AstNode *TOK_DECLARATION() {
        CallContext ctx("TOK_DECLARATION");
        ast::TOK_DECLARATIONNode *node = new ast::TOK_DECLARATIONNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::TOK_DOUBLE || current == TokenType::TOK_FLOAT ||
            current == TokenType::TOK_INT || current == TokenType::TOK_STRING || current == TokenType::TOK_VECTOR) {
            ast::AstNode *child_tok_type = TOK_TYPE();
            node->add_child(child_tok_type);
            ast::AstNode *child_tok_id = TOK_ID();
            node->add_child(child_tok_id);
            ast::AstNode *child_tok_declsuffix = TOK_DECLSUFFIX();
            node->add_child(child_tok_declsuffix);
            return node;
        } else if (current == TokenType::TOK_CLASS) {
            ast::AstNode *child_tok_classdecl = TOK_CLASSDECL();
            node->add_child(child_tok_classdecl);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_DECLARATION");
        return nullptr;
    }

    ast::AstNode *TOK_DECLSUFFIX() {
        CallContext ctx("TOK_DECLSUFFIX");
        ast::TOK_DECLSUFFIXNode *node = new ast::TOK_DECLSUFFIXNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_LPAREN) {
            if (current != TokenType::TOK_LPAREN) {
                delete node;
                syntax_error("expected TOK_LPAREN in TOK_DECLSUFFIX");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_paramlist = TOK_PARAMLIST();
            node->add_child(child_tok_paramlist);
            if (current != TokenType::TOK_RPAREN) {
                delete node;
                syntax_error("expected TOK_RPAREN in TOK_DECLSUFFIX");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_compoundstmt = TOK_COMPOUNDSTMT();
            node->add_child(child_tok_compoundstmt);
            return node;
        } else if (true) {
            ast::AstNode *child_tok_vardeclrest = TOK_VARDECLREST();
            node->add_child(child_tok_vardeclrest);
            if (current != TokenType::TOK_SEMICOLON) {
                delete node;
                syntax_error("expected TOK_SEMICOLON in TOK_DECLSUFFIX");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_DECLSUFFIX");
        return nullptr;
    }

    ast::AstNode *TOK_CLASSDECL() {
        CallContext ctx("TOK_CLASSDECL");
        ast::TOK_CLASSDECLNode *node = new ast::TOK_CLASSDECLNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_CLASS) {
            if (current != TokenType::TOK_CLASS) {
                delete node;
                syntax_error("expected TOK_CLASS in TOK_CLASSDECL");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_id = TOK_ID();
            node->add_child(child_tok_id);
            if (current != TokenType::TOK_LBRACE) {
                delete node;
                syntax_error("expected TOK_LBRACE in TOK_CLASSDECL");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_memberlist = TOK_MEMBERLIST();
            node->add_child(child_tok_memberlist);
            if (current != TokenType::TOK_RBRACE) {
                delete node;
                syntax_error("expected TOK_RBRACE in TOK_CLASSDECL");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            if (current != TokenType::TOK_SEMICOLON) {
                delete node;
                syntax_error("expected TOK_SEMICOLON in TOK_CLASSDECL");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_CLASSDECL");
        return nullptr;
    }

    ast::AstNode *TOK_MEMBERLIST() {
        CallContext ctx("TOK_MEMBERLIST");
        ast::TOK_MEMBERLISTNode *node = new ast::TOK_MEMBERLISTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::TOK_DOUBLE || current == TokenType::TOK_FLOAT ||
            current == TokenType::TOK_INT || current == TokenType::TOK_STRING || current == TokenType::TOK_VECTOR) {
            ast::AstNode *child_tok_member = TOK_MEMBER();
            node->add_child(child_tok_member);
            ast::AstNode *child_tok_memberlist = TOK_MEMBERLIST();
            node->add_child(child_tok_memberlist);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_MEMBERLIST");
        return nullptr;
    }

    ast::AstNode *TOK_MEMBER() {
        CallContext ctx("TOK_MEMBER");
        ast::TOK_MEMBERNode *node = new ast::TOK_MEMBERNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::TOK_DOUBLE || current == TokenType::TOK_FLOAT ||
            current == TokenType::TOK_INT || current == TokenType::TOK_STRING || current == TokenType::TOK_VECTOR) {
            ast::AstNode *child_tok_type = TOK_TYPE();
            node->add_child(child_tok_type);
            ast::AstNode *child_tok_id = TOK_ID();
            node->add_child(child_tok_id);
            ast::AstNode *child_tok_membersuffix = TOK_MEMBERSUFFIX();
            node->add_child(child_tok_membersuffix);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_MEMBER");
        return nullptr;
    }

    ast::AstNode *TOK_MEMBERSUFFIX() {
        CallContext ctx("TOK_MEMBERSUFFIX");
        ast::TOK_MEMBERSUFFIXNode *node = new ast::TOK_MEMBERSUFFIXNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_LPAREN) {
            if (current != TokenType::TOK_LPAREN) {
                delete node;
                syntax_error("expected TOK_LPAREN in TOK_MEMBERSUFFIX");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_paramlist = TOK_PARAMLIST();
            node->add_child(child_tok_paramlist);
            if (current != TokenType::TOK_RPAREN) {
                delete node;
                syntax_error("expected TOK_RPAREN in TOK_MEMBERSUFFIX");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_compoundstmt = TOK_COMPOUNDSTMT();
            node->add_child(child_tok_compoundstmt);
            return node;
        } else if (current == TokenType::TOK_SEMICOLON) {
            if (current != TokenType::TOK_SEMICOLON) {
                delete node;
                syntax_error("expected TOK_SEMICOLON in TOK_MEMBERSUFFIX");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_MEMBERSUFFIX");
        return nullptr;
    }

    ast::AstNode *TOK_VARDECLREST() {
        CallContext ctx("TOK_VARDECLREST");
        ast::TOK_VARDECLRESTNode *node = new ast::TOK_VARDECLRESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_EQUAL) {
            if (current != TokenType::TOK_EQUAL) {
                delete node;
                syntax_error("expected TOK_EQUAL in TOK_VARDECLREST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_VARDECLREST");
        return nullptr;
    }

    ast::AstNode *TOK_TYPE() {
        CallContext ctx("TOK_TYPE");
        ast::TOK_TYPENode *node = new ast::TOK_TYPENode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_INT) {
            if (current != TokenType::TOK_INT) {
                delete node;
                syntax_error("expected TOK_INT in TOK_TYPE");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_FLOAT) {
            if (current != TokenType::TOK_FLOAT) {
                delete node;
                syntax_error("expected TOK_FLOAT in TOK_TYPE");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_DOUBLE) {
            if (current != TokenType::TOK_DOUBLE) {
                delete node;
                syntax_error("expected TOK_DOUBLE in TOK_TYPE");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_STRING) {
            if (current != TokenType::TOK_STRING) {
                delete node;
                syntax_error("expected TOK_STRING in TOK_TYPE");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::IDENTIFIER) {
            ast::AstNode *child_tok_id = TOK_ID();
            node->add_child(child_tok_id);
            return node;
        } else if (current == TokenType::TOK_VECTOR) {
            if (current != TokenType::TOK_VECTOR) {
                delete node;
                syntax_error("expected TOK_VECTOR in TOK_TYPE");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            if (current != TokenType::TOK_LT) {
                delete node;
                syntax_error("expected TOK_LT in TOK_TYPE");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_type = TOK_TYPE();
            node->add_child(child_tok_type);
            if (current != TokenType::TOK_GT) {
                delete node;
                syntax_error("expected TOK_GT in TOK_TYPE");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_TYPE");
        return nullptr;
    }

    ast::AstNode *TOK_PARAMLIST() {
        CallContext ctx("TOK_PARAMLIST");
        ast::TOK_PARAMLISTNode *node = new ast::TOK_PARAMLISTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::TOK_DOUBLE || current == TokenType::TOK_FLOAT ||
            current == TokenType::TOK_INT || current == TokenType::TOK_STRING || current == TokenType::TOK_VECTOR) {
            ast::AstNode *child_tok_param = TOK_PARAM();
            node->add_child(child_tok_param);
            ast::AstNode *child_tok_paramlistrest = TOK_PARAMLISTREST();
            node->add_child(child_tok_paramlistrest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_PARAMLIST");
        return nullptr;
    }

    ast::AstNode *TOK_PARAMLISTREST() {
        CallContext ctx("TOK_PARAMLISTREST");
        ast::TOK_PARAMLISTRESTNode *node = new ast::TOK_PARAMLISTRESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_COMMA) {
            if (current != TokenType::TOK_COMMA) {
                delete node;
                syntax_error("expected TOK_COMMA in TOK_PARAMLISTREST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_param = TOK_PARAM();
            node->add_child(child_tok_param);
            ast::AstNode *child_tok_paramlistrest = TOK_PARAMLISTREST();
            node->add_child(child_tok_paramlistrest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_PARAMLISTREST");
        return nullptr;
    }

    ast::AstNode *TOK_PARAM() {
        CallContext ctx("TOK_PARAM");
        ast::TOK_PARAMNode *node = new ast::TOK_PARAMNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::TOK_DOUBLE || current == TokenType::TOK_FLOAT ||
            current == TokenType::TOK_INT || current == TokenType::TOK_STRING || current == TokenType::TOK_VECTOR) {
            ast::AstNode *child_tok_type = TOK_TYPE();
            node->add_child(child_tok_type);
            ast::AstNode *child_tok_id = TOK_ID();
            node->add_child(child_tok_id);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_PARAM");
        return nullptr;
    }

    ast::AstNode *TOK_COMPOUNDSTMT() {
        CallContext ctx("TOK_COMPOUNDSTMT");
        ast::TOK_COMPOUNDSTMTNode *node = new ast::TOK_COMPOUNDSTMTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_LBRACE) {
            if (current != TokenType::TOK_LBRACE) {
                delete node;
                syntax_error("expected TOK_LBRACE in TOK_COMPOUNDSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_stmtlist = TOK_STMTLIST();
            node->add_child(child_tok_stmtlist);
            if (current != TokenType::TOK_RBRACE) {
                delete node;
                syntax_error("expected TOK_RBRACE in TOK_COMPOUNDSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_COMPOUNDSTMT");
        return nullptr;
    }

    ast::AstNode *TOK_STMTLIST() {
        CallContext ctx("TOK_STMTLIST");
        ast::TOK_STMTLISTNode *node = new ast::TOK_STMTLISTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_AFTER || current == TokenType::TOK_DOUBLE || current == TokenType::TOK_EXCL || current ==
            TokenType::TOK_FLOAT || current == TokenType::TOK_FOR || current == TokenType::TOK_IF || current ==
            TokenType::TOK_INPUT || current == TokenType::TOK_INT || current == TokenType::TOK_LBRACE || current ==
            TokenType::TOK_LPAREN || current == TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current
            == TokenType::TOK_PLUSPLUS || current == TokenType::TOK_PRINT || current == TokenType::TOK_RETURN || current
            == TokenType::TOK_STRING || current == TokenType::TOK_VECTOR || current == TokenType::TOK_WHILE) {
            ast::AstNode *child_tok_statement = TOK_STATEMENT();
            node->add_child(child_tok_statement);
            ast::AstNode *child_tok_stmtlist = TOK_STMTLIST();
            node->add_child(child_tok_stmtlist);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_STMTLIST");
        return nullptr;
    }

    ast::AstNode *TOK_STATEMENT() {
        CallContext ctx("TOK_STATEMENT");
        ast::TOK_STATEMENTNode *node = new ast::TOK_STATEMENTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        // --- Кастомная логика для разрешения конфликта FIRST/FIRST ---

        // 1. Сначала проверяем все НЕОДНОЗНАЧНЫЕ альтернативы <Statement>
        if (current == TokenType::TOK_IF) {
            node->add_child(TOK_IFSTMT());
            return node;
        }
        if (current == TokenType::TOK_WHILE) {
            node->add_child(TOK_WHILESTMT());
            return node;
        }
        if (current == TokenType::TOK_RETURN) {
            node->add_child(TOK_RETURNSTMT());
            return node;
        }
        if (current == TokenType::TOK_LBRACE) {
            node->add_child(TOK_COMPOUNDSTMT());
            return node;
        }
        if (current == TokenType::TOK_FOR) {
            node->add_child(TOK_FORSTMT());
            return node;
        }
        if (current == TokenType::TOK_AFTER) {
            node->add_child(TOK_AFTERSTMT());
            return node;
        }
        if (current == TokenType::TOK_DOUBLE || current == TokenType::TOK_FLOAT || current == TokenType::TOK_INT ||
            current == TokenType::TOK_STRING || current == TokenType::TOK_VECTOR) {
            // Это однозначно LocalVarDecl (начинается с int, float и т.д.)
            node->add_child(TOK_LOCALVARDECL());
            return node;
        }
        if (current == TokenType::NUMBER || current == TokenType::STRING || current == TokenType::TOK_EXCL || current ==
            TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current == TokenType::TOK_MINUS || current ==
            TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS || current == TokenType::TOK_PRINT) {
            // Это однозначно ExpressionStmt (начинается с NUMBER, '(', '!' и т.д.)
            node->add_child(TOK_EXPRESSIONSTMT());
            return node;
        }
        if (current == TokenType::IDENTIFIER) {
            // НЕОДНОЗНАЧНЫЙ СЛУЧАЙ: начинается с IDENTIFIER.
            // Нам нужно заглянуть на следующий токен (peek).

            if (peek == TokenType::IDENTIFIER) {
                // IDENTIFIER IDENTIFIER ... -> это LocalVarDecl (напр. 'human p0;')
                node->add_child(TOK_LOCALVARDECL());
            } else {
                // IDENTIFIER (что-то другое) ... -> это ExpressionStmt (напр. 'now = 0;' или 'p0.id = 1;')
                node->add_child(TOK_EXPRESSIONSTMT());
            }
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_STATEMENT");
        return nullptr;
    }

    ast::AstNode *TOK_FORSTMT() {
        CallContext ctx("TOK_FORSTMT");
        ast::TOK_FORSTMTNode *node = new ast::TOK_FORSTMTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_FOR) {
            if (current != TokenType::TOK_FOR) {
                delete node;
                syntax_error("expected TOK_FOR in TOK_FORSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            if (current != TokenType::TOK_LPAREN) {
                delete node;
                syntax_error("expected TOK_LPAREN in TOK_FORSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            if (current != TokenType::TOK_RPAREN) {
                delete node;
                syntax_error("expected TOK_RPAREN in TOK_FORSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_statement = TOK_STATEMENT();
            node->add_child(child_tok_statement);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_FORSTMT");
        return nullptr;
    }

    ast::AstNode *TOK_AFTERSTMT() {
        CallContext ctx("TOK_AFTERSTMT");
        ast::TOK_AFTERSTMTNode *node = new ast::TOK_AFTERSTMTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_AFTER) {
            if (current != TokenType::TOK_AFTER) {
                delete node;
                syntax_error("expected TOK_AFTER in TOK_AFTERSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            ast::AstNode *child_tok_loopstmt = TOK_LOOPSTMT();
            node->add_child(child_tok_loopstmt);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_AFTERSTMT");
        return nullptr;
    }

    ast::AstNode *TOK_LOOPSTMT() {
        CallContext ctx("TOK_LOOPSTMT");
        ast::TOK_LOOPSTMTNode *node = new ast::TOK_LOOPSTMTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_WHILE) {
            ast::AstNode *child_tok_whilestmt = TOK_WHILESTMT();
            node->add_child(child_tok_whilestmt);
            return node;
        } else if (current == TokenType::TOK_FOR) {
            ast::AstNode *child_tok_forstmt = TOK_FORSTMT();
            node->add_child(child_tok_forstmt);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_LOOPSTMT");
        return nullptr;
    }

    ast::AstNode *TOK_LOCALVARDECL() {
        CallContext ctx("TOK_LOCALVARDECL");
        ast::TOK_LOCALVARDECLNode *node = new ast::TOK_LOCALVARDECLNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::TOK_DOUBLE || current == TokenType::TOK_FLOAT ||
            current == TokenType::TOK_INT || current == TokenType::TOK_STRING || current == TokenType::TOK_VECTOR) {
            ast::AstNode *child_tok_type = TOK_TYPE();
            node->add_child(child_tok_type);
            ast::AstNode *child_tok_id = TOK_ID();
            node->add_child(child_tok_id);
            ast::AstNode *child_tok_vardeclrest = TOK_VARDECLREST();
            node->add_child(child_tok_vardeclrest);
            if (current != TokenType::TOK_SEMICOLON) {
                delete node;
                syntax_error("expected TOK_SEMICOLON in TOK_LOCALVARDECL");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_LOCALVARDECL");
        return nullptr;
    }

    ast::AstNode *TOK_IFSTMT() {
        CallContext ctx("TOK_IFSTMT");
        ast::TOK_IFSTMTNode *node = new ast::TOK_IFSTMTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_IF) {
            if (current != TokenType::TOK_IF) {
                delete node;
                syntax_error("expected TOK_IF in TOK_IFSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            if (current != TokenType::TOK_LPAREN) {
                delete node;
                syntax_error("expected TOK_LPAREN in TOK_IFSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            if (current != TokenType::TOK_RPAREN) {
                delete node;
                syntax_error("expected TOK_RPAREN in TOK_IFSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_statement = TOK_STATEMENT();
            node->add_child(child_tok_statement);
            ast::AstNode *child_tok_elsepart = TOK_ELSEPART();
            node->add_child(child_tok_elsepart);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_IFSTMT");
        return nullptr;
    }

    ast::AstNode *TOK_ELSEPART() {
        CallContext ctx("TOK_ELSEPART");
        ast::TOK_ELSEPARTNode *node = new ast::TOK_ELSEPARTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_ELSE) {
            if (current != TokenType::TOK_ELSE) {
                delete node;
                syntax_error("expected TOK_ELSE in TOK_ELSEPART");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_statement = TOK_STATEMENT();
            node->add_child(child_tok_statement);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_ELSEPART");
        return nullptr;
    }

    ast::AstNode *TOK_WHILESTMT() {
        CallContext ctx("TOK_WHILESTMT");
        ast::TOK_WHILESTMTNode *node = new ast::TOK_WHILESTMTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_WHILE) {
            if (current != TokenType::TOK_WHILE) {
                delete node;
                syntax_error("expected TOK_WHILE in TOK_WHILESTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            if (current != TokenType::TOK_LPAREN) {
                delete node;
                syntax_error("expected TOK_LPAREN in TOK_WHILESTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            if (current != TokenType::TOK_RPAREN) {
                delete node;
                syntax_error("expected TOK_RPAREN in TOK_WHILESTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_statement = TOK_STATEMENT();
            node->add_child(child_tok_statement);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_WHILESTMT");
        return nullptr;
    }

    ast::AstNode *TOK_RETURNSTMT() {
        CallContext ctx("TOK_RETURNSTMT");
        ast::TOK_RETURNSTMTNode *node = new ast::TOK_RETURNSTMTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_RETURN) {
            if (current != TokenType::TOK_RETURN) {
                delete node;
                syntax_error("expected TOK_RETURN in TOK_RETURNSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            if (current != TokenType::TOK_SEMICOLON) {
                delete node;
                syntax_error("expected TOK_SEMICOLON in TOK_RETURNSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_RETURNSTMT");
        return nullptr;
    }

    ast::AstNode *TOK_EXPRESSIONSTMT() {
        CallContext ctx("TOK_EXPRESSIONSTMT");
        ast::TOK_EXPRESSIONSTMTNode *node = new ast::TOK_EXPRESSIONSTMTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            if (current != TokenType::TOK_SEMICOLON) {
                delete node;
                syntax_error("expected TOK_SEMICOLON in TOK_EXPRESSIONSTMT");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPRESSIONSTMT");
        return nullptr;
    }

    ast::AstNode *TOK_EXPRESSION() {
        CallContext ctx("TOK_EXPRESSION");
        ast::TOK_EXPRESSIONNode *node = new ast::TOK_EXPRESSIONNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr01 = TOK_EXPR01();
            node->add_child(child_tok_expr01);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPRESSION");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR01() {
        CallContext ctx("TOK_EXPR01");
        ast::TOK_EXPR01Node *node = new ast::TOK_EXPR01Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr02 = TOK_EXPR02();
            node->add_child(child_tok_expr02);
            ast::AstNode *child_tok_expr01rest = TOK_EXPR01REST();
            node->add_child(child_tok_expr01rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR01");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR01REST() {
        CallContext ctx("TOK_EXPR01REST");
        ast::TOK_EXPR01RESTNode *node = new ast::TOK_EXPR01RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_COMMA) {
            if (current != TokenType::TOK_COMMA) {
                delete node;
                syntax_error("expected TOK_COMMA in TOK_EXPR01REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr02 = TOK_EXPR02();
            node->add_child(child_tok_expr02);
            ast::AstNode *child_tok_expr01rest = TOK_EXPR01REST();
            node->add_child(child_tok_expr01rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR01REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR02() {
        CallContext ctx("TOK_EXPR02");
        ast::TOK_EXPR02Node *node = new ast::TOK_EXPR02Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr03 = TOK_EXPR03();
            node->add_child(child_tok_expr03);
            ast::AstNode *child_tok_expr02rest = TOK_EXPR02REST();
            node->add_child(child_tok_expr02rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR02");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR02REST() {
        CallContext ctx("TOK_EXPR02REST");
        ast::TOK_EXPR02RESTNode *node = new ast::TOK_EXPR02RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::None || current == TokenType::TOK_AMPEQUAL || current == TokenType::TOK_CARETEQUAL ||
            current == TokenType::TOK_EQUAL || current == TokenType::TOK_LSHIFTEQUAL || current ==
            TokenType::TOK_MINUSEQUAL || current == TokenType::TOK_PERCENTEQUAL || current == TokenType::TOK_PLUSEQUAL
            || current == TokenType::TOK_RSHIFTEQUAL || current == TokenType::TOK_SLASHEQUAL || current ==
            TokenType::TOK_STAREQUAL) {
            ast::AstNode *child_tok_assignop = TOK_ASSIGNOP();
            node->add_child(child_tok_assignop);
            ast::AstNode *child_tok_expr02 = TOK_EXPR02();
            node->add_child(child_tok_expr02);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR02REST");
        return nullptr;
    }

    ast::AstNode *TOK_EQUAL() {
        CallContext ctx("TOK_EQUAL");
        ast::TOK_EQUALNode *node = new ast::TOK_EQUALNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_EQUAL) {
            if (current != TokenType::TOK_EQUAL) {
                delete node;
                syntax_error("expected TOK_EQUAL in TOK_EQUAL");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EQUAL");
        return nullptr;
    }

    ast::AstNode *TOK_ASSIGNOP() {
        CallContext ctx("TOK_ASSIGNOP");
        ast::TOK_ASSIGNOPNode *node = new ast::TOK_ASSIGNOPNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_EQUAL) {
            ast::AstNode *child_tok_equal = TOK_EQUAL();
            node->add_child(child_tok_equal);
            return node;
        } else if (current == TokenType::TOK_PLUSEQUAL) {
            if (current != TokenType::TOK_PLUSEQUAL) {
                delete node;
                syntax_error("expected TOK_PLUSEQUAL in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_MINUSEQUAL) {
            if (current != TokenType::TOK_MINUSEQUAL) {
                delete node;
                syntax_error("expected TOK_MINUSEQUAL in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_STAREQUAL) {
            if (current != TokenType::TOK_STAREQUAL) {
                delete node;
                syntax_error("expected TOK_STAREQUAL in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_SLASHEQUAL) {
            if (current != TokenType::TOK_SLASHEQUAL) {
                delete node;
                syntax_error("expected TOK_SLASHEQUAL in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_PERCENTEQUAL) {
            if (current != TokenType::TOK_PERCENTEQUAL) {
                delete node;
                syntax_error("expected TOK_PERCENTEQUAL in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_LSHIFTEQUAL) {
            if (current != TokenType::TOK_LSHIFTEQUAL) {
                delete node;
                syntax_error("expected TOK_LSHIFTEQUAL in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_RSHIFTEQUAL) {
            if (current != TokenType::TOK_RSHIFTEQUAL) {
                delete node;
                syntax_error("expected TOK_RSHIFTEQUAL in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_AMPEQUAL) {
            if (current != TokenType::TOK_AMPEQUAL) {
                delete node;
                syntax_error("expected TOK_AMPEQUAL in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::None) {
            if (current != TokenType::None) {
                delete node;
                syntax_error("expected None in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_EQUAL) {
            if (current != TokenType::TOK_EQUAL) {
                delete node;
                syntax_error("expected TOK_EQUAL in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_CARETEQUAL) {
            if (current != TokenType::TOK_CARETEQUAL) {
                delete node;
                syntax_error("expected TOK_CARETEQUAL in TOK_ASSIGNOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_ASSIGNOP");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR03() {
        CallContext ctx("TOK_EXPR03");
        ast::TOK_EXPR03Node *node = new ast::TOK_EXPR03Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr04 = TOK_EXPR04();
            node->add_child(child_tok_expr04);
            ast::AstNode *child_tok_expr03rest = TOK_EXPR03REST();
            node->add_child(child_tok_expr03rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR03");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR03REST() {
        CallContext ctx("TOK_EXPR03REST");
        ast::TOK_EXPR03RESTNode *node = new ast::TOK_EXPR03RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_QMARK) {
            if (current != TokenType::TOK_QMARK) {
                delete node;
                syntax_error("expected TOK_QMARK in TOK_EXPR03REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr02 = TOK_EXPR02();
            node->add_child(child_tok_expr02);
            if (current != TokenType::TOK_COLON) {
                delete node;
                syntax_error("expected TOK_COLON in TOK_EXPR03REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr03 = TOK_EXPR03();
            node->add_child(child_tok_expr03);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR03REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR04() {
        CallContext ctx("TOK_EXPR04");
        ast::TOK_EXPR04Node *node = new ast::TOK_EXPR04Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr05 = TOK_EXPR05();
            node->add_child(child_tok_expr05);
            ast::AstNode *child_tok_expr04rest = TOK_EXPR04REST();
            node->add_child(child_tok_expr04rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR04");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR04REST() {
        CallContext ctx("TOK_EXPR04REST");
        ast::TOK_EXPR04RESTNode *node = new ast::TOK_EXPR04RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_OR) {
            if (current != TokenType::TOK_OR) {
                delete node;
                syntax_error("expected TOK_OR in TOK_EXPR04REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr05 = TOK_EXPR05();
            node->add_child(child_tok_expr05);
            ast::AstNode *child_tok_expr04rest = TOK_EXPR04REST();
            node->add_child(child_tok_expr04rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR04REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR05() {
        CallContext ctx("TOK_EXPR05");
        ast::TOK_EXPR05Node *node = new ast::TOK_EXPR05Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr06 = TOK_EXPR06();
            node->add_child(child_tok_expr06);
            ast::AstNode *child_tok_expr05rest = TOK_EXPR05REST();
            node->add_child(child_tok_expr05rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR05");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR05REST() {
        CallContext ctx("TOK_EXPR05REST");
        ast::TOK_EXPR05RESTNode *node = new ast::TOK_EXPR05RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_ANDAND) {
            if (current != TokenType::TOK_ANDAND) {
                delete node;
                syntax_error("expected TOK_ANDAND in TOK_EXPR05REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr06 = TOK_EXPR06();
            node->add_child(child_tok_expr06);
            ast::AstNode *child_tok_expr05rest = TOK_EXPR05REST();
            node->add_child(child_tok_expr05rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR05REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR06() {
        CallContext ctx("TOK_EXPR06");
        ast::TOK_EXPR06Node *node = new ast::TOK_EXPR06Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr07 = TOK_EXPR07();
            node->add_child(child_tok_expr07);
            ast::AstNode *child_tok_expr06rest = TOK_EXPR06REST();
            node->add_child(child_tok_expr06rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR06");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR06REST() {
        CallContext ctx("TOK_EXPR06REST");
        ast::TOK_EXPR06RESTNode *node = new ast::TOK_EXPR06RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::None) {
            if (current != TokenType::None) {
                delete node;
                syntax_error("expected None in TOK_EXPR06REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (true) {
            /* ε */
            return node;
        } else if (current == TokenType::None) {
            if (current != TokenType::None) {
                delete node;
                syntax_error("expected None in TOK_EXPR06REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr07 = TOK_EXPR07();
            node->add_child(child_tok_expr07);
            ast::AstNode *child_tok_expr06rest = TOK_EXPR06REST();
            node->add_child(child_tok_expr06rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR06REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR07() {
        CallContext ctx("TOK_EXPR07");
        ast::TOK_EXPR07Node *node = new ast::TOK_EXPR07Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr08 = TOK_EXPR08();
            node->add_child(child_tok_expr08);
            ast::AstNode *child_tok_expr07rest = TOK_EXPR07REST();
            node->add_child(child_tok_expr07rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR07");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR07REST() {
        CallContext ctx("TOK_EXPR07REST");
        ast::TOK_EXPR07RESTNode *node = new ast::TOK_EXPR07RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_CARET) {
            if (current != TokenType::TOK_CARET) {
                delete node;
                syntax_error("expected TOK_CARET in TOK_EXPR07REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr08 = TOK_EXPR08();
            node->add_child(child_tok_expr08);
            ast::AstNode *child_tok_expr07rest = TOK_EXPR07REST();
            node->add_child(child_tok_expr07rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR07REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR08() {
        CallContext ctx("TOK_EXPR08");
        ast::TOK_EXPR08Node *node = new ast::TOK_EXPR08Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr09 = TOK_EXPR09();
            node->add_child(child_tok_expr09);
            ast::AstNode *child_tok_expr08rest = TOK_EXPR08REST();
            node->add_child(child_tok_expr08rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR08");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR08REST() {
        CallContext ctx("TOK_EXPR08REST");
        ast::TOK_EXPR08RESTNode *node = new ast::TOK_EXPR08RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_AMP) {
            if (current != TokenType::TOK_AMP) {
                delete node;
                syntax_error("expected TOK_AMP in TOK_EXPR08REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr09 = TOK_EXPR09();
            node->add_child(child_tok_expr09);
            ast::AstNode *child_tok_expr08rest = TOK_EXPR08REST();
            node->add_child(child_tok_expr08rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR08REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR09() {
        CallContext ctx("TOK_EXPR09");
        ast::TOK_EXPR09Node *node = new ast::TOK_EXPR09Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr10 = TOK_EXPR10();
            node->add_child(child_tok_expr10);
            ast::AstNode *child_tok_expr09rest = TOK_EXPR09REST();
            node->add_child(child_tok_expr09rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR09");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR09REST() {
        CallContext ctx("TOK_EXPR09REST");
        ast::TOK_EXPR09RESTNode *node = new ast::TOK_EXPR09RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_EQEQ) {
            if (current != TokenType::TOK_EQEQ) {
                delete node;
                syntax_error("expected TOK_EQEQ in TOK_EXPR09REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr10 = TOK_EXPR10();
            node->add_child(child_tok_expr10);
            ast::AstNode *child_tok_expr09rest = TOK_EXPR09REST();
            node->add_child(child_tok_expr09rest);
            return node;
        } else if (current == TokenType::TOK_NEQ) {
            if (current != TokenType::TOK_NEQ) {
                delete node;
                syntax_error("expected TOK_NEQ in TOK_EXPR09REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr10 = TOK_EXPR10();
            node->add_child(child_tok_expr10);
            ast::AstNode *child_tok_expr09rest = TOK_EXPR09REST();
            node->add_child(child_tok_expr09rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR09REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR10() {
        CallContext ctx("TOK_EXPR10");
        ast::TOK_EXPR10Node *node = new ast::TOK_EXPR10Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr11 = TOK_EXPR11();
            node->add_child(child_tok_expr11);
            ast::AstNode *child_tok_expr10rest = TOK_EXPR10REST();
            node->add_child(child_tok_expr10rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR10");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR10REST() {
        CallContext ctx("TOK_EXPR10REST");
        ast::TOK_EXPR10RESTNode *node = new ast::TOK_EXPR10RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_LT) {
            if (current != TokenType::TOK_LT) {
                delete node;
                syntax_error("expected TOK_LT in TOK_EXPR10REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr11 = TOK_EXPR11();
            node->add_child(child_tok_expr11);
            ast::AstNode *child_tok_expr10rest = TOK_EXPR10REST();
            node->add_child(child_tok_expr10rest);
            return node;
        } else if (current == TokenType::TOK_GT) {
            if (current != TokenType::TOK_GT) {
                delete node;
                syntax_error("expected TOK_GT in TOK_EXPR10REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr11 = TOK_EXPR11();
            node->add_child(child_tok_expr11);
            ast::AstNode *child_tok_expr10rest = TOK_EXPR10REST();
            node->add_child(child_tok_expr10rest);
            return node;
        } else if (current == TokenType::TOK_LEQ) {
            if (current != TokenType::TOK_LEQ) {
                delete node;
                syntax_error("expected TOK_LEQ in TOK_EXPR10REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr11 = TOK_EXPR11();
            node->add_child(child_tok_expr11);
            ast::AstNode *child_tok_expr10rest = TOK_EXPR10REST();
            node->add_child(child_tok_expr10rest);
            return node;
        } else if (current == TokenType::TOK_GEQ) {
            if (current != TokenType::TOK_GEQ) {
                delete node;
                syntax_error("expected TOK_GEQ in TOK_EXPR10REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr11 = TOK_EXPR11();
            node->add_child(child_tok_expr11);
            ast::AstNode *child_tok_expr10rest = TOK_EXPR10REST();
            node->add_child(child_tok_expr10rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR10REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR11() {
        CallContext ctx("TOK_EXPR11");
        ast::TOK_EXPR11Node *node = new ast::TOK_EXPR11Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr12 = TOK_EXPR12();
            node->add_child(child_tok_expr12);
            ast::AstNode *child_tok_expr11rest = TOK_EXPR11REST();
            node->add_child(child_tok_expr11rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR11");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR11REST() {
        CallContext ctx("TOK_EXPR11REST");
        ast::TOK_EXPR11RESTNode *node = new ast::TOK_EXPR11RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_LSHIFT) {
            if (current != TokenType::TOK_LSHIFT) {
                delete node;
                syntax_error("expected TOK_LSHIFT in TOK_EXPR11REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr12 = TOK_EXPR12();
            node->add_child(child_tok_expr12);
            ast::AstNode *child_tok_expr11rest = TOK_EXPR11REST();
            node->add_child(child_tok_expr11rest);
            return node;
        } else if (current == TokenType::TOK_RSHIFT) {
            if (current != TokenType::TOK_RSHIFT) {
                delete node;
                syntax_error("expected TOK_RSHIFT in TOK_EXPR11REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr12 = TOK_EXPR12();
            node->add_child(child_tok_expr12);
            ast::AstNode *child_tok_expr11rest = TOK_EXPR11REST();
            node->add_child(child_tok_expr11rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR11REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR12() {
        CallContext ctx("TOK_EXPR12");
        ast::TOK_EXPR12Node *node = new ast::TOK_EXPR12Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr13 = TOK_EXPR13();
            node->add_child(child_tok_expr13);
            ast::AstNode *child_tok_expr12rest = TOK_EXPR12REST();
            node->add_child(child_tok_expr12rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR12");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR12REST() {
        CallContext ctx("TOK_EXPR12REST");
        ast::TOK_EXPR12RESTNode *node = new ast::TOK_EXPR12RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_PLUS) {
            if (current != TokenType::TOK_PLUS) {
                delete node;
                syntax_error("expected TOK_PLUS in TOK_EXPR12REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr13 = TOK_EXPR13();
            node->add_child(child_tok_expr13);
            ast::AstNode *child_tok_expr12rest = TOK_EXPR12REST();
            node->add_child(child_tok_expr12rest);
            return node;
        } else if (current == TokenType::TOK_MINUS) {
            if (current != TokenType::TOK_MINUS) {
                delete node;
                syntax_error("expected TOK_MINUS in TOK_EXPR12REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr13 = TOK_EXPR13();
            node->add_child(child_tok_expr13);
            ast::AstNode *child_tok_expr12rest = TOK_EXPR12REST();
            node->add_child(child_tok_expr12rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR12REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR13() {
        CallContext ctx("TOK_EXPR13");
        ast::TOK_EXPR13Node *node = new ast::TOK_EXPR13Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr14 = TOK_EXPR14();
            node->add_child(child_tok_expr14);
            ast::AstNode *child_tok_expr13rest = TOK_EXPR13REST();
            node->add_child(child_tok_expr13rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR13");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR13REST() {
        CallContext ctx("TOK_EXPR13REST");
        ast::TOK_EXPR13RESTNode *node = new ast::TOK_EXPR13RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_STAR) {
            if (current != TokenType::TOK_STAR) {
                delete node;
                syntax_error("expected TOK_STAR in TOK_EXPR13REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr14 = TOK_EXPR14();
            node->add_child(child_tok_expr14);
            ast::AstNode *child_tok_expr13rest = TOK_EXPR13REST();
            node->add_child(child_tok_expr13rest);
            return node;
        } else if (current == TokenType::TOK_SLASH) {
            if (current != TokenType::TOK_SLASH) {
                delete node;
                syntax_error("expected TOK_SLASH in TOK_EXPR13REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr14 = TOK_EXPR14();
            node->add_child(child_tok_expr14);
            ast::AstNode *child_tok_expr13rest = TOK_EXPR13REST();
            node->add_child(child_tok_expr13rest);
            return node;
        } else if (current == TokenType::TOK_PERCENT) {
            if (current != TokenType::TOK_PERCENT) {
                delete node;
                syntax_error("expected TOK_PERCENT in TOK_EXPR13REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expr14 = TOK_EXPR14();
            node->add_child(child_tok_expr14);
            ast::AstNode *child_tok_expr13rest = TOK_EXPR13REST();
            node->add_child(child_tok_expr13rest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR13REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR14() {
        CallContext ctx("TOK_EXPR14");
        ast::TOK_EXPR14Node *node = new ast::TOK_EXPR14Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_EXCL || current == TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS ||
            current == TokenType::TOK_PLUSPLUS) {
            ast::AstNode *child_tok_unaryop = TOK_UNARYOP();
            node->add_child(child_tok_unaryop);
            ast::AstNode *child_tok_expr14 = TOK_EXPR14();
            node->add_child(child_tok_expr14);
            return node;
        } else if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING ||
                   current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
                   TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr15 = TOK_EXPR15();
            node->add_child(child_tok_expr15);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR14");
        return nullptr;
    }

    ast::AstNode *TOK_UNARYOP() {
        CallContext ctx("TOK_UNARYOP");
        ast::TOK_UNARYOPNode *node = new ast::TOK_UNARYOPNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_EXCL) {
            if (current != TokenType::TOK_EXCL) {
                delete node;
                syntax_error("expected TOK_EXCL in TOK_UNARYOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_MINUS) {
            if (current != TokenType::TOK_MINUS) {
                delete node;
                syntax_error("expected TOK_MINUS in TOK_UNARYOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_PLUSPLUS) {
            if (current != TokenType::TOK_PLUSPLUS) {
                delete node;
                syntax_error("expected TOK_PLUSPLUS in TOK_UNARYOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_MINUSMINUS) {
            if (current != TokenType::TOK_MINUSMINUS) {
                delete node;
                syntax_error("expected TOK_MINUSMINUS in TOK_UNARYOP");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_UNARYOP");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR15() {
        CallContext ctx("TOK_EXPR15");
        ast::TOK_EXPR15Node *node = new ast::TOK_EXPR15Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expr16 = TOK_EXPR16();
            node->add_child(child_tok_expr16);
            ast::AstNode *child_tok_expr15rest = TOK_EXPR15REST();
            node->add_child(child_tok_expr15rest);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR15");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR15REST() {
        CallContext ctx("TOK_EXPR15REST");
        ast::TOK_EXPR15RESTNode *node = new ast::TOK_EXPR15RESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_PLUSPLUS) {
            if (current != TokenType::TOK_PLUSPLUS) {
                delete node;
                syntax_error("expected TOK_PLUSPLUS in TOK_EXPR15REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_MINUSMINUS) {
            if (current != TokenType::TOK_MINUSMINUS) {
                delete node;
                syntax_error("expected TOK_MINUSMINUS in TOK_EXPR15REST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR15REST");
        return nullptr;
    }

    ast::AstNode *TOK_EXPR16() {
        CallContext ctx("TOK_EXPR16");
        ast::TOK_EXPR16Node *node = new ast::TOK_EXPR16Node();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_atom = TOK_ATOM();
            node->add_child(child_tok_atom);
            ast::AstNode *child_tok_postfixtail = TOK_POSTFIXTAIL();
            node->add_child(child_tok_postfixtail);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_EXPR16");
        return nullptr;
    }

    ast::AstNode *TOK_ATOM() {
        CallContext ctx("TOK_ATOM");
        ast::TOK_ATOMNode *node = new ast::TOK_ATOMNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER) {
            ast::AstNode *child_tok_id = TOK_ID();
            node->add_child(child_tok_id);
            return node;
        } else if (current == TokenType::NUMBER || current == TokenType::STRING) {
            ast::AstNode *child_tok_literal = TOK_LITERAL();
            node->add_child(child_tok_literal);
            return node;
        } else if (current == TokenType::TOK_LPAREN) {
            if (current != TokenType::TOK_LPAREN) {
                delete node;
                syntax_error("expected TOK_LPAREN in TOK_ATOM");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            if (current != TokenType::TOK_RPAREN) {
                delete node;
                syntax_error("expected TOK_RPAREN in TOK_ATOM");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_INPUT || current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_ino = TOK_INO();
            node->add_child(child_tok_ino);
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_ATOM");
        return nullptr;
    }

    ast::AstNode *TOK_POSTFIXTAIL() {
        CallContext ctx("TOK_POSTFIXTAIL");
        ast::TOK_POSTFIXTAILNode *node = new ast::TOK_POSTFIXTAILNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_DOT || current == TokenType::TOK_LBRACKET || current == TokenType::TOK_LPAREN) {
            ast::AstNode *child_tok_postfix_item = TOK_POSTFIX_ITEM();
            node->add_child(child_tok_postfix_item);
            ast::AstNode *child_tok_postfixtail = TOK_POSTFIXTAIL();
            node->add_child(child_tok_postfixtail);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_POSTFIXTAIL");
        return nullptr;
    }

    ast::AstNode *TOK_POSTFIX_ITEM() {
        CallContext ctx("TOK_POSTFIX_ITEM");
        ast::TOK_POSTFIX_ITEMNode *node = new ast::TOK_POSTFIX_ITEMNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_DOT) {
            if (current != TokenType::TOK_DOT) {
                delete node;
                syntax_error("expected TOK_DOT in TOK_POSTFIX_ITEM");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_id = TOK_ID();
            node->add_child(child_tok_id);
            return node;
        } else if (current == TokenType::TOK_LBRACKET) {
            if (current != TokenType::TOK_LBRACKET) {
                delete node;
                syntax_error("expected TOK_LBRACKET in TOK_POSTFIX_ITEM");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            if (current != TokenType::TOK_RBRACKET) {
                delete node;
                syntax_error("expected TOK_RBRACKET in TOK_POSTFIX_ITEM");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_LPAREN) {
            if (current != TokenType::TOK_LPAREN) {
                delete node;
                syntax_error("expected TOK_LPAREN in TOK_POSTFIX_ITEM");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_arglist = TOK_ARGLIST();
            node->add_child(child_tok_arglist);
            if (current != TokenType::TOK_RPAREN) {
                delete node;
                syntax_error("expected TOK_RPAREN in TOK_POSTFIX_ITEM");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_POSTFIX_ITEM");
        return nullptr;
    }

    ast::AstNode *TOK_INO() {
        CallContext ctx("TOK_INO");
        ast::TOK_INONode *node = new ast::TOK_INONode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_PRINT) {
            if (current != TokenType::TOK_PRINT) {
                delete node;
                syntax_error("expected TOK_PRINT in TOK_INO");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            if (current != TokenType::TOK_LPAREN) {
                delete node;
                syntax_error("expected TOK_LPAREN in TOK_INO");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_arglist = TOK_ARGLIST();
            node->add_child(child_tok_arglist);
            if (current != TokenType::TOK_RPAREN) {
                delete node;
                syntax_error("expected TOK_RPAREN in TOK_INO");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::TOK_INPUT) {
            if (current != TokenType::TOK_INPUT) {
                delete node;
                syntax_error("expected TOK_INPUT in TOK_INO");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            if (current != TokenType::TOK_LPAREN) {
                delete node;
                syntax_error("expected TOK_LPAREN in TOK_INO");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_arglist = TOK_ARGLIST();
            node->add_child(child_tok_arglist);
            if (current != TokenType::TOK_RPAREN) {
                delete node;
                syntax_error("expected TOK_RPAREN in TOK_INO");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_INO");
        return nullptr;
    }

    ast::AstNode *TOK_ARGLIST() {
        CallContext ctx("TOK_ARGLIST");
        ast::TOK_ARGLISTNode *node = new ast::TOK_ARGLISTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::IDENTIFIER || current == TokenType::NUMBER || current == TokenType::STRING || current
            == TokenType::TOK_EXCL || current == TokenType::TOK_INPUT || current == TokenType::TOK_LPAREN || current ==
            TokenType::TOK_MINUS || current == TokenType::TOK_MINUSMINUS || current == TokenType::TOK_PLUSPLUS ||
            current == TokenType::TOK_PRINT) {
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            ast::AstNode *child_tok_arglistrest = TOK_ARGLISTREST();
            node->add_child(child_tok_arglistrest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_ARGLIST");
        return nullptr;
    }

    ast::AstNode *TOK_ARGLISTREST() {
        CallContext ctx("TOK_ARGLISTREST");
        ast::TOK_ARGLISTRESTNode *node = new ast::TOK_ARGLISTRESTNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::TOK_COMMA) {
            if (current != TokenType::TOK_COMMA) {
                delete node;
                syntax_error("expected TOK_COMMA in TOK_ARGLISTREST");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            ast::AstNode *child_tok_expression = TOK_EXPRESSION();
            node->add_child(child_tok_expression);
            ast::AstNode *child_tok_arglistrest = TOK_ARGLISTREST();
            node->add_child(child_tok_arglistrest);
            return node;
        } else if (true) {
            /* ε */
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_ARGLISTREST");
        return nullptr;
    }

    ast::AstNode *TOK_LITERAL() {
        CallContext ctx("TOK_LITERAL");
        ast::TOK_LITERALNode *node = new ast::TOK_LITERALNode();
        node->line = current_token.line;
        node->col = current_token.col;

        if (current == TokenType::NUMBER) {
            if (current != TokenType::NUMBER) {
                delete node;
                syntax_error("expected NUMBER in TOK_LITERAL");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        } else if (current == TokenType::STRING) {
            if (current != TokenType::STRING) {
                delete node;
                syntax_error("expected STRING in TOK_LITERAL");
            }
            node->add_child(new ast::TerminalNode(current, current_token.value, current_token.name, current_token.line,
                                                  current_token.col));
            gc();
            return node;
        }
        delete node;
        syntax_error("unexpected token " + token_to_string(current) + " in TOK_LITERAL");
        return nullptr;
    }
} // namespace parser
