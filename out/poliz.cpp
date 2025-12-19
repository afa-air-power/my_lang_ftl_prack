#include "poliz.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>

namespace poliz {

    // Вспомогательная функция для имен операций
    std::string op_to_str(OpType op) {
        switch(op) {
            case OpType::LOAD: return "LOAD";
            case OpType::STORE: return "STORE";
            case OpType::MOV: return "MOV";
            case OpType::ADD: return "ADD";
            case OpType::SUB: return "SUB";
            case OpType::MUL: return "MUL";
            case OpType::DIV: return "DIV";
            case OpType::CMP: return "CMP";
            case OpType::JMP: return "JMP";
            case OpType::JE:  return "JE";
            case OpType::JNE: return "JNE";
            case OpType::RET: return "RET";
            case OpType::HALT: return "HALT";
            case OpType::LABEL: return "LABEL";
            default: return "UNKNOWN";
        }
    }

    // Вспомогательная функция для вывода операндов
    std::string arg_to_str(const Operand& op) {
        switch(op.type) {
            case OperandType::REGISTER:  return "r" + std::to_string(op.reg_num);
            case OperandType::IMMEDIATE: return "#" + op.value;
            case OperandType::VARIABLE:  return "[" + op.value + "]";
            case OperandType::LABEL:     return "@" + op.value;
            default: return "";
        }
    }

    // =============================================================================
    // POLIZ GENERATOR
    // =============================================================================

    int Poliz::alloc_register() {
        if (next_reg >= MAX_REGS) {
            // Если регистры кончились, используем стек
            emit(Instruction(OpType::PUSH, Operand(0)));
            return 0; // Переиспользуем r0
        }
        return next_reg++;
    }

    void Poliz::free_register(int reg) {
        if (reg >= 0 && reg < next_reg && reg == next_reg - 1) {
            next_reg--;
        }
    }

    std::string Poliz::new_label(const std::string &prefix) {
        return prefix + std::to_string(next_label++);
    }

    std::string Poliz::new_temp() {
        return "_t" + std::to_string(next_temp++);
    }

    void Poliz::emit(const Instruction &instr) {
        code.push_back(instr);
    }

    void Poliz::emit_label(const std::string &label) {
        Instruction instr(OpType::LABEL);
        instr.label = label;
        label_map[label] = code.size();
        emit(instr);
    }

    Operand Poliz::generate_expr(ast::AstNode *node) {
        if (!node) return Operand();

        std::string nodename = node->to_string();

        // Обработка TOK_LITERAL
        if (nodename == "TOK_LITERAL") {
            // Найти NUMBER или STRING внутри
            for (auto *c: node->children) {
                if (auto *tn = dynamic_cast<ast::TerminalNode *>(c)) {
                    if (tn->token_type == parser::TokenType::NUMBER) {
                        int reg = alloc_register();
                        Operand dst(reg);
                        Operand src(OperandType::IMMEDIATE, tn->value);
                        emit(Instruction(OpType::LOAD, dst, src));
                        return dst;
                    }
                    if (tn->token_type == parser::TokenType::STRING) {
                        int reg = alloc_register();
                        Operand dst(reg);
                        Operand src(OperandType::IMMEDIATE, tn->value);
                        emit(Instruction(OpType::LOAD, dst, src));
                        return dst;
                    }
                }
            }
            return Operand();
        }

        // Обработка TOK_ID узлов (содержат идентификатор и TOK_IDREST)
        if (nodename == "TOK_ID") {
            // Найти IDENTIFIER терминал в детях
            for (auto *c: node->children) {
                if (auto *tn = dynamic_cast<ast::TerminalNode *>(c)) {
                    if (tn->token_type == parser::TokenType::IDENTIFIER) {
                        // Загрузить переменную
                        int reg = alloc_register();
                        Operand dst(reg);
                        Operand src(OperandType::VARIABLE, tn->value);
                        emit(Instruction(OpType::LOAD, dst, src));
                        return dst;
                    }
                }
            }
            return Operand();
        }

        // Терминалы
        if (nodename.find("Terminal") != std::string::npos) {
            auto *term = dynamic_cast<ast::TerminalNode *>(node);
            if (!term) return Operand();

            if (term->token_type == parser::TokenType::NUMBER) {
                // Загрузить константу в регистр
                int reg = alloc_register();
                Operand dst(reg);
                Operand src(OperandType::IMMEDIATE, term->value);
                emit(Instruction(OpType::LOAD, dst, src));
                return dst;
            }

            if (term->token_type == parser::TokenType::IDENTIFIER) {
                // Загрузить переменную
                int reg = alloc_register();
                Operand dst(reg);
                Operand src(OperandType::VARIABLE, term->value);
                emit(Instruction(OpType::LOAD, dst, src));
                return dst;
            }

            if (term->token_type == parser::TokenType::STRING) {
                int reg = alloc_register();
                Operand dst(reg);
                Operand src(OperandType::IMMEDIATE, term->value);
                emit(Instruction(OpType::LOAD, dst, src));
                return dst;
            }
        }

        // Если это нетерминал выражения, рекурсивно обработать
        if (nodename.find("EXPR") != std::string::npos || nodename.find("ATOM") != std::string::npos || nodename.
            find("ARG") != std::string::npos) {
            if (node->children.empty()) {
                return Operand();
            }

            // Специальная обработка для EXPR16 с POSTFIXTAIL (функции)
            if (nodename == "TOK_EXPR16" && node->children.size() >= 2) {
                ast::AstNode *atom = node->children[0];

                ast::AstNode *postfix = node->children[1];

                if (atom && postfix && postfix->to_string() == "TOK_POSTFIXTAIL") {
                    // Найти функцию в ATOM
                    std::string func_name;

                    std::function<bool(ast::AstNode *)> find_func_name =
                            [&](ast::AstNode *n) -> bool {
                        if (auto *tn = dynamic_cast<ast::TerminalNode *>(n)) {
                            if (tn->token_type == parser::TokenType::IDENTIFIER) {
                                func_name = tn->value;
                                return true;
                            }
                        }
                        for (auto *child: n->children) {
                            if (find_func_name(child)) return true;
                        }
                        return false;
                    };

                    find_func_name(atom);

                    if (!func_name.empty() && !postfix->children.empty()) {
                        // Найти аргументы в POSTFIX_ITEM
                        for (auto *item: postfix->children) {
                            if (item->to_string() == "TOK_POSTFIX_ITEM") {
                                // Ищем TOK_ARGLIST в POSTFIX_ITEM
                                for (auto *child: item->children) {
                                    if (child->to_string() == "TOK_ARGLIST") {
                                        // Обработать аргументы
                                        if (func_name == "print") {
                                            // ИСПРАВЛЕНО: Правильная обработка для print
                                            std::function<void(ast::AstNode *)> process_arglist;
                                            process_arglist = [&](ast::AstNode *arglist_node) {
                                                if (!arglist_node) return;

                                                for (auto *arg_child: arglist_node->children) {
                                                    std::string arg_name = arg_child->to_string();

                                                    // Пропускаем запятые и REST узлы
                                                    if (arg_name == "TOK_ARGLISTREST") {
                                                        process_arglist(arg_child);
                                                        continue;
                                                    }

                                                    // Обрабатываем выражения
                                                    if (arg_name == "TOK_EXPRESSION" ||
                                                        arg_name.find("EXPR") != std::string::npos) {
                                                        Operand val = generate_expr(arg_child);
                                                        if (val.type != OperandType::NONE) {
                                                            emit(Instruction(OpType::PRINT, val));
                                                            free_register(val.reg_num);
                                                        }
                                                    }
                                                }
                                            };
                                            process_arglist(child);
                                            return Operand();
                                        } else if (func_name == "input") {
                                            // ДОБАВЛЕНО: Обработка input
                                            int result_reg = alloc_register();
                                            Operand result(result_reg);
                                            emit(Instruction(OpType::INPUT, result));
                                            return result;
                                        } else {
                                            // Обычная функция
                                            std::vector<Operand> args;

                                            std::function<void(ast::AstNode *)> collect_args;
                                            collect_args = [&](ast::AstNode *arglist_node) {
                                                if (!arglist_node) return;

                                                for (auto *arg_child: arglist_node->children) {
                                                    std::string arg_name = arg_child->to_string();

                                                    if (arg_name == "TOK_ARGLISTREST") {
                                                        collect_args(arg_child);
                                                        continue;
                                                    }

                                                    if (arg_name == "TOK_EXPRESSION" ||
                                                        arg_name.find("EXPR") != std::string::npos) {
                                                        Operand val = generate_expr(arg_child);
                                                        if (val.type != OperandType::NONE) {
                                                            emit(Instruction(OpType::PUSH, val));
                                                            args.push_back(val);
                                                        }
                                                    }
                                                }
                                            };

                                            collect_args(child);

                                            // Вызов функции
                                            emit(Instruction(OpType::CALL,
                                                             Operand(OperandType::LABEL, func_name)));

                                            // Освободить регистры аргументов
                                            for (auto &arg: args) {
                                                free_register(arg.reg_num);
                                            }

                                            // Результат возвращается в r0
                                            int result_reg = alloc_register();
                                            emit(Instruction(OpType::MOV, Operand(result_reg),
                                                             Operand(0)));
                                            return Operand(result_reg);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (node->children.size() == 1) {
                return generate_expr(node->children[0]);
            }

            // Проверить есть ли в этом выражении оператор присваивания
            // Если да - обработать специально
            if (node->children.size() >= 2) {
                ast::AstNode *rest = node->children[1];
                if (rest && !rest->children.empty()) {
                    // Ищем ASSIGNOP в rest
                    for (auto *c: rest->children) {
                        if (c->to_string() == "TOK_ASSIGNOP") {
                            // Это присваивание!
                            // Левая часть - это node->children[0] (переменная)
                            // Ищем переменную в левой части
                            std::string var_name;

                            std::function<bool(ast::AstNode*)> find_var_name =
                                [&](ast::AstNode *n) -> bool {
                                    if (auto *tn = dynamic_cast<ast::TerminalNode *>(n)) {
                                        if (tn->token_type == parser::TokenType::IDENTIFIER) {
                                            var_name = tn->value;
                                            return true;
                                        }
                                    }
                                    for (auto *child: n->children) {
                                        if (find_var_name(child)) return true;
                                    }
                                    return false;
                                };

                            find_var_name(node->children[0]);

                            if (!var_name.empty()) {
                                // Ищем выражение для присваивания (TOK_EXPR02 в rest)
                                ast::AstNode *right_expr = nullptr;
                                for (auto *sibling: rest->children) {
                                    if (sibling->to_string().find("EXPR") != std::string::npos) {
                                        right_expr = sibling;
                                        break;
                                    }
                                }

                                if (right_expr) {
                                    Operand value = generate_expr(right_expr);
                                    Operand var_op(OperandType::VARIABLE, var_name);
                                    emit(Instruction(OpType::STORE, var_op, value));
                                    free_register(value.reg_num);
                                    return value;  // Возвращаем значение присваивания
                                }
                            }
                            return Operand();
                        }
                    }
                }
            }

            // Нормальное выражение без присваивания
            Operand left = generate_expr(node->children[0]);

            if (node->children.size() >= 2) {
                ast::AstNode *rest = node->children[1];
                if (!rest) return left;

                // Найти оператор в rest
                ast::TerminalNode *op_node = nullptr;
                ast::AstNode *right_expr = nullptr;

                // Сначала смотрим прямо в rest->children
                for (size_t i = 0; i < rest->children.size(); i++) {
                    if (auto *tn = dynamic_cast<ast::TerminalNode *>(rest->children[i])) {
                        op_node = tn;
                        if (i + 1 < rest->children.size()) {
                            right_expr = rest->children[i + 1];
                        }
                        break;
                    }
                }

                // Если не нашли, пробуем ещё раз пропустив нетерминалы
                if (!op_node && rest->children.size() > 0) {
                    // Может быть структура другая - ищем рекурсивно
                    std::function<void(ast::AstNode*)> search_deeper = [&](ast::AstNode* n) {
                        if (!n || op_node) return;

                        for (size_t i = 0; i < n->children.size(); i++) {
                            auto *c = n->children[i];
                            if (auto *tn = dynamic_cast<ast::TerminalNode *>(c)) {
                                if (tn->value == "+" || tn->value == "-" || tn->value == "*" ||
                                    tn->value == "/" || tn->value == "%" || tn->value == "==" ||
                                    tn->value == "!=" || tn->value == "<" || tn->value == ">" ||
                                    tn->value == "<=" || tn->value == ">=") {
                                    op_node = tn;
                                    if (i + 1 < n->children.size()) {
                                        right_expr = n->children[i + 1];
                                    }
                                    return;
                                }
                            }
                            search_deeper(c);
                        }
                    };
                    search_deeper(rest);
                }

                if (!op_node || !right_expr) return left;

                Operand right = generate_expr(right_expr);
                int result_reg = alloc_register();
                Operand result(result_reg);

                std::string op = op_node->value;

                // Арифметические операции
                if (op == "+") emit(Instruction(OpType::ADD, result, left, right));
                else if (op == "-") emit(Instruction(OpType::SUB, result, left, right));
                else if (op == "*") emit(Instruction(OpType::MUL, result, left, right));
                else if (op == "/") emit(Instruction(OpType::DIV, result, left, right));
                else if (op == "%") emit(Instruction(OpType::MOD, result, left, right));


                    // Логические операции
                else if (op == "&&") emit(Instruction(OpType::AND, result, left, right));
                else if (op == "||") emit(Instruction(OpType::OR, result, left, right));

                    // Сравнения
                else if (op == "==") {
                    emit(Instruction(OpType::CMP, left, right));
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "0")));
                    std::string label_true = new_label("eq_true");
                    std::string label_end = new_label("eq_end");
                    emit(Instruction(OpType::JE, Operand(OperandType::LABEL, label_true)));
                    emit(Instruction(OpType::JMP, Operand(OperandType::LABEL, label_end)));
                    emit_label(label_true);
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "1")));
                    emit_label(label_end);
                } else if (op == "!=") {
                    emit(Instruction(OpType::CMP, left, right));
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "0")));
                    std::string label_true = new_label("ne_true");
                    std::string label_end = new_label("ne_end");
                    emit(Instruction(OpType::JNE, Operand(OperandType::LABEL, label_true)));
                    emit(Instruction(OpType::JMP, Operand(OperandType::LABEL, label_end)));
                    emit_label(label_true);
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "1")));
                    emit_label(label_end);
                } else if (op == "<") {
                    emit(Instruction(OpType::CMP, left, right));
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "0")));
                    std::string label_true = new_label("lt_true");
                    std::string label_end = new_label("lt_end");
                    emit(Instruction(OpType::JL, Operand(OperandType::LABEL, label_true)));
                    emit(Instruction(OpType::JMP, Operand(OperandType::LABEL, label_end)));
                    emit_label(label_true);
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "1")));
                    emit_label(label_end);
                } else if (op == ">") {
                    emit(Instruction(OpType::CMP, left, right));
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "0")));
                    std::string label_true = new_label("gt_true");
                    std::string label_end = new_label("gt_end");
                    emit(Instruction(OpType::JG, Operand(OperandType::LABEL, label_true)));
                    emit(Instruction(OpType::JMP, Operand(OperandType::LABEL, label_end)));
                    emit_label(label_true);
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "1")));
                    emit_label(label_end);
                } else if (op == "<=") {
                    emit(Instruction(OpType::CMP, left, right));
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "0")));
                    std::string label_true = new_label("le_true");
                    std::string label_end = new_label("le_end");
                    emit(Instruction(OpType::JLE, Operand(OperandType::LABEL, label_true)));
                    emit(Instruction(OpType::JMP, Operand(OperandType::LABEL, label_end)));
                    emit_label(label_true);
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "1")));
                    emit_label(label_end);
                } else if (op == ">=") {
                    emit(Instruction(OpType::CMP, left, right));
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "0")));
                    std::string label_true = new_label("ge_true");
                    std::string label_end = new_label("ge_end");
                    emit(Instruction(OpType::JGE, Operand(OperandType::LABEL, label_true)));
                    emit(Instruction(OpType::JMP, Operand(OperandType::LABEL, label_end)));
                    emit_label(label_true);
                    emit(Instruction(OpType::LOAD, result, Operand(OperandType::IMMEDIATE, "1")));
                    emit_label(label_end);
                }

                free_register(left.reg_num);
                free_register(right.reg_num);

                return result;
            }

            return left;
        }


        // Обработка функций (например, print)
        if (nodename.find("INO") != std::string::npos || nodename.find("POSTFIX") != std::string::npos) {
            // Поиск идентификатора функции и аргументов
            for (auto *c: node->children) {
                if (auto *tn = dynamic_cast<ast::TerminalNode *>(c)) {
                    if (tn->token_type == parser::TokenType::TOK_PRINT) {
                        std::string func_name = tn->value;

                        if (func_name == "print") {
                            // Найти аргументы
                            for (auto *arg: node->children) {
                                if (arg->to_string() == "TOK_ARGLIST" ||
                                    arg->to_string().find("EXPR") != std::string::npos) {
                                    Operand val = generate_expr(arg);
                                    emit(Instruction(OpType::PRINT, val));
                                    free_register(val.reg_num);
                                }
                            }
                        }
                    }
                }
                generate_expr(c);
            }
        }

        return Operand();
    }

    void Poliz::generate_stmt(ast::AstNode *node) {
        if (!node) return;

        std::string nodename = node->to_string();

        // DECLARATIONLIST - обрабатывать все декларации
        if (nodename == "TOK_DECLARATIONLIST") {
            for (auto *c: node->children) {
                generate_stmt(c);
            }
            return;
        }

        // DECLARATION - обработать деклараци функции или переменной
        if (nodename == "TOK_DECLARATION") {
            // Поиск DECLSUFFIX который содержит функцию
            ast::AstNode *declsuffix = nullptr;
            ast::AstNode *id_node = nullptr;

            for (auto *c: node->children) {
                if (c->to_string() == "TOK_DECLSUFFIX") {
                    declsuffix = c;
                }
                if (c->to_string() == "TOK_ID") {
                    id_node = c;
                }
            }

            if (declsuffix) {
                // Ищем TOK_COMPOUNDSTMT (тело функции)
                for (auto *c: declsuffix->children) {
                    if (c->to_string() == "TOK_COMPOUNDSTMT") {
                        // Это функция! Создать метку и сгенерировать тело
                        if (id_node) {
                            std::string func_name;
                            for (auto *id_child: id_node->children) {
                                if (auto *tn = dynamic_cast<ast::TerminalNode *>(id_child)) {
                                    func_name = tn->value;
                                    break;
                                }
                            }

                            if (!func_name.empty()) {
                                emit_label(func_name);
                                generate_stmt(c);
                                // Добавить RET если его нет
                                if (code.empty() || code.back().op != OpType::RET) {
                                    emit(Instruction(OpType::RET));
                                }
                            }
                        }
                        return;
                    }
                }
            }
            return;
        }

        // STMTLIST - обрабатывать все выражения в списке
        if (nodename == "TOK_STMTLIST") {
            for (auto *c: node->children) {
                generate_stmt(c);
            }
            return;
        }

        // STATEMENT - обработать различные типы выражений
        if (nodename == "TOK_STATEMENT") {
            for (auto *c: node->children) {
                generate_stmt(c);
            }
            return;
        }

        // COMPOUNDSTMT - обрабатывать блок кода
        if (nodename == "TOK_COMPOUNDSTMT") {
            for (auto *c: node->children) {
                generate_stmt(c);
            }
            return;
        }

        // RETURN statement
        if (nodename == "TOK_RETURNSTMT") {
            // Найти выражение для возврата
            ast::AstNode *return_expr = nullptr;
            for (auto *c: node->children) {
                if (c->to_string().find("EXPR") != std::string::npos) {
                    return_expr = c;
                    break;
                }
            }

            if (return_expr) {
                Operand result = generate_expr(return_expr);
                // Переместить результат в r0 (стандартный регистр возврата)
                if (result.reg_num != 0) {
                    emit(Instruction(OpType::MOV, Operand(0), result));
                }
                free_register(result.reg_num);
            }

            emit(Instruction(OpType::RET));
            return;
        }

        // IF statement
        if (nodename == "TOK_IFSTMT") {
            ast::AstNode *cond = nullptr;
            ast::AstNode *then_stmt = nullptr;
            ast::AstNode *else_part = nullptr;

            for (auto *c: node->children) {
                if (c->to_string().find("EXPR") != std::string::npos && !cond) cond = c;
                else if (c->to_string() == "TOK_COMPOUNDSTMT" && !then_stmt) then_stmt = c;
                else if (c->to_string() == "TOK_ELSEPART") else_part = c;
            }

            if (cond && then_stmt) {
                Operand cond_reg = generate_expr(cond);

                std::string label_else = new_label("else");
                std::string label_end = new_label("endif");

                emit(Instruction(OpType::CMP, cond_reg, Operand(OperandType::IMMEDIATE, "0")));
                emit(Instruction(OpType::JE, Operand(OperandType::LABEL, label_else)));

                generate_stmt(then_stmt);
                emit(Instruction(OpType::JMP, Operand(OperandType::LABEL, label_end)));

                emit_label(label_else);
                if (else_part && !else_part->children.empty()) {
                    generate_stmt(else_part->children[0]);
                }

                emit_label(label_end);
                free_register(cond_reg.reg_num);
            }
        }

        // WHILE statement
        else if (nodename == "TOK_WHILESTMT") {
            ast::AstNode *cond = nullptr;
            ast::AstNode *body = nullptr;

            for (auto *c: node->children) {
                if (c->to_string().find("EXPR") != std::string::npos) cond = c;
                else if (c->to_string() == "TOK_COMPOUNDSTMT") body = c;
            }

            if (cond && body) {
                std::string label_start = new_label("while_start");
                std::string label_end = new_label("while_end");

                emit_label(label_start);
                Operand cond_reg = generate_expr(cond);
                emit(Instruction(OpType::CMP, cond_reg, Operand(OperandType::IMMEDIATE, "0")));
                emit(Instruction(OpType::JE, Operand(OperandType::LABEL, label_end)));

                generate_stmt(body);
                emit(Instruction(OpType::JMP, Operand(OperandType::LABEL, label_start)));

                emit_label(label_end);
                free_register(cond_reg.reg_num);
            }
        }

        // Variable declaration with initialization
        else if (nodename == "TOK_LOCALVARDECL") {
            ast::AstNode *id_node = nullptr;
            ast::AstNode *init_expr = nullptr;

            for (auto *c: node->children) {
                if (c->to_string() == "TOK_ID") id_node = c;
                else if (c->to_string() == "TOK_VARDECLREST") {
                    std::function<ast::AstNode*(ast::AstNode *)> find_expr = [&](ast::AstNode *n) -> ast::AstNode * {
                        if (!n) return nullptr;
                        if (n->to_string().find("EXPR") != std::string::npos) return n;
                        for (auto *sub: n->children) {
                            if (auto *found = find_expr(sub)) return found;
                        }
                        return nullptr;
                    };
                    init_expr = find_expr(c);
                }
            }

            // Если нет инициализации, просто декларировать переменную
            if (id_node) {
                std::string var_name;
                for (auto *c: id_node->children) {
                    if (auto *tn = dynamic_cast<ast::TerminalNode *>(c)) {
                        var_name = tn->value;
                        break;
                    }
                }

                if (!var_name.empty()) {
                    if (init_expr) {
                        Operand value_reg = generate_expr(init_expr);
                        Operand var_op(OperandType::VARIABLE, var_name);
                        emit(Instruction(OpType::STORE, var_op, value_reg));
                        free_register(value_reg.reg_num);
                    }
                }
            }
        }

        // Expression statement
        else if (nodename == "TOK_EXPRESSIONSTMT") {
            for (auto *c: node->children) {
                if (c->to_string() == "TOK_EXPRESSION") {
                    Operand result = generate_expr(c);
                    free_register(result.reg_num);
                } else if (c->to_string().find("EXPR") != std::string::npos) {
                    Operand result = generate_expr(c);
                    free_register(result.reg_num);
                }
            }
        }

        // PRINT (legacy handling)
        else if (nodename == "TOK_INO") {
            for (auto *c: node->children) {
                if (auto *tn = dynamic_cast<ast::TerminalNode *>(c)) {
                    if (tn->value == "print") {
                        for (auto *arg: node->children) {
                            if (arg->to_string() == "TOK_ARGLIST") {
                                for (auto *expr: arg->children) {
                                    if (expr->to_string().find("EXPR") != std::string::npos) {
                                        Operand val = generate_expr(expr);
                                        emit(Instruction(OpType::PRINT, val));
                                        free_register(val.reg_num);
                                    }
                                }
                            }
                        }
                        return;
                    }
                }
            }
        }

        // Рекурсивная обработка для неизвестных узлов
        for (auto *c: node->children) {
            generate_stmt(c);
        }
    }

    void Poliz::generate(ast::AstNode *root) {
        if (!root) return;

        // Инициализация
        next_reg = 0;
        next_temp = 0;
        next_label = 0;
        code.clear();
        label_map.clear();

        // Обработка корня программы
        std::string root_name = root->to_string();

        // Если это программа, ищем main функцию
        if (root_name == "TOK_PROGRAM") {
            // Рекурсивно обработать все потомки
            for (auto* child : root->children) {
                if (child) {
                    generate_stmt(child);
                }
            }
        } else {
            // Генерация кода для общего случая
            generate_stmt(root);
        }

        // Завершение программы
        emit(Instruction(OpType::HALT));
    }

    // =============================================================================
    // OPTIMIZATIONS
    // =============================================================================

    void Poliz::optimize_peephole() {
        // Peephole оптимизации (шаблоны)
        bool changed = true;
        while (changed) {
            changed = false;

            for (size_t i = 0; i + 1 < code.size(); i++) {
                // LOAD r0, X; STORE Y, r0 -> MOV Y, X
                if (code[i].op == OpType::LOAD && code[i + 1].op == OpType::STORE &&
                    code[i].dst.reg_num == code[i + 1].src1.reg_num) {
                    code[i].op = OpType::MOV;
                    code[i].dst = code[i + 1].dst;
                    code.erase(code.begin() + i + 1);
                    changed = true;
                    continue;
                }

                // ADD r0, r1, 0 -> MOV r0, r1
                if (code[i].op == OpType::ADD &&
                    code[i].src2.type == OperandType::IMMEDIATE &&
                    code[i].src2.value == "0") {
                    code[i].op = OpType::MOV;
                    code[i].src2 = Operand();
                    changed = true;
                    continue;
                }

                // MUL r0, r1, 1 -> MOV r0, r1
                if (code[i].op == OpType::MUL &&
                    code[i].src2.type == OperandType::IMMEDIATE &&
                    code[i].src2.value == "1") {
                    code[i].op = OpType::MOV;
                    code[i].src2 = Operand();
                    changed = true;
                    continue;
                }

                // JMP L; L: -> удалить JMP
                if (code[i].op == OpType::JMP && i + 1 < code.size() &&
                    code[i + 1].op == OpType::LABEL &&
                    code[i].dst.value == code[i + 1].label) {
                    code.erase(code.begin() + i);
                    changed = true;
                    continue;
                }
            }
        }
    }

    void Poliz::optimize_constant_folding() {
        for (size_t i = 0; i < code.size(); i++) {
            auto &instr = code[i];

            // Складывание констант в арифметических операциях
            if ((instr.op == OpType::ADD || instr.op == OpType::SUB ||
                 instr.op == OpType::MUL || instr.op == OpType::DIV) &&
                instr.src1.type == OperandType::IMMEDIATE &&
                instr.src2.type == OperandType::IMMEDIATE) {
                double val1 = std::stod(instr.src1.value);
                double val2 = std::stod(instr.src2.value);
                double result = 0;

                switch (instr.op) {
                    case OpType::ADD: result = val1 + val2;
                        break;
                    case OpType::SUB: result = val1 - val2;
                        break;
                    case OpType::MUL: result = val1 * val2;
                        break;
                    case OpType::DIV: result = val2 != 0 ? val1 / val2 : 0;
                        break;
                    default: break;
                }

                instr.op = OpType::LOAD;
                instr.src1 = Operand(OperandType::IMMEDIATE, std::to_string(result));
                instr.src2 = Operand();
            }
        }
    }

    void Poliz::optimize_dead_code() {
        // Удаление недостижимого кода после безусловных переходов
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].op == OpType::JMP || code[i].op == OpType::HALT || code[i].op == OpType::RET) {
                // Удалить все инструкции до следующей метки
                size_t j = i + 1;
                while (j < code.size() && code[j].op != OpType::LABEL) {
                    code.erase(code.begin() + j);
                }
            }
        }
    }

    void Poliz::optimize() {
        optimize_constant_folding();
        optimize_peephole();
        optimize_dead_code();
    }

    // =============================================================================
    // PRINTING
    // =============================================================================

    void Poliz::print() const {
        std::cout << "\n========== ASSEMBLY-LIKE CODE ==========\n";

        for (size_t i = 0; i < code.size(); i++) {
            const auto &instr = code[i];

            std::cout << std::setw(4) << i << ": ";

            if (instr.op == OpType::LABEL) {
                std::cout << instr.label << ":\n";
                continue;
            }

            // Вывод мнемоники
            std::string mnem;
            switch (instr.op) {
                case OpType::NOP: mnem = "nop";
                    break;
                case OpType::LOAD: mnem = "load";
                    break;
                case OpType::STORE: mnem = "store";
                    break;
                case OpType::MOV: mnem = "mov";
                    break;
                case OpType::ADD: mnem = "add";
                    break;
                case OpType::SUB: mnem = "sub";
                    break;
                case OpType::MUL: mnem = "mul";
                    break;
                case OpType::DIV: mnem = "div";
                    break;
                case OpType::MOD: mnem = "mod";
                    break;
                case OpType::INC: mnem = "inc";
                    break;
                case OpType::DEC: mnem = "dec";
                    break;
                case OpType::NEG: mnem = "neg";
                    break;
                case OpType::AND: mnem = "and";
                    break;
                case OpType::OR: mnem = "or";
                    break;
                case OpType::XOR: mnem = "xor";
                    break;
                case OpType::NOT: mnem = "not";
                    break;
                case OpType::CMP: mnem = "cmp";
                    break;
                case OpType::JMP: mnem = "jmp";
                    break;
                case OpType::JZ: mnem = "jz";
                    break;
                case OpType::JNZ: mnem = "jnz";
                    break;
                case OpType::JE: mnem = "je";
                    break;
                case OpType::JNE: mnem = "jne";
                    break;
                case OpType::JL: mnem = "jl";
                    break;
                case OpType::JG: mnem = "jg";
                    break;
                case OpType::JLE: mnem = "jle";
                    break;
                case OpType::JGE: mnem = "jge";
                    break;
                case OpType::CALL: mnem = "call";
                    break;
                case OpType::RET: mnem = "ret";
                    break;
                case OpType::PRINT: mnem = "print";
                    break;
                case OpType::INPUT: mnem = "input";
                    break;
                case OpType::HALT: mnem = "halt";
                    break;
                default: mnem = "???";
                    break;
            }

            std::cout << std::setw(8) << std::left << mnem;

            // Вывод операндов
            auto print_operand = [](const Operand &op) {
                if (op.type == OperandType::REGISTER) return op.value;
                if (op.type == OperandType::IMMEDIATE) return "#" + op.value;
                if (op.type == OperandType::VARIABLE) return "[" + op.value + "]";
                if (op.type == OperandType::LABEL) return op.value;
                return std::string("");
            };

            if (instr.dst.type != OperandType::NONE) {
                std::cout << " " << print_operand(instr.dst);
            }
            if (instr.src1.type != OperandType::NONE) {
                std::cout << ", " << print_operand(instr.src1);
            }
            if (instr.src2.type != OperandType::NONE) {
                std::cout << ", " << print_operand(instr.src2);
            }

            if (!instr.comment.empty()) {
                std::cout << "  ; " << instr.comment;
            }

            std::cout << "\n";
        }

        std::cout << "========================================\n\n";
    }

    void Poliz::save_to_file(const std::string &filename) const {
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Failed to open " << filename << "\n";
            return;
        }

        // Перенаправить вывод в файл
        std::streambuf *cout_buf = std::cout.rdbuf();
        std::cout.rdbuf(out.rdbuf());
        print();
        std::cout.rdbuf(cout_buf);

        out.close();
    }

    // =============================================================================
    // INTERPRETER
    // =============================================================================

    Interpreter::Interpreter(const std::vector<Instruction> &code) : code(code) {
        registers.resize(16, 0.0); // r0-r15

        // Построить таблицу меток
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].op == OpType::LABEL) {
                labels[code[i].label] = i;
            }
        }
    }

    double Interpreter::get_operand_value(const Operand &op) {
        switch (op.type) {
            case OperandType::REGISTER:
                if (op.reg_num >= 0 && op.reg_num < 16) return registers[op.reg_num];
                break;
            case OperandType::IMMEDIATE:
                return std::stod(op.value);
            case OperandType::VARIABLE:
                return memory[op.value];
            default:
                break;
        }
        return 0.0;
    }

    void Interpreter::set_operand_value(const Operand &op, double value) {
        if (op.type == OperandType::REGISTER && op.reg_num >= 0 && op.reg_num < 16) {
            registers[op.reg_num] = value;
        } else if (op.type == OperandType::VARIABLE) {
            memory[op.value] = value;
        }
    }

    void Interpreter::update_flags(double result) {
        zero_flag = (result == 0);
        sign_flag = (result < 0);
    }

    void Interpreter::update_hotspot(int addr) {
        hotspots[addr]++;
    }

    bool Interpreter::step() {
        if (pc >= (int) code.size()) return false;

        const Instruction &instr = code[pc];
        update_hotspot(pc);

        switch (instr.op) {
            case OpType::NOP:
                break;

            case OpType::LOAD: {
                double val = get_operand_value(instr.src1);
                set_operand_value(instr.dst, val);
                break;
            }

            case OpType::STORE: {
                double val = get_operand_value(instr.src1);
                set_operand_value(instr.dst, val);
                break;
            }

            case OpType::MOV: {
                double val = get_operand_value(instr.src1);
                set_operand_value(instr.dst, val);
                break;
            }

            case OpType::ADD: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                set_operand_value(instr.dst, val1 + val2);
                break;
            }

            case OpType::SUB: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                set_operand_value(instr.dst, val1 - val2);
                break;
            }

            case OpType::MUL: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                set_operand_value(instr.dst, val1 * val2);
                break;
            }

            case OpType::DIV: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                if (val2 != 0) set_operand_value(instr.dst, val1 / val2);
                else std::cerr << "Runtime error: division by zero\n";
                break;
            }

            case OpType::MOD: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                set_operand_value(instr.dst, std::fmod(val1, val2));
                break;
            }

            case OpType::INC: {
                double val = get_operand_value(instr.dst);
                set_operand_value(instr.dst, val + 1);
                break;
            }

            case OpType::DEC: {
                double val = get_operand_value(instr.dst);
                set_operand_value(instr.dst, val - 1);
                break;
            }

            case OpType::NEG: {
                double val = get_operand_value(instr.dst);
                set_operand_value(instr.dst, -val);
                break;
            }

            case OpType::AND: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                set_operand_value(instr.dst, (int) val1 & (int) val2);
                break;
            }

            case OpType::OR: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                set_operand_value(instr.dst, (int) val1 | (int) val2);
                break;
            }

            case OpType::XOR: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                set_operand_value(instr.dst, (int) val1 ^ (int) val2);
                break;
            }

            case OpType::NOT: {
                double val = get_operand_value(instr.dst);
                set_operand_value(instr.dst, ~(int) val);
                break;
            }

            case OpType::CMP: {
                double val1 = get_operand_value(instr.dst);
                double val2 = get_operand_value(instr.src1);
                update_flags(val1 - val2);
                break;
            }

            case OpType::JMP: {
                if (labels.count(instr.dst.value)) {
                    pc = labels[instr.dst.value];
                    return true;
                }
                break;
            }

            case OpType::JZ:
            case OpType::JE: {
                if (zero_flag && labels.count(instr.dst.value)) {
                    pc = labels[instr.dst.value];
                    return true;
                }
                break;
            }

            case OpType::JNZ:
            case OpType::JNE: {
                if (!zero_flag && labels.count(instr.dst.value)) {
                    pc = labels[instr.dst.value];
                    return true;
                }
                break;
            }

            case OpType::JL: {
                if (sign_flag && labels.count(instr.dst.value)) {
                    pc = labels[instr.dst.value];
                    return true;
                }
                break;
            }

            case OpType::JG: {
                if (!sign_flag && !zero_flag && labels.count(instr.dst.value)) {
                    pc = labels[instr.dst.value];
                    return true;
                }
                break;
            }

            case OpType::JLE: {
                if ((sign_flag || zero_flag) && labels.count(instr.dst.value)) {
                    pc = labels[instr.dst.value];
                    return true;
                }
                break;
            }

            case OpType::JGE: {
                if (!sign_flag && labels.count(instr.dst.value)) {
                    pc = labels[instr.dst.value];
                    return true;
                }
                break;
            }

            case OpType::PRINT: {
                double val = get_operand_value(instr.dst);
                std::cout << val << std::endl;
                break;
            }

            case OpType::INPUT: {
                double val;
                std::cin >> val;
                set_operand_value(instr.dst, val);
                break;
            }

            case OpType::HALT:
                return false;

            case OpType::LABEL:
                break;

            default:
                std::cerr << "Unknown instruction at " << pc << "\n";
                break;
        }

        pc++;
        return true;
    }

    void Interpreter::run() {
        std::cout << "\n========== PROGRAM EXECUTION ==========\n";

        pc = 0;
        int steps = 0;
        const int MAX_STEPS = 100000; // Защита от бесконечных циклов

        while (step() && steps < MAX_STEPS) {
            steps++;
        }

        if (steps >= MAX_STEPS) {
            std::cerr << "Program terminated: maximum steps exceeded\n";
        }

        std::cout << "\n========== EXECUTION COMPLETE ==========\n";
        std::cout << "Total steps: " << steps << "\n";

        // Статистика hotspots (для JIT-подобного анализа)
        if (!hotspots.empty()) {
            std::cout << "\nHotspots (most executed instructions):\n";
            std::vector<std::pair<int, int> > sorted_hotspots(hotspots.begin(), hotspots.end());
            std::sort(sorted_hotspots.begin(), sorted_hotspots.end(),
                      [](const auto &a, const auto &b) { return a.second > b.second; });

            for (size_t i = 0; i < std::min(size_t(10), sorted_hotspots.size()); i++) {
                std::cout << "  Address " << sorted_hotspots[i].first
                        << ": " << sorted_hotspots[i].second << " executions\n";
            }
        }

        std::cout << "\n========================================\n";
    }

    void Interpreter::dump_state() const {
        std::cout << "\n========== MACHINE STATE ==========\n";
        std::cout << "PC: " << pc << "\n";
        std::cout << "Flags: Z=" << zero_flag << " S=" << sign_flag << " C=" << carry_flag << "\n";

        std::cout << "\nRegisters:\n";
        for (int i = 0; i < 16; i++) {
            if (registers[i] != 0) {
                std::cout << "  r" << i << " = " << registers[i] << "\n";
            }
        }

        std::cout << "\nMemory (variables):\n";
        for (const auto &[name, value]: memory) {
            std::cout << "  " << name << " = " << value << "\n";
        }

        std::cout << "===================================\n\n";
    }
} // namespace poliz
