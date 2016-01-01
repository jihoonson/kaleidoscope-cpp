//
// Created by SonJihoon on 2015. 12. 31..
//

#ifndef KALEIDOSCOPE_LEXER_H
#define KALEIDOSCOPE_LEXER_H

#include <string>
#include <regex>

using namespace std;

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
    TOK_LP = -7,
    TOK_RP = -8,
};

class UndefinedTokenException : public exception {

public:
    UndefinedTokenException(int Token) : Token(Token) {};

    virtual const char* what() const throw() {
        return "Undefined token: " + Token;
    }

private:
    int Token;
};

static string IdentifierStr;
static double NumVal;
static string OpStr;

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

    switch (LastChar) {
        case '+':
        case '-':
        case '*':
        case '/':
            OpStr = LastChar;
            LastChar = getchar();
            return TOK_OP;
        case '(':
            LastChar = getchar();
            return TOK_LP;
        case ')':
            LastChar = getchar();
            return TOK_RP;
    }

    if (LastChar == EOF) {
        return TOK_EOF;
    }

//    int ThisChar = LastChar;
//    LastChar = getchar();
//    return ThisChar;
    // error
    throw UndefinedTokenException(LastChar);
}

static int CurTok;
static int getNextToken() {
    return CurTok = gettok();
}

#endif //KALEIDOSCOPE_LEXER_H
