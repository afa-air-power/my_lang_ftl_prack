//
// Created by afa on 19.10.2025.
//

#include <gtest/gtest.h>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <sstream>

TEST(GrammarTest, IntegrityCheck) {
    std::ifstream file("grammar.txt");
    ASSERT_TRUE(file.is_open()) << "grammar.txt not found";

    std::set<std::string> defined;
    std::set<std::string> referenced;
    std::string line;
    std::regex rule_re(R"(^\s*<[^<>]+>\s*->)");
    std::regex nonterm_re(R"(<[^<>]+>)");

    int lineno = 0;
    while (std::getline(file, line)) {
        ++lineno;
        if (line.empty() || line.starts_with("//"))
            continue;

        ASSERT_TRUE(std::regex_search(line, rule_re))
            << "Line " << lineno << ": missing '->' or invalid rule";

        auto pos = line.find("->");
        std::string head = line.substr(0, pos);
        std::smatch m;
        if (std::regex_search(head, m, nonterm_re))
            defined.insert(m.str());

        std::string rhs = line.substr(pos + 2);
        std::istringstream iss(rhs);
        std::string sym;
        while (iss >> sym) {
            if (std::regex_match(sym, nonterm_re))
                referenced.insert(sym);
        }
    }

    for (const auto &ref : referenced) {
        EXPECT_TRUE(defined.count(ref))
            << "Referenced nonterminal not defined: " << ref;
    }

    EXPECT_TRUE(defined.count("<Program>"))
        << "Missing start rule <Program>";
}
