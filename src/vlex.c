
#include <ctype.h>
#include <limits.h>
#include "vmisc.h"
#include "vlex.h"
#include "vstring.h"

typedef struct LexState {
	VFile* file;
	char curr, last;
	int lineno, col;
	int error;
	Ast* head;
	Ast* tail;
	int stkcnt, stkcap;
	Ast** stack;
	char* buffer;
	int bufcnt, bufcap;
} LexState;

void step(LexState* ls) {
	ls->last = ls->curr;
	ls->curr = v_getc(ls->file);
	if (ls->curr == '\n') { ls->lineno++; ls->col = 1; }
	else if (ls->curr == '\t') { ls->col += 3; }
	else { ls->col++; }
}

char curr(LexState* ls) { return ls->curr; }
char peek(LexState* ls) { char c = v_getc(ls->file); v_ungetc(c, ls->file); return c; }
void step_buf(LexState* ls) { ls->buffer[ls->bufcnt++] = curr(ls); step(ls); }
void term_buf(LexState* ls) { ls->buffer[ls->bufcnt] = 0; ls->bufcnt = 0; }

void lexerr(LexState* ls, const char* info) {
	ls->error = 1;
	printf("error %d %d: %s\n", ls->lineno, ls->col, info);
}

const char* TkStr[] = {
	// Operators
	"++", "--",
	"<=", ">=", "==", "!=", "<", ">",
	"&&", "||",
	"+", "-", "*", "/", "%",
	"-", "!", ".",
	"=",

	// Punct
	",", ":", ";", "(", ")", "[", "]", "{", "}",

	// Reserved
	"var", "function",
	"this", "null", "true", "false",
	"if", "else",
	"while", "break", "return",
	"new", "delete", "import",
};

const int OpPrec[] = {
	-1, -1,
	6, 6, 5, 5, 6, 6,
	1, 0,
	8, 8, 9, 9, 9,
	-1,
	-1, -1, -1
};

const int TkOpBegin = 0;
const int TkOpEnd = PComma;
const int TkPunctBegin = PComma;
const int TkPunctEnd = RVar;
const int TkRsvdBegin = RVar;
const int TkRsvdEnd = TkId;

Ast* ast_new(int lineno, int col) {
	Ast* ast = (Ast*)malloc(sizeof(Ast));
	if (ast == 0)
		vfataloom("parsing source code");
	ast->next = ast->child = 0;
	ast->lineno = lineno;
	ast->col = col;
	return ast;
}

void ast_freetree(Ast* t) {
	if (t == 0) return;
	ast_freetree(t->child);
	ast_freetree(t->next);
	free(t);
}

static Ast* ast_newnext(LexState* ls) {
	Ast* t = ast_new(ls->lineno, ls->col);
	if (ls->stkcnt > 0 && ls->tail == ls->stack[ls->stkcnt - 1]) {
		ls->tail->child = t;
	}
	else {
		ls->tail->next = t;
	}
	ls->tail = t;
	return t;
}

static void ast_print(Ast* ast, int indent) {
	if (ast == 0) return;
	printf("%4d %3d: ", ast->lineno, ast->col);
	for (int i = 0; i < indent; i++) { printf("    "); }
	switch (ast->t) {
	case TkVar:		vvar_print(ast->v); break;
	case TkId:		printf("<id> "); vvar_print(ast->v); break;
	case AstSetIdx: printf("<seti>"); break;
	case AstGetIdx: printf("<geti>"); break;
	case AstSetKey: printf("<setk>"); break;
	case AstGetKey: printf("<getk>"); break;
	case AstInvoke:	printf("<invoke>"); break;
	case AstObjLiteral: printf("<obj>"); break;
	case AstArrLiteral: printf("<arr>"); break;
	default:		printf("%s", TkStr[ast->t]); break;
	}
	printf("\n");
}

void ast_printtree(Ast* t, int indent) {
	if (t == 0) return;
	ast_print(t, indent);
	ast_printtree(t->child, indent + 1);
	ast_printtree(t->next, indent);
}

static void ast_open(LexState* ls, enum AstEnum t) {
	Ast* ast = ast_newnext(ls);
	ast->t = t;
	ls->stack[ls->stkcnt++] = ast;
}

static void ast_close(LexState* ls, enum AstEnum t) {
	if (ls->stkcnt > 0 && ls->tail == ls->stack[ls->stkcnt - 1]) {
		ls->stkcnt--;
	}
	else {
		if (ls->stkcnt == 0 || ls->stack[ls->stkcnt - 1]->t != t - 1) {
			lexerr(ls, "unmatched brace / bracket / parenthesis");
		}
		else {
			ls->tail = ls->stack[ls->stkcnt - 1];
			ls->stkcnt--;
		}
	}
}

// lex

int lexcomment(LexState *ls) {
	if (curr(ls) == '/' && peek(ls) == '/') {
		while (curr(ls) != EOF && curr(ls) != '\n')
			step(ls);
		return 1;
	}
	else if (curr(ls) == '/' && peek(ls) == '*') {
		while (curr(ls) != EOF && (curr(ls) != '*' || peek(ls) != '/')) {
			step(ls);
		}
		if (curr(ls) == EOF) {
			vfatal("unterminated block comment");
		}
		else {
			step(ls); step(ls);
		}
		return 1;
	}
	else {
		return 0;
	}
}

int lexspace(LexState *ls) {
	if (!isspace(curr(ls))) return 0;
	else {
		while (isspace(curr(ls))) step(ls);
		return 1;
	}
}

int lexnumber(LexState *ls) {
	if (!isdigit(curr(ls)) && (curr(ls) != '.' || !isdigit(peek(ls)))) return 0;

	int isfloat = 0, base = 10;
	ls->bufcnt = 0;

	if (curr(ls) == '0' && peek(ls) == 'x') {
		step(ls); step(ls);
		base = 16;
	}

	while (isxdigit(curr(ls)) || curr(ls) == '.') {
		if (curr(ls) == '.') isfloat = 1;
		step_buf(ls);
	}

	char* end;
	term_buf(ls);

	if (isfloat) {
		double num = strtod(ls->buffer, &end);
		if (end) {
			Ast *ast = ast_newnext(ls);
			ast->t = TkVar;
			ast->v = newvfloat(num);
		}
		else {
			lexerr(ls, "invalid number");
		}
	}
	else {
		int64_t num = strtoll(ls->buffer, &end, 10);
		if (end) {
			if (num <= INT32_MAX && num >= INT32_MIN) {
				Ast *ast = ast_newnext(ls);
				ast->t = TkVar;
				ast->v = newvint((int32_t)num);
			}
			else {
				Ast *ast = ast_newnext(ls);
				ast->t = TkVar;
				ast->v = newvfloat((double)num);
			}
		}
		else {
			lexerr(ls, "invalid number");
		}
	}
	return 1;
}

int lexstring(LexState *ls) {
	if (curr(ls) == '\"' || curr(ls) == '\'') {
		char quote = curr(ls);

		ls->bufcnt = 0;
		step(ls);
		while (curr(ls) != EOF && (curr(ls) != quote || ls->last == '\\')) {
			if ((curr(ls) >= 0 && curr(ls) < 0x20) || curr(ls) == 0x7F) {
				lexerr(ls, "unsupported character in string");
				return 1;
			}
			step_buf(ls);
		}
		term_buf(ls);

		if (curr(ls) == EOF) {
			lexerr(ls, "unterminated string literal");
		}
		else {
			step(ls);
			Ast *ast = ast_newnext(ls);
			ast->t = TkVar;
			ast->v = newvstrn(strdup(ls->buffer), ls->bufcnt);
		}
		return 1;
	}
	else {
		return 0;
	}
}

int lexreserved(LexState *ls) {
	for (int i = TkRsvdBegin; i < TkRsvdEnd; i++) {
		if (strcmp(ls->buffer, TkStr[i]) == 0) {
			Ast *ast = ast_newnext(ls);
			switch (i) {
			case RNull:		ast->t = TkVar; ast->v = newvnull(); break;
			case RTrue:		ast->t = TkVar; ast->v = newvbool(1); break;
			case RFalse:	ast->t = TkVar; ast->v = newvbool(0); break;
			default:		ast->t = i; break;
			}
			return 1;
		}
	}
	return 0;
}

int lexid(LexState *ls) {
	if (!isalpha(curr(ls)) && curr(ls) != '_') return 0;
	step_buf(ls);
	while (curr(ls) != EOF && (isalnum(curr(ls)) || curr(ls) == '_')) {
		step_buf(ls);
	}
	term_buf(ls);
	if (!lexreserved(ls)) {
		Ast *ast = ast_newnext(ls);
		ast->t = TkId;
		ast->v = newvstrn(ls->buffer, ls->bufcnt);
	}
	return 1;
}

int lexpunct(LexState *ls) {
	if (!ispunct(curr(ls)) || curr(ls) == '\'' || curr(ls) == '\"') return 0;

	for (int i = TkOpBegin; i < TkPunctEnd; i++) {
		switch (strlen(TkStr[i])) {
		case 1:
			if (curr(ls) == TkStr[i][0]) {
				step(ls);
				if (i == PLBrace || i == PLBracket || i == PLParen) {
					ast_open(ls, i);
				}
				else if (i == PRBrace || i == PRBracket || i == PRParen) {
					ast_close(ls, i);
				}
				else {
					Ast *ast = ast_newnext(ls);
					ast->t = i;
				}
				return 1;
			}
			break;
		case 2:
			if (curr(ls) == TkStr[i][0] && peek(ls) == TkStr[i][1]) {
				Ast *ast = ast_newnext(ls);
				ast->t = i;
				step(ls); step(ls);
				return 1;
			}
			break;
		default:
			vfatal("unimplemented punct %s", TkStr[i]);
			break;
		}
	}
	return 0;
}

Ast* lex(VFile* file) {
	LexState ls;
	ls.file = file;
	ls.lineno = 1;
	ls.col = 1;
	ls.curr = v_getc(ls.file);
	ls.last = 0;
	ls.error = 0;
	ls.tail = ls.head = ast_new(ls.lineno, ls.col);
	ls.stkcap = 128;
	ls.stkcnt = 0;
	ls.stack = (Ast**)malloc(ls.stkcap * sizeof(Ast*));
	ls.bufcap = 2048;
	ls.bufcnt = 0;
	ls.buffer = (char*)malloc(ls.bufcap * sizeof(char*));

	if (ls.stack == 0 || ls.buffer == 0)
		vfataloom("parsing source code");

	while (ls.curr != EOF && ls.error == 0) {
		if (lexcomment(&ls)) {}
		else if (lexspace(&ls)) {}
		else if (lexnumber(&ls)) {}
		else if (lexstring(&ls)) {}
		else if (lexid(&ls)) {}
		else if (lexpunct(&ls)) {}
		else lexerr(&ls, "unkown char");
	}

	free(ls.stack);
	free(ls.buffer);
	return ls.head->next;
}
