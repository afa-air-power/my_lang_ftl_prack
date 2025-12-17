#!/usr/bin/env python3
"""
Автоматический тестер для компилятора my_lang_ftl_prak
Проверяет ПОЛИЗ генерацию и интерпретацию
"""

import os
import sys
import subprocess
import json
from pathlib import Path
from dataclasses import dataclass
from typing import List, Tuple, Optional

@dataclass
class TestCase:
    name: str
    code: str
    expected_output: str
    expected_poliz_contains: List[str]  # Инструкции, которые должны быть в ПОЛИЗ
    description: str

class TestSuite:
    def __init__(self, compiler_path: str, build_dir: str):
        self.compiler_path = compiler_path
        self.build_dir = build_dir
        self.test_dir = Path(build_dir) / "test_cases"
        self.test_dir.mkdir(exist_ok=True)
        self.passed = 0
        self.failed = 0
        self.results = []

    def create_test_file(self, test_name: str, code: str) -> Path:
        """Создать файл с кодом теста"""
        test_file = self.test_dir / f"{test_name}.txt"
        test_file.write_text(code)
        return test_file

    def run_compiler(self, test_file: Path) -> Tuple[str, str, int]:
        """Запустить компилятор на файле теста"""
        try:
            result = subprocess.run(
                [self.compiler_path, str(test_file)],
                cwd=self.build_dir,
                capture_output=True,
                text=True,
                timeout=10
            )
            return result.stdout, result.stderr, result.returncode
        except subprocess.TimeoutExpired:
            return "", "Timeout", 1
        except Exception as e:
            return "", str(e), 1

    def extract_poliz(self, output: str) -> str:
        """Извлечь ПОЛИЗ из вывода"""
        start = output.find("========== ASSEMBLY-LIKE CODE")
        end = output.find("========================================", start)
        if start != -1 and end != -1:
            return output[start:end+40]
        return ""

    def extract_program_output(self, output: str) -> str:
        """Извлечь вывод программы из результата интерпретатора"""
        start = output.find("========== INTERPRETER OUTPUT")
        end = output.find("========================================", start)
        if start != -1 and end != -1:
            section = output[start:end]
            # Извлечь только строки с выводом (между маркерами)
            lines = section.split('\n')[1:]  # Пропустить заголовок
            output_lines = []
            for line in lines:
                if line and not line.startswith('='):
                    output_lines.append(line.strip())
            return '\n'.join(output_lines)
        return ""

    def run_test(self, test_case: TestCase) -> bool:
        """Запустить тестовый случай"""
        print(f"\n{'='*70}")
        print(f"📋 Тест: {test_case.name}")
        print(f"📝 Описание: {test_case.description}")
        print(f"{'='*70}")

        # Создать файл теста
        test_file = self.create_test_file(test_case.name, test_case.code)

        # Запустить компилятор
        stdout, stderr, returncode = self.run_compiler(test_file)

        # Проверить результат
        passed = True
        errors = []

        # 1. Проверить успешное выполнение
        if returncode != 0 and stderr:
            passed = False
            errors.append(f"❌ Ошибка компиляции:\n{stderr}")

        # 2. Проверить наличие ПОЛИЗ
        poliz_output = self.extract_poliz(stdout)
        if not poliz_output:
            passed = False
            errors.append("❌ ПОЛИЗ не был сгенерирован")

        # 3. Проверить наличие требуемых инструкций в ПОЛИЗ
        for required_instr in test_case.expected_poliz_contains:
            if required_instr not in poliz_output:
                passed = False
                errors.append(f"❌ Инструкция '{required_instr}' не найдена в ПОЛИЗ")

        # 4. Проверить вывод программы
        program_output = self.extract_program_output(stdout).strip()
        expected = test_case.expected_output.strip()
        if program_output != expected:
            passed = False
            errors.append(f"❌ Неправильный вывод программы")
            errors.append(f"   Ожидается: '{expected}'")
            errors.append(f"   Получено: '{program_output}'")

        # Вывести результаты
        if passed:
            print("✅ ТЕСТ ПРОЙДЕН")
            self.passed += 1
        else:
            print("❌ ТЕСТ НЕ ПРОЙДЕН")
            for error in errors:
                print(error)
            self.failed += 1

        # Показать ПОЛИЗ
        print("\n📊 Сгенерированный ПОЛИЗ:")
        if poliz_output:
            print(poliz_output[:500])  # Первые 500 символов
            if len(poliz_output) > 500:
                print("...(обрезано)...")
        else:
            print("(Не сгенерирован)")

        # Показать вывод программы
        print(f"\n🖨️  Вывод программы: '{program_output}'")

        self.results.append({
            'name': test_case.name,
            'passed': passed,
            'errors': errors
        })

        return passed

    def run_all_tests(self, test_cases: List[TestCase]) -> bool:
        """Запустить все тесты"""
        print("\n" + "="*70)
        print("🚀 ЗАПУСК ТЕСТОВ КОМПИЛЯТОРА my_lang_ftl_prak")
        print("="*70)

        for test_case in test_cases:
            self.run_test(test_case)

        # Итоговый отчёт
        self.print_summary()
        return self.failed == 0

    def print_summary(self):
        """Вывести итоговый отчёт"""
        total = self.passed + self.failed
        print("\n" + "="*70)
        print("📊 ИТОГОВЫЙ ОТЧЁТ")
        print("="*70)
        print(f"✅ Пройдено: {self.passed}/{total}")
        print(f"❌ Не пройдено: {self.failed}/{total}")

        if self.failed > 0:
            print("\n❌ Не пройдённые тесты:")
            for result in self.results:
                if not result['passed']:
                    print(f"  - {result['name']}")

        percentage = (self.passed / total * 100) if total > 0 else 0
        print(f"\n📈 Процент успеха: {percentage:.1f}%")
        print("="*70 + "\n")


def create_test_cases() -> List[TestCase]:
    """Создать набор тестовых случаев"""
    return [
        # Тест 1: Присваивание константы
        TestCase(
            name="test_01_assign_const",
            code="""int main() {
    int x;
    x = 42;
}""",
            expected_output="",
            expected_poliz_contains=["load", "store", "halt"],
            description="Присваивание целого числа переменной"
        ),

        # Тест 2: Простое арифметическое выражение
        TestCase(
            name="test_02_simple_add",
            code="""int main() {
    int result;
    result = 5 + 3;
}""",
            expected_output="",
            expected_poliz_contains=["load", "add", "store", "halt"],
            description="Сложение двух констант"
        ),

        # Тест 3: Вычитание
        TestCase(
            name="test_03_subtraction",
            code="""int main() {
    int x;
    x = 10 - 3;
}""",
            expected_output="",
            expected_poliz_contains=["load", "sub", "store"],
            description="Вычитание двух чисел"
        ),

        # Тест 4: Умножение
        TestCase(
            name="test_04_multiplication",
            code="""int main() {
    int result;
    result = 6 * 7;
}""",
            expected_output="",
            expected_poliz_contains=["load", "mul", "store"],
            description="Умножение двух чисел"
        ),

        # Тест 5: Деление
        TestCase(
            name="test_05_division",
            code="""int main() {
    int result;
    result = 20 / 4;
}""",
            expected_output="",
            expected_poliz_contains=["load", "div", "store"],
            description="Деление двух чисел"
        ),

        # Тест 6: Модуль
        TestCase(
            name="test_06_modulo",
            code="""int main() {
    int result;
    result = 17 % 5;
}""",
            expected_output="",
            expected_poliz_contains=["load", "mod", "store"],
            description="Остаток от деления"
        ),

        # Тест 7: Несколько переменных
        TestCase(
            name="test_07_multiple_vars",
            code="""int main() {
    int a;
    int b;
    a = 10;
    b = 20;
}""",
            expected_output="",
            expected_poliz_contains=["load", "store", "halt"],
            description="Объявление и инициализация нескольких переменных"
        ),

        # Тест 8: Цепочка присваиваний
        TestCase(
            name="test_08_chain_assign",
            code="""int main() {
    int x;
    int y;
    x = 5;
    y = x;
}""",
            expected_output="",
            expected_poliz_contains=["load", "store"],
            description="Присваивание значения от одной переменной к другой"
        ),

        # Тест 9: Двойное арифметическое выражение
        TestCase(
            name="test_09_double_arith",
            code="""int main() {
    int result;
    result = (5 + 3) * 2;
}""",
            expected_output="",
            expected_poliz_contains=["load", "add", "mul", "store"],
            description="Комплексное арифметическое выражение"
        ),

        # Тест 10: Сравнение (==)
        TestCase(
            name="test_10_compare_eq",
            code="""int main() {
    int result;
    result = 5 == 5;
}""",
            expected_output="",
            expected_poliz_contains=["load", "cmp", "halt"],
            description="Проверка равенства"
        ),

        # Тест 11: Сравнение (<)
        TestCase(
            name="test_11_compare_lt",
            code="""int main() {
    int result;
    result = 3 < 5;
}""",
            expected_output="",
            expected_poliz_contains=["load", "cmp"],
            description="Проверка 'меньше'"
        ),

        # Тест 12: Сравнение (>)
        TestCase(
            name="test_12_compare_gt",
            code="""int main() {
    int result;
    result = 10 > 5;
}""",
            expected_output="",
            expected_poliz_contains=["load", "cmp"],
            description="Проверка 'больше'"
        ),

        # Тест 13: IF условие
        TestCase(
            name="test_13_if_statement",
            code="""int main() {
    int x;
    x = 5;
    if (x > 3) {
        x = 10;
    }
}""",
            expected_output="",
            expected_poliz_contains=["load", "cmp", "jg"],
            description="IF условие с переходом"
        ),

        # Тест 14: WHILE цикл
        TestCase(
            name="test_14_while_loop",
            code="""int main() {
    int x;
    x = 0;
    while (x < 5) {
        x = x + 1;
    }
}""",
            expected_output="",
            expected_poliz_contains=["load", "cmp", "jle", "add"],
            description="WHILE цикл"
        ),

        # Тест 15: Логическое И
        TestCase(
            name="test_15_logical_and",
            code="""int main() {
    int result;
    result = 1 && 1;
}""",
            expected_output="",
            expected_poliz_contains=["load", "and"],
            description="Логическое И (&&)"
        ),

        # Тест 16: Логическое ИЛИ
        TestCase(
            name="test_16_logical_or",
            code="""int main() {
    int result;
    result = 0 || 1;
}""",
            expected_output="",
            expected_poliz_contains=["load", "or"],
            description="Логическое ИЛИ (||)"
        ),

        # Тест 17: Отрицание
        TestCase(
            name="test_17_logical_not",
            code="""int main() {
    int a;
    a = 5;
    if (!(a == 10)) {
        a = 1;
    }
}""",
            expected_output="",
            expected_poliz_contains=["load", "cmp"],
            description="Логическое отрицание (!)"
        ),

        # Тест 18: Локальная переменная в области видимости
        TestCase(
            name="test_18_local_var",
            code="""int main() {
    int x;
    x = 5;
    {
        int y;
        y = 10;
    }
}""",
            expected_output="",
            expected_poliz_contains=["load", "store"],
            description="Локальная переменная в блоке"
        ),

        # Тест 19: Дробные числа
        TestCase(
            name="test_19_float",
            code="""int main() {
    double x;
    x = 3.14;
}""",
            expected_output="",
            expected_poliz_contains=["load", "store"],
            description="Работа с дробными числами"
        ),

        # Тест 20: Пустая программа
        TestCase(
            name="test_20_empty_main",
            code="""int main() {
}""",
            expected_output="",
            expected_poliz_contains=["halt"],
            description="Пустая функция main"
        ),
    ]


def main():
    # Найти пути
    script_dir = Path(__file__).parent
    build_dir = script_dir / "cmake-build-debug"
    compiler_path = build_dir / "my_lang_ftl_prak"

    # Проверить наличие компилятора
    if not compiler_path.exists():
        print(f"❌ Компилятор не найден: {compiler_path}")
        print("Пожалуйста, сначала соберите проект:")
        print("  cd /home/afa/CLionProjects/my_lang_ftl_prak")
        print("  cmake --build cmake-build-debug --target my_lang_ftl_prak")
        return 1

    # Создать тестер и запустить тесты
    tester = TestSuite(str(compiler_path), str(build_dir))
    test_cases = create_test_cases()

    success = tester.run_all_tests(test_cases)

    # Сохранить результаты в JSON
    results_file = build_dir / "test_results.json"
    with open(results_file, 'w', encoding='utf-8') as f:
        json.dump({
            'passed': tester.passed,
            'failed': tester.failed,
            'total': tester.passed + tester.failed,
            'results': tester.results
        }, f, ensure_ascii=False, indent=2)

    print(f"\n📁 Результаты сохранены в: {results_file}")

    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())

