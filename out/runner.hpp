#ifndef RUNNER_HPP
#define RUNNER_HPP

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include "poliz.hpp"

namespace runner {

/**
 * @brief Интерпретатор ассемблерного кода с расширенными возможностями
 */
class Interpreter {
private:
    std::vector<poliz::Instruction> code;
    std::unordered_map<std::string, int> labels;

    // Регистры и память для чисел
    std::vector<double> registers;
    std::unordered_map<std::string, double> memory;
    std::vector<double> stack;

    // Регистры и память для строк
    std::vector<std::string> string_registers;
    std::unordered_map<std::string, std::string> string_memory;
    std::vector<std::string> string_stack;

    // Отслеживание типов в регистрах (true = строка, false = число)
    std::vector<bool> register_is_string;

    // Флаги процессора
    bool zero_flag = false;
    bool sign_flag = false;
    bool carry_flag = false;

    int pc = 0;

    // JIT-like оптимизации
    std::unordered_map<int, int> hotspots;
    void update_hotspot(int addr);

    // Вспомогательные методы
    double get_operand_value(const poliz::Operand& op);
    void set_operand_value(const poliz::Operand& op, double value);
    void update_flags(double result);
    
    // Методы для работы со строками
    std::string get_string_operand_value(const poliz::Operand& op);
    void set_string_operand_value(const poliz::Operand& op, const std::string& value);

    // Вывод инструкций
    void print_instruction(const poliz::Instruction& instr) const;

public:
    Interpreter(const std::vector<poliz::Instruction>& code);

    /**
     * @brief Запустить программу до конца
     */
    void run();

    /**
     * @brief Выполнить одну инструкцию
     * @return true если программа продолжается, false если завершилась
     */
    bool step();

    /**
     * @brief Пошаговое выполнение с интерактивным управлением
     */
    void step_by_step_mode();

    /**
     * @brief Вывести текущее состояние машины
     */
    void dump_state() const;
    
    /**
     * @brief Получить значение регистра
     */
    double get_register(int num) const {
        if (num >= 0 && num < 16) return registers[num];
        return 0.0;
    }
    
    /**
     * @brief Получить значение переменной из памяти
     */
    double get_variable(const std::string& name) const {
        auto it = memory.find(name);
        return it != memory.end() ? it->second : 0.0;
    }
    
    /**
     * @brief Получить счетчик команд
     */
    int get_pc() const { return pc; }
    
    /**
     * @brief Получить размер стека
     */
    size_t get_stack_size() const { return stack.size(); }
};

// =============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ТЕСТИРОВАНИЯ
// =============================================================================

/**
 * @brief Базовый тест интерпретатора (5 + 3)
 */
void test_interpreter_basic();

/**
 * @brief Тест циклов (сумма от 1 до 10)
 */
void test_interpreter_loops();

/**
 * @brief Тест условных переходов
 */
void test_interpreter_conditionals();

} // namespace runner

#endif // RUNNER_HPP