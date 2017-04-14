
// Charles Timmerman - cttimm4427@ung.edu

#ifndef PARSER_H
#define PARSER_H

#include "tlang.h"

// --- Globals ---

static double NumVal;
static int currToken;
static std::string IdentStr;

enum Token {
    _EOF = -1,
    _FN = -2,
    _IMPORT = -3,
    _IDENT = -4,
    _NUMBER = -5,
    _EXIT = -6,
    _EQ = -7,
    _ASSIGN = -8
};

// --- Lexer functions --- 

static int get_token();
static int get_next_token();
static int get_token_precedence();

// --- Parser Prototypes ---

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

// --- Top level parsing --- 

static void handle_definition();
static void handle_import();
static void handle_top();



//             //
// -- Lexer -- //
//             //

static int get_token() {

    static int last_char = ' ';

    while(isspace(last_char))
        last_char = getchar();
    
    // Ensure first character is alpha and following is alphanum
    if (isalpha(last_char)) {
        IdentStr = last_char; // Global IdentStr
        while(isalnum((last_char = getchar())))
            IdentStr += last_char;
        if (IdentStr == "fn") return _FN;
        if (IdentStr == "import") return _IMPORT;
        if (IdentStr == "exit") return _EXIT;
        return _IDENT;
    }
    
    // Numbers are all Doubles
    if (isdigit(last_char)) {
        std::string NumStr;
        do {
            NumStr += last_char;
            last_char = getchar();
        } while (isdigit(last_char));
        if(last_char == '.') {
            do {
                NumStr += last_char;
                last_char = getchar();
            } while (isdigit(last_char));
        }
        NumVal = strtod(NumStr.c_str(),0); // Global NumVal
        return _NUMBER;
    }

    if (last_char == '#') { // Until end of line
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

// HELPER FUNCTION -- look ahead 
static int get_next_token() {
    return currToken = get_token();
}
// HELPER FUNCTION -- gets token precedence
static int get_token_precedence() {
    // isascii can be used because the values of our tokens are negative integers and ascii is 0-255

    switch(currToken) {
        case '=':
            return 10;
        case '>':
            return 10;
        case '<':
            return 10;
        case '+':
            return 20;
        case '-':
            return 20;
        case '*':
            return 40;
        case '/':
            return 40;
        default:
            return -1;

    }
}



// <number>
static std::unique_ptr<Expression> parse_numexpr() {
    auto Result = llvm::make_unique<NumExpression>(NumVal);
    get_next_token(); 
    return std::move(Result);
}

// (<expression>)
static std::unique_ptr<Expression> parse_paren() {
    get_next_token(); // Pops current token, and sends control to parse Expression
    auto V = parse_expression();
    if(!V) return nullptr;
    // Parse Expression will return to here, and checks for close parenth
    if (currToken != ')') return log_error("expected ')'");
    get_next_token();
    return V;
}

// <identifier>
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

// <primary>
static std::unique_ptr<Expression> parse_primary() {

    // Use switch to guide parsing
    switch (currToken) {
        default:
            printf("%c\n", currToken);
            return log_error("unknown token");
        case _IDENT:
            return parse_idexp();
        case _NUMBER:
            return parse_numexpr();
        case '(':
            return parse_paren();
    }
}

// part of <operation>
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

// <operation>
static std::unique_ptr<Expression> parse_expression() {
    // Pops the current token and gets an operator
    auto leftSide = parse_primary();
    if(!leftSide) {
        return nullptr;
    }
    // Return the return of parsing right hand operator (initial precedence of 0, and move left side))
    return parse_rbinop(0, std::move(leftSide));
}

// <used to parse function imports 
static std::unique_ptr<ProtoFn> parse_prototype() {
    if (currToken != _IDENT)
        return log_errorp("Expected function name in prototype\n");

    std::string fnName = IdentStr;
    get_next_token();

    if (currToken != '(')
        return log_errorp("Expected '(' in prototype\n");

    std::vector<std::string> argNames;

    while (get_next_token() == _IDENT) argNames.push_back(IdentStr);

    if(currToken != ')')
        return log_errorp("Expected ')' in prototype\n");

    get_next_token();
    return llvm::make_unique<ProtoFn>(fnName, std::move(argNames));
}

// <function>
static std::unique_ptr<FnExpression> parse_definition() {
    get_next_token();
    auto Proto = parse_prototype();
    if(!Proto) return nullptr;
    if(auto E = parse_expression())
        return llvm::make_unique<FnExpression>(std::move(Proto), std::move(E));
    
    return nullptr;
}

// <import>
static std::unique_ptr<ProtoFn> parse_import() {
    get_next_token(); // Eat the "import" token
    return parse_prototype();
}


static std::unique_ptr<FnExpression> parse_top_expr() {
    if (auto E = parse_expression()) {
        auto Proto = llvm::make_unique<ProtoFn>("__anonexpr",std::vector<std::string>());
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
        if(FnExpr->codegen()) {
            auto H = jit->addModule(std::move(MODULE));
	    initialize_module();
	    

	    auto expr_symbol = jit->findSymbol("__anonexpr");
	    assert(expr_symbol && "Function not found.");
	    double (*fp)() = (double (*)())(intptr_t)expr_symbol.getAddress();
            fprintf(stderr, "Evaluated to %f\n", fp());

	    jit->removeModule(H);
       	}
        
    } else {
	get_next_token();    
}
}
static void handle_return() {
    get_next_token();
}

#endif
