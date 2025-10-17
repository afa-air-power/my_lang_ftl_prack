import os
import re
from textwrap import dedent


class FormalGrammarParser:
    def __init__(self, filename):
        self.filename = filename
        self.rules = {}

    def parse(self):
        nonterm_re = re.compile(r"<([^<>]+)>")
        with open(self.filename, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                head, body = map(str.strip, line.split("->"))
                head_name = nonterm_re.findall(head)[0]
                alts = [alt.strip() for alt in body.split("|")]
                self.rules[head_name] = [self.tokenize_rule(alt) for alt in alts]
        return self.rules

    def tokenize_rule(self, rule_str):
        tokens = []
        i = 0
        while i < len(rule_str):
            if rule_str[i].isspace():
                i += 1
                continue
            if rule_str[i] == '<':
                j = rule_str.find('>', i)
                tokens.append(rule_str[i:j+1])
                i = j + 1
            else:
                # терминал (может быть символом или словом)
                m = re.match(r"[a-zA-Z0-9_]+|.", rule_str[i:])
                tokens.append(m.group(0))
                i += len(m.group(0))
        return tokens


class CppRecursiveDescentGenerator:
    def __init__(self, grammar: dict[str, list[list[str]]]):
        self.grammar = grammar

    def write_file(self, path, content):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(dedent(content))
        print(f"✅ Generated: {path}")

    def generate(self, output_dir="generated_parser"):
        os.makedirs(output_dir, exist_ok=True)
        header = self._generate_header()
        source = self._generate_source()
        self.write_file(os.path.join(output_dir, "parser.h"), header)
        self.write_file(os.path.join(output_dir, "parser.cpp"), source)

    def _generate_header(self):
        funcs = "\n".join(f"    void {self._fn(n)}();" for n in self.grammar.keys())
        return f"""
        #pragma once
        #include <stdexcept>
        #include <string>
        #include <vector>
        #include <iostream>

        extern char c; // current token
        void gc();     // get next token

        class Parser {{
        public:
            {funcs}
        }};
        """

    def _generate_source(self):
        impls = []
        for nt, rules in self.grammar.items():
            impls.append(self._gen_rule(nt, rules))
        return "\n".join(impls)

    def _fn(self, name: str) -> str:
        """Имя функции парсера по нетерминалу"""
        return f"parse_{name}"

    def _gen_rule(self, nt, productions):
        code = [f"void Parser::{self._fn(nt)}(){{"]
        first_alt = True
        for alt in productions:
            cond = self._first_terminal(alt)
            kw = "if" if first_alt else "else if"
            first_alt = False

            if cond:
                code.append(f"    {kw} (c == '{cond}'){{")
            else:
                code.append(f"    {kw} (true){{")

            for sym in alt:
                if sym.startswith("<") and sym.endswith(">"):
                    subname = sym[1:-1]
                    code.append(f"        {self._fn(subname)}();")
                else:
                    code.append(f"        if (c != '{sym}') throw std::runtime_error(\"expected '{sym}' in <{nt}>\");")
                    code.append(f"        gc();")
            code.append("        return; }")

        code.append(f"    throw std::runtime_error(\"Unexpected symbol in <{nt}>\");")
        code.append("}\n")
        return "\n".join(code)

    def _first_terminal(self, alt):
        """Определяет первый терминал (для условия if)"""
        for sym in alt:
            if not (sym.startswith("<") and sym.endswith(">")):
                return sym
        return None

def main():
    grammar_file = "grammar.txt"
    grammar = FormalGrammarParser(grammar_file).parse()
    gen = CppRecursiveDescentGenerator(grammar)
    gen.generate("generated_parser")

    print("\n🎯 Parser generation complete.")


if __name__ == "__main__":
    main()
