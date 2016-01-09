#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "KaleidoscopeJIT.h"

using namespace std;
using namespace llvm;

enum Token {
    TOK_EOF = -1,

    // commands
    TOK_DEF = -2,
    TOK_EXTERN = -3,

    // primary
    TOK_IDENT = -4,
    TOK_NUMBER = -5,
    TOK_OP = -6,

    // parenthesis
    TOK_LP = '(',
    TOK_RP = ')',
};

static string IdentifierStr;
static double NumVal;

static int gettok() {
    static int LastChar = ' ';

    // eat whitespaces
    while (isspace(LastChar)) {
        LastChar = getchar();
    }

    if (isalpha(LastChar)) { // [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar())))
            IdentifierStr += LastChar;

        if (IdentifierStr == "def")
            return TOK_DEF;
        else if (IdentifierStr == "extern")
            return TOK_EXTERN;
        else
            return TOK_IDENT;
    }

    if (isdigit(LastChar)) { // [0-9].[0-9]+
        string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) && LastChar != '.');

        if (LastChar == '.') {
            NumStr += LastChar;
            LastChar = getchar();
            if (isdigit(LastChar)) {
                do {
                    NumStr += LastChar;
                    LastChar = getchar();
                } while (isdigit(LastChar));
            } else {
                // add 0
                NumStr += '0';
            }
        }
        NumVal = strtod(NumStr.c_str(), 0);
        return TOK_NUMBER;
    }

    if (LastChar == '#') { // comment
        do {
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF) {
            return gettok();
        }
    }

    if (LastChar == EOF) {
        return TOK_EOF;
    }

    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}

static int CurTok;
static int getNextToken() {
    return CurTok = gettok();
}

// The module is the top-level structure that the LLVM IR uses to contain code like functions and global variables.
// It will own memory for all of the IR, so we need to keep the raw pointer for Value rather than unique_ptr<Value>.
static unique_ptr<Module> TheModule;

static IRBuilder<> Builder(getGlobalContext());
static map<string, Value*> NamedValues; // symbol table

class ExprAST;
class PrototypeAST;

std::unique_ptr<ExprAST> Error(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> ErrorP(const char *Str) {
    Error(Str);
    return nullptr;
}

Value* ErrorV(const char *Str) {
    Error(Str);
    return nullptr;
}

class ExprAST {
public:
    virtual ~ExprAST() {}
    virtual Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
    double Val;

public:
    NumberExprAST(double Val) : Val(Val) {}

    Value* codegen() {
        return ConstantFP::get(getGlobalContext(), APFloat(Val));
    }
};

class VariableExprAST : public ExprAST {
    string Name;

public:
    VariableExprAST(const string &Name) : Name(Name) {}

    Value* codegen() {
        Value *V = NamedValues[Name];
        if (!V)
            ErrorV("Unknown variable name");
        return V;
    }
};

class BinaryExprAST : public ExprAST {
    char Op;
    unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char op, unique_ptr<ExprAST> LHS, unique_ptr<ExprAST> RHS)
            : Op(op), LHS(move(LHS)), RHS(move(RHS)) {}

    Value* codegen() {
        Value *L = LHS->codegen();
        Value *R = RHS->codegen();
        if (!L || !R)
            return nullptr;

        switch (Op) {
            case '+':
                return Builder.CreateFAdd(L, R, "addtmp");
            case '-':
                return Builder.CreateFSub(L, R, "subtmp");
            case '*':
                return Builder.CreateFMul(L, R, "multmp");
            case '<':
                L = Builder.CreateFCmpULT(L, R, "cmptmp");
                // convert i1 type for bool to double type
                return Builder.CreateUIToFP(L, Type::getDoubleTy(getGlobalContext()), "booltmp");
            default:
                return ErrorV("invalid binary operator");
        }
    }
};

class CallExprAST : public ExprAST {
    string Callee;
    vector<unique_ptr<ExprAST>> Args;

public:
    CallExprAST(const string &Callee, vector<unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(move(Args)) {}

    Value* codegen() {
        Function *CalleeFunc = TheModule->getFunction(Callee);
        if (!CalleeFunc)
            return ErrorV("Unknown function referenced");

        if (CalleeFunc->arg_size() != Args.size())
            return ErrorV("Incorrect # arguments passed");

        vector<Value*> ArgsValues;
        for (unsigned i = 0, e = Args.size(); i != e; ++i) {
            ArgsValues.push_back(Args[i]->codegen());
            if (!ArgsValues.back())
                return nullptr;
        }

        return Builder.CreateCall(CalleeFunc, ArgsValues, "calltmp");
    }
};

class PrototypeAST : public ExprAST {
    string Name;
    vector<string> Args;

public:
    PrototypeAST(const string &name, vector<string> Args)
            : Name(name), Args(move(Args)) {}

    Function* codegen() {
        vector<Type*> Doubles(Args.size(), Type::getDoubleTy(getGlobalContext()));

        FunctionType *FT = FunctionType::get(Type::getDoubleTy(getGlobalContext()), Doubles, false);

        // function type, linkage, name, and which module to insert into
        // external linkage means that the funciton may be defined outside the current module
        Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

        // not mandatory, but improve readability and allow to refer directly to the arguments for their names
        unsigned Idx = 0;
        for (auto &Arg : F->args())
            Arg.setName(Args[Idx++]);

        return F;
    }

    const string &getName() const {
        return Name;
    }
};

class FunctionAST : public ExprAST {
    unique_ptr<PrototypeAST> Proto;
    unique_ptr<ExprAST> Body;

public:
    FunctionAST(unique_ptr<PrototypeAST> Proto, unique_ptr<ExprAST> Body)
            : Proto(move(Proto)), Body(move(Body)) {}

    Function* codegen() {
        Function *TheFunction = TheModule->getFunction(Proto->getName());

        if (!TheFunction)
            TheFunction = Proto->codegen();

        if (!TheFunction) {
            return nullptr;
        }

//        if (!TheFunction->empty())
//            return (Function *) ErrorV("Function cannot be redefined");

        // TODO: check that proto and the found function have the same arguments

        BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", TheFunction);
        Builder.SetInsertPoint(BB);

        NamedValues.clear();
        for (auto &Arg : TheFunction->args())
            NamedValues[Arg.getName()] = &Arg;

        if (Value *RetVal = Body->codegen()) {
            Builder.CreateRet(RetVal);
            verifyFunction(*TheFunction);

            return TheFunction;
        }

        TheFunction->eraseFromParent();
        return nullptr;
    }
};

static map<char, int> BinopPrecedence;

static int GetTokPrecedence() {
    if (!isascii(CurTok))
        return -1;

    int TokPrec = BinopPrecedence[CurTok];
    return TokPrec <= 0 ? -1 : TokPrec;
}

static unique_ptr<ExprAST> ParseExpression();

static unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = llvm::make_unique<NumberExprAST>(NumVal);
    getNextToken();
    return move(Result);
}

static unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken();
    auto V = ParseExpression();
    if (!V)
        return nullptr;

    if (CurTok != TOK_RP)
        return Error("expected ')'");
    getNextToken();
    return V;
}

static unique_ptr<ExprAST> ParseIdentExpr() {
    string IdName = IdentifierStr;
    getNextToken();

    if (CurTok != TOK_LP)
        return llvm::make_unique<VariableExprAST>(IdName);

    // eat '('
    getNextToken();
    vector<unique_ptr<ExprAST>> Args;
    if (CurTok != TOK_RP) {
        while (1) {
            if (auto Arg = ParseExpression())
                Args.push_back(move(Arg));
            else
                return nullptr;

            if (CurTok == TOK_RP)
                break;
            if (CurTok != ',')
                return Error("Expected ')' or ',' in argument list");
            getNextToken();
        }
    }

    // eat the ')'
    getNextToken();

    return llvm::make_unique<CallExprAST>(IdName, move(Args));
}

static unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
        default:
            return Error("unknown token when expecting an expression");
        case TOK_IDENT:
            return ParseIdentExpr();
        case TOK_NUMBER:
            return ParseNumberExpr();
        case TOK_LP:
            return ParseParenExpr();
    }
}

static unique_ptr<ExprAST> ParseBinOpRHS(int ParentPrec, unique_ptr<ExprAST> LHS) {
    while (1) {
        int CurPrec = GetTokPrecedence();

        if (CurPrec < ParentPrec) {
            return LHS;
        }

        int BinOp = CurTok;
        getNextToken();

        auto RHS = ParsePrimary();
        if (!RHS)
            return nullptr;

        int NextPrec = GetTokPrecedence();
        if (CurPrec < NextPrec) {
            RHS = ParseBinOpRHS(CurPrec + 1, move(RHS));
            if (!RHS)
                return nullptr;
        }

        LHS = llvm::make_unique<BinaryExprAST>(BinOp, move(LHS), move(RHS));
    }
}

static unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();
    if (!LHS)
        return nullptr;
    return ParseBinOpRHS(0, move(LHS));
}

static unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != TOK_IDENT)
        return ErrorP("Expected function name in prototype");

    string FuncName = IdentifierStr;
    getNextToken();

    if (CurTok != TOK_LP)
        return ErrorP("Expected '(' in prototype");

    vector<string> ArgNames;
    while (getNextToken() == TOK_IDENT)
        ArgNames.push_back(IdentifierStr);
    if (CurTok != TOK_RP)
        return ErrorP("Expected ')' in prototype");

    getNextToken();

    return llvm::make_unique<PrototypeAST>(FuncName, move(ArgNames));
}

static unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken();
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;

    if (auto E = ParseExpression())
        return llvm::make_unique<FunctionAST>(move(Proto), move(E));
    return nullptr;
}

static unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken();
    return ParsePrototype();
}

static unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        auto Proto = llvm::make_unique<PrototypeAST>("", vector<string>());
        return llvm::make_unique<FunctionAST>(move(Proto), move(E));
    }
    return nullptr;
}

static void initBinopPrecedence() {
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40;
}

static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            FnIR->dump();
        }
    } else {
        // for error recovery
        getNextToken();
    }
}

static void HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        if (auto *FnIR = ProtoAST->codegen()) {
            FnIR->dump();
        }
    } else {
        getNextToken();
    }
}

static void HandleTopLevelExpression() {
    if (auto FnAST = ParseTopLevelExpr()) {
        if (auto *FnIR = FnAST->codegen()) {
            FnIR->dump();
        }
    } else {
        getNextToken();
    }
}

static void MainLoop() {
    while (1) {
        fprintf(stderr, "> ");
        switch (CurTok) {
            case TOK_EOF:
                return;
            case ';':
                getNextToken();
                break;
            case TOK_DEF:
                HandleDefinition();
                break;
            case TOK_EXTERN:
                HandleExtern();
                break;
            default:
                HandleTopLevelExpression();
                break;
        }
    }
}

int main() {
    initBinopPrecedence();

    fprintf(stderr, "> ");
    getNextToken();

    TheModule = llvm::make_unique<Module>("my cool jit!", getGlobalContext());

    MainLoop();

    TheModule->dump();

    return 0;
}