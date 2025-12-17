// command_tree.hpp - Структура для оптимизации дерева вызовов
#ifndef COMMAND_TREE_HPP
#define COMMAND_TREE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include "ast.hpp"
#include "poliz.hpp"

namespace optimizer {

    // =============================================================================
    // ТИПЫ КОМАНД
    // =============================================================================

    enum class CommandType {
        FUNCTION_CALL,      // Вызов функции
        ASSIGNMENT,         // Присваивание
        RETURN,             // Возврат значения
        CONDITIONAL,        // Условное выражение (if/else)
        LOOP,               // Цикл (while/for)
        EXPRESSION,         // Выражение
        DECLARATION,        // Объявление переменной
        NOP                 // Пустая операция
    };

    // =============================================================================
    // УЗЕЛ ДЕРЕВА КОМАНД
    // =============================================================================

    struct CommandNode {
        CommandType type;
        std::string name;                           // Имя функции/переменной
        std::vector<std::shared_ptr<CommandNode>> children;
        std::vector<std::shared_ptr<CommandNode>> dependencies; // Зависимости

        // Метаданные для оптимизации
        int execution_count = 0;                    // Счетчик выполнений (hotspot)
        int depth = 0;                              // Глубина в дереве
        bool is_pure = false;                       // Чистая функция (без побочных эффектов)
        bool can_inline = false;                    // Можно ли инлайнить
        bool is_constant = false;                   // Результат константный
        std::optional<double> constant_value;       // Значение константы

        // Исходная информация
        ast::AstNode* source_node = nullptr;        // Ссылка на AST узел
        int line = 0;
        int col = 0;

        CommandNode(CommandType t, const std::string& n = "")
            : type(t), name(n) {}

        void add_child(std::shared_ptr<CommandNode> child) {
            if (child) {
                child->depth = this->depth + 1;
                children.push_back(child);
            }
        }

        void add_dependency(std::shared_ptr<CommandNode> dep) {
            if (dep) {
                dependencies.push_back(dep);
            }
        }
    };

    // =============================================================================
    // ПОСТРОИТЕЛЬ ДЕРЕВА КОМАНД
    // =============================================================================

    class CommandTreeBuilder {
    private:
        std::unordered_map<std::string, std::shared_ptr<CommandNode>> functions;
        std::unordered_map<std::string, std::shared_ptr<CommandNode>> variables;
        std::vector<std::string> pure_functions = {"abs", "sqrt", "pow", "sin", "cos"};

    public:
        // Построить дерево команд из AST
        std::shared_ptr<CommandNode> build_from_ast(ast::AstNode* root);

        // Получить функцию по имени
        std::shared_ptr<CommandNode> get_function(const std::string& name) {
            auto it = functions.find(name);
            return it != functions.end() ? it->second : nullptr;
        }

    private:
        std::shared_ptr<CommandNode> process_node(ast::AstNode* node);
        std::shared_ptr<CommandNode> process_function_call(ast::AstNode* node);
        std::shared_ptr<CommandNode> process_assignment(ast::AstNode* node);
        std::shared_ptr<CommandNode> process_expression(ast::AstNode* node);
        std::shared_ptr<CommandNode> process_statement(ast::AstNode* node);

        bool is_pure_function(const std::string& name);
    };

    // =============================================================================
    // АНАЛИЗАТОР ДЕРЕВА КОМАНД
    // =============================================================================

    class CommandTreeAnalyzer {
    public:
        // Найти hotspots (часто выполняемые команды)
        std::vector<std::shared_ptr<CommandNode>> find_hotspots(
            std::shared_ptr<CommandNode> root, int threshold = 10);

        // Обнаружить циклы в графе вызовов
        bool detect_cycles(std::shared_ptr<CommandNode> root);

        // Найти возможности для инлайнинга
        std::vector<std::shared_ptr<CommandNode>> find_inline_candidates(
            std::shared_ptr<CommandNode> root);

        // Найти константные выражения
        std::vector<std::shared_ptr<CommandNode>> find_constant_expressions(
            std::shared_ptr<CommandNode> root);

        // Построить граф зависимостей
        std::unordered_map<std::string, std::vector<std::string>>
            build_dependency_graph(std::shared_ptr<CommandNode> root);

        // Анализ сложности (оценка O-нотации)
        std::string estimate_complexity(std::shared_ptr<CommandNode> root);

    private:
        void find_hotspots_recursive(
            std::shared_ptr<CommandNode> node,
            std::vector<std::shared_ptr<CommandNode>>& hotspots,
            int threshold);

        bool detect_cycles_dfs(
            std::shared_ptr<CommandNode> node,
            std::unordered_set<std::shared_ptr<CommandNode>>& visited,
            std::unordered_set<std::shared_ptr<CommandNode>>& rec_stack);
    };

    // =============================================================================
    // ОПТИМИЗАТОР ДЕРЕВА КОМАНД
    // =============================================================================

    class CommandTreeOptimizer {
    public:
        // Применить все оптимизации
        void optimize(std::shared_ptr<CommandNode> root);

        // Инлайнинг функций
        void inline_functions(std::shared_ptr<CommandNode> root);

        // Удаление мертвого кода
        void eliminate_dead_code(std::shared_ptr<CommandNode> root);

        // Свертка констант
        void fold_constants(std::shared_ptr<CommandNode> root);

        // Оптимизация хвостовой рекурсии
        void optimize_tail_recursion(std::shared_ptr<CommandNode> root);

        // Векторизация циклов
        void vectorize_loops(std::shared_ptr<CommandNode> root);

        // Переупорядочивание для кэша
        void reorder_for_cache(std::shared_ptr<CommandNode> root);

    private:
        bool can_inline(std::shared_ptr<CommandNode> node);
        bool is_tail_recursive(std::shared_ptr<CommandNode> node);
        bool can_vectorize(std::shared_ptr<CommandNode> node);

        std::shared_ptr<CommandNode> inline_node(std::shared_ptr<CommandNode> node);
        std::shared_ptr<CommandNode> convert_to_loop(std::shared_ptr<CommandNode> node);
    };

    // =============================================================================
    // ВИЗУАЛИЗАЦИЯ ДЕРЕВА КОМАНД
    // =============================================================================

    class CommandTreeVisualizer {
    public:
        // Печать дерева в консоль
        void print(std::shared_ptr<CommandNode> root, int indent = 0);

        // Экспорт в DOT формат (для Graphviz)
        std::string export_to_dot(std::shared_ptr<CommandNode> root);

        // Экспорт в JSON
        std::string export_to_json(std::shared_ptr<CommandNode> root);

        // Статистика
        struct Statistics {
            int total_nodes = 0;
            int function_calls = 0;
            int loops = 0;
            int conditionals = 0;
            int max_depth = 0;
            std::unordered_map<std::string, int> function_call_counts;
        };

        Statistics gather_statistics(std::shared_ptr<CommandNode> root);

    private:
        void print_recursive(std::shared_ptr<CommandNode> node, int indent);
        void export_to_dot_recursive(
            std::shared_ptr<CommandNode> node,
            std::ostringstream& out,
            std::unordered_set<std::shared_ptr<CommandNode>>& visited);
        void gather_statistics_recursive(
            std::shared_ptr<CommandNode> node,
            Statistics& stats);
    };

    // =============================================================================
    // ИНТЕГРАЦИЯ С POLIZ
    // =============================================================================

    class CommandTreeToPoliz {
    public:
        // Конвертировать дерево команд в POLIZ инструкции
        std::vector<poliz::Instruction> convert(std::shared_ptr<CommandNode> root);

    private:
        void convert_recursive(
            std::shared_ptr<CommandNode> node,
            std::vector<poliz::Instruction>& instructions);
    };

    // =============================================================================
    // ПУБЛИЧНЫЙ API ДЛЯ AST_FUNCTIONTREE
    // =============================================================================

    // Построить и оптимизировать дерево команд из AST
    std::shared_ptr<CommandNode> build_and_optimize_command_tree(ast::AstNode* root);

    // Применить оптимизации дерева команд обратно к AST
    void apply_optimizations_to_ast(ast::AstNode* root, std::shared_ptr<CommandNode> cmd_root);

    // Вывести отчет об оптимизациях
    void print_optimization_report(std::shared_ptr<CommandNode> cmd_root);

} // namespace optimizer

#endif // COMMAND_TREE_HPP