#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Lexer enum for tokens
enum Token
{
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    // primary
    tok_identifier = -4,
    tok_number = -5,
};

static std::string IdentifierStr; // if tok_identifier
static double NumVal;             // if tok_number

class ExprAST
{
public:
    virtual ~ExprAST() = default;
};

class NumberExprAST : public ExprAST
{
    double Val;

public:
    NumberExprAST(double Val) : Val(Val) {}
};

class VariableExprAST : public ExprAST
{
    std::string Name;

public:
    VariableExprAST(const std::string &Name) : Name(Name) {}
};

class BinaryExprAST : public ExprAST
{
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS) : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

class CallExprAST : public ExprAST
{
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

public:
    CallExprAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args) : Callee(Callee), Args(std::move(Args)) {}
};

class PrototypeAST
{
    std::string Name;
    std::vector<std::string> Args;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args) : Name(Name), Args(std::move(Args)) {}
};

class FunctionAST
{
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<ExprAST> Body) : Proto(std::move(Proto)), Body(std::move(Body)) {}
};

// gettok - return the next token from standard input

static int gettok()
{
    static int LastChar = ' ';

    // skip any whitespace
    while (isspace(LastChar))
        LastChar = getchar();

    // identify keywords
    if (isalpha(LastChar))
    {
        while (isalnum(LastChar = getchar()))
            IdentifierStr += LastChar;

        if (IdentifierStr == "def")
            return tok_def;
        if (IdentifierStr == "extern")
            return tok_extern;

        return tok_identifier;
    }

    if (isdigit(LastChar) || LastChar == '.')
    {
        std::string NumStr;
        do
        {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');

        // Setting the NumVal and returning the num token
        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    if (LastChar == '#')
    {
        do
            LastChar = getchar();
        while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return gettok();
    }

    if (LastChar == EOF)
        return tok_eof;

    // return unknown lexeme or token
    int ThisChar = LastChar;
    LastChar = getchar();

    return ThisChar;
}

// PARSER

// CurTok/getNextToken
static int CurTok;
static int getNextToken() {
	return CurTok = gettok();
}

// Error Log
std::unique_ptr<ExprAST> LogError(const char *Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
	LogError(Str);
	return nullptr;
}


// Basic Expression Parsing
static std::unique_ptr<ExprAST> ParseNumberExpr() {
	auto Result = std::make_unique<NumberExprAST>(NumVal);
	getNextToken();
	return std::move(Result);
}


static std::unique_ptr<ExprAST> ParseParenExpr() {
	getNextToken();

	//parsing subexpression after the '('
	auto V = ParseExpression();

	if(!V) return nullptr;

	if(CurTok != ')') return LogError("Expected ')'");
	getNextToken();

	return V;
}


static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
	std::string IdName = IdentifierStr;

	getNextToken();

	if(CurTok != '(') return std::make_unique<VariableExprAST>(IdName);
	
	// if ( exists, then it might be a method
	getNextToken();
	std::vector<std::unique_ptr<ExprAST>> Args;

	if (CurTok != ')') {
	    while(true) {
	      // Parse a String or expression, argument
	      if (auto Arg = ParseExpression()) Args.push_back(std::move(Arg));
	      else return nullptr;

	      if(CurTok == ')') break;

	      if(CurTok != ',') return LogError("Expected ')' or ',' in argument list");
	      getNextToken();
	    }
	}

	getNextToken();

	return std::make_unique<CallExprAST>(IdName, std::move(Args));
}


static std::unique_ptr<ExprAST> ParsePrimary() {
	switch(CurTok) {
		default:
			return LogError("Unknown Token! Expected an expression");
		case tok_identifier:
			return ParseIdentifierExpr();
		case tok_number:
			return ParseNumberExpr();
		case '(':
			return ParseParenExpr();
	}
}


// Binary Expression Parsing

static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence() {
	if(!isascii(CurTok)) return -1;
	
	int TokPrec = BinopPrecedence[CurTok];
	if(TokPrec <= 0) return -1;

	return TokPrec;
}

static std::unique_ptr<ExprAST> ParseExpression() {
	auto LHS = ParsePrimary();

	if(!LHS) return nullptr;
	// first call with 0 as def val for binOp
	return ParseBinOpRHS(0, std::move(LHS));
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unqiue_pointer<ExprAST> LHS) {
	while(true) {
		int TokPrec = GetTokPrecedence();
		
		// Check if it's a binop
		if(TokPrec < ExprPrec) return LHS;

		int BinOp = CurTok;
		getNextToken();

		auto RHS = ParsePrimary();

		if(!RHS) return nullptr;

		int NextPrec = GetTokPrecedence();

		if(TokPrec < NextPrec) {
			// Unfinished
		}

		LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
	}
}

int main() {
	BinopPrecedence['<'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 30;
	BinopPrecedence['*'] = 40;
}
