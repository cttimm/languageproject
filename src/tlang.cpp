// Charles Timmerman - cttimm4427@ung.edu

#include "tlang.h"

//             //
// -- Lexer -- //
//             //

static int get_token() {
    // Return the next token from stdin
    static int last_char = ' ';

    // Handle whitespace
    while(isspace(last_char)) last_char = getchar();
    
    // Ensure first character is alpha and following is alphanum
    if (isalpha(last_char)) {
        IdentStr = last_char;
        while(isalnum((last_char = getchar())))
            IdentStr += last_char;
        // Function
        if (IdentStr == "fn") return _FN;
        // Import
        if (IdentStr == "import") return _IMPORT;

        if (IdentStr == "exit") return _EXIT;
        // Identifier *variable*
        return _IDENT;
    }
    // Number *double*
    if (isdigit(last_char)) {
        std::string NumStr;
        do {
            NumStr += last_char;
            last_char = getchar();
        } while (isdigit(last_char));
        // Handle decim once
        if(last_char == '.') {
            do {
                NumStr += last_char;
                last_char = getchar();
            } while (isdigit(last_char));
        }
        // Store number value
        NumVal = strtod(NumStr.c_str(),0);
        return _NUMBER;
    }
    // Comments
    if (last_char == '#') {
        // Until end line
        do last_char = getchar();
        while (last_char != EOF && last_char != '\n' && last_char != '\r');

        if (last_char != EOF) return get_token();
    }

    if (last_char == EOF) return _EOF;
    int ThisChar = last_char;
    last_char = getchar();
    return ThisChar;
};


//                //
// --- Parser --- //
//                //

// Token buffer and function to get next token through buffer
// Implementing look ahead
static int get_next_token() {
    return currToken = get_token();
}

// Error handling functions
std::unique_ptr<Expression> log_error(const char *Str) {
    fprintf(stderr, "log_error: %s\n", Str);
    return nullptr;
}
std::unique_ptr<ProtoFn> log_errorp(const char *Str) {
    log_error(Str);
    return nullptr;
}


// numExpression ::= number
// Numberical Literals - Returns result as AST Node
// This is called when if currtoken is num
static std::unique_ptr<Expression> parse_numexpr() {
    auto Result = llvm::make_unique<NumExpression>(NumVal);
    get_next_token(); // Consumes Identifier
    return std::move(Result);
}
// parenExpression ::= (Expression)
// Parenthesis don't generate a Parse Tree class
static std::unique_ptr<Expression> parse_paren() {
    get_next_token(); // Pops current token, and sends control to parse Expression
    auto V = parse_expression();
    if(!V) return nullptr;
    // Parse Expression will return to here, and checks for close parenth
    if (currToken != ')') return log_error("expected ')'");
    get_next_token();
    return V;
}
// identifierExpression ::= identifier
//                ::= identifier (Expression)
static std::unique_ptr<Expression> parse_idexp() {
    std::string IdName = IdentStr;
    get_next_token(); // Consumes Identifier
    
    if(currToken != '(') return llvm::make_unique<VarExpression>(IdName); // simple identifier = done

    get_next_token(); // Consume open parenth
    std::vector<std::unique_ptr<Expression>> Args;
    if(currToken != ')') {
        while(1) {
            if(auto Arg = parse_expression()) Args.push_back(std::move(Arg));
            else return nullptr;
            if (currToken == ')') break;
            if (currToken != ',') return log_error("Expected ')' or ',' in argument list");
            get_next_token();
        }
    }
    get_next_token(); // Consume close parenth
    return llvm::make_unique<CallExpression>(IdName, std::move(Args));
}

// primary ::= identifierExpression  
//         ::= numberExpression
//         ::= parenExpression
static std::unique_ptr<Expression> parse_primary() {
    // Use switch to guide parsing
    switch (currToken) {
        default:
            return log_error("unknown token");
        case _IDENT:
            return parse_idexp();
        case _NUMBER:
            return parse_numexpr();
        case '(':
            return parse_paren();
    }
}


// Gets the precedence of the current token
static int get_token_precedence() {
    // isascii can be used because the values of our tokens are negative integers and ascii is 0-255
    if(!isascii(currToken)) return -1;

    int token_precedence = binopPrec[currToken];
    if(token_precedence <= 0) return -1;
    return token_precedence;
}

static std::unique_ptr<Expression> parse_rbinop(int current_prec, std::unique_ptr<Expression> leftSide) {
    while(1) {
        int token_prec = get_token_precedence();

        if (token_prec < current_prec) return leftSide;
        
        int binOp = currToken;
        get_next_token();

        auto rightSide = parse_primary();
        if(!rightSide) return nullptr;

        int next_prec = get_token_precedence();
        if(token_prec < next_prec) {
            rightSide = parse_rbinop(token_prec+1, std::move(rightSide));
            if(!rightSide) return nullptr;
        }

        leftSide = llvm::make_unique<OpExpression>(binOp, std::move(leftSide), std::move(rightSide));
    }
}

// Uses precedence to compare right and left side operators
static std::unique_ptr<Expression> parse_expression() {
    // Pops the current token and gets an operator
    auto leftSide = parse_primary();
    if(!leftSide) {
        return nullptr;
    }
    // Return the return of parsing right hand operator (initial precedence of 0, and move left side))
    return parse_rbinop(0, std::move(leftSide));
}

static std::unique_ptr<ProtoFn> parse_prototype() {
    // Ensures that the syntax resembles "fn fnname(args)"
    
    if (currToken != _IDENT) return log_errorp("Expected function name in prototype\n");
    std::string fnName = IdentStr;

    // Eats that token, gets next token
    get_next_token();

    // Ensure that token is beginning of arguments parenthesis
    if (currToken != '(') return log_errorp("Expected '(' in prototype\n");

    // Create vector of strings for arguments
    std::vector<std::string> argNames;

    // While recieving identifiers, push back argument names from global str
    while (get_next_token() == _IDENT) argNames.push_back(IdentStr);

    // Check for closed parenth
    if(currToken != ')') return log_errorp("Expected ')' in prototype\n");

    // Eat the )
    get_next_token();

    // Return the node 
    return llvm::make_unique<ProtoFn>(fnName, std::move(argNames));
}

static std::unique_ptr<FnExpression> parse_definition() {
    get_next_token();
    auto Proto = parse_prototype();
    if(!Proto) return nullptr;
    if(auto E = parse_expression())
        return llvm::make_unique<FnExpression>(std::move(Proto), std::move(E));
    
    return nullptr;
}

static std::unique_ptr<ProtoFn> parse_import() {
    get_next_token(); // Eat the "import" token
    return parse_prototype();
}

static std::unique_ptr<FnExpression> parse_top_expr() {
    if (auto E = parse_expression()) {
        auto Proto = llvm::make_unique<ProtoFn>("",std::vector<std::string>());
        return llvm::make_unique<FnExpression>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

static void handle_definition() {
    if(auto FnExpr = parse_definition()) {
        if(auto *FnIR = FnExpr->codegen()) {
            fprintf(stderr, "Read function definition");
            FnIR->print(llvm::errs());
            fprintf(stderr,"\n");
        }
    } else {
        get_next_token();
    }
}

static void handle_import() {
    if(auto ImportExpression = parse_import()) {
        if(auto *ImIR = ImportExpression->codegen()) {
            fprintf(stderr, "Parsed an import.\n");
            ImIR->print(llvm::errs());
            fprintf(stderr, "\n");
        }
    } else {
        get_next_token();
    }
}

static void handle_top() {
    if(auto FnExpr = parse_top_expr()) {
        if(auto *FnIR = FnExpr->codegen()) {
            fprintf(stderr, "Parsed a top-level expression\n");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
        }
        
    } else {
        get_next_token();
    }
}

static void handle_return() {
    get_next_token();
}

//                       //
// -- Code Generator  -- //
//                       //

// Error helper function
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

//                  //
// --- JIT Loop --- //
//                  //

static void MainLoop() {
    while(1) {
        fprintf(stderr, "tlang > ");
        switch (currToken) {
            case _EOF:
                return;
            case ';':
                handle_return();
                break;
            case _FN:
                handle_definition();
                break;
            case _IMPORT:
                handle_import();
                break;
            case _EXIT:
                fprintf(stderr, "exiting...\n");
                return;
            default:
                handle_top();
                break;
        }
    }
}







int main() {
    binopPrec['<'] = 10;
    binopPrec['+'] = 20;
    binopPrec['-'] = 20;
    binopPrec['*'] = 40;

    fprintf(stderr, "tlang > ");
    get_next_token();

    // Memory allocation
    MODULE = llvm::make_unique<llvm::Module>("tlang JIT", CONTEXT);
    MainLoop();
    // Dumps all messages upon closing with CTRL-D
    MODULE->print(llvm::errs(), nullptr);

    return 0;
};