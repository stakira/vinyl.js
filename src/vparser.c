
#include "vinyl.h"
#include "vio.h"
#include "vgc.h"
#include "vparser.h"
#include "vstring.h"
#include "vmisc.h"

// transforms

Ast* ast_rmnext(Ast* t) {
	Ast* tn = t->next;
	t->next = tn->next;
	tn->next = 0;
	return tn;
}

void ast_swap(Ast* a, Ast* b) {
	Ast *an = a->next, *bn = b->next;
	Ast tmp = *a; *a = *b; *b = tmp;
	a->next = an; b->next = bn;
}

void ast_next_to_first_child(Ast* t) {
	Ast* tn = t->next;
	t->next = tn->next;
	tn->next = 0;
	tn->next = t->child;
	t->child = tn;
}

void ast_next_to_last_child(Ast* t) {
	Ast* tn = t->next;
	t->next = tn->next;
	tn->next = 0;
	Ast* child = t->child;
	if (child != 0) {
		while (child->next != 0)
			child = child->next;
		child->next = tn;
	}
	else {
		t->child = tn;
	}
}

Ast* ast_insert_after(Ast* t) {
	Ast* p = ast_new(t->lineno, t->col);
	p->next = t->next;
	t->next = p;
	return p;
}

ParserState parserstate_new() {
	ParserState ps;
	ps.head = 0;
	ps.curr = 0;
	return ps;
}

static void parerr(ParserState* ps, const char* info) {
	if (ps->curr != 0) {
		printf("error %d %d: %s\n", ps->curr->lineno, ps->curr->col, info);
	}
	else {
		printf("error: %s\n", info);
	}
	//ast_printtree(ps->head, 0);
	exit(1);
}

static void parwrn(ParserState* ps, const char* info) {
	if (ps->curr != 0) {
		printf("warning %d %d: %s\n", ps->curr->lineno, ps->curr->col, info);
	}
	else {
		printf("warning: %s\n", info);
	}
}

static void step(ParserState* ps) {
	ps->curr = ps->curr->next;
}
static int curris(ParserState* ps, enum AstEnum t) {
	return ps->curr != 0 && ps->curr->t == t;
}
static int stepifis(ParserState* ps, enum AstEnum t) {
	if (curris(ps, t)) { 
		step(ps);
		return 1;
	}
	else return 0;
}
#define enterblk()		Ast* backup = ps->curr->next; ps->curr = ps->curr->child;
#define exitblk()		ps->curr = backup;

int expression(ParserState* ps, Ast** expr); // forward

static int declarator(ParserState* ps, VVar* id) {
	Ast* start = ps->curr;
	if (curris(ps, TkId)) {
		*id = ps->curr->v;
		step(ps);

		Ast* op = ps->curr;
		if (stepifis(ps, OpAsgn)) {
			Ast* expr;
			if (expression(ps, &expr)) {
				ast_swap(start, op);
				ast_next_to_first_child(start);
				ast_next_to_last_child(start);
			}
			else { parerr(ps, "invalid initialization syntax"); }
		}

		return 1;
	}
	else return 0;
}

static int variable_decl(ParserState* ps) {
	Ast* rvar = ps->curr;
	if (stepifis(ps, RVar)) {
		int count = 0;
		while (count == 0 || curris(ps, PComma)) {
			if (stepifis(ps, PComma)) free(ast_rmnext(rvar));

			VVar id;
			if (declarator(ps, &id)) {
				ast_next_to_last_child(rvar);
			}
			else {
				parerr(ps, "invalid declarator");
				break;
			}
			count++;
		}

		if (!stepifis(ps, PSemi)) {
			parerr(ps, "expecting ',' or ';'");
		}

		return 1;
	}
	else return 0;
}

static int literal_expr(ParserState* ps, Ast** expr) {
	*expr = ps->curr;
	if (stepifis(ps, TkVar) || stepifis(ps, RThis)) return 1;
	else return 0;
}

static int dot_id(ParserState* ps, Ast** expr, Ast* obj) {
	Ast* op = ps->curr;
	if (stepifis(ps, OpDot) && stepifis(ps, TkId)) {
		ast_swap(obj, op);
		ast_next_to_first_child(obj);
		ast_next_to_last_child(obj);
		obj->t = AstGetKey;

		*expr = obj;
		return 1;
	}
	return 0;
}

static int indexing(ParserState* ps, Ast** expr, Ast* obj) {
	if (curris(ps, PLBracket) && ps->curr->child != 0) {
		Ast* idx = ps->curr;

		enterblk();
		if (!expression(ps, expr))
			parerr(ps, "invalid index");
		exitblk();

		ast_swap(obj, idx);
		ast_next_to_first_child(obj);
		obj->t = AstGetIdx;

		*expr = obj;
		return 1;
	}
	else return 0;
}

static int arg_list(ParserState* ps, Ast* base) {
	Ast* args = ps->curr;
	if (curris(ps, PLParen)) {
		enterblk();
		while (ps->curr != 0) {
			Ast* expr;
			if (!expression(ps, &expr))
				parerr(ps, "invalid argument");

			if (stepifis(ps, PComma))
				free(ast_rmnext(expr));
			else if (ps->curr != 0)
				parerr(ps, "expecting \',\' in parameter list");
		}
		exitblk();

		ast_swap(base, args);
		ast_next_to_first_child(base);
		base->t = AstInvoke;
		return 1;
	}
	else return 0;
}

static int obj_literal_expr(ParserState* ps, Ast** expr) {
	if (curris(ps, PLBrace)) {
		*expr = ps->curr;
		enterblk();
		Ast* start = ps->curr, *value;
		while (ps->curr != 0) {
			Ast *key, *op;
			if (curris(ps, TkId) || curris(ps, TkVar) && ps->curr->v.type == T_VString) {
				key = ps->curr;
				step(ps);
				op = ps->curr;
				if (stepifis(ps, PColon)) {
					if (expression(ps, &value)) {
						ast_swap(key, op);
						ast_next_to_last_child(key);
						ast_next_to_last_child(key);
					}
					else {
						parerr(ps, "invalid initialization in object literal"); break;
					}
				}
				else {
					parerr(ps, "expecting ':'"); break;
				}
			}
			else {
				parerr(ps, "invalid key in object literal"); break;
			}

			if (ps->curr != 0 && !stepifis(ps, PComma)) {
				parerr(ps, "expecting ','"); break;
			}
		}

		while (start != 0 && start->next != 0) {
			if (start->next->t == PComma)
				ast_rmnext(start);
			else
				start = start->next;
		}
		exitblk();
		(*expr)->t = AstObjLiteral;
		return 1;
	}
	else return 0;
}

static int array_literal_expr(ParserState* ps, Ast** expr) {
	if (curris(ps, PLBracket)) {
		*expr = ps->curr;
		enterblk();
		Ast* start = ps->curr, *value;
		while (ps->curr != 0) {
			if (expression(ps, &value)) {}
			else {
				parerr(ps, "invalid initialization in list literal"); break;
			}

			if (ps->curr != 0 && !stepifis(ps, PComma)) {
				parerr(ps, "expecting ','"); break;
			}
		}

		while (start != 0 && start->next != 0) {
			if (start->next->t == PComma)
				ast_rmnext(start);
			else
				start = start->next;
		}
		exitblk();
		(*expr)->t = AstArrLiteral;
		return 1;
	}
	else return 0;
}

static int primary_expr(ParserState* ps, Ast** expr) {
	Ast* base = ps->curr;
	if (literal_expr(ps, &base)) { *expr = base; }
	else if (curris(ps, PLParen)) {
		enterblk();
		if (expression(ps, expr) && ps->curr == 0) {
			exitblk();
			ast_swap(base, *expr);
			free(*expr);
			*expr = base;
			return 1;
		}
		else {
			exitblk();
			parerr(ps, "invlaid syntax");
		}
	}
	else if (obj_literal_expr(ps, expr)) {}
	else if (array_literal_expr(ps, expr)) {}
	else if (curris(ps, TkId)) { *expr = base; step(ps); }
	// else if (stepifis(ps, RNew)) {}
	else return 0;

	while (indexing(ps, expr, base)
		|| dot_id(ps, expr, base)
		|| arg_list(ps, base)) {
	}

	return 1;
}

static int unary_expr(ParserState* ps, Ast** expr) {
	Ast* start = ps->curr;

	if (curris(ps, OpSub) || curris(ps, OpNot)) {
		if (curris(ps, OpSub))
			ps->curr->t = OpNeg;
		Ast* op = ps->curr;
		step(ps);
		if (unary_expr(ps, expr)) {
			ast_next_to_first_child(op);
			*expr = op;
		}
		else {
			parerr(ps, "invlaid syntax");
		}
		return 1;
	}
	else if (primary_expr(ps, expr)) { return 1; }

	ps->curr = start;
	return 0;
}

static int binary_expr(ParserState* ps, int p, Ast** expr) {
	Ast *left, *right;
	if (unary_expr(ps, &left)) {
		*expr = left;
		int r = 0xffff;
		while (ps->curr != 0 &&
			ps->curr->t < TkOpEnd &&
			OpPrec[ps->curr->t] >= p &&
			OpPrec[ps->curr->t] <= r)
		{
			Ast* op = ps->curr;
			step(ps);
			if (binary_expr(ps, OpPrec[op->t] + 1, &right)) {
				r = OpPrec[op->t];

				ast_swap(left, op);
				ast_next_to_first_child(left);
				ast_next_to_last_child(left);
				*expr = left;
			}
			else {
				parerr(ps, "invalid syntax");
			}
		}
		return 1;
	}
	else return 0;
}

int expression(ParserState* ps, Ast** expr) {
	if (binary_expr(ps, 0, expr)) { return 1; }
	else return 0;
}

int statement_block(ParserState* ps); // forward

int if_statement(ParserState* ps) {
	Ast* rif = ps->curr, *expr, *relse;
	if (stepifis(ps, RIf)) {
		if (expression(ps, &expr)) {
			if (statement_block(ps)) {
				ast_next_to_first_child(rif);
				ast_next_to_last_child(rif);
			}
			else { parerr(ps, "expecting '{'"); }
		}
		else { parerr(ps, "expecting test expression"); }
		
		relse = ps->curr;
		if (stepifis(ps, RElse)) {
			if (if_statement(ps) || statement_block(ps)) {
				ast_swap(relse, relse->next);
				ast_rmnext(relse);
				ast_next_to_last_child(rif);
			}
			else { parerr(ps, "expecting '{' or 'if' after 'else'"); }
		}

		return 1;
	}
	else return 0;
}

int while_statement(ParserState* ps) {
	Ast* rwhile = ps->curr;
	if (stepifis(ps, RWhile)) {
		Ast* expr; // test expr
		if (expression(ps, &expr)) {
			if (statement_block(ps)) {
				ast_next_to_first_child(rwhile);
				ast_next_to_last_child(rwhile);
			}
			else { parerr(ps, "expecting '{'"); }
		}
		else { parerr(ps, "expecting test expression"); }
		return 1;
	}
	else return 0;
}

int assign_op(ParserState* ps) {
	return stepifis(ps, OpAsgn);
}

int statement(ParserState* ps) {
	Ast *start = ps->curr, *expr = 0;
	if (stepifis(ps, PSemi)) { parwrn(ps, "empty statement"); }
	else if (stepifis(ps, RBreak)) {
		if (stepifis(ps, PSemi)) {}
		else { parerr(ps, "expecting ';' after break"); }
	}
	else if (stepifis(ps, RReturn)) {
		if (expression(ps, &expr)) {
			ast_next_to_first_child(start);
		}
		if (stepifis(ps, PSemi)) {}
		else { parerr(ps, "expecting ';'"); }
	}
	else if (if_statement(ps)) {}
	else if (while_statement(ps)) {}
	else if (variable_decl(ps)) {}
	else if (statement_block(ps)) {}
	else if (expression(ps, &expr)) {
		Ast* lhs = expr;
		Ast* op = ps->curr;
		if (curris(ps, OpAsgn)) {
			step(ps);
			if (expression(ps, &expr)) {
				if (lhs->t == AstGetIdx || lhs->t == AstGetKey) {
					lhs->t++;
					free(ast_rmnext(lhs));
					ast_next_to_last_child(lhs);
				}
				else {
					ast_swap(lhs, op);
					ast_next_to_first_child(lhs);
					ast_next_to_last_child(lhs);
				}
			}
			else {
				parerr(ps, "invalid assignment");
			}
		}
		else if (stepifis(ps, OpInc) || stepifis(ps, OpDec)) {
			ast_swap(lhs, op);
			ast_next_to_first_child(lhs);
		}

		if (stepifis(ps, PSemi)) {}
		else {
			parerr(ps, "invalid syntax, missing ';'?");
		}
	}
	else return 0;
	return 1;
}

int statement_block(ParserState* ps) {
	if (curris(ps, PLBrace)) {
		enterblk();
		while (ps->curr != 0)
			if (!statement(ps))
				parerr(ps, "invalid syntax");
		exitblk();
		return 1;
	}
	else return 0;
}

int parameter_list(ParserState* ps) {
	if (curris(ps, PLParen)) {
		enterblk();

		while (ps->curr != 0) {
			VVar id;
			if (!declarator(ps, &id))
				parerr(ps, "invalid parameter syntax");

			if (curris(ps, PComma)) {
				Ast* comma = ps->curr;
				if (comma->next != 0) {
					ast_swap(comma, comma->next);
					ast_rmnext(comma);
				}
			}
			else if (ps->curr != 0) {
				parerr(ps, "expecting \',\' in parameter list");
			}
		}

		exitblk();
		return 1;
	}
	else return 0;
}

int function_decl(ParserState* ps) {
	Ast *func = ps->curr, *t0 = 0;
	if (stepifis(ps, RFunction)) {
		if (stepifis(ps, TkId)) {
			ast_swap(func, func->next);
			t0 = func;
			func = func->next;
		}
		if (parameter_list(ps)) {
			if (statement_block(ps)) {
				ast_next_to_last_child(func);
				ast_next_to_last_child(func);
				if (t0 != 0) {
					Ast* t2 = ast_insert_after(t0);
					t2->t = OpAsgn;
					Ast* t1 = ast_insert_after(t0);
					t1->t = RVar;
					ast_swap(t0, t1);
					ast_swap(t1, t2);
					ast_next_to_last_child(t1);
					ast_next_to_last_child(t1);
					ast_next_to_last_child(t0);
				}
			}
			else parerr(ps, "expecting '{'");
		}
		else parerr(ps, "expecting '('");
		return 1;
	}
	else return 0;
}

int compilation_unit(ParserState* ps) {
	while (ps->curr != 0) {
		if (function_decl(ps)) {}
		else if (variable_decl(ps)) {}
		else if (statement(ps)) {}
		else break;
	}

	if (ps->curr != 0) {
		parerr(ps, "invalid syntax");
	}

	return 1;
}
