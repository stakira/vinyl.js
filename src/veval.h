
#ifndef VEVAL_H
#define VEVAL_H

#include "vinyl.h"
#include "vlex.h"

typedef struct EvalState EvalState;

typedef void(*cfunc)(EvalState* es, VVar* result);

void eval(Ast* root);

#endif // VEVAL_H
