
#include "llvm/ADT/STLExtras.h"
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <map>

//                           //
//  --- Lexical Analysis --- //
//                           //
enum Token {
    _EOF = -1,
    _FN = -2,
    _IMPORT = -3,
    _IDENT = -4,
    _NUMBER = -5,
};
static std::string IdentStr;
static double NumVal;

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

//                        //
//  ---  Parse Tree  ---  //
//                        //
// Base
class Expression {
    public:
        virtual ~Expression() {}
};

// Numerical Literals
class NumExpression : public Expression {
    double Val;

public:
    NumExpression(double Val) : Val(Val) {}
};

// Variables
class VarExpression : public Expression {
    std::string Name;
public:
    VarExpression(const std::string &Name) : Name(Name) {}
};

// Operators
class OpExpression : public Expression {
    char Op;
    std::unique_ptr<Expression> leftSide, rightSide;
public:
    OpExpression(char op, std::unique_ptr<Expression> leftSide,
            std::unique_ptr<Expression> rightSide)
            : Op(op), leftSide(std::move(leftSide)), rightSide(std::move(rightSide)) {}
};

// fn calls
class CallExpression : public Expression {
    std::string Callee;
    std::vector<std::unique_ptr<Expression>> Args;
public:
    CallExpression(const std::string &Callee,
            std::vector<std::unique_ptr<Expression>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
};

// Function prototype
class ProtoFn {
    std::string Name;
    std::vector<std::string> Args;
public:
    ProtoFn(const std::string &name, std::vector<std::string> Args)
        : Name(name), Args(std::move(Args)) {}
};

// Function Body
class FnExpression {
    std::unique_ptr<ProtoFn> Proto;
    std::unique_ptr<Expression> Body;
public:
    FnExpression(std::unique_ptr<ProtoFn> Proto,
           std::unique_ptr<Expression> Body)
           : Proto(std::move(Proto)), Body(std::move(Body)) {}
};

//                //
// --- Parser --- //
//                //

// Token buffer and function to get next token through buffer
static int currToken;
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

static std::unique_ptr<Expression> parse_expression(); // Prototype

// numExpression ::= number
// Numberical Literals - Returns result as parse tree class
static std::unique_ptr<Expression> parse_numexpr() {
    auto Result = llvm::make_unique<NumExpression>(NumVal);
    get_next_token(); // Consumes Identifier
    return std::move(Result);
}
// parenExpression ::= (Expressionession)
// Parenthesis don't generate an Parse Tree class
static std::unique_ptr<Expression> parse_paren() {
    get_next_token(); // Pops current token, and sends control to parse Expressionession
    auto V = parse_expression();
    if(!V) return nullptr;
    // Parse Expressionession will return to here, and checks for close parenth
    if (currToken != ')') return log_error("expected ')'");
    get_next_token();
    return V;
}
// identifierExpression ::= identifier
//                ::= identifier (Expressionession)
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

// Holds precedence values for operators
static std::map<char, int> binopPrec;
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
    if(!leftSide) return nullptr;
    // Return the return of parsing right hand operator (initial precedence of 0, and move left side))
    return parse_rbinop(0, std::move(leftSide));
}

static std::unique_ptr<ProtoFn> parse_prototype() {

    // Ensures that the token is a valid identifier
    if (currToken != _IDENT) return log_errorp("Expected function name in prototype");

    // sets fn name to global str set by lexer
    std::string fnName = IdentStr;

    // Eats that token, gets next token
    get_next_token();

    // Ensure that token is beginning of arguments parenthesis
    if (currToken != '(') return log_errorp("Expected '(' in prototype");

    // Create vector of strings for arguments
    std::vector<std::string> argNames;

    // While recieving identifiers, push back argument names from global str
    while (get_next_token() == _IDENT) argNames.push_back(IdentStr);

    // Check for closed parenth
    if(currToken != ')') return log_errorp("Expected ')' in prototype");

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
//                         //
// -- Top-Level Parsing -- //
//                         //
static void handle_definition() {
    if(parse_definition()) {
        fprintf(stderr, "Parsed a function definition.\n");
    } else {
        get_next_token();
    }
}

static void handle_import() {
    if(parse_import()) {
        fprintf(stderr, "Parsed a import.x+\n");
    } else {
        get_next_token();
    }
}

static void handle_top() {
    if(parse_top_expr()) {
        fprintf(stderr, "Parsed a top-level expression\n");
    } else {
        get_next_token();
    }
}

//                //
// --- Driver --- //
//                //
static void MainLoop() {
    while(1) {
        fprintf(stderr, "ready> ");
        switch (currToken) {
            case _EOF:
                return;
            case ';':
                get_next_token();
                break;
            case _FN:
                handle_definition();
                break;
            case _IMPORT:
                handle_import();
                break;
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

    fprintf(stderr, "ready> ");
    get_next_token();

    MainLoop();

    return 0;
};