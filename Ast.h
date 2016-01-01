//
// Created by SonJihoon on 2015. 12. 31..
//

#ifndef KALEIDOSCOPE_AST_H
#define KALEIDOSCOPE_AST_H

#include <string>
#include <vector>

using namespace std;

class ExprAst {
public:
    virtual ~ExprAst() {}
};

class NumberExprAst : public ExprAst {
    double Val;

public:
    NumberExprAst(double Val) : Val(Val) {}
};

class VariableExprAst : public ExprAst {
    string Name;

public:
    VariableExprAst(const string &Name) : Name(Name) {}
};

class BinaryExprAst : public ExprAst {
    char Op;
    unique_ptr<ExprAst> LHS, RHS;

public:
    BinaryExprAst(char op, unique_ptr<ExprAst> LHS, unique_ptr<ExprAst> RHS)
            : Op(op), LHS(move(LHS)), RHS(move(RHS)) {}
};

class CallExprAst : public ExprAst {
    string Callee;
    vector<unique_ptr<ExprAst>> Args;

public:
    CallExprAst(const string &Callee, vector<unique_ptr<ExprAst>> Args)
            : Callee(Callee), Args(move(Args)) {}
};

class PrototypeAst : public ExprAst {
    string Name;
    vector<string> Args;

public:
    PrototypeAst(const string &name, vector<string> Args)
            : Name(name), Args(move(Args)) {}
};

class FunctionAst : public ExprAst {
    unique_ptr<PrototypeAst> Proto;
    unique_ptr<ExprAst> Body;

public:
    FunctionAst(unique_ptr<PrototypeAst> Proto, unique_ptr<ExprAst> Body)
            : Proto(move(Proto)), Body(move(Body)) {}
};

#endif //KALEIDOSCOPE_AST_H
