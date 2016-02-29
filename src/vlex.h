
#ifndef VLEX_H
#define VLEX_H

#include "vinyl.h"
#include "vvar.h"

enum AstEnum {
	// Operators
	OpInc, OpDec,
	OpLe, OpGe, OpEq, OpNe, OpLt, OpGt,
	OpAnd, OrOp,
	OpAdd, OpSub, OpMul, OpDiv, OpMod,
	OpNeg, OpNot, OpDot,
	OpAsgn,

	// Punct
	PComma, PColon, PSemi, 
	PLParen, PRParen,
	PLBracket, PRBracket,
	PLBrace, PRBrace,

	// Reserved
	RVar, RFunction,
	RThis, RNull, RTrue, RFalse,
	RIf, RElse,
	RWhile, RBreak, RReturn,
	RNew, RDelete, RImport,

	// Other
	TkId, TkVar,

	// Non-terminal
	AstGetIdx, AstSetIdx, AstGetKey, AstSetKey,
	AstInvoke, AstObjLiteral, AstArrLiteral
};

extern const char* TkStr[];
extern const int OpPrec[];

extern const int TkOpBegin;
extern const int TkOpEnd;
extern const int TkPunctBegin;
extern const int TkPunctEnd;
extern const int TkRsvdBegin;
extern const int TkRsvdEnd;

typedef struct Ast Ast;

struct Ast {
	int lineno, col;
	Ast* next;
	Ast* child;
	enum AstEnum t;
	VVar v;
};

Ast* lex(VFile* file);

Ast* ast_new(int lineno, int col);
void ast_printtree(Ast* t, int indent);

#endif // VLEX_H
