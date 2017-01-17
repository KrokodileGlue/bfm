#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

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

enum {
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
		if (!strcmp(operators[i], str))
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
	
	while (*end) {
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
		} else if (!strncmp(start, "//", 2)) { /* skip single line comments by scanning for newline or NUL */
			end += 2;
			while (strncmp(end, "\n", 1) && *end) end++;
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
	current->next = NULL;
	
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

int cell_pointer = 0, temp_cells = 2;

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
	printf("dividing cell %d by cell %d.\n", x, y);
	
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
	move_pointer_to(0);
	emit("[-]"), add(str[0]), emit(".");
	
	int i = 1;
	while (str[i] != '\0') {
		add(str[i] - str[i - 1]);
		emit(".");
		i++;
	}
}

#define MAX_VARIABLES 4096
char* variable_names[MAX_VARIABLES];
int num_variables;

void add_variable(char* varname)
{
	variable_names[num_variables++] = varname;
}

enum {
	KYWRD_VAR,
	KYWRD_PRINTC,
	KYWRD_WHILE,
	KYWRD_END,
	KYWRD_GOTO,
	KYWRD_IF,
	KYWRD_ENDIF,
	KYWRD_NOT,
	KYWRD_PRINT,
	KYWRD_ARRAY,
	KYWRD_BF,
	KYWRD_PRINTV
};

#define NUM_KEYWORDS 12
char keywords[NUM_KEYWORDS][15] = {
	"var",
	"printc",
	"while",
	"endwhile",
	"goto",
	"if",
	"endif",
	"not",
	"print",
	"array",
	"bf",
	"printv"
};

int is_keyword(char* str)
{
	for (int i = 0; i < NUM_KEYWORDS; i++)
		if (!strcmp(str, keywords[i]))
			return i;
	return -1;
}

void parse_keyword(Token** token)
{
	Token* tok = *token;
	if (!strcmp(tok->value, "print")) {
		tok = tok->next;
		emit_print_string(tok->value);
	} else if (!strcmp(tok->value, "var")) {
		tok = tok->next;
		add_variable(tok->value);
	}
	*token = tok;
}

void parse(Token* tok)
{
	while (tok->next) {
		if (is_keyword(tok->value) != -1) {
			parse_keyword(&tok);
		} else {
			//printf("unhandled token %s.\n", tok->value);
		}

		tok = tok->next;
	}
}

int main(int argc, char **argv)
{
	for (int i = 0; i < argc; i++) {
		if (!strncmp(argv[i], "-o", 2))
			output_path = &argv[i][2];
		else
			input_path = argv[i];
	}

	if (!input_path)
		fatal_error(-1, "Usage: bfm INPUT_PATH -oOUTPUT_PATH");

	char *src = load_file(input_path);
	Token *tok = tokenize(src);
	parse(tok);
	
	reset_emit();
	emit("++++++++++>+++++<");
	emit_algo(ALGO_DIV, 0, 1, -1);
	puts(output);

	while (tok->next) {
		printf("[%s][%s]\n", token_types[tok->type], tok->value);
		tok = tok->next;
	}
	for (int i = 0; i < num_variables; i++)
		printf("variable: %s\n", variable_names[i]);

	return 0;
}