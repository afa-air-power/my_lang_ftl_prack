#!/usr/bin/env python3
"""
Быстрая шпаргалка для тестирования и отладки
"""

import subprocess
import sys
from pathlib import Path

def print_header(text):
    print("\n" + "="*70)
    print(f"🔧 {text}")
    print("="*70 + "\n")

def run_command(cmd, description):
    print(f"📌 {description}...")
    print(f"   Команда: {' '.join(cmd)}\n")
    try:
        result = subprocess.run(cmd, cwd="/home/afa/CLionProjects/my_lang_ftl_prak")
        if result.returncode != 0:
            print(f"⚠️  Ошибка (код {result.returncode})\n")
            return False
        print("✅ Успешно\n")
        return True
    except Exception as e:
        print(f"❌ Исключение: {e}\n")
        return False

def main():
    print_header("БЫСТРАЯ ШПАРГАЛКА ДЛЯ ТЕСТИРОВАНИЯ")

    print("""
Доступные команды:
  1. build    - Собрать проект
  2. test     - Запустить основные тесты
  3. test_ext - Запустить расширенные тесты (с функциями)
  4. report   - Показать отчёт результатов
  5. run      - Запустить программу
  6. clean    - Очистить build директорию
  7. help     - Показать эту справку

Пример: python3 quick_test.py build
    """)

    if len(sys.argv) < 2:
        print("\n⚠️  Укажите команду")
        print("   python3 quick_test.py <команда>")
        return 1

    cmd = sys.argv[1].lower()

    if cmd == "build":
        print_header("СБОРКА ПРОЕКТА")
        run_command(["cmake", "--build", "cmake-build-debug", "--target", "my_lang_ftl_prak"],
                   "Компилирование")

    elif cmd == "test":
        print_header("ОСНОВНЫЕ ТЕСТЫ")
        run_command(["python3", "autotest.py"],
                   "Запуск 20 основных тестов")

    elif cmd == "test_ext":
        print_header("РАСШИРЕННЫЕ ТЕСТЫ (С ФУНКЦИЯМИ)")
        run_command(["python3", "autotest_extended.py"],
                   "Запуск 20 тестов с функциями")

    elif cmd == "report":
        print_header("АНАЛИЗ РЕЗУЛЬТАТОВ")
        run_command(["python3", "test_report.py"],
                   "Просмотр детального отчёта")

    elif cmd == "run":
        if len(sys.argv) < 3:
            print("⚠️  Укажите файл программы")
            print("   python3 quick_test.py run <файл.txt>")
            return 1

        program_file = sys.argv[2]
        print_header(f"ЗАПУСК ПРОГРАММЫ: {program_file}")
        run_command(["./cmake-build-debug/my_lang_ftl_prak", program_file],
                   f"Выполнение {program_file}")

    elif cmd == "clean":
        print_header("ОЧИСТКА")
        try:
            import shutil
            build_dir = Path("/home/afa/CLionProjects/my_lang_ftl_prak/cmake-build-debug")
            if build_dir.exists():
                print("Удаляю директорию cmake-build-debug...")
                shutil.rmtree(build_dir)
                print("✅ Успешно\n")
            else:
                print("Директория уже отсутствует\n")
        except Exception as e:
            print(f"❌ Ошибка: {e}\n")
            return 1

    elif cmd == "help":
        print("""
📚 ПОЛНАЯ СПРАВКА

СБОРКА И ТЕСТИРОВАНИЕ:
  build              Собрать проект
  test               Запустить основные тесты (13 успешных)
  test_ext           Запустить тесты с функциями (19 успешных - 95%!)
  report             Показать детальный отчёт результатов

ЗАПУСК И ОТЛАДКА:
  run <файл.txt>     Запустить программу
  clean              Очистить build директорию

ПРИМЕРЫ ИСПОЛЬЗОВАНИЯ:
  python3 quick_test.py build
  python3 quick_test.py test_ext
  python3 quick_test.py report
  python3 quick_test.py run test_simple.txt

ПРОВЕРКА РЕЗУЛЬТАТОВ:
  Откройте cmake-build-debug/test_results_extended.json
  Или запустите: python3 quick_test.py report

ТестовыЕ ФАЙЛЫ:
  test_simple.txt           - Простое присваивание
  test_add.txt              - Арифметика
  test_arith.txt            - Сложные выражения

ДОКУМЕНТАЦИЯ:
  README.md                 - Краткое описание проекта
  FINAL_REPORT.md          - Полный отчёт о реализации
  TESTING.md               - Подробное описание тестов
        """)

    else:
        print(f"❌ Неизвестная команда: {cmd}")
        print("   Используйте: python3 quick_test.py help")
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())

