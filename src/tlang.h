
// Charles Timmerman - cttimm4427@ung.edu

#ifndef TLANG_H
#define TLANG_H

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



// --- Node Prototypes --- 

class Expression;
class NumExpression;
class VarExpression;
class OpExpression;
class CallExpression;
class ProtoFn;
class FnExpression;
static std::map<char, int> binopPrec; // Map for binary op precedence

// ---  Code Generation --- 

static llvm::LLVMContext CONTEXT;
static llvm::IRBuilder<> BUILDER(CONTEXT);
static std::unique_ptr<llvm::Module> MODULE;
static std::map<std::string, llvm::Value *> NamedValues;
llvm::Value *log_errorv(const char *Str);

// --- Error functions ---

std::unique_ptr<Expression> log_error();
std::unique_ptr<ProtoFn> log_errorp();




//                            //
// --   Parse Tree Nodes   -- //
//                            //

class Expression {
    public:
        virtual ~Expression() {}
        virtual llvm::Value *codegen() = 0;
};

class NumExpression : public Expression {
    double Val;
public:
    NumExpression(double Val) : Val(Val) {}
    virtual llvm::Value *codegen();
};

class VarExpression : public Expression {
    std::string Name;
public:
    VarExpression(const std::string &Name) : Name(Name) {}
    llvm::Value *codegen() override;
};

class OpExpression : public Expression {
    char Op;
    std::unique_ptr<Expression> leftSide, rightSide;
public:
    OpExpression(char op, std::unique_ptr<Expression> leftSide,
            std::unique_ptr<Expression> rightSide)
            : Op(op), leftSide(std::move(leftSide)), rightSide(std::move(rightSide)) {}
    llvm::Value *codegen() override;
};

class CallExpression : public Expression {
    std::string Callee;
    std::vector<std::unique_ptr<Expression>> Args;
public:
    CallExpression(const std::string &Callee,
            std::vector<std::unique_ptr<Expression>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
    llvm::Value *codegen() override;
};

class ProtoFn {
    std::string Name;
    std::vector<std::string> Args;
public:
    ProtoFn(const std::string &name, std::vector<std::string> Args)
        : Name(name), Args(std::move(Args)) {}
    llvm::Function *codegen();
    const std::string &getName() const { return Name; }

};

class FnExpression{
    std::unique_ptr<ProtoFn> Proto;
    std::unique_ptr<Expression> Body;
public:
    FnExpression(std::unique_ptr<ProtoFn> Proto,
           std::unique_ptr<Expression> Body)
           : Proto(std::move(Proto)), Body(std::move(Body)) {}
    
    llvm::Function *codegen();

};
// -- End Nodes


// Error handling functions
std::unique_ptr<Expression> log_error(const char *Str) {
    fprintf(stderr, "log_error: %s\n", Str);
    return nullptr;
}
std::unique_ptr<ProtoFn> log_errorp(const char *Str) {
    log_error(Str);
    return nullptr;
}



//                        //
// --- Code Generator --- //
//                        //

// These all override the codegen routine from the parse tree node_destruct

llvm::Value *log_errorv(const char *Str) {
    log_error(Str);
    return nullptr;
}

llvm::Value *NumExpression::codegen() {
    return llvm::ConstantFP::get(CONTEXT, llvm::APFloat(Val));
}

llvm::Value *VarExpression::codegen() {
    llvm::Value *V = NamedValues[Name];
    if(!V)
        log_errorv("Unknow variable name.");
    return V;
}

llvm::Value *OpExpression::codegen() {
    llvm::Value *L = leftSide->codegen();
    llvm::Value *R = rightSide->codegen();
    if(!L || !R) return nullptr;
    switch(Op) {
        case '+':
            return BUILDER.CreateFAdd(L, R, "addop");
        case '-':
            return BUILDER.CreateFSub(L, R, "subop");
        case '*':
            return BUILDER.CreateFMul(L, R, "mulop");
        case '<':
            L = BUILDER.CreateFCmpULT(L, R, "cmpop");
            return BUILDER.CreateUIToFP(L, llvm::Type::getDoubleTy(CONTEXT), "booltmp");
        default:
            return log_errorv("Invalid binary operator.");
    }
}

llvm::Value *CallExpression::codegen() {
    llvm::Function *callee = MODULE->getFunction(Callee);
    if(!callee) return log_errorv("Unknown function referenced.");

    if(callee->arg_size() != Args.size()) return log_errorv("Incorrect number of arguments.");

    std::vector<llvm::Value *> args;
    for(unsigned i = 0, e = Args.size(); i != e; i++) {
        args.push_back(Args[i]->codegen());
        if(!args.back()) return nullptr;
    }
    return BUILDER.CreateCall(callee, args, "retval");
}

llvm::Function *ProtoFn::codegen() {
    std::vector<llvm::Type *> Doubles(Args.size(), llvm::Type::getDoubleTy(CONTEXT));
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(CONTEXT), Doubles, false);
    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, MODULE.get());

    unsigned Idx = 0;
    for (auto &Arg : F->args()) Arg.setName(Args[Idx++]);
    return F;
}

llvm::Function *FnExpression::codegen() {
    llvm::Function *function = MODULE->getFunction(Proto->getName());

    if(!function) function = Proto->codegen();

    if(!function) return nullptr;

    llvm::BasicBlock *BB = llvm::BasicBlock::Create(CONTEXT, "entry", function);
    BUILDER.SetInsertPoint(BB);

    NamedValues.clear();
    for(auto &Arg : function->args())
        NamedValues[Arg.getName()] = &Arg;

    if (llvm::Value *retval = Body->codegen()) {
        BUILDER.CreateRet(retval);

        llvm::verifyFunction(*function);

        return function;
    }

    function->eraseFromParent();
    return nullptr;
}
// End code gen
#endif