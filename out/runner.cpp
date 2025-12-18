//
// Created by afa on 07.12.2025.
// Complete Interpreter implementation for POLIZ code execution
//

#include "runner.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <sstream>

namespace runner {

    // =============================================================================
    // INTERPRETER IMPLEMENTATION
    // =============================================================================

    Interpreter::Interpreter(const std::vector<poliz::Instruction>& instr_code)
        : code(instr_code), pc(0) {
        registers.resize(16, 0.0);  // r0-r15
        stack.reserve(1024);         // Stack capacity

        // Build label table for jumps
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].op == poliz::OpType::LABEL) {
                labels[code[i].label] = i;
            }
        }
    }

    double Interpreter::get_operand_value(const poliz::Operand& op) {
        switch (op.type) {
            case poliz::OperandType::REGISTER:
                if (op.reg_num >= 0 && op.reg_num < 16) {
                    return registers[op.reg_num];
                }
                break;

            case poliz::OperandType::IMMEDIATE:
                try {
                    return std::stod(op.value);
                } catch (...) {
                    return 0.0;
                }

            case poliz::OperandType::VARIABLE:
                if (memory.find(op.value) != memory.end()) {
                    return memory[op.value];
                }
                return 0.0;

            case poliz::OperandType::LABEL:
                if (labels.find(op.value) != labels.end()) {
                    return static_cast<double>(labels[op.value]);
                }
                break;

            default:
                break;
        }
        return 0.0;
    }

    void Interpreter::set_operand_value(const poliz::Operand& op, double value) {
        if (op.type == poliz::OperandType::REGISTER) {
            if (op.reg_num >= 0 && op.reg_num < 16) {
                registers[op.reg_num] = value;
            }
        } else if (op.type == poliz::OperandType::VARIABLE) {
            memory[op.value] = value;
        }
    }

    void Interpreter::update_flags(double result) {
        zero_flag = (result == 0.0);
        sign_flag = (result < 0.0);
        carry_flag = false;  // Simplified for now
    }

    void Interpreter::print_instruction(const poliz::Instruction& instr) const {
        auto print_operand = [](const poliz::Operand& op) -> std::string {
            if (op.type == poliz::OperandType::REGISTER) {
                return op.value;
            } else if (op.type == poliz::OperandType::IMMEDIATE) {
                return "#" + op.value;
            } else if (op.type == poliz::OperandType::VARIABLE) {
                return "[" + op.value + "]";
            } else if (op.type == poliz::OperandType::LABEL) {
                return op.value;
            }
            return "???";
        };

        std::string mnem;
        switch (instr.op) {
            case poliz::OpType::NOP: mnem = "nop"; break;
            case poliz::OpType::LOAD: mnem = "load"; break;
            case poliz::OpType::STORE: mnem = "store"; break;
            case poliz::OpType::MOV: mnem = "mov"; break;
            case poliz::OpType::ADD: mnem = "add"; break;
            case poliz::OpType::SUB: mnem = "sub"; break;
            case poliz::OpType::MUL: mnem = "mul"; break;
            case poliz::OpType::DIV: mnem = "div"; break;
            case poliz::OpType::MOD: mnem = "mod"; break;
            case poliz::OpType::INC: mnem = "inc"; break;
            case poliz::OpType::DEC: mnem = "dec"; break;
            case poliz::OpType::NEG: mnem = "neg"; break;
            case poliz::OpType::AND: mnem = "and"; break;
            case poliz::OpType::OR: mnem = "or"; break;
            case poliz::OpType::XOR: mnem = "xor"; break;
            case poliz::OpType::NOT: mnem = "not"; break;
            case poliz::OpType::CMP: mnem = "cmp"; break;
            case poliz::OpType::JMP: mnem = "jmp"; break;
            case poliz::OpType::JZ: mnem = "jz"; break;
            case poliz::OpType::JNZ: mnem = "jnz"; break;
            case poliz::OpType::JE: mnem = "je"; break;
            case poliz::OpType::JNE: mnem = "jne"; break;
            case poliz::OpType::JL: mnem = "jl"; break;
            case poliz::OpType::JG: mnem = "jg"; break;
            case poliz::OpType::JLE: mnem = "jle"; break;
            case poliz::OpType::JGE: mnem = "jge"; break;
            case poliz::OpType::CALL: mnem = "call"; break;
            case poliz::OpType::RET: mnem = "ret"; break;
            case poliz::OpType::PRINT: mnem = "print"; break;
            case poliz::OpType::INPUT: mnem = "input"; break;
            case poliz::OpType::LABEL: mnem = "label"; break;
            case poliz::OpType::HALT: mnem = "halt"; break;
            default: mnem = "???"; break;
        }

        std::cout << "  [" << std::setw(4) << pc << "] " << std::setw(8) << mnem;

        if (instr.dst.type != poliz::OperandType::NONE) {
            std::cout << " " << print_operand(instr.dst);
        }
        if (instr.src1.type != poliz::OperandType::NONE) {
            std::cout << ", " << print_operand(instr.src1);
        }
        if (instr.src2.type != poliz::OperandType::NONE) {
            std::cout << ", " << print_operand(instr.src2);
        }

        std::cout << std::endl;
    }

    bool Interpreter::step() {
        if (pc < 0 || pc >= (int)code.size()) {
            return false;
        }

        const poliz::Instruction& instr = code[pc];

        // Skip labels
        if (instr.op == poliz::OpType::LABEL) {
            pc++;
            return true;
        }

        switch (instr.op) {
            case poliz::OpType::NOP:
                pc++;
                break;

            case poliz::OpType::LOAD: {
                double val = get_operand_value(instr.src1);
                set_operand_value(instr.dst, val);
                pc++;
                break;
            }

            case poliz::OpType::STORE: {
                double val = get_operand_value(instr.src1);
                set_operand_value(instr.dst, val);
                pc++;
                break;
            }

            case poliz::OpType::MOV: {
                double val = get_operand_value(instr.src1);
                set_operand_value(instr.dst, val);
                pc++;
                break;
            }

            case poliz::OpType::ADD: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                double result = val1 + val2;
                set_operand_value(instr.dst, result);
                update_flags(result);
                pc++;
                break;
            }

            case poliz::OpType::SUB: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                double result = val1 - val2;
                set_operand_value(instr.dst, result);
                update_flags(result);
                pc++;
                break;
            }

            case poliz::OpType::MUL: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                double result = val1 * val2;
                set_operand_value(instr.dst, result);
                update_flags(result);
                pc++;
                break;
            }

            case poliz::OpType::DIV: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                if (val2 == 0.0) {
                    std::cerr << "❌ Runtime Error: Division by zero at PC=" << pc << std::endl;
                    return false;
                }
                double result = val1 / val2;
                set_operand_value(instr.dst, result);
                update_flags(result);
                pc++;
                break;
            }

            case poliz::OpType::MOD: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                if (val2 == 0.0) {
                    std::cerr << "❌ Runtime Error: Modulo by zero at PC=" << pc << std::endl;
                    return false;
                }
                double result = std::fmod(val1, val2);
                set_operand_value(instr.dst, result);
                update_flags(result);
                pc++;
                break;
            }

            case poliz::OpType::INC: {
                double val = get_operand_value(instr.dst);
                set_operand_value(instr.dst, val + 1.0);
                pc++;
                break;
            }

            case poliz::OpType::DEC: {
                double val = get_operand_value(instr.dst);
                set_operand_value(instr.dst, val - 1.0);
                pc++;
                break;
            }

            case poliz::OpType::NEG: {
                double val = get_operand_value(instr.src1);
                set_operand_value(instr.dst, -val);
                pc++;
                break;
            }

            case poliz::OpType::AND: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                double result = (val1 != 0.0 && val2 != 0.0) ? 1.0 : 0.0;
                set_operand_value(instr.dst, result);
                update_flags(result);
                pc++;
                break;
            }

            case poliz::OpType::OR: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                double result = (val1 != 0.0 || val2 != 0.0) ? 1.0 : 0.0;
                set_operand_value(instr.dst, result);
                update_flags(result);
                pc++;
                break;
            }

            case poliz::OpType::XOR: {
                double val1 = get_operand_value(instr.src1);
                double val2 = get_operand_value(instr.src2);
                long v1 = static_cast<long>(val1);
                long v2 = static_cast<long>(val2);
                set_operand_value(instr.dst, static_cast<double>(v1 ^ v2));
                pc++;
                break;
            }

            case poliz::OpType::NOT: {
                double val = get_operand_value(instr.src1);
                set_operand_value(instr.dst, (val == 0.0) ? 1.0 : 0.0);
                pc++;
                break;
            }

            case poliz::OpType::CMP: {
                double val1 = get_operand_value(instr.dst);
                double val2 = get_operand_value(instr.src1);
                update_flags(val1 - val2);
                pc++;
                break;
            }

            case poliz::OpType::JMP: {
                auto it = labels.find(instr.dst.value);
                if (it != labels.end()) {
                    pc = it->second;
                } else {
                    std::cerr << "❌ Runtime Error: Label not found: " << instr.dst.value << std::endl;
                    return false;
                }
                break;
            }

            case poliz::OpType::JZ: {
                if (zero_flag) {
                    auto it = labels.find(instr.dst.value);
                    if (it != labels.end()) {
                        pc = it->second;
                    } else {
                        std::cerr << "❌ Runtime Error: Label not found: " << instr.dst.value << std::endl;
                        return false;
                    }
                } else {
                    pc++;
                }
                break;
            }

            case poliz::OpType::JNZ: {
                if (!zero_flag) {
                    auto it = labels.find(instr.dst.value);
                    if (it != labels.end()) {
                        pc = it->second;
                    } else {
                        std::cerr << "❌ Runtime Error: Label not found: " << instr.dst.value << std::endl;
                        return false;
                    }
                } else {
                    pc++;
                }
                break;
            }

            case poliz::OpType::JE: {
                if (zero_flag) {  // Equal is when result is 0 (a == b -> a - b == 0)
                    auto it = labels.find(instr.dst.value);
                    if (it != labels.end()) {
                        pc = it->second;
                    } else {
                        std::cerr << "❌ Runtime Error: Label not found: " << instr.dst.value << std::endl;
                        return false;
                    }
                } else {
                    pc++;
                }
                break;
            }

            case poliz::OpType::JNE: {
                if (!zero_flag) {  // Not equal is when result is not 0
                    auto it = labels.find(instr.dst.value);
                    if (it != labels.end()) {
                        pc = it->second;
                    } else {
                        std::cerr << "❌ Runtime Error: Label not found: " << instr.dst.value << std::endl;
                        return false;
                    }
                } else {
                    pc++;
                }
                break;
            }

            case poliz::OpType::JL: {
                if (sign_flag && !zero_flag) {  // Less than (result < 0)
                    auto it = labels.find(instr.dst.value);
                    if (it != labels.end()) {
                        pc = it->second;
                    } else {
                        std::cerr << "❌ Runtime Error: Label not found: " << instr.dst.value << std::endl;
                        return false;
                    }
                } else {
                    pc++;
                }
                break;
            }

            case poliz::OpType::JG: {
                if (!sign_flag && !zero_flag) {  // Greater than (result > 0)
                    auto it = labels.find(instr.dst.value);
                    if (it != labels.end()) {
                        pc = it->second;
                    } else {
                        std::cerr << "❌ Runtime Error: Label not found: " << instr.dst.value << std::endl;
                        return false;
                    }
                } else {
                    pc++;
                }
                break;
            }

            case poliz::OpType::JLE: {
                if (sign_flag || zero_flag) {  // Less or equal (result <= 0)
                    auto it = labels.find(instr.dst.value);
                    if (it != labels.end()) {
                        pc = it->second;
                    } else {
                        std::cerr << "❌ Runtime Error: Label not found: " << instr.dst.value << std::endl;
                        return false;
                    }
                } else {
                    pc++;
                }
                break;
            }

            case poliz::OpType::JGE: {
                if (!sign_flag || zero_flag) {  // Greater or equal (result >= 0)
                    auto it = labels.find(instr.dst.value);
                    if (it != labels.end()) {
                        pc = it->second;
                    } else {
                        std::cerr << "❌ Runtime Error: Label not found: " << instr.dst.value << std::endl;
                        return false;
                    }
                } else {
                    pc++;
                }
                break;
            }

            case poliz::OpType::CALL: {
                // Push return address to stack
                stack.push_back(static_cast<double>(pc + 1));

                // Jump to function
                auto it = labels.find(instr.dst.value);
                if (it != labels.end()) {
                    pc = it->second;
                } else {
                    std::cerr << "❌ Runtime Error: Function not found: " << instr.dst.value << std::endl;
                    return false;
                }
                break;
            }

            case poliz::OpType::RET: {
                if (stack.empty()) {
                    return 1;
                }
                pc = static_cast<int>(stack.back());
                stack.pop_back();
                break;
            }

            case poliz::OpType::PUSH: {
                double val = get_operand_value(instr.src1);
                stack.push_back(val);
                pc++;
                break;
            }

            case poliz::OpType::POP: {
                if (stack.empty()) {
                    std::cerr << "❌ Runtime Error: Pop from empty stack" << std::endl;
                    return false;
                }
                set_operand_value(instr.dst, stack.back());
                stack.pop_back();
                pc++;
                break;
            }

            case poliz::OpType::PRINT: {
                double val = get_operand_value(instr.dst);
                // Format output
                if (val == static_cast<long>(val)) {
                    std::cout << static_cast<long>(val);
                } else {
                    std::cout << val;
                }
                pc++;
                break;
            }

            case poliz::OpType::INPUT: {
                double input_val;
                std::cin >> input_val;
                set_operand_value(instr.dst, input_val);
                pc++;
                break;
            }

            case poliz::OpType::HALT:
                return false;

            case poliz::OpType::LABEL:
                // Already handled above
                pc++;
                break;

            default:
                std::cerr << "❌ Runtime Error: Unknown opcode at PC=" << pc << std::endl;
                return false;
        }

        return true;
    }

    void Interpreter::run() {
        std::cout << "\n========== INTERPRETER OUTPUT ==========\n";
        while (step()) {
            // Continue execution
        }
        std::cout << "\n========================================\n";
        std::cout << "✅ Program halted successfully\n\n";
    }

    void Interpreter::step_by_step_mode() {
        std::cout << "\n========== STEP-BY-STEP DEBUG MODE ==========\n";
        std::cout << "Commands: [s]tep, [c]ontinue, [d]ump, [q]uit\n\n";

        while (true) {
            if (pc < 0 || pc >= (int)code.size()) {
                std::cout << "Program terminated.\n";
                break;
            }

            std::cout << "\nNext instruction:\n";
            print_instruction(code[pc]);

            std::cout << "> ";
            std::string cmd;
            std::getline(std::cin, cmd);

            if (cmd == "s" || cmd == "step") {
                if (!step()) {
                    std::cout << "Program halted.\n";
                    break;
                }
            } else if (cmd == "c" || cmd == "continue") {
                while (step()) {
                    // Continue to end
                }
                std::cout << "Program halted.\n";
                break;
            } else if (cmd == "d" || cmd == "dump") {
                dump_state();
            } else if (cmd == "q" || cmd == "quit") {
                break;
            }
        }
    }

    void Interpreter::dump_state() const {
        std::cout << "\n========== MACHINE STATE ==========\n";

        std::cout << "📊 PC: " << pc << " / " << code.size() << "\n";
        std::cout << "🚩 Flags: Z=" << (zero_flag ? "1" : "0")
                  << " S=" << (sign_flag ? "1" : "0")
                  << " C=" << (carry_flag ? "1" : "0") << "\n";

        std::cout << "\n📝 Registers (r0-r15):\n";
        for (int i = 0; i < 16; i++) {
            std::cout << "  r" << i << " = " << std::setw(10) << registers[i];
            if ((i + 1) % 4 == 0) std::cout << "\n";
            else std::cout << " | ";
        }

        std::cout << "\n💾 Memory (" << memory.size() << " variables):\n";
        if (memory.empty()) {
            std::cout << "  (empty)\n";
        } else {
            for (const auto& [name, value] : memory) {
                std::cout << "  [" << name << "] = " << value << "\n";
            }
        }

        std::cout << "\n📚 Stack (" << stack.size() << " items):\n";
        if (stack.empty()) {
            std::cout << "  (empty)\n";
        } else {
            for (size_t i = 0; i < stack.size() && i < 10; i++) {
                std::cout << "  [" << i << "] = " << stack[i] << "\n";
            }
            if (stack.size() > 10) {
                std::cout << "  ... (" << (stack.size() - 10) << " more items)\n";
            }
        }

        std::cout << "===================================\n\n";
    }

    // =============================================================================
    // TEST FUNCTIONS
    // =============================================================================

    void test_basic_arithmetic() {
        std::cout << "🧪 Test: Basic Arithmetic (5 + 3)\n";

        std::vector<poliz::Instruction> test_code;
        test_code.push_back(poliz::Instruction(poliz::OpType::LOAD,
                                               poliz::Operand(0),
                                               poliz::Operand(poliz::OperandType::IMMEDIATE, "5")));
        test_code.push_back(poliz::Instruction(poliz::OpType::LOAD,
                                               poliz::Operand(1),
                                               poliz::Operand(poliz::OperandType::IMMEDIATE, "3")));
        test_code.push_back(poliz::Instruction(poliz::OpType::ADD,
                                               poliz::Operand(2),
                                               poliz::Operand(0),
                                               poliz::Operand(1)));
        test_code.push_back(poliz::Instruction(poliz::OpType::PRINT, poliz::Operand(2)));
        test_code.push_back(poliz::Instruction(poliz::OpType::HALT));

        Interpreter interp(test_code);
        interp.run();
        std::cout << "Expected: 8\n\n";
    }

    void test_variables() {
        std::cout << "🧪 Test: Variables (x=10, y=20, x+y)\n";

        std::vector<poliz::Instruction> test_code;
        test_code.push_back(poliz::Instruction(poliz::OpType::LOAD,
                                               poliz::Operand(poliz::OperandType::VARIABLE, "x"),
                                               poliz::Operand(poliz::OperandType::IMMEDIATE, "10")));
        test_code.push_back(poliz::Instruction(poliz::OpType::LOAD,
                                               poliz::Operand(poliz::OperandType::VARIABLE, "y"),
                                               poliz::Operand(poliz::OperandType::IMMEDIATE, "20")));
        test_code.push_back(poliz::Instruction(poliz::OpType::LOAD,
                                               poliz::Operand(0),
                                               poliz::Operand(poliz::OperandType::VARIABLE, "x")));
        test_code.push_back(poliz::Instruction(poliz::OpType::LOAD,
                                               poliz::Operand(1),
                                               poliz::Operand(poliz::OperandType::VARIABLE, "y")));
        test_code.push_back(poliz::Instruction(poliz::OpType::ADD,
                                               poliz::Operand(2),
                                               poliz::Operand(0),
                                               poliz::Operand(1)));
        test_code.push_back(poliz::Instruction(poliz::OpType::PRINT, poliz::Operand(2)));
        test_code.push_back(poliz::Instruction(poliz::OpType::HALT));

        Interpreter interp(test_code);
        interp.run();
        std::cout << "Expected: 30\n\n";
    }

    void test_comparison() {
        std::cout << "🧪 Test: Comparison (5 < 10)\n";

        std::vector<poliz::Instruction> test_code;
        test_code.push_back(poliz::Instruction(poliz::OpType::LOAD,
                                               poliz::Operand(0),
                                               poliz::Operand(poliz::OperandType::IMMEDIATE, "5")));
        test_code.push_back(poliz::Instruction(poliz::OpType::LOAD,
                                               poliz::Operand(1),
                                               poliz::Operand(poliz::OperandType::IMMEDIATE, "10")));
        test_code.push_back(poliz::Instruction(poliz::OpType::CMP,
                                               poliz::Operand(0),
                                               poliz::Operand(1)));
        test_code.push_back(poliz::Instruction(poliz::OpType::HALT));

        Interpreter interp(test_code);
        interp.run();
        interp.dump_state();
    }

} // namespace runner

