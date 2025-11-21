#ifndef POLIZ_HPP
#define POLIZ_HPP

#include <iostream>
#include <vector>
#include <string>
#include "ast.hpp" // Для AstNode

namespace poliz {

    /**
     * @brief Типы операций виртуальной машины (ПОЛИЗ)
     * * Определяет набор команд, которые будут выполняться стековой машиной.
     */
    enum class OpType {
        NONE,
        PUSH_VAL,   // Операнд (число, переменная, строка)
        ADD, SUB, MUL, DIV, MOD, // Арифметика
        ASSIGN,     // =
        EQ, NEQ, LT, GT, LEQ, GEQ, // Сравнения
        GOTO,       // Безусловный переход
        JMP_FALSE,  // Переход, если на вершине стека ложь (0)
        PRINT,      // Вывод
        INPUT,      // Ввод
        MEMBER,     // Доступ к полю (точка, например, p.hp)
        CALL,       // Вызов функции/метода
    };

    /**
     * @brief Элемент Обратной Польской Записи (Reverse Polish Notation Item)
     * * Содержит либо операцию, либо значение операнда.
     */
    struct PolizItem {
        OpType op = OpType::NONE;   // Тип операции
        std::string value;          // Строковое значение (имя переменной, число, строка)
        int jump_index = -1;        // Куда прыгать (для GOTO/JMP_FALSE, -1 по умолчанию)

        // Конструкторы для удобства
        PolizItem(OpType t) : op(t) {}
        PolizItem(std::string v); // Реализация в poliz.cpp
        PolizItem(OpType t, int idx) : op(t), jump_index(idx) {}
    };

    /**
     * @brief Класс генератора ПОЛИЗа
     * * Отвечает за обход AST и линеаризацию кода в последовательность команд.
     */
    class Poliz {
    private:
        std::vector<PolizItem> items;

    public:
        OpType token_to_op(parser::TokenType tt, const std::string& val);
        // Конструктор по умолчанию
        Poliz() = default;

        /**
         * @brief Запускает генерацию ПОЛИЗа, обходя дерево AST.
         * * @param node Корень AST.
         */
        void generate(ast::AstNode* node);

        /**
         * @brief Возвращает сгенерированный вектор команд ПОЛИЗа.
         */
        const std::vector<PolizItem>& get_items() const;

        /**
         * @brief Печатает сгенерированный ПОЛИЗ в консоль для отладки.
         */
        void print();
    };
} // namespace poliz

#endif // POLIZ_HPP