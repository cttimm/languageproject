// Charles Timmerman - cttimm4427@ung.edu
#ifndef TLANG_H
#define TLANG_H

// LLVM headers
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <map>


//                  //
// -- Token Defs -- //
//                  //

enum Token {
    _EOF = -1,
    _FN = -2,
    _IMPORT = -3,
    _IDENT = -4,
    _NUMBER = -5,
    _EXIT = -6
};

//                 //
// -- Prototype -- //
//                 //

// AST Prototypes
class Expression;
class NumExpression;
class VarExpression;
class OpExpression;
class CallExpression;
class ProtoFn;
class FnExpression;

static double NumVal;
static int currToken;
static std::string IdentStr;
static std::map<char, int> binopPrec; // Map for binary op precedence

// Lexer functions
static int get_token();
static int get_next_token();
static int get_token_precedence();

// Error functions 
std::unique_ptr<Expression> log_error();
std::unique_ptr<ProtoFn> log_errorp();

// Parser functions
static std::unique_ptr<Expression> parse_expression();
static std::unique_ptr<Expression> parse_numexpr();
static std::unique_ptr<Expression> parse_paren();
static std::unique_ptr<Expression> parse_idexp();
static std::unique_ptr<Expression> parse_primary();
static std::unique_ptr<Expression> parse_rbinop();
static std::unique_ptr<Expression> parse_expression();
static std::unique_ptr<ProtoFn> parse_prototype();
static std::unique_ptr<FnExpression> parse_definition();
static std::unique_ptr<ProtoFn> parse_import();
static std::unique_ptr<FnExpression> parse_top_expr();

// Top-level parsing
static void handle_definition();
static void handle_import();
static void handle_top();

// llvm
static llvm::LLVMContext CONTEXT;
static llvm::IRBuilder<> BUILDER(CONTEXT);
static std::unique_ptr<llvm::Module> MODULE;
static std::map<std::string, llvm::Value *> NamedValues;
llvm::Value *log_errorv(const char *Str);

//                            //
// -- Abstract Syntax Tree -- //
//                            //

// AST Base Node
class Expression {
    public:
        virtual ~Expression() {}
        virtual llvm::Value *codegen() = 0;
};

// AST Node for numerical expressions
class NumExpression : public Expression {
    double Val;
public:
    NumExpression(double Val) : Val(Val) {}
    virtual llvm::Value *codegen();
};

// AST Node for variables
class VarExpression : public Expression {
    std::string Name;
public:
    VarExpression(const std::string &Name) : Name(Name) {}
    llvm::Value *codegen() override;
};

// AST Node for operators
class OpExpression : public Expression {
    char Op;
    std::unique_ptr<Expression> leftSide, rightSide;
public:
    OpExpression(char op, std::unique_ptr<Expression> leftSide,
            std::unique_ptr<Expression> rightSide)
            : Op(op), leftSide(std::move(leftSide)), rightSide(std::move(rightSide)) {}
    llvm::Value *codegen() override;
};

// AST Node for fn calls
class CallExpression : public Expression {
    std::string Callee;
    std::vector<std::unique_ptr<Expression>> Args;
public:
    CallExpression(const std::string &Callee,
            std::vector<std::unique_ptr<Expression>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
    llvm::Value *codegen() override;
};

// AST Node for prototype functions
class ProtoFn {
    std::string Name;
    std::vector<std::string> Args;
public:
    ProtoFn(const std::string &name, std::vector<std::string> Args)
        : Name(name), Args(std::move(Args)) {}
    llvm::Function *codegen();
    const std::string &getName() const { return Name; }

};

// AST Node for function body
class FnExpression{
    std::unique_ptr<ProtoFn> Proto;
    std::unique_ptr<Expression> Body;
public:
    FnExpression(std::unique_ptr<ProtoFn> Proto,
           std::unique_ptr<Expression> Body)
           : Proto(std::move(Proto)), Body(std::move(Body)) {}
    
    llvm::Function *codegen();

};

#endif