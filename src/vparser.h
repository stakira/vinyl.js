
#ifndef VPARSER_H
#define VPARSER_H

#include "vlex.h"
#include "vvm.h"

typedef struct ParserState {
	Ast* head;
	Ast* curr;
} ParserState;

ParserState parserstate_new();

int compilation_unit(ParserState* ps); // testing this

#endif // VPARSER_H
