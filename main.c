#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define DEBUG 1

void check_errors();

void init_errors(char*, char*);
void fatal_error(int, const char*, ...);
int get_line_num(int);
int get_column_num(int index);
int count_leading_whitespace(int);
char* get_line_from_index(int);
void errprint(char*);
void print_location(int);
void push_error(int, const char*, ...);
void list_errors();

void *bfm_malloc(size_t bytes)
{
	void *res = malloc(bytes);
	
	if (!res)
		fatal_error(-1, "out of memory.");

	return res;
}

void *bfm_realloc(void *block, size_t bytes)
{
	block = realloc(block, bytes);
	
	if (!block)
		fatal_error(-1, "out of memory.");

	return block;
}

char *load_file(const char *path)
{
	char *buf = NULL;
	FILE *file = fopen(path, "r");
	
	if (file) {
		if (fseek(file, 0L, SEEK_END) == 0) {
			long len = ftell(file);
			if (len == -1) return NULL;
			
			buf = bfm_malloc(len + 1);

			if (fseek(file, 0L, SEEK_SET) != 0)
				return NULL;

			size_t new_len = fread(buf, 1, len, file);
			if (ferror(file) != 0) {
				fatal_error(-1, "Input file could not be read.");
			} else {
				buf[new_len++] = '\0';
			}
		}
		
		fclose(file);
	}
	
	return buf;
}

#define MAX_ERRORS 128
typedef struct {
	int errloc;
	char* error;
} Error;
Error errors[MAX_ERRORS];

char *raw = NULL, *input_path = NULL, *output_path = NULL;
int num_errors;

void
check_errors()
{
	if (num_errors) {
		list_errors();
		exit(EXIT_FAILURE);
	}
}

int
get_line_num(int index)
{
	int counter = 0, i;
	for (i = 0; i < index; i++)
		if (raw[i] == '\n')
			counter++;

	return counter;
}

int
get_column_num(int index)
{
	int counter = 0, i;
	for (i = 0; i < index; i++) {
		if (raw[i] == '\n') counter = 0;
		else counter++;
	}
	
	return counter;
}

int
count_leading_whitespace(int index)
{
	char* start = &raw[index];
	int counter = 0;
	
	if (get_line_num(index) >= 1) {
		while (*start != '\n') start--;
		start++;
	} else
		start = raw;
	
	while (isspace(*start)) {
		counter++;
		start++;
	}
	
	return counter;
}

char*
get_line_from_index(int index)
{
	char* start = &raw[index], *end = start;
	
	if (get_line_num(index) >= 1) {
		while (*start != '\n') {
			start--;
		}
		start++;
	} else {
		start = raw;
	}
	
	while (isspace(*start)) start++;
	
	while (*end) {
		if (*end == '\n') break;
		end++;
	}
	
	char* buf = bfm_malloc(end - start + 1);
	int i = 0;
	while (start < end) {
		buf[i++] = *start;
		start++;
	}
	buf[i] = 0;
	
	return buf;
}

void
errprint(char* str) /* when printing errors, the size of tabs can be an issue.
                     * This prints a string with four spaces for tabs. */
{
	size_t i;
	for (i = 0; i < strlen(str); i++) {
		if (str[i] == '\t')
			printf("    ");
		else if (str[i] != '\n')
			putchar(str[i]);
	}
}

void
print_location(int index)
{
	char* line = get_line_from_index(index);
	
	putchar('\t');
	errprint(line);
	printf("\n\t");
	
	int i;
	for (i = 0; i < get_column_num(index) - count_leading_whitespace(index); i++)
		putchar(' ');
	
	printf("^\n");
}

#define MAX_ERROR_LENGTH 512
void
fatal_error(int errloc /* the location of the error */, const char* message, ...)
{
	char error_buf[MAX_ERROR_LENGTH + 1];
	
	if (errloc >= 0) {
		printf("%s:%d:%d: ", input_path,
            get_line_num(errloc) + 1,
            get_column_num(errloc) + 1);
	}
	
	printf("error: ");
	
	if (message) {
		va_list args;
		va_start(args, message);
		vsnprintf(error_buf, MAX_ERROR_LENGTH, message, args);
		va_end(args);
		printf("%s\n", error_buf);
	}
	
	if (errloc >= 0) /* if errloc is positive we will print out the line that caused the error */
		print_location(errloc);
	
	exit(EXIT_FAILURE);
}

void
push_error(int errloc /* the location of the error */, const char* message, ...)
{
	errors[num_errors].errloc = errloc;
	errors[num_errors].error = bfm_malloc(MAX_ERROR_LENGTH + 1);
	errors[num_errors].error[0] = 0;
	
	if (message) {
		va_list args;
		va_start(args, message);
		
		vsnprintf(errors[num_errors].error, MAX_ERROR_LENGTH, message, args);
		
		va_end(args);
	}
	
	num_errors++;
}

void
list_errors()
{
	int i;
	for (i = 0; i < num_errors; i++) {
		int errloc = errors[i].errloc;
		char* error = errors[i].error;
		
		if (errloc >= 0) {
			printf("%s:%d:%d: ", input_path,
				get_line_num(errloc) + 1,
				get_column_num(errloc) + 1);
		}
		
		printf("error: ");
		errprint(error);
		putchar('\n');
		
		if (errloc >= 0) /* if errloc is positive we will print out the line that caused the error */
			print_location(errloc);
	}
}

typedef struct struct_token {
	enum {
		TOK_IDENTIFIER, TOK_NUMBER,
		TOK_STRING, TOK_OPERATOR,
		TOK_SYMBOL
	} type;
	
	int origin; /* the index of the token's original location in the source file */
	char* value; /* the body of the token */
	
	struct struct_token* next; /* all tokens are part of a linked list */
	struct struct_token* prev;
} Token;

enum { /* math operators */
	MOP_EQUEQU, MOP_NEQU  ,
	MOP_ANDAND, MOP_OROR  ,
	MOP_ADDEQU, MOP_SUBEQU,
	MOP_MULEQU, MOP_DIVEQU,
	MOP_MODEQU, MOP_ADDADD,
	MOP_SUBSUB, MOP_GEQU  ,
	MOP_LEQU  , MOP_ADD   ,
	MOP_SUB   , MOP_EX    ,
	MOP_LESS  , MOP_MORE  ,
	MOP_MOD   , MOP_EQU   ,
	MOP_MUL   , MOP_DIV
};

#define is_legal_in_identifer(c) ( \
        (c >= 'a' && c <= 'z')     \
        || (c >= 'A' && c <= 'Z')  \
        || (c == '_')              \
        || (c >= '0' && c <= '9'))

/* support the escape characters \b, \t, \n, \f, \r */
char*
parse_escape_characters(char* str)
{
	char* buf = bfm_malloc(strlen(str) + 1);
	
	int i = 0;
	char* c = buf;
	while (str[i]) {
		if (str[i] == '\\') {
			i++;
			
			switch (str[i]) {
				case 't':
					*c = '\t';
					break;
				case 'n':
					*c = '\n';
					break;
				case 'b':
					*c = '\b';
					break;
				case 'f':
					*c = '\f';
					break;
				case 'r':
					*c = '\r';
					break;
				default:
					*c = str[i];
					break;
			}
			
			c++;
		} else {
			*c = str[i];
			c++;
		}
		
		i++;
	}
	*c = 0;
	
	free(str);
	return buf;
}

int
is_number(const char* str)
{
	int i = 0, result = 1, is_hex = 0;
	
	if (!strncmp(str, "0x", 2)) {
		i += 2;
		is_hex = 1;
	}
	
	while (str[i]) {
		if (is_hex) {
			if (!isalnum(str[i]) || tolower(str[i]) > 'f')
				result = 0;
		} else {
			if (!isdigit(str[i]))
				result = 0;
		}
		i++;
	}
	
	return result;
}

#define NUM_OPERATORS 22
char operators[NUM_OPERATORS][3] = {
	"==", "!=",
	"&&", "||",
	"+=", "-=",
	"*=", "/=",
	"%=", "++",
	"--", ">=",
	"<=", "+" ,
	"-" , "!" ,
	"<" , ">" ,
	"%" , "=" ,
	"*" , "/"
};

int
get_operator_type(const char* str)
{
	int i;
	for (i = 0; i < NUM_OPERATORS; i++) {
		if (!strncmp(operators[i], str, strlen(operators[i])))
			return i;
	}
	
	return -1;
}

char token_types[][11] = { /* for debugging */
	"IDENTIFIER",
	"NUMBER    ",
	"STRING    ",
	"OPERATOR  ",
	"SYMBOL    "
};

Token*
tokenize(char* in)
{
	Token* head    = bfm_malloc(sizeof(Token));
	Token* current = head;
	char * start   = in, *end = in;

	/* end will march ahead of start to the end of a token, then
	start will walk to end. repeat. (like an inchworm walking
	along the string) */

	int skip_char = 0, tok_type = -1, tok_origin = -1;
	
	while (*end != '\0') {
		/* whitespace is meaningless in this language,
                   it shouldn't be tokenized. */
		while (isspace(*start) && *start) {
			start++;
			end++;
		}
		tok_origin = start - in;
		
		if (!strncmp(start, "*/", 2)) {
			push_error(tok_origin, "comment terminator has no intializer.");
			
			start += 2;
			end = start;
			tok_origin = start - in;
		}

		/* skip multi-line comments by scanning for the
                 * terminator */
		if (!strncmp(start, "/*", 2)) {
			/* we will allow comments within comments,
			 * so we have to keep track of how many comments
			 * are embedded, so we know when we've reached
			 * the _actual_ comment terminator. */
			int comment_depth = 1;
			end += 2;

			/* when comment_depth is zero we have matched
			 * all comment initializers */
			while (comment_depth > 0 && *end) {
				end++;
				if (!strncmp(end, "/*", 2))
					comment_depth++;
				if (!strncmp(end, "*/", 2))
					comment_depth--;
			}
			
			if (comment_depth || !*end) {
				push_error(tok_origin, "unterminated comment.");
				
				start += 2;
				end = start;
				tok_origin = start - in;
			}
			
			end += 2;
			start = end;
			continue;
		} else if (!strncmp(start, "//", 2)) { /* skip single line comments by scanning for newline or NUL */
			end += 2;
			while (strncmp(end, "\n", 1) && *end) end++;
			start = end;
			continue;
		}
		
		start = end;
		
		while (isspace(*start) && *start) /* skip any more whitespace */
			start++;
		
		if (!*start)
			break; /* there was whitespace at the end of the file, don't try to tokenize the void */
		
		end = start;
		tok_origin = start - in;
		skip_char = 0;
		
		/* main tokenization */
		if (get_operator_type(start) >= 0) {
			tok_type = TOK_OPERATOR;
			end += strlen(operators[get_operator_type(start)]);
		} else if (is_legal_in_identifer(*start)) {
			tok_type = TOK_IDENTIFIER;
			while (is_legal_in_identifer(*end)) end++;
		} else if (*start == '"') {
			skip_char = 1;
			
			end++;
			start++;
			
			tok_type = TOK_STRING;
			while (*end != '"' && *end) {
				if (*end == '\n') {
					push_error(tok_origin, "unmatched \" character.");
					
					start += 1;
					end = start;
					tok_origin = start - in;
					
					break;
				}
				else {
					if (*end == '\\' && *(end + 1) != 0)
						end++;
					end++;
				}
			}
			
			if (!*end) {
				push_error(tok_origin, "unmatched \" character.");
				
				start += 1;
				end = start;
				tok_origin = start - in;
			}
		} else {
			tok_type = TOK_SYMBOL;
			end++;
		}
		
		current->type   = tok_type;
		current->origin = tok_origin;
		current->value  = bfm_malloc(end - start + 1);
		
		int i = 0;
		while (start < end) { /* walk start until it catches up with end */
			current->value[i] = *start;
			start++;
			i++;
		}
		current->value[i] = 0;
		
		if (is_number(current->value))
			current->type = TOK_NUMBER;
		
		if (current->type == TOK_STRING)
			current->value = parse_escape_characters(current->value);
		
		current->next = bfm_malloc(sizeof(Token));
		current->next->prev = current;
		current = current->next;
		
		if (skip_char) {
			end++;
			start++;
		}
	}

	current->prev->next = NULL;

	
	return head;
}

#define OUT_GROWTH_RATE 4096 /* how many bytes we should use as padding when we run out of room */
char *output;
int out_index = 0, out_allocated = 0;

void reset_emit()
{
	if (output)
		free(output);
	
	output = bfm_malloc(128 /* arbitrary number */);
	out_allocated = 128;
	out_index = 0;
	
	output[0] = '\0';
}

void emit(const char* out)
{
	if (!output) reset_emit();
	
	int bytes_needed = strlen(out) + out_index - out_allocated;
	if (bytes_needed >= 0) {
		output = bfm_realloc(output, out_allocated + bytes_needed + OUT_GROWTH_RATE);
		out_allocated = out_allocated + bytes_needed + OUT_GROWTH_RATE;
	}
	
	strcpy(&output[out_index], out);
	out_index += strlen(out);
}

void emit_char(char c)
{
	char buf[2] = { 0 };
	buf[0] = c;
	
	emit(buf);
}

#define IS_DIGIT(c) \
	(c >= '0' && c <= '9')

#define IS_ALPHA(c)               \
	((c >= 'a' && c <= 'z')   \
	|| (c >= 'A' && c <= 'Z') \
	|| IS_DIGIT(c))

#define IS_BF_COMMAND(c) (   \
    c == '<' || c == '>'     \
    || c == '+' || c == '-'  \
    || c == '[' || c == ']'  \
    || c == ',' || c == '.')

#define IS_CONTRACTABLE(c) ( \
    c == '<' || c == '>'     \
    || c == '+' || c == '-')

#define SKIP_COMMENTS(c)                      \
    do {                                      \
        while (!IS_BF_COMMAND(*c) && *c) c++; \
    } while (0);

#define NUM_TEMP_CELLS 10
int cell_pointer = 0, temp_cells = 0, temp_x = 0, temp_x_index = 0, temp_y = 0, temp_y_index = 0;

int count_variables(Token* tok)
{
	int res = 0;

	while (tok->next) {
		if (!strcmp(tok->value, "var"))
			res++;

		tok = tok->next;
	}

	return res;
}

void move_pointer(int distance)
{
	cell_pointer += distance;
	int i;
	if (distance > 0)
		for (i = 0; i < distance; i++)
			emit(">");
	else
		for (i = 0; i > distance; i--)
			emit("<");
}

void move_pointer_to(int position)
{
	int cells_to_move = position - cell_pointer;
	move_pointer(cells_to_move);
}

enum {
	ALGO_DIV, ALGO_MUL,
	ALGO_ADD, ALGO_SUB,
	ALGO_EQU, ALGO_MOD,
	ALGO_GRT, ALGO_NOT,
	ALGO_CEQU, ALGO_ARRAY_WRITE,
	ALGO_ARRAY_READ, ALGO_PRINTV,
	ALGO_OR
};

#define NUM_ALGORITHMS 13
char algorithms[NUM_ALGORITHMS][256] = {
	"0[-]1[-]2[-]3[-]x[0+x-]0[y[1+2+y-]2[y+2-]1[2+0-[2[-]3+0-]3[0+3-]2[1-[x-1[-]]+2-]1-]x+0]", /* division */
	"0[-]1[-]x[1+x-]1[y[x+0+y-]0[y+0-]1-]", /* multiplication */
	"0[-]y[x+0+y-]0[y+0-]", /* addition */
	"0[-]y[x-0+y-]0[y+0-]", /* subtraction */
	"0[-]x[-]y[x+0+y-]0[y+0-]", /* equalization */
	"0[-]1[-]2[-]3[-]4[-]5[-]x[0+x-]y[1+2+y-]2[y+2-]0[>->+<[>]>[<+>-]<<[<]>-]2[x+2-]x", /* modulus */
	"0[-]1[-]2[-]x[0+y[-0[-]1+y]0[-2+0]1[-y+1]y-x-]2[x+2-]", /* greater than*/
	"0[-]x[0+x[-]]+0[x-0-]", /* logical not */
	"0[-]1[-]x[1+x-]+y[1-0+y-]0[y+0-]1[x-1[-]]", /* conditional equality */
	"z[-x+x>>>+<<<z]x[-z+x]y[-x+x>+<y]x[-y+x]y[-x+x>>+<<y]x[-y+x]>[>>>[-<<<<+>>>>]<[->+<]<[->+<]<[->+<]>-]>>>[-]<[->+<]<[[-<+>]<<<[->>>>+<<<<]>>-]<<", /* x(y) = z (array write) */
	"z[-y+y>+<z]y[-z+y]z[-y+y>>+<<z]y[-z+y]>[>>>[-<<<<+>>>>]<<[->+<]<[->+<]>-]>>>[-<+<<+>>>]<<<[->>>+<<<]>[[-<+>]>[-<+>]<<<<[->>>>+<<<<]>>-]<<x[-]y>>>[-<<<x+y>>>]<<<", /* x = y(z) (array read) */
	"0[-]1[-]2[-]3[-]4[-]5[-]6[-]7[-]x[0+1+x-]1[x+1-]0[>>+>+<<<-]>>>[<<<+>>>-]<<+>[<->[>++++++++++<[->-[>+>>]>[+[-<+>]>+>>]<<<<<]>[-]++++++++[<++++++>-]>[<<+>>-]>[<<+>>-]<<]>]<[->>++++++++[<++++++>-]]<[.[-]<]<", /* printv */
	"0[-]1[-]x[1+x-]1[x-1[-]]y[1+0+y-]0[y+0-]1[x[-]-1[-]]" /* logical or */
};

void emit_algo(int algo, int x, int y, int z)
{
	int i = 0;
	while (algorithms[algo][i] != '\0') {
		if (IS_BF_COMMAND(algorithms[algo][i])) {
			emit_char(algorithms[algo][i]);
		} else {
			switch (algorithms[algo][i]) {
				case 'x':
					move_pointer_to(x);
					break;
				case 'y':
					move_pointer_to(y);
					break;
				case 'z':
					move_pointer_to(z);
					break;
				default:
					move_pointer_to(temp_cells + (algorithms[algo][i] - '0'));
					break;
			}
		}
		i++;
	}
}

void add(int amount)
{
	int i;
	
	if (amount > 0)
		for (i = 0; i < amount; i++) emit("+");
	else
		for (i = 0; i > amount; i--) emit("-");
}

void emit_print_string(const char* str)
{
	move_pointer_to(temp_cells);
	emit("[-]"), add(str[0]), emit(".");
	
	int i = 1;
	while (str[i] != '\0') {
		add(str[i] - str[i - 1]);
		emit(".");
		i++;
	}
}

enum {
	KYWRD_VAR,
	KYWRD_WHILE,
	KYWRD_END,
	KYWRD_GOTO,
	KYWRD_IF,
	KYWRD_NOT,
	KYWRD_PRINT,
	KYWRD_ARRAY,
	KYWRD_BF,
};

#define NUM_KEYWORDS 12
char keywords[NUM_KEYWORDS][15] = {
	"var",
	"while",
	"end",
	"goto",
	"if",
	"not",
	"print",
	"array",
	"fuck",
};

int get_keyword(char* str)
{
	for (int i = 0; i < NUM_KEYWORDS; i++)
		if (!strcmp(str, keywords[i]))
			return i;
	return -1;
}

typedef struct {
	char* name;
	int location, scope;
} Variable;
Variable variables[4096];
int num_variables = 0, scope = 0;

void add_variable(char* varname)
{
	variables[num_variables].location = num_variables;
	variables[num_variables].scope = scope;
	variables[num_variables++].name = varname;
}

void kill_variables_of_scope(int killscope)
{
	int numvars = num_variables;
	for (int i = num_variables - 1; i >= 0; i--) {
		if (variables[i].scope == killscope) {
			num_variables--;
		}
	}
}

int get_variable_index(char* varname)
{
	for (int i = 0; i < num_variables; i++) {
		if (!strcmp(variables[i].name, varname))
			return i;
	}
	return -1;
}

void parse_operation(Token** token)
{
	Token* tok = *token;

	int left = 0, operation = 0, right = 0;

	int left_index = get_variable_index(tok->value);
	if (left_index == -1)
		fatal_error(tok->origin, "expected an a variable identifier.\n");

	left = variables[left_index].location;
	tok = tok->next;

	operation = get_operator_type(tok->value);
	tok = tok->next;

	int right_index = get_variable_index(tok->value);
	if (right_index == -1) {
		if (operation == MOP_ADD) {
			long a = strtol(tok->value, NULL, 0);
			move_pointer_to(left);
			add((int)a);
			*token = tok;
			return;
		} if (operation == MOP_SUB) {
			long a = strtol(tok->value, NULL, 0);
			move_pointer_to(left);
			add(-a);
			*token = tok;
			return;
		} if (operation == MOP_EQU) {
			long a = strtol(tok->value, NULL, 0);
			move_pointer_to(left);
			emit("[-]");
			add((int)a);
			*token = tok;
			return;
		} else {
			move_pointer_to(temp_y);
			emit("[-]");
			long a = strtol(tok->value, NULL, 0);
			add((int)a);
			right = temp_y;
		}
	} else {
		right = variables[right_index].location;
	}

	int algo;
	switch (operation) {
		case MOP_EQU: algo = ALGO_EQU; break;
		case MOP_MOD: algo = ALGO_MOD; break;
		case MOP_EQUEQU: algo = ALGO_CEQU; break;
		case MOP_ADD: algo = ALGO_ADD; break;
		default: push_error(tok->origin, "unrecognized operator %s\n", tok->value); return;
	}

	emit_algo(algo, left, right, -1);

	*token = tok;
}

enum {
	STACK_WHILE,
	STACK_IF
};

int stack[4096], stack_ptr = 0;

#define NEXT_TOKEN(t) \
	do { \
		if (!(t->next)) { \
			fatal_error(t->origin, "expected a valid token."); \
		} else t = t->next; \
	} while (0);

void parse_keyword(Token** token)
{
	Token* tok = *token;

	switch (get_keyword(tok->value)) {
		case KYWRD_VAR:
			NEXT_TOKEN(tok)

			add_variable(tok->value);
			break;
		case KYWRD_WHILE: {
			NEXT_TOKEN(tok)

			int var_index = get_variable_index(tok->value);
			
			if (var_index == -1)
				fatal_error(tok->origin, "invalid identifier.");

			int variable_location = variables[var_index].location;
			move_pointer_to(variable_location);
			emit("[");

			stack[stack_ptr++] = variable_location;
			stack[stack_ptr++] = STACK_WHILE;

			scope++;
		} break;
		case KYWRD_END: {
			if (stack_ptr < 1)
				fatal_error(tok->origin, "unmatched end statement.");

			switch (stack[--stack_ptr]) {
				case STACK_WHILE:
					move_pointer_to(stack[--stack_ptr]);
					emit("]");
					break;
				case STACK_IF:
					move_pointer_to(stack[--stack_ptr]);
					emit("[-]]");
					break;
			}

			if (scope <= 0)
				fatal_error(tok->origin, "unable to resolve scope.");

			kill_variables_of_scope(scope--);
		} break;
		case KYWRD_GOTO: {
			NEXT_TOKEN(tok)

			int var_index = get_variable_index(tok->value);
			if (var_index == -1)
				fatal_error(tok->origin, "invalid identifier.");

			int variable_location = variables[var_index].location;
			move_pointer_to(variable_location);
		}  break;
		case KYWRD_IF: {
			NEXT_TOKEN(tok)

			int var_index = get_variable_index(tok->value);
			if (var_index == -1)
				fatal_error(tok->origin, "invalid identifier.");

			int variable_location = variables[var_index].location;
			
			emit_algo(ALGO_EQU, temp_x, variable_location, -1);
			move_pointer_to(temp_x);
			emit("[");

			stack[stack_ptr++] = temp_x;
			stack[stack_ptr++] = STACK_IF;

			scope++;
		} break;
		case KYWRD_NOT: {
			NEXT_TOKEN(tok)

			int var_index = get_variable_index(tok->value);
			if (var_index == -1)
				fatal_error(tok->origin, "invalid identifier.");

			int variable_location = variables[var_index].location;
			emit_algo(ALGO_NOT, variable_location, -1, -1);
		} break;
		case KYWRD_PRINT:
			NEXT_TOKEN(tok)

			switch (tok->type) {
				case TOK_STRING:
					emit_print_string(tok->value);
					break;
				case TOK_NUMBER: {
					char c[2] = { 0, 0 };
					c[0] = (char)strtol(tok->value, NULL, 0);
					
					emit_print_string(c);
					break;
				}
				case TOK_IDENTIFIER: {
					int var_index = get_variable_index(tok->value);

					if (var_index == -1)
						fatal_error(tok->origin, "invalid identifier.");

					emit_algo(ALGO_PRINTV, var_index, -1, -1);
					break;
				}
				default:
					push_error(-1, "unexpected token \"%s\", expected a string, number, or identifier.", tok->value);
			}
			break;
		case KYWRD_ARRAY: break;
		case KYWRD_BF: {
			NEXT_TOKEN(tok)

			if (tok->type != TOK_STRING)
				push_error(tok->origin, "expected a string literal.");
			else
				emit(tok->value);
		} break;
	}

	*token = tok;
}

void parse(Token* tok)
{
	while (tok) {
		if (get_keyword(tok->value) != -1) {
			parse_keyword(&tok);
			tok = tok->next;
		} else if (get_variable_index(tok->value) != -1) {
			parse_operation(&tok);
			tok = tok->next;
		} else {
			push_error(tok->origin, "unhandled token.\n", tok->value);
			tok = tok->next;
		}
	}

	check_errors();
}

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "-o", 2))
			output_path = &argv[i][2];
		else
			input_path = argv[i];
	}

	if (!input_path)
		fatal_error(-1, "Usage: bfm INPUT_PATH -oOUTPUT_PATH");

	char *src = load_file(input_path);
	raw = src;

	if (!src)
		fatal_error(-1, "Usage: bfm INPUT_PATH -oOUTPUT_PATH");

	Token *tok = tokenize(src);
	check_errors();

#if 0
	puts("\nPRINTING COMPLETE TOKEN LISTING:");
	Token* ttok = tok;
	while (ttok) {
		printf("\n[%s][%s]", token_types[ttok->type], ttok->value);
		ttok = ttok->next;
	}
	puts("\nFINISHED PRINTING COMPLETE TOKEN LISTING.");
#endif

	temp_cells = count_variables(tok) + 4;
	temp_x = temp_cells - 4, temp_y = temp_x + 2;

	parse(tok);

	FILE* output_file = stdout;
	
	int index = 0;
	while (output[index] != '\0') {
		if (index % 80 == 0 && index != 0) {
			fputc('\n', output_file);
		}
		fputc(output[index], output_file);
		index++;
	}
	
	//fclose(output_file);

	return 0;
}