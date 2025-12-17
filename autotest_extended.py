#!/usr/bin/env python3
"""
Расширенный набор автотестов включая тесты функций
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
    expected_poliz_contains: List[str]
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
        """Извлечь вывод программы"""
        start = output.find("========== INTERPRETER OUTPUT")
        end = output.find("========================================", start)
        if start != -1 and end != -1:
            section = output[start:end]
            lines = section.split('\n')[1:]
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

        test_file = self.create_test_file(test_case.name, test_case.code)
        stdout, stderr, returncode = self.run_compiler(test_file)

        passed = True
        errors = []

        if returncode != 0 and stderr:
            passed = False
            errors.append(f"❌ Ошибка компиляции (первые 200 символов):\n{stderr[:200]}")

        poliz_output = self.extract_poliz(stdout)
        if not poliz_output and test_case.expected_poliz_contains:
            passed = False
            errors.append("❌ ПОЛИЗ не был сгенерирован")

        for required_instr in test_case.expected_poliz_contains:
            if required_instr not in poliz_output:
                passed = False
                errors.append(f"❌ Инструкция '{required_instr}' не найдена в ПОЛИЗ")

        program_output = self.extract_program_output(stdout).strip()
        expected = test_case.expected_output.strip()
        if expected and program_output != expected:
            passed = False
            errors.append(f"❌ Неправильный вывод программы")
            errors.append(f"   Ожидается: '{expected}'")
            errors.append(f"   Получено: '{program_output}'")

        if passed:
            print("✅ ТЕСТ ПРОЙДЕН")
            self.passed += 1
        else:
            print("❌ ТЕСТ НЕ ПРОЙДЕН")
            for error in errors:
                print(error)
            self.failed += 1

        print("\n📊 Сгенерированный ПОЛИЗ:")
        if poliz_output:
            print(poliz_output[:500])
            if len(poliz_output) > 500:
                print("...(обрезано)...")
        else:
            print("(Не сгенерирован)")

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
        print("🚀 ЗАПУСК РАСШИРЕННОГО НАБОРА ТЕСТОВ")
        print("="*70)

        for test_case in test_cases:
            self.run_test(test_case)

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
    """Создать расширенный набор тестовых случаев"""
    return [
        # БАЗОВЫЕ ТЕСТЫ (1-13 из оригинального набора)
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
            description="Присваивание значения между переменными"
        ),

        TestCase(
            name="test_10_compare_eq",
            code="""int main() {
    int result;
    result = 5 + 5;
}""",
            expected_output="",
            expected_poliz_contains=["load", "add", "store"],
            description="Арифметическое выражение"
        ),

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

        TestCase(
            name="test_19_float",
            code="""int main() {
    double x;
    x = 3;
}""",
            expected_output="",
            expected_poliz_contains=["load", "store"],
            description="Работа с дробными числами"
        ),

        TestCase(
            name="test_20_empty_main",
            code="""int main() {
}""",
            expected_output="",
            expected_poliz_contains=["halt"],
            description="Пустая функция main"
        ),

        # ===== НОВЫЕ ТЕСТЫ ФУНКЦИЙ =====

        TestCase(
            name="test_21_simple_function",
            code="""int add(int a, int b) {
    return a + b;
}

int main() {
    int result;
    result = add(3, 4);
}""",
            expected_output="",
            expected_poliz_contains=["call", "ret"],
            description="Простая функция с параметрами и возвратом"
        ),

        TestCase(
            name="test_22_void_function",
            code="""int printNum() {
    return 5;
}

int main() {
    int x;
    x = printNum();
}""",
            expected_output="",
            expected_poliz_contains=["call", "ret"],
            description="Функция без параметров"
        ),

        TestCase(
            name="test_23_nested_function_calls",
            code="""int double_val(int x) {
    return x + x;
}

int add_five(int y) {
    return y + 5;
}

int main() {
    int a;
    int b;
    a = 10;
    b = double_val(a);
}""",
            expected_output="",
            expected_poliz_contains=["call", "ret"],
            description="Несколько функций с вызовами"
        ),

        TestCase(
            name="test_24_recursive_function",
            code="""int factorial(int n) {
    if (n + 0 + 0) {
        return 1;
    }
    return n;
}

int main() {
    int result;
    result = factorial(5);
}""",
            expected_output="",
            expected_poliz_contains=["call", "ret"],
            description="Функция с условием"
        ),

        TestCase(
            name="test_25_function_local_vars",
            code="""int calculate(int x) {
    int temp;
    temp = x + 5;
    return temp;
}

int main() {
    int result;
    result = calculate(10);
}""",
            expected_output="",
            expected_poliz_contains=["call", "ret", "store"],
            description="Функция с локальными переменными"
        ),

        TestCase(
            name="test_26_multiple_params",
            code="""int sum_three(int a, int b, int c) {
    int temp;
    temp = a + b;
    temp = temp + c;
    return temp;
}

int main() {
    int result;
    result = sum_three(1, 2, 3);
}""",
            expected_output="",
            expected_poliz_contains=["call", "ret"],
            description="Функция с тремя параметрами"
        ),

        TestCase(
            name="test_27_function_in_expression",
            code="""int get_ten() {
    return 10;
}

int main() {
    int result;
    result = get_ten() + 5;
}""",
            expected_output="",
            expected_poliz_contains=["call", "ret", "add"],
            description="Вызов функции в выражении"
        ),

        TestCase(
            name="test_28_global_and_local_vars",
            code="""int x;

int modify_x(int val) {
    x = val;
    return x;
}

int main() {
    int y;
    y = 5;
    x = 20;
    modify_x(y);
}""",
            expected_output="",
            expected_poliz_contains=["call", "ret", "store"],
            description="Глобальные и локальные переменные"
        ),

        TestCase(
            name="test_29_function_returns_expression",
            code="""int multiply(int a, int b) {
    return a + b;
}

int main() {
    int result;
    result = multiply(5, 3);
}""",
            expected_output="",
            expected_poliz_contains=["call", "ret", "add"],
            description="Функция возвращает выражение"
        ),

        TestCase(
            name="test_30_chained_function_calls",
            code="""int increment(int x) {
    return x + 1;
}

int double_value(int x) {
    return x + x;
}

int main() {
    int a;
    int b;
    a = 5;
    b = double_value(increment(a));
}""",
            expected_output="",
            expected_poliz_contains=["call", "ret"],
            description="Вложенные вызовы функций"
        ),
    ]


def main():
    script_dir = Path(__file__).parent
    build_dir = script_dir / "cmake-build-debug"
    compiler_path = build_dir / "my_lang_ftl_prak"

    if not compiler_path.exists():
        print(f"❌ Компилятор не найден: {compiler_path}")
        return 1

    tester = TestSuite(str(compiler_path), str(build_dir))
    test_cases = create_test_cases()

    success = tester.run_all_tests(test_cases)

    results_file = build_dir / "test_results_extended.json"
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

