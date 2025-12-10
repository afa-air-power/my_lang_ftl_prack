#ifndef POLIZ_HPP
#define POLIZ_HPP

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include "ast.hpp"

namespace poliz {
    /**
     * @brief Набор инструкций виртуальной машины (Assembly-like)
     * Регистровая архитектура с оптимизациями
     */
    enum class OpType {
        // ===== DATA MOVEMENT =====
        NOP, // No operation
        LOAD, // LOAD reg, value/var    - загрузить в регистр
        STORE, // STORE var, reg         - сохранить из регистра
        MOV, // MOV reg_dst, reg_src   - копировать между регистрами
        LEA, // LEA reg, [address]     - load effective address
        PUSH, // PUSH reg               - положить на стек
        POP, // POP reg                - снять со стека

        // ===== ARITHMETIC =====
        ADD, // ADD reg_dst, reg_src1, reg_src2
        SUB, // SUB reg_dst, reg_src1, reg_src2
        MUL, // MUL reg_dst, reg_src1, reg_src2
        DIV, // DIV reg_dst, reg_src1, reg_src2
        MOD, // MOD reg_dst, reg_src1, reg_src2
        INC, // INC reg                - инкремент
        DEC, // DEC reg                - декремент
        NEG, // NEG reg                - унарный минус

        // ===== LOGIC & BITWISE =====
        AND, // AND reg_dst, reg_src1, reg_src2
        OR, // OR reg_dst, reg_src1, reg_src2
        XOR, // XOR reg_dst, reg_src1, reg_src2
        NOT, // NOT reg
        SHL, // SHL reg, count         - shift left
        SHR, // SHR reg, count         - shift right

        // ===== COMPARISON =====
        CMP, // CMP reg1, reg2         - сравнить (устанавливает флаги)
        TEST, // TEST reg1, reg2        - логическое AND (флаги)

        // ===== CONDITIONAL JUMPS =====
        JMP, // JMP label              - безусловный переход
        JZ, // JZ label               - jump if zero
        JNZ, // JNZ label              - jump if not zero
        JE, // JE label               - jump if equal
        JNE, // JNE label              - jump if not equal
        JL, // JL label               - jump if less
        JG, // JG label               - jump if greater
        JLE, // JLE label              - jump if less or equal
        JGE, // JGE label              - jump if greater or equal

        // ===== FUNCTION CALLS =====
        CALL, // CALL function_name
        RET, // RET                    - возврат из функции

        // ===== SPECIAL =====
        PRINT, // PRINT reg
        INPUT, // INPUT reg
        MEMBER, // MEMBER reg_dst, reg_obj, field_offset
        LABEL, // LABEL name             - метка
        HALT, // HALT                   - остановка программы
    };

    /**
     * @brief Типы операндов
     */
    enum class OperandType {
        NONE,
        REGISTER, // Регистр (r0-r15)
        IMMEDIATE, // Константа
        VARIABLE, // Имя переменной
        LABEL, // Метка для переходов
        MEMORY, // Адрес памяти [base + offset]
    };

    /**
     * @brief Операнд инструкции
     */
    struct Operand {
        OperandType type = OperandType::NONE;
        std::string value; // Значение (имя, число, метка)
        int reg_num = -1; // Номер регистра (-1 если не регистр)
        int offset = 0; // Смещение для MEMORY

        Operand() = default;

        Operand(OperandType t, const std::string &v) : type(t), value(v) {
        }

        Operand(int reg) : type(OperandType::REGISTER), reg_num(reg) {
            value = "r" + std::to_string(reg);
        }
    };

    /**
     * @brief Инструкция виртуальной машины
     */
    struct Instruction {
        OpType op = OpType::NOP;
        Operand dst; // Операнд назначения
        Operand src1; // Первый операнд-источник
        Operand src2; // Второй операнд-источник
        std::string label; // Метка (для LABEL и переходов)
        std::string comment; // Комментарий для отладки

        Instruction() = default;

        Instruction(OpType o) : op(o) {
        }

        Instruction(OpType o, const Operand &d) : op(o), dst(d) {
        }

        Instruction(OpType o, const Operand &d, const Operand &s1)
            : op(o), dst(d), src1(s1) {
        }

        Instruction(OpType o, const Operand &d, const Operand &s1, const Operand &s2)
            : op(o), dst(d), src1(s1), src2(s2) {
        }
    };

    /**
     * @brief Генератор кода в стиле ассемблера
     */
    class Poliz {
    private:
        std::vector<Instruction> code;
        std::unordered_map<std::string, int> label_map; // Метка -> адрес
        std::unordered_map<std::string, int> var_map; // Переменная -> регистр/адрес
        int next_reg = 0; // Следующий свободный регистр
        int next_temp = 0; // Счетчик временных переменных
        int next_label = 0; // Счетчик меток
        const int MAX_REGS = 16; // r0-r15

        // Вспомогательные методы
        int alloc_register();

        void free_register(int reg);

        std::string new_label(const std::string &prefix = "L");

        std::string new_temp();

        void emit(const Instruction &instr);

        void emit_label(const std::string &label);

        // Генерация кода для AST
        Operand generate_expr(ast::AstNode *node);

        void generate_stmt(ast::AstNode *node);

        void generate_binary_op(ast::AstNode *node, OpType op, Operand &result);

        void generate_comparison(ast::AstNode *node, OpType cmp_op, Operand &result);

        // Оптимизации
        void optimize_peephole();

        void optimize_constant_folding();

        void optimize_dead_code();

    public:
        Poliz() = default;

        /**
         * @brief Генерация кода из AST
         */
        void generate(ast::AstNode *root);

        /**
         * @brief Применить все оптимизации
         */
        void optimize();

        /**
         * @brief Получить сгенерированный код
         */
        const std::vector<Instruction> &get_code() const { return code; }

        /**
         * @brief Печать кода в ассемблерном стиле
         */
        void print() const;

        /**
         * @brief Сохранить код в файл
         */
        void save_to_file(const std::string &filename) const;
    };

    /**
     * @brief Интерпретатор с оптимизациями
     */
    class Interpreter {
    private:
        std::vector<Instruction> code;
        std::unordered_map<std::string, int> labels;

        // Регистры и память
        std::vector<double> registers; // r0-r15
        std::unordered_map<std::string, double> memory; // Переменные
        std::vector<double> stack;

        // Флаги процессора
        bool zero_flag = false;
        bool sign_flag = false;
        bool carry_flag = false;

        int pc = 0; // Program counter

        // JIT-like оптимизации
        std::unordered_map<int, int> hotspots; // Адрес -> счетчик вызовов
        void update_hotspot(int addr);

        // Вспомогательные методы
        double get_operand_value(const Operand &op);

        void set_operand_value(const Operand &op, double value);

        void update_flags(double result);

    public:
        Interpreter(const std::vector<Instruction> &code);

        /**
         * @brief Запустить программу
         */
        void run();

        /**
         * @brief Выполнить одну инструкцию
         */
        bool step();

        /**
         * @brief Вывести состояние
         */
        void dump_state() const;
    };
} // namespace poliz

#endif // POLIZ_HPP
