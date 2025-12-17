#!/usr/bin/env python3
"""
Утилита для анализа результатов тестирования
Показывает статистику, графики и детальный отчёт
"""

import json
import sys
from pathlib import Path
from datetime import datetime

def print_detailed_report(results_file):
    """Вывести детальный отчёт результатов"""

    with open(results_file, 'r', encoding='utf-8') as f:
        data = json.load(f)

    passed = data['passed']
    failed = data['failed']
    total = data['total']

    print("\n" + "="*80)
    print("📊 ДЕТАЛЬНЫЙ ОТЧЁТ ТЕСТИРОВАНИЯ")
    print("="*80)

    print(f"\n📈 Статистика:")
    print(f"  ✅ Пройдено:      {passed:3d} тестов ({passed/total*100:5.1f}%)")
    print(f"  ❌ Не пройдено:   {failed:3d} тестов ({failed/total*100:5.1f}%)")
    print(f"  📋 Всего:         {total:3d} тестов")

    # Анализ ошибок
    error_types = {}
    for result in data['results']:
        if not result['passed']:
            for error in result['errors']:
                if error.startswith('❌'):
                    error_type = error[2:].split('\n')[0][:50]
                    error_types[error_type] = error_types.get(error_type, 0) + 1

    if error_types:
        print(f"\n🔍 Типы ошибок:")
        for error_type, count in sorted(error_types.items(), key=lambda x: -x[1]):
            print(f"  • {error_type}: {count} раз(а)")

    # Не пройдённые тесты
    failed_tests = [r for r in data['results'] if not r['passed']]
    if failed_tests:
        print(f"\n❌ Не пройдённые тесты ({len(failed_tests)}):")
        for test in failed_tests:
            print(f"\n  📋 {test['name']}")
            for error in test['errors'][:2]:  # Первые 2 ошибки
                print(f"     {error}")

    # Пройдённые тесты
    passed_tests = [r for r in data['results'] if r['passed']]
    print(f"\n✅ Пройдённые тесты ({len(passed_tests)}):")
    for test in passed_tests[:10]:  # Первые 10
        print(f"  ✓ {test['name']}")
    if len(passed_tests) > 10:
        print(f"  ... и ещё {len(passed_tests) - 10} тестов")

    print("\n" + "="*80)
    print("Временная метка: " + datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
    print("="*80 + "\n")

def print_summary_table(results_file):
    """Вывести сводную таблицу результатов"""

    with open(results_file, 'r', encoding='utf-8') as f:
        data = json.load(f)

    print("\n" + "="*100)
    print("📋 СВОДНАЯ ТАБЛИЦА ТЕСТОВ")
    print("="*100)
    print(f"{'№':<4} {'Тест':<35} {'Статус':<12} {'Ошибки':<45}")
    print("-"*100)

    for i, result in enumerate(data['results'], 1):
        status = "✅ PASS" if result['passed'] else "❌ FAIL"
        errors = result['errors']
        error_str = errors[0][4:40] if errors else ""
        test_name = result['name'][:32]
        print(f"{i:<4} {test_name:<35} {status:<12} {error_str:<45}")

    print("="*100 + "\n")

def main():
    build_dir = Path("/home/afa/CLionProjects/my_lang_ftl_prak/cmake-build-debug")
    results_file = build_dir / "test_results.json"

    if not results_file.exists():
        print(f"❌ Файл результатов не найден: {results_file}")
        print("Пожалуйста, сначала запустите автотесты:")
        print("  cd /home/afa/CLionProjects/my_lang_ftl_prak")
        print("  python3 autotest.py")
        return 1

    print_summary_table(results_file)
    print_detailed_report(results_file)

    return 0

if __name__ == "__main__":
    sys.exit(main())

