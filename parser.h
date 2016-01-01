//
// Created by SonJihoon on 2016. 1. 1..
//

#ifndef KALEIDOSCOPE_PARSER_H_H
#define KALEIDOSCOPE_PARSER_H_H

#include <memory>
#include "Ast.h"
#include "lexer.h"
#include "common.h"

using namespace std;

static unique_ptr<ExprAst> ParseNumberExpr() {
    auto Result = helper::make_unique<NumberExprAst>(NumVal);
    getNextToken();
    return move(Result);
}

#endif //KALEIDOSCOPE_PARSER_H_H
