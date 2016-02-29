
#include <math.h>
#include "vvar.h"
#include "vgc.h"
#include "vlist.h"
#include "vdict.h"
#include "veval.h"
#include "vmisc.h"

VVar newvfunc(Ast* ast) { VVar var; gc_newgcobj(&var, T_VFunc); var.gc->fn = ast; return var; }

#define veval_arith(name, op, t0, t1) \
inline static void veval_##name (VVar* ra, VVar* rb, VVar* rc) { \
	switch (rb->type) { \
	case T_VInt: \
		switch (rc->type) { \
		case T_VInt:	ra->type = t0; ra->i = rb->i op rc->i; return; \
		case T_VFloat:	ra->type = t1; ra->f = rb->i op rc->f; return; \
		default: break; \
		} \
	case T_VFloat: \
		switch (rc->type) { \
		case T_VInt:	ra->type = t1; ra->f = rb->f op rc->i; return; \
		case T_VFloat:	ra->type = t1; ra->f = rb->f op rc->f; return; \
		default: break; \
		} \
	default: break; \
	} \
	verror("invalid type for "#name); \
}

#define veval_bool(name, op) \
inline static void veval_##name(VVar* ra, VVar* rb, VVar* rc) { \
	if (rb->type == T_VBool && rc->type == T_VBool) { \
		ra->type = T_VBool; \
		ra->i = rb->i op rc->i; \
		return; \
	} \
	else verror("invalid type for "#name); \
}

veval_arith(add, +, T_VInt, T_VFloat);
veval_arith(sub, -, T_VInt, T_VFloat);
veval_arith(mul, *, T_VInt, T_VFloat);
veval_arith(eq, == , T_VBool, T_VBool);
veval_arith(ne, != , T_VBool, T_VBool);
veval_arith(lt, < , T_VBool, T_VBool);
veval_arith(le, <= , T_VBool, T_VBool);
veval_arith(gt, > , T_VBool, T_VBool);
veval_arith(ge, >= , T_VBool, T_VBool);

inline static void veval_add_general(VVar* ra, VVar* rb, VVar* rc) {
	if (rb->type == T_VString && rc->type == T_VString)
		*ra = vstr_cat(rb, rc);
	else
		veval_add(ra, rb, rc);
}

inline static void veval_div(VVar* ra, VVar* rb, VVar* rc) {
	if (rb->type == T_VInt && rc->type == T_VInt && rb->i % rc->i == 0) {
		ra->type = T_VInt;
		ra->i = rb->i / rc->i;
	}
	else {
		ra->type = T_VFloat;
		switch (rb->type) {
		case T_VInt:
			switch (rc->type) {
			case T_VInt:	ra->f = (double)rb->i / rc->i; return;
			case T_VFloat:	ra->f = (double)rb->i / rc->f; return;
			default: break;
			}
		case T_VFloat:
			switch (rc->type) {
			case T_VInt:	ra->f = rb->f / rc->i; return;
			case T_VFloat:	ra->f = rb->f / rc->f; return;
			default: break;
			}
		default: break;
		}
		verror("invalid type for div");
	}
}

inline static void veval_mod(VVar* ra, VVar* rb, VVar* rc) {
	ra->type = T_VFloat;
	switch (rb->type) {
	case T_VInt:
		switch (rc->type) {
		case T_VInt:	ra->type = T_VInt; ra->f = rb->i % rc->i; return;
		case T_VFloat:	ra->f = fmod(rb->i, rc->f); return;
		default: break;
		}
	case T_VFloat:
		switch (rc->type) {
		case T_VInt:	fmod(rb->f, rc->i); return;
		case T_VFloat:	fmod(rb->f, rc->f); return;
		default: break;
		}
	default: break;
	}
	verror("invalid type for div");
}

inline static void veval_neg(VVar* ra, VVar* rb) {
	switch (rb->type) {
	case T_VInt: ra->type = T_VInt; ra->i = -rb->i; return;
	case T_VFloat: ra->type = T_VFloat; ra->f = -rb->f; return;
	default: break;
	}
	verror("invalid type for neg");
}

veval_bool(and, &&);
veval_bool(or , || );

inline static void veval_not(VVar* ra, VVar* rb) {
	if (rb->type == T_VBool) {
		ra->type = T_VBool;
		ra->i = !rb->i;
	}
}

struct EvalState {
	VVar ids;
	VVar values;
	VVar scopes;
	VVar context;
	VVar parent;
	int flag_break;
	EvalState* global;
};

static void begin_scope(EvalState* es) {
	vlist_add(es->scopes, newvint(vlist_count(es->ids)));
}

static void end_scope(EvalState* es) {
	unsigned int top = vlist_get(es->scopes, vlist_count(es->scopes) - 1).i;
	while (vlist_count(es->ids) > top) {
		int size = vlist_count(es->ids);
		vlist_remove(es->ids, size - 1);
		vlist_remove(es->values, size - 1);
	}
	vlist_remove(es->scopes, vlist_count(es->scopes) - 1);
}

// find id in current scope and parent scopes
static int id_resove(EvalState* es, VVar id, VVar* stack, int* idx) {
	for (int i = vlist_count(es->ids) - 1; i >= 0; i--)
		if (vvar_cmp(vlist_get(es->ids, i), id) == 0) {
			*stack = es->values;
			*idx = i;
			return 1;
		}
	for (int i = vlist_count(es->global->ids) - 1; i >= 0; i--)
		if (vvar_cmp(vlist_get(es->global->ids, i), id) == 0) {
			*stack = es->global->values;
			*idx = i;
			return 1;
		}
	return 0;
}

// if id is already declared in current scope
static int id_declared(EvalState* es, VVar id) {
	int scope_count = vlist_count(es->scopes);
	int bottom = scope_count == 0 ? 0 : vlist_get(es->scopes, scope_count - 1).i;
	for (int idx = vlist_count(es->ids) - 1; idx >= bottom; idx--) {
		if (vvar_cmp(vlist_get(es->ids, idx), id) == 0)
			return 1;
	}
	return 0;
}

static void eval_ast(EvalState* es, Ast* root, VVar *ra); // forward

static void eval_expr(EvalState* es, Ast* root, VVar *ra) {
	VVar rb, rc;

	if (root->t >= OpLe && root->t <= OpMod) {
		eval_expr(es, root->child, &rb);
		eval_expr(es, root->child->next, &rc);
		switch (root->t) {
		case OpLe: veval_le(ra, &rb, &rc); break;
		case OpGe: veval_ge(ra, &rb, &rc); break;
		case OpEq: veval_eq(ra, &rb, &rc); break;
		case OpNe: veval_ne(ra, &rb, &rc); break;
		case OpLt: veval_lt(ra, &rb, &rc); break;
		case OpGt: veval_gt(ra, &rb, &rc); break;
		case OpAnd: veval_and(ra, &rb, &rc); break;
		case OrOp: veval_or(ra, &rb, &rc); break;
		case OpAdd: veval_add_general(ra, &rb, &rc); break;
		case OpSub: veval_sub(ra, &rb, &rc); break;
		case OpMul: veval_mul(ra, &rb, &rc); break;
		case OpDiv: veval_div(ra, &rb, &rc); break;
		case OpMod: veval_mod(ra, &rb, &rc); break;
		default: vfatal("unexpected error - binary op"); break;
		}
	}
	else if (root->t >= OpNeg && root->t <= OpNot) {
		eval_expr(es, root->child, &rb);
		switch (root->t) {
		case OpNeg: veval_neg(ra, &rb); break;
		case OpNot: veval_not(ra, &rb); break;
		default: vfatal("unexpected error - unary op"); break;
		}
	}
	else {
		switch (root->t) {
		case TkId: {
			VVar stack;
			int idx;
			if (!id_resove(es, root->v, &stack, &idx)) {
				verror("undefined variable %s", vvar_tocstr(&root->v));
				*ra = newvnull();
			}
			else {
				*ra = vlist_get(stack, idx);
			}
			break;
		}

		case TkVar:
			*ra = root->v;
			break;

		case RFunction:
			*ra = newvfunc(root);
			break;

		case RThis:
			*ra = es->context;
			break;

		case AstInvoke: {
			VVar vfunc;
			eval_expr(es, root->child, &vfunc);
			if (vfunc.type == T_VFunc) {
				// new scope
				EvalState fes;
				fes.ids = newvlist(VLISTSIZE);
				fes.values = newvlist(VLISTSIZE);
				fes.scopes = newvlist(VLISTSIZE);
				fes.context = es->parent;
				fes.flag_break = 0;
				fes.global = es->global;

				es->parent = newvnull();

				Ast* func = vfunc.gc->fn;
				Ast* ast = func->child->child;

				while (ast != 0) {
					if (ast->t == OpAsgn) {
						eval_expr(es, ast->child->next, ra);
						vlist_add(fes.ids, ast->child->v);
						vlist_add(fes.values, *ra);
					}
					else {
						vlist_add(fes.ids, ast->v);
						vlist_add(fes.values, newvnull());
					}
					ast = ast->next;
				}

				// pass arguments
				ast = root->child->next;
				for (unsigned int i = 0; ast != 0 && i < vlist_count(fes.ids); i++) {
					eval_expr(es, ast, ra);
					vlist_set(fes.values, i, *ra);
					ast = ast->next;
				}

				// eval
				eval_ast(&fes, func->child->next, ra);
			}
			else if (vfunc.type == T_VCFunc) {
				cfunc nfunc = (cfunc)vfunc.cfunc;
				EvalState fes;
				fes.values = newvlist(VLISTSIZE);
				fes.flag_break = 0;
				fes.global = es->global;

				Ast* ast = root->child->next;
				while (ast != 0) {
					eval_expr(es, ast, ra);
					vlist_add(fes.values, *ra);
					ast = ast->next;
				}

				nfunc(&fes, ra);
			}
			else {
				verror("not a function");
				*ra = newvnull();
			}
			break;
		}

		case AstGetIdx: {
			VVar obj, key;
			eval_expr(es, root->child, &obj);
			eval_expr(es, root->child->next, &key);
			if (obj.type == T_VList) {
				if (key.type == T_VInt) {
					*ra = vlist_get(obj, key.i);
				}
				else verror("array idx must be integer");
			}
			else if (obj.type == T_VDict) {
				es->parent = obj;
				*ra = vdict_get(obj, key);
			}
			else verror("trying to index invalid type");
			break;
		}

		case AstGetKey: {
			VVar obj;
			eval_expr(es, root->child, &obj);
			if (obj.type == T_VDict) {
				es->parent = obj;
				*ra = vdict_get(obj, root->child->next->v);
			}
			else verror("trying to get key from invalid type");
			break;
		}

		case AstObjLiteral: {
			Ast* ast = root->child;
			*ra = newvdict(VDICTSIZE);
			VVar value;
			while (ast != 0) {
				eval_expr(es, ast->child->next, &value);
				vdict_put(*ra, ast->child->v, value);
				ast = ast->next;
			}
			break;
		}

		case AstArrLiteral: {
			Ast* ast = root->child;
			*ra = newvlist(VLISTSIZE);
			VVar value;
			while (ast != 0) {
				eval_expr(es, ast, &value);
				vlist_add(*ra, value);
				ast = ast->next;
			}
			break;
		}

		default:
			vfatal("unexpected error - expr");
		}
	}
}

static void eval_stmt(EvalState* es, Ast* root, VVar *ra) {
	switch (root->t) {
	case OpInc:
	case OpDec: {
		VVar stack;
		int idx;
		if (!id_resove(es, root->child->v, &stack, &idx)) {
			verror("undefined variable %s", vvar_tocstr(&root->child->v));
		}
		else {
			*ra = vlist_get(stack, idx);
			switch (ra->type) {
			case T_VInt:
				ra->i = ra->i + (root->t == OpInc ? 1 : -1);
				vlist_set(stack, idx, *ra);
				break;
			case T_VFloat:
				ra->f = ra->f + (root->t == OpInc ? 1.0 : -1.0);
				vlist_set(stack, idx, *ra);
				break;
			default:
				verror("trying to inc/dec %s", Type[ra->type]);
			}
		}
		*ra = newvnull();
		break;
	}

	case OpAsgn: {
		VVar stack;
		int idx;
		if (!id_resove(es, root->child->v, &stack, &idx)) {
			verror("undefined variable %s", vvar_tocstr(&root->child->v));
		}
		else {
			eval_ast(es, root->child->next, ra);
			vlist_set(stack, idx, *ra);
		}
		*ra = newvnull();
		break;
	}

	case PComma:
		vfatal("unexpected error - paren");
	case PSemi:
		// ignore
		break;

	case PLParen:
		vfatal("unexpected error - paren");
		break;

	case PLBracket:
		vfatal("unexpected error - bracket");
		break;

	case PLBrace:
		begin_scope(es);
		eval_ast(es, root->child, ra);
		end_scope(es);
		break;

	case RVar: {
		Ast* ast = root->child;
		while (ast != 0) {
			if (ast->t == OpAsgn) {
				int idx = id_declared(es, ast->child->v);
				if (idx == 0) {
					eval_expr(es, ast->child->next, ra);
					vlist_add(es->ids, ast->child->v);
					vlist_add(es->values, *ra);
				}
				else {
					verror("redefined variable %s", vvar_tocstr(&ast->child->v));
				}
			}
			else {
				int idx = id_declared(es, ast->v);
				if (idx == 0) {
					vlist_add(es->ids, ast->v);
					vlist_add(es->values, newvnull());
				}
				else {
					verror("redefined variable %s", vvar_tocstr(&ast->v));
				}
			}
			ast = ast->next;
		}
		*ra = newvnull();
		break;
	}

	case RIf:
		eval_expr(es, root->child, ra);
		if (ra->type != T_VBool) {
			verror("test expression must evalutes boolean value");
		}
		else if (ra->i == 1) {
			begin_scope(es);
			eval_stmt(es, root->child->next, ra);
			end_scope(es);
		}
		else if (root->child->next->next != 0) {
			begin_scope(es);
			eval_stmt(es, root->child->next->next, ra);
			end_scope(es);
		}
		break;

	case RWhile: {
		VVar condition;
		while (es->flag_break == 0) {
			eval_expr(es, root->child, &condition);
			if (condition.type != T_VBool) {
				verror("invalid while loop condition");
				break;
			}
			if (condition.i == 1) {
				begin_scope(es);
				eval_ast(es, root->child->next, ra);
				end_scope(es);
			}
			else break;
		}
		es->flag_break = 0;
		*ra = newvnull();
		break;
	}

	case RBreak:
		es->flag_break = 1;
		break;

	case RReturn:
		eval_expr(es, root->child, ra);
		return;

	case RNew:
	case RDelete:
	case RImport:
		break;

	case AstSetIdx: {
		VVar obj, key, value;
		eval_expr(es, root->child, &obj);
		eval_expr(es, root->child->next, &key);
		eval_expr(es, root->child->next->next, &value);
		if (obj.type == T_VList) {
			if (key.type == T_VInt) {
				vlist_set(obj, key.i, value);
			}
			else verror("array idx must be integer");
		}
		else if (obj.type == T_VDict) {
			vdict_put(obj, key, value);
		}
		else verror("trying to index invalid type");
		break;
	}

	case AstSetKey: {
		VVar obj, value;
		eval_expr(es, root->child, &obj);
		eval_expr(es, root->child->next->next, &value);
		if (obj.type == T_VDict) {
			vdict_put(obj, root->child->next->v, value);
		}
		else verror("trying to set key for invalid type");
		break;
	}

	default:
		eval_expr(es, root, ra);
	}
}

static void eval_ast(EvalState* es, Ast* root, VVar *ra) {
	while (root != 0 && es->flag_break == 0) {
		eval_stmt(es, root, ra);
		root = root->next;
	}
}

void nf_print(EvalState* es, VVar* result) {
	VVar v = vlist_get(es->values, 0);
	printf("%s\n", vvar_tocstr(&v));
}

void eval(Ast* root) {
	EvalState es;
	es.ids = newvlist(VLISTSIZE);
	es.values = newvlist(VLISTSIZE);
	es.scopes = newvlist(VLISTSIZE);
	es.context = newvnull();
	es.parent = newvnull();
	es.flag_break = 0;
	es.global = &es;

	vlist_add(es.ids, newvstr("print"));
	VVar fn = newvnull();
	fn.type = T_VCFunc;
	fn.cfunc = (void*)nf_print;
	vlist_add(es.values, fn);

	VVar ra;
	eval_ast(&es, root, &ra);

	vvar_print(es.ids);
	vvar_print(es.values);
	vvar_print(es.scopes);
}
