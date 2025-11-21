#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include <map>
#include <functional>
// Включаем заголовок Poliz для объявлений
#include "poliz.hpp"

// Используем ваши пути к другим заголовкам:
#include "ast.hpp"
#include "ast_utils.hpp"
#include "parser.hpp" // Для TokenType

namespace poliz {

    // --- Реализация конструктора PolizItem ---
    // Этот конструктор был объявлен в poliz.hpp, но не определен явно в poliz.cpp
    PolizItem::PolizItem(std::string v) : op(OpType::PUSH_VAL), value(v) {}

    // --- Вспомогательная функция для маппинга токенов в операции ---
    // Вынесена из тела класса Poliz с указанием области видимости
    OpType Poliz::token_to_op(parser::TokenType tt, const std::string& val) {
        using namespace parser;
        switch (tt) {
            case TokenType::TOK_PLUS: return OpType::ADD;
            case TokenType::TOK_MINUS: return OpType::SUB;
            case TokenType::TOK_STAR: return OpType::MUL;
            case TokenType::TOK_SLASH: return OpType::DIV;
            case TokenType::TOK_PERCENT: return OpType::MOD;
            case TokenType::TOK_EQUAL: return OpType::ASSIGN; // =
            case TokenType::TOK_EQEQ: return OpType::EQ;      // ==
            case TokenType::TOK_NEQ: return OpType::NEQ;
            case TokenType::TOK_LT: return OpType::LT;
            case TokenType::TOK_GT: return OpType::GT;
            case TokenType::TOK_LEQ: return OpType::LEQ;
            case TokenType::TOK_GEQ: return OpType::GEQ;
            case TokenType::TOK_DOT: return OpType::MEMBER;
            default: return OpType::NONE;
        }
    }

    // --- Реализация get_items ---
    const std::vector<PolizItem>& Poliz::get_items() const {
        return items;
    }

    // --- Основная функция generate (Генерация ПОЛИЗа) ---
    // Вынесена из тела класса Poliz с указанием области видимости
    void Poliz::generate(ast::AstNode* node) {
        if (!node) return;

        std::string type = node->to_string();

        // 1. Программа / Блок кода / Список инструкций (Рекурсивный обход)
        if (type == "TOK_PROGRAM" || type == "TOK_COMPOUNDSTMT" || type == "TOK_STMTLIST" || type.find("REST") != std::string::npos || type == "TOK_BLOCK") {
            for (auto* child : node->children) {
                generate(child);
            }
        }
        // 2. Объявление переменных (int a = 5;)
        else if (type == "TOK_LOCALVARDECL") {
            ast::AstNode* id_node = nullptr;
            ast::AstNode* expr_node = nullptr;

            for (auto* c : node->children) {
                if (c->to_string() == "TOK_ID") id_node = c;
                if (c->to_string() == "TOK_VARDECLREST") {
                     // Вспомогательная рекурсивная функция для поиска выражения
                     std::function<ast::AstNode*(ast::AstNode*)> find_expr =
                         [&](ast::AstNode* n) -> ast::AstNode* {
                         if(!n) return nullptr;
                         if (n->to_string().find("EXPR") != std::string::npos && n->children.size() > 0) return n;
                         for(auto* sub : n->children) {
                             if(auto* found = find_expr(sub)) return found;
                         }
                         return nullptr;
                     };
                     expr_node = find_expr(c);
                }
            }

            if (id_node && expr_node) {
                // Структура: ID, EXPR, ASSIGN
                generate(id_node); // PUSH ID
                generate(expr_node); // PUSH Value
                items.emplace_back(OpType::ASSIGN);
            }
        }
        // 3. Выражение (Expression) - Обработка в постфиксной форме
        else if (type.find("EXPR") != std::string::npos || type == "TOK_VARDECLREST") {

            ast::AstNode* op_node = nullptr;
            int op_idx = -1;

            // Ищем оператор в списке потомков
            for(size_t i=0; i<node->children.size(); ++i) {
                auto* child = node->children[i];
                if (child->to_string().find("OP") != std::string::npos ||
                    child->to_string().find("EQUAL") != std::string::npos ||
                    child->to_string() == "TOK_ASSIGNOP" ||
                    child->to_string().find("EQEQ") != std::string::npos ||
                    child->to_string().find("NEQ") != std::string::npos ||
                    child->to_string().find("LT") != std::string::npos ||
                    child->to_string().find("GT") != std::string::npos) {
                    op_node = child;
                    op_idx = i;
                    break;
                }
            }

            if (op_node) {
                // Постфиксная запись: [Левая часть] [Правая часть] [Оператор]
                for (int i = 0; i < op_idx; ++i) generate(node->children[i]); // Левая часть
                for (size_t i = op_idx + 1; i < node->children.size(); ++i) generate(node->children[i]); // Правая часть

                // Получаем терминальный узел оператора
                ast::TerminalNode* tn = nullptr;
                if (!op_node->children.empty()) {
                    tn = dynamic_cast<ast::TerminalNode*>(op_node->children[0]);
                }
                if (!tn) {
                    tn = dynamic_cast<ast::TerminalNode*>(op_node);
                }

                if (tn) {
                     items.emplace_back(token_to_op(tn->token_type, tn->value)); // Оператор
                }

            } else {
                // Если оператора нет, просто рекурсивно обходим
                for (auto* child : node->children) generate(child);
            }
        }
        // 4. IF (Условный оператор) - Генерация переходов
        else if (type == "TOK_IFSTMT") {
            ast::AstNode* cond = nullptr;
            ast::AstNode* then_block = nullptr;
            ast::AstNode* else_part = nullptr;

            for (auto* c : node->children) {
                std::string ctype = c->to_string();
                if (ctype.find("EXPR") != std::string::npos && !cond) cond = c;
                else if (ctype == "TOK_COMPOUNDSTMT" && !then_block) then_block = c;
                else if (ctype == "TOK_ELSEPART") else_part = c;
            }

            if (cond && then_block) {
                generate(cond); // 1. Условие

                int jz_idx = items.size();
                items.emplace_back(OpType::JMP_FALSE, -1); // 2. Резервируем JMP_FALSE (прыжок через THEN)

                generate(then_block); // 3. Тело IF

                if (else_part && !else_part->children.empty()) {
                    ast::AstNode* else_block = else_part->children[0];

                    int jmp_idx = items.size();
                    items.emplace_back(OpType::GOTO, -1); // 4. Безусловный прыжок через ELSE

                    items[jz_idx].jump_index = items.size(); // 5. Патчим JMP_FALSE на начало ELSE

                    generate(else_block); // 6. Тело ELSE

                    items[jmp_idx].jump_index = items.size(); // 7. Патчим GOTO на конец IF/ELSE
                } else {
                    items[jz_idx].jump_index = items.size(); // Патчим JMP_FALSE на конец IF
                }
            }
        }
        // 5. WHILE (Цикл) - Генерация цикла
        else if (type == "TOK_WHILESTMT") {
            ast::AstNode* cond = nullptr;
            ast::AstNode* body = nullptr;

            for (auto* c : node->children) {
                if (c->to_string().find("EXPR") != std::string::npos) cond = c;
                if (c->to_string() == "TOK_COMPOUNDSTMT") body = c;
            }

            if (cond && body) {
                int start_label = items.size(); // Метка начала цикла

                generate(cond); // Условие

                int jz_idx = items.size();
                items.emplace_back(OpType::JMP_FALSE, -1); // Выход из цикла

                generate(body); // Тело

                items.emplace_back(OpType::GOTO, start_label); // Прыжок назад в начало

                items[jz_idx].jump_index = items.size(); // Патчим выход
            }
        }
        // 6. Терминалы (Листья) - PUSH
        else if (type.find("Terminal") != std::string::npos) {
            auto* term = dynamic_cast<ast::TerminalNode*>(node);
            if (term) {
                if (term->token_type == parser::TokenType::IDENTIFIER ||
                    term->token_type == parser::TokenType::NUMBER ||
                    term->token_type == parser::TokenType::STRING) {

                    items.emplace_back(term->value);
                }
                else {
                    OpType op = token_to_op(term->token_type, term->value);
                    if (op != OpType::NONE) items.emplace_back(op);
                }
            }
        }
        // 7. Вызов функции / Постфикс
        else if (type == "TOK_POSTFIX_ITEM" || type == "TOK_INO") {
            for (auto* c : node->children) {
                 generate(c);
            }

            bool is_call = false;
            bool is_member = false;

            for(auto* c : node->children) {
                if (c->to_string() == "TOK_ARGLIST") is_call = true;
                if (auto* t = dynamic_cast<ast::TerminalNode*>(c)) {
                    if(t->value == ".") is_member = true;
                }
            }

            if (is_call) {
                 // Специальная обработка для 'print'
                 if (!node->children.empty()) {
                      if (auto* t = dynamic_cast<ast::TerminalNode*>(node->children[0])) {
                          if (t->value == "print") {
                              items.pop_back(); // Удаляем 'print'
                              items.emplace_back(OpType::PRINT);
                              return;
                          }
                      }
                 }
                 items.emplace_back(OpType::CALL);
            } else if (is_member) {
                items.emplace_back(OpType::MEMBER);
            }
        }
        else {
            // Fallback (рекурсивный обход для необработанных узлов)
            for (auto* child : node->children) {
                generate(child);
            }
        }
    }

    // --- Реализация print (Печать ПОЛИЗа) ---
    // Вынесена из тела класса Poliz с указанием области видимости
    void Poliz::print() {
        std::cout << "--- POLIZ ---\n";
        for (int i = 0; i < items.size(); ++i) {
            std::cout << i << ": ";
            const auto& item = items[i];
            switch (item.op) {
                case OpType::PUSH_VAL: std::cout << "PUSH " << item.value; break;
                case OpType::ADD: std::cout << "ADD"; break;
                case OpType::SUB: std::cout << "SUB"; break;
                case OpType::MUL: std::cout << "MUL"; break;
                case OpType::DIV: std::cout << "DIV"; break;
                case OpType::MOD: std::cout << "MOD"; break;
                case OpType::ASSIGN: std::cout << "ASSIGN"; break;
                case OpType::GOTO: std::cout << "GOTO " << item.jump_index; break;
                case OpType::JMP_FALSE: std::cout << "JMP_FALSE " << item.jump_index; break;
                case OpType::PRINT: std::cout << "PRINT"; break;
                case OpType::MEMBER: std::cout << "DOT_ACCESS"; break;
                case OpType::EQ: std::cout << "EQ (==)"; break;
                case OpType::NEQ: std::cout << "NEQ (!=)"; break;
                case OpType::LT: std::cout << "LT (<)"; break;
                case OpType::GT: std::cout << "GT (>)"; break;
                case OpType::LEQ: std::cout << "LEQ (<=)"; break;
                case OpType::GEQ: std::cout << "GEQ (>=)"; break;
                case OpType::CALL: std::cout << "CALL"; break;
                case OpType::INPUT: std::cout << "INPUT"; break;
                default: std::cout << "OP_" << (int)item.op; break;
            }
            std::cout << "\n";
        }
        std::cout << "-------------\n";
    }
} // namespace poliz