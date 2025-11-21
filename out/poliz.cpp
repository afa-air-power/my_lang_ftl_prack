//
// Created by afa on 21.11.2025.
//
#include <iostream>
#include "ast.hpp"
#include "ast_utils.hpp"
namespace poliz {
    enum class operations {
        ADD,//COUNT AR1 and AR2, put to AR3
        SUB, // SUBTRACT AR2 from AR1, put to AR3
        DIVIDE,// DIVIDE AR1 by AR2, put to AR3
        ASSIGN,// ASSIGN AR2 to AR1
        MUL,// MULTIPLY AR1 and AR2, put to AR3
        DIV,// DIVIDE AR1 by AR2, put to AR3
        PERSENT, // MODULO AR1 by AR2, put to AR3
        INIT, // INIT AR1 with 0
        SET, // SET AR2 to value AR1
        GOTO, // GO TO AR1
        GOTOIF, // GO TO IF (AR1) TO AR2 ELSE TO AR3
        LT, // less than AR1 < AR2
        GT, // greater than AR1 > AR2
        EQ, // equal AR1 == AR2
        LEQ, // less or equal AR1 <= AR2
        GEQ, // greater or equal AR1 >= AR2
        NEQ, // not equal AR1 != AR2
        CALL, // call function AR1 with AR2-AR3(ADDRESSES) arguments
        INPUT, // input to AR1
        OUTPUT, // output from AR1
        RET, // return AR1

    };

    class poliz_token {
    };

    class poliz_node {
        poliz_node *next;
        poliz_node *prev;
        poliz_token token;

        poliz_node() {
            poliz_node::next = poliz_node::prev = poliz_node::prev = nullptr;
        }
    };

    class poliz {
        poliz_node *root;
        poliz_node *gpt;

        poliz() {
            gpt = root = nullptr;
        }



        void make_poliz(ast::AstNode node) {

            for (auto child: node.children) {
                make_poliz(*child);
            }

        }
    };
}
