#include "poliz.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>

namespace poliz {
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

        // Терминалы
        if (nodename == "Terminal") {
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

        // Бинарные операции
        if (nodename.find("EXPR") != std::string::npos && node->children.size() >= 2) {
            Operand left = generate_expr(node->children[0]);

            ast::AstNode *rest = node->children[1];
            if (!rest || rest->children.empty()) return left;

            // Найти оператор
            ast::TerminalNode *op_node = nullptr;
            ast::AstNode *right_expr = nullptr;

            for (size_t i = 0; i < rest->children.size(); i++) {
                if (auto *tn = dynamic_cast<ast::TerminalNode *>(rest->children[i])) {
                    op_node = tn;
                    if (i + 1 < rest->children.size()) {
                        right_expr = rest->children[i + 1];
                    }
                    break;
                }
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

            // Присваивание
            else if (op == "=") {
                if (left.type == OperandType::VARIABLE) {
                    emit(Instruction(OpType::STORE, left, right));
                    return right;
                }
            }

            free_register(left.reg_num);
            free_register(right.reg_num);

            return result;
        }

        // Рекурсия для вложенных выражений
        if (node->children.size() == 1) {
            return generate_expr(node->children[0]);
        }

        return Operand();
    }

    void Poliz::generate_stmt(ast::AstNode *node) {
        if (!node) return;

        std::string nodename = node->to_string();

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

            if (id_node && init_expr) {
                std::string var_name;
                for (auto *c: id_node->children) {
                    if (auto *tn = dynamic_cast<ast::TerminalNode *>(c)) {
                        var_name = tn->value;
                        break;
                    }
                }

                if (!var_name.empty()) {
                    Operand value_reg = generate_expr(init_expr);
                    Operand var_op(OperandType::VARIABLE, var_name);
                    emit(Instruction(OpType::STORE, var_op, value_reg));
                    free_register(value_reg.reg_num);
                }
            }
        }

        // Expression statement
        else if (nodename == "TOK_EXPRESSIONSTMT") {
            for (auto *c: node->children) {
                if (c->to_string().find("EXPR") != std::string::npos) {
                    Operand result = generate_expr(c);
                    free_register(result.reg_num);
                }
            }
        }

        // PRINT
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

        // Рекурсивная обработка
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

        // Генерация кода
        generate_stmt(root);

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
