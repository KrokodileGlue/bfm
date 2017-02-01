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

void* bfm_malloc(size_t bytes)
{
	void* res = malloc(bytes);
	
	if (!res)
		fatal_error(-1, "out of memory.");

	return res;
}

void* bfm_realloc(void* block, size_t bytes)
{
	block = realloc(block, bytes);
	
	if (!block)
		fatal_error(-1, "out of memory.");

	return block;
}

char* load_file(const char* path)
{
	char* buf = NULL;
	FILE* file = fopen(path, "r");
	
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

void save_file(FILE* file, char* str)
{
	if (!str)
		return;

	int index = 0;
	while (str[index] != '\0') {
		if (index % 80 == 0 && index != 0) {
			fputc('\n', file);
		}
		fputc(str[index], file);
		index++;
	}
}

#define MAX_ERRORS 128
typedef struct {
	int errloc;
	char* error;
} Error;
Error errors[MAX_ERRORS];

char *raw = NULL, *input_path = NULL, *output_path = NULL;
int num_errors;

void check_errors()
{
	if (num_errors) {
		list_errors();
		exit(EXIT_FAILURE);
	}
}

int get_line_num(int index)
{
	int counter = 0, i;
	for (i = 0; i < index; i++)
		if (raw[i] == '\n')
			counter++;

	return counter;
}

int get_column_num(int index)
{
	int counter = 0, i;
	for (i = 0; i < index; i++) {
		if (raw[i] == '\n') counter = 0;
		else counter++;
	}
	
	return counter;
}

int count_leading_whitespace(int index)
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

char* get_line_from_index(int index)
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

void errprint(char* str) /* when printing errors, the size of tabs can be an issue.
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

void print_location(int index)
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
__attribute__ ((noreturn)) void fatal_error(int errloc /* the location of the error */, const char* message, ...)
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

void push_error(int errloc /* the location of the error */, const char* message, ...)
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

void list_errors()
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

#define NUM_KEYWORDS 12
char keywords[NUM_KEYWORDS][15] = {
	"var",
	"while",
	"end",
	"point",
	"if",
	"not",
	"print",
	"array",
	"fuck",
	"define",
	"input",
	"write"
};

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
	KYWRD_DEFINE,
	KYWRD_INPUT,
	KYWRD_WRITE
};

int get_keyword(char* str)
{
	for (int i = 0; i < NUM_KEYWORDS; i++)
		if (!strcmp(str, keywords[i]))
			return i;
	return -1;
}

typedef struct struct_token {
	enum {
		TOK_IDENTIFIER, TOK_NUMBER,
		TOK_STRING, TOK_OPERATOR,
		TOK_SYMBOL, TOK_CHAR,
		TOK_KYWRD
	} type;
	
	int origin, data; /* the index of the token's original location in the source file */
	char* value; /* the body of the token */
	
	struct struct_token* next; /* all tokens are part of a linked list */
	struct struct_token* prev;
} Token;

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

int is_number(const char* str)
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

int is_hex_num(char* str)
{
#define HEX_CHARACTER(c) \
        ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || (c >= '0' && c <= '9'))

	for (int i = 0; i < (int)strlen(str); i++) {
		if (!HEX_CHARACTER(str[i]))
			return 0;
	}
	return 1;
}

char* parse_escape_characters(char* str, int location)
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
				case 'x':
					if (strlen(&str[i]) < 3)
						push_error(location + i, "malformed escape sequence.");

					char num[3];
					strncpy(num, &str[i + 1], 2);
					num[2] = '\0';

					if (!is_hex_num(num))
						push_error(location + i, "malformed escape sequence.");

					*c = (char)strtol(num, NULL, 16);
					i += 2;
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
	*c = '\0';
	
	free(str);
	return buf;
}

int get_operator_type(const char* str)
{
	int i;
	for (i = 0; i < NUM_OPERATORS; i++) {
		if (!strncmp(operators[i], str, strlen(operators[i])))
			return i;
	}
	
	return -1;
}

#ifdef DEBUG
char token_types[][11] = { /* for debugging */
	"IDENTIFIER",
	"NUMBER    ",
	"STRING    ",
	"OPERATOR  ",
	"SYMBOL    "
};
#endif

Token* tokenize(char* in)
{
	Token* current = NULL, *head = NULL, *prev = NULL;
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
		} else if (*start == '\'') {
			skip_char = 1;
			
			end++;
			start++;
			
			tok_type = TOK_CHAR;
			while (*end != '\'' && *end) {
				if (*end == '\n') {
					push_error(tok_origin, "unmatched ' character.");
					
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
				push_error(tok_origin, "unmatched ' character.");
				
				start += 1;
				end = start;
				tok_origin = start - in;
			}
		} else {
			tok_type = TOK_SYMBOL;
			end++;
		}
		
		if (!current)
			current = bfm_malloc(sizeof(Token));

		current->prev   = prev;
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
		
		if (current->type == TOK_IDENTIFIER) {
			if (is_number(current->value))
				current->type = TOK_NUMBER;

			if (get_keyword(current->value) != -1) {
				current->type = TOK_KYWRD;
				current->data = get_keyword(current->value);
			}
		}
		
		if (current->type == TOK_STRING || current->type == TOK_CHAR) {
			current->value = parse_escape_characters(current->value, current->origin);
		}

		if (current->type == TOK_NUMBER) {
			current->data = strtol(current->value, NULL, 0);
		}

		if (current->type == TOK_CHAR) {
			if (strlen(current->value) > 1) {
				push_error(tok_origin, "multi-character chars are not permitted.");
			}

			current->type = TOK_NUMBER;
			current->data = (int)(current->value[0]);
		}
		
		prev = current;
		current->next = bfm_malloc(sizeof(Token));
		current = current->next;
		
		if (skip_char) {
			end++;
			start++;
		}
	}
	
	if (current)
		free(current); 

	head = prev;
	if (head) {
		prev->next = NULL;

		while (head->prev) {
			head = head->prev;
		}
	}

	return head;
}

#define OUT_GROWTH_SPEED 4096
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
		output = bfm_realloc(output, out_allocated + bytes_needed + OUT_GROWTH_SPEED);
		out_allocated = out_allocated + bytes_needed + OUT_GROWTH_SPEED;
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

#define SKIP_COMMENTS(c)                      \
    do {                                      \
        while (!IS_BF_COMMAND(*c) && *c) c++; \
    } while (0);

void sanitize(char* str)
{
	if (!str)
		return;

#define IS_BF_COMMAND(c) (   \
        c == '<' || c == '>'     \
        || c == '+' || c == '-'  \
        || c == '[' || c == ']'  \
        || c == ',' || c == '.')

#define ADD(a, c) \
        if (a >= 0) { \
                for (int counter = 0; counter < abs(a); counter++) { \
                        *c++ = '+'; \
                } \
        } else { \
    	        for (int counter = 0; counter < abs(a); counter++) { \
                        *c++ = '-'; \
                } \
        }

#define MOVE_PTR(a, c) \
    if (a >= 0) { \
        for (int counter = 0; counter < abs(a); counter++) { \
            *c++ = '>'; \
        } \
    } else { \
    	for (int counter = 0; counter < abs(a); counter++) { \
            *c++ = '<'; \
        } \
    }

#define IS_CONTRACTABLE(c) (     \
        c == '<' || c == '>'     \
        || c == '+' || c == '-')

	char* buf = malloc(strlen(str) + 1);

	size_t starting_len;
	char* i, *out;

	starting_len = strlen(str);
	i = str, out = buf;

	while (*i != '\0') {
		if (IS_CONTRACTABLE(*i)) {
			if (*i == '+' || *i == '-') {
				int sum = 0;
				while (*i == '+' || *i == '-') {
					if (*i == '+') sum++;
					else sum--;
					i++;
				}
				ADD(sum, out)
			} else {
				int sum = 0;
				while (*i == '>' || *i == '<') {
					if (*i == '>') sum++;
					else sum--;
					i++;
				}
				MOVE_PTR(sum, out)
			}
		} else if (!strncmp(i, "][", 2)) {
			i += 2;
			int depth = 1;
			while (*i != '\0' && depth) {
				if (*i == '[') depth++;
				else if (*i == ']') depth--;
				i++;
			}
			i--;
		} else if (IS_BF_COMMAND(*i)) {
			*out++ = *i++;
		} else {
			i++;
		}
	}
	*out = '\0';
	strcpy(str, buf);
	free(buf);

	if (strlen(str) < starting_len) {
		sanitize(str);
	}
}

#define NUM_TEMP_CELLS 10
int cell_pointer = 0, temp_cells = 0, temp_x = 0, temp_x_index = 0, temp_y = 0, temp_y_index = 0,
	arrays = 0, if_cell;

int count_variables(Token* tok)
{
	int res = 0;

	while (tok) {
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
	"0<[-]>0[-]1[-]2[-]3[-]4[-]5[-]x[0+x-]y[1+2+y-]2[y+2-]0[>->+<[>]>[<+>-]<<[<]>-]2[x+2-]x", /* modulus */
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

void emit_write_string(const char* str)
{
	int i = 0;
	while (str[i] != '\0') {
		emit("[-]"), add(str[i]), emit(">");
		i++;
	}

	for (i = 0; i < (int)strlen(str); i++) {
		emit("<");
	}
}

#define SYNTAX_ASSERT(err_cond, err_str)                  \
	do {                                              \
		if (err_cond) {                           \
			push_error(tok->origin, err_str); \
			*token = tok;                     \
			return;                           \
		}                                         \
	} while(0);

typedef struct {
	char* name;
	int location, scope, num_elements;
	enum {
		VAR_CELL, VAR_ARRAY
	} type;
} Variable;
Variable variables[4096];
int num_variables = 0, scope = 0, used_array_cells = 0, used_variable_cells = 0;

int get_variable_index(char* varname)
{
	for (int i = 0; i < num_variables; i++) {
		if (!strcmp(variables[i].name, varname))
			return i;
	}
	return -1;
}

void add_variable(char* varname, int num_elements, int type, int location)
{
	if (get_variable_index(varname) != -1) {
		push_error(location, "variable already defined.");
		return;
	}

	if (type == VAR_CELL) {
		variables[num_variables].location = used_variable_cells;
		used_variable_cells++;
	} else {
		variables[num_variables].location = arrays + used_array_cells;
		used_array_cells += num_elements + 5;
	}

	variables[num_variables].scope = scope;
	variables[num_variables].type = type;
	variables[num_variables].num_elements = num_elements;
	variables[num_variables++].name = varname;
}

void kill_variables_of_scope(int killscope)
{
	for (int i = num_variables - 1; i >= 0; i--) {
		if (variables[i].scope == killscope) {
			num_variables--;
		}
	}
}

typedef struct {
	char* name;
	Token tok;
} Definition;
Definition definitions[4096];
int num_definitions = 0;

void add_definition(char* name, Token tok)
{
	definitions[num_definitions].name = name;
	definitions[num_definitions++].tok = tok;
}

int get_definition_index(char* name)
{
	for (int i = 0; i < num_definitions; i++) {
		if (!strcmp(definitions[i].name, name))
			return i;
	}
	return -1;
}

#define NEXT_TOKEN(t)                                                               \
	do {                                                                        \
		if (!(t->next)) {                                                   \
			push_error(t->origin, "expected a valid token to follow."); \
			return;                                                     \
		} else t = t->next;                                                 \
	} while (0);

#define EXPECT_TOKEN(t,str)                                                     \
	do {                                                                    \
		NEXT_TOKEN(t)                                                   \
		if (strcmp(t->value,str)) {                                     \
			push_error(t->origin, "expected token \"%s\".\n", str); \
		}                                                               \
	} while (0);

void parse_lefthand_side(Token** token, int* left, int* left_index, int* array)
{
	Token* tok = *token;

	*left_index = get_variable_index(tok->value);
	SYNTAX_ASSERT(*left_index == -1, "expected an a variable identifier.")

	if (variables[*left_index].type == VAR_ARRAY) {
		*array = 1;
		EXPECT_TOKEN(tok, "[")
		NEXT_TOKEN(tok)

		Token* parse_tok = tok;
		int definition_index = get_definition_index(tok->value);
		if (definition_index != -1) {
			parse_tok = &(definitions[definition_index].tok);
		}

		/* now get the index value, we must account for variables and constants */
		if (parse_tok->type == TOK_NUMBER) {
			move_pointer_to(temp_x_index);
			emit("[-]");
			add(parse_tok->data);
		} else if (parse_tok->type == TOK_IDENTIFIER) {
			int subscript_index = get_variable_index(parse_tok->value);

			SYNTAX_ASSERT(subscript_index == -1, "unrecognized identifier.")
			
			emit_algo(ALGO_EQU, temp_x_index, variables[subscript_index].location, -1);
		} else {
			SYNTAX_ASSERT(1, "expected a valid subscript.")
		}

		EXPECT_TOKEN(tok, "]")

		emit_algo(ALGO_ARRAY_READ, temp_x, variables[*left_index].location, temp_x_index); /* x = y(z) */
		*left = temp_x;
	} else {
		*left = variables[*left_index].location;
	}

	*token = tok;
}

void parse_operation(Token** token)
{
	Token* tok = *token;

	int left = 0, operation = 0,
	        right = 0,
	        array = 0; /* if the lefthand side is an array, we need to ferry
                            * the new value to the original location. */

	int left_index = 0;
	parse_lefthand_side(&tok, &left, &left_index, &array);

	NEXT_TOKEN(tok)

	operation = get_operator_type(tok->value);

	int algo;
	switch (operation) {
		case MOP_EQU:    algo = ALGO_EQU;  break;
		case MOP_MOD:    algo = ALGO_MOD;  break;
		case MOP_EQUEQU: algo = ALGO_CEQU; break;
		case MOP_ADD:    algo = ALGO_ADD;  break;
		case MOP_SUB:    algo = ALGO_SUB;  break;
		case MOP_OROR:   algo = ALGO_OR;   break;
		case MOP_DIV:    algo = ALGO_DIV;  break;
		case MOP_MUL:    algo = ALGO_MUL;  break;
		default:
			push_error(tok->origin, "unrecognized operator.");
			return;
	}

#define FERRY_ARRAY_BACK                                                                           \
	if (array) {                                                                               \
		/* x(y) = z (array write) */                                                       \
		emit_algo(ALGO_ARRAY_WRITE, variables[left_index].location, temp_x_index, temp_x); \
	}

	NEXT_TOKEN(tok)

	Token* parse_tok = tok;
	int definition_index = get_definition_index(tok->value);
	if (definition_index != -1) {
		parse_tok = &(definitions[definition_index].tok);
	}

	int right_index = get_variable_index(tok->value);

	if (right_index != -1) {
		if (variables[right_index].type == VAR_ARRAY) {
			EXPECT_TOKEN(tok, "[")
			NEXT_TOKEN(tok)

			parse_tok = tok;
			definition_index = get_definition_index(tok->value);
			if (definition_index != -1) {
				parse_tok = &(definitions[definition_index].tok);
			}

			/* now get the index value, we must account for variables and constants */
			if (parse_tok->type == TOK_NUMBER) {
				move_pointer_to(temp_y_index);
				emit("[-]");
				add(parse_tok->data);
			} else if (parse_tok->type == TOK_IDENTIFIER) {
				int subscript_index = get_variable_index(parse_tok->value);
				SYNTAX_ASSERT(subscript_index == -1, "unrecognized identifier.")

				emit_algo(ALGO_EQU, temp_y_index, variables[subscript_index].location, -1);
			} else {
				SYNTAX_ASSERT(1, "not a valid subscript.")
			}

			EXPECT_TOKEN(tok, "]")

			emit_algo(ALGO_ARRAY_READ, temp_y, variables[right_index].location, temp_y_index); /* x = y(z) */
			right = temp_y;
		} else {
			right = variables[right_index].location;
		}
	} else {
		SYNTAX_ASSERT(parse_tok->type != TOK_NUMBER, "invalid identifier.")

		if (operation == MOP_ADD) {
			move_pointer_to(left);
			add(parse_tok->data);
			*token = tok;
			FERRY_ARRAY_BACK
			return;
		} else if (operation == MOP_SUB) {
			move_pointer_to(left);
			add(-parse_tok->data);
			*token = tok;
			FERRY_ARRAY_BACK
			return;
		} else if (operation == MOP_EQU) {
			move_pointer_to(left);
			emit("[-]");
			add(parse_tok->data);
			*token = tok;
			FERRY_ARRAY_BACK
			return;
		} else {
			move_pointer_to(temp_y);
			emit("[-]");
			add(parse_tok->data);
			right = temp_y;
		}
	}

	emit_algo(algo, left, right, -1);

	FERRY_ARRAY_BACK

	*token = tok;
}

enum {
	STACK_WHILE,
	STACK_IF
};

int stack[4096], stack_ptr = 0;

void parse_keyword(Token** token)
{
	Token* tok = *token;

	switch (tok->data) {
		case KYWRD_VAR:
			NEXT_TOKEN(tok)

			SYNTAX_ASSERT(tok->type != TOK_IDENTIFIER, "expected an identifier.")
			SYNTAX_ASSERT(get_keyword(tok->value) != -1, "variable names must not be keywords.")

			add_variable(tok->value, -1, VAR_CELL, tok->origin);
			break;
		case KYWRD_WHILE: {
			NEXT_TOKEN(tok)

			int var_index = get_variable_index(tok->value);
			
			SYNTAX_ASSERT(var_index == -1, "invalid identifier.")

			int variable_location = variables[var_index].location;
			move_pointer_to(variable_location);
			emit("[");

			stack[stack_ptr++] = variable_location;
			stack[stack_ptr++] = STACK_WHILE;

			scope++;
			
		} break;
		case KYWRD_END: {
			SYNTAX_ASSERT(stack_ptr < 1, "unmatched end statement.")

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

			kill_variables_of_scope(scope--);
		} break;
		case KYWRD_GOTO: {
			NEXT_TOKEN(tok)

			int var_index = get_variable_index(tok->value);

			SYNTAX_ASSERT(var_index == -1, "invalid identifier.")

			int variable_location = variables[var_index].location;
			move_pointer_to(variable_location);
		}  break;
		case KYWRD_IF: {
			NEXT_TOKEN(tok)

			int var_index = get_variable_index(tok->value);
			SYNTAX_ASSERT(var_index == -1, "invalid identifier.")

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
			SYNTAX_ASSERT(var_index == -1, "invalid identifier.")

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
					c[0] = (char)tok->data;
					
					emit_print_string(c);
					break;
				}
				case TOK_IDENTIFIER: {
					int var_index = get_variable_index(tok->value);
					int left = 0;

					SYNTAX_ASSERT(var_index == -1, "invalid identifier.")

					if (variables[var_index].type == VAR_ARRAY) {
						EXPECT_TOKEN(tok, "[")
						NEXT_TOKEN(tok)

						if (tok->type == TOK_NUMBER) {
							move_pointer_to(temp_x_index);
							emit("[-]");
							add(tok->data);
						} else if (tok->type == TOK_IDENTIFIER) {
							int subscript_index = get_variable_index(tok->value);

							SYNTAX_ASSERT(subscript_index == -1, "unrecognized identifier.")
							emit_algo(ALGO_EQU, temp_x_index, variables[subscript_index].location, -1);
						} else {
							SYNTAX_ASSERT(1, "expected a number or an identifier.")
						}

						EXPECT_TOKEN(tok, "]")

						emit_algo(ALGO_ARRAY_READ, temp_x, variables[var_index].location, temp_x_index); /* x = y(z) */
						left = temp_x;
					} else {
						left = variables[var_index].location;
					}

					emit_algo(ALGO_PRINTV, left, -1, -1);
					break;
				}
				default:
					push_error(-1, "unexpected token \"%s\", expected a string, number, or identifier.", tok->value);
			} break;
		case KYWRD_ARRAY: {
			NEXT_TOKEN(tok)

			char* name = tok->value;
			NEXT_TOKEN(tok)

			Token* parse_tok = tok;
			
			int definition_index = get_definition_index(tok->value);
			if (definition_index != -1) {
				parse_tok = &(definitions[definition_index].tok);
			}

			SYNTAX_ASSERT(parse_tok->type != TOK_NUMBER, "expected a number or constant identifier.")
			add_variable(name, parse_tok->data, VAR_ARRAY, tok->origin);
		} break;
		case KYWRD_BF: {
			NEXT_TOKEN(tok)

			SYNTAX_ASSERT(tok->type != TOK_STRING, "expected a string literal.")
			
			emit(tok->value);
		} break;
		case KYWRD_DEFINE: {
			NEXT_TOKEN(tok)

			char* name = tok->value;
			NEXT_TOKEN(tok)

			add_definition(name, *tok);
		} break;
		case KYWRD_INPUT: {
			NEXT_TOKEN(tok)

			int var_index = get_variable_index(tok->value);

			SYNTAX_ASSERT(var_index == -1, "invalid identifier.")
			
			move_pointer_to(variables[var_index].location);
			emit(",");
		} break;
		case KYWRD_WRITE: {
			NEXT_TOKEN(tok)
			SYNTAX_ASSERT(tok->type != TOK_STRING, "expected a string.")

			emit_write_string(tok->value);
		} break;
	}

	*token = tok;
}

void parse(Token* tok)
{
	while (tok) {
		if (tok->type == TOK_KYWRD) {
			parse_keyword(&tok);
			tok = tok->next;
		} else if (tok->type == TOK_IDENTIFIER && get_variable_index(tok->value) != -1) {
			parse_operation(&tok);
			tok = tok->next;
		} else {
			push_error(tok->origin, "unexpected token.", tok->value);
			tok = tok->next;
		}
	}

	check_errors();
}

void delete_list(Token* tok)
{
	Token* current = tok;
	Token* next = NULL, *prev = NULL;
	while (current) {
		prev = current;
		next = current->next;

		free(current->value);
		if (current->prev)
			free(current->prev);

		current = next;
	}

	if (prev)
		free(prev);
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

	raw = load_file(input_path);

	if (!raw)
		fatal_error(-1, "Usage: bfm INPUT_PATH -oOUTPUT_PATH");

	Token *tok = tokenize(raw);
	check_errors();

#if 0
	puts("\nCOMPLETE TOKEN LISTING:");
	Token* ttok = tok;
	while (ttok) {
		printf("\n[%s][%s]", token_types[ttok->type], ttok->value);
		ttok = ttok->next;
	}
	puts("\nFINISHED COMPLETE TOKEN LISTING.");
#endif

	temp_cells   = count_variables(tok) + 4;
	temp_x       = temp_cells - 4,   temp_y = temp_cells - 2;
	temp_x_index = temp_x + 1,       temp_y_index = temp_y + 1;
	arrays = temp_cells + NUM_TEMP_CELLS;

	parse(tok);
	free(raw);
	delete_list(tok);

	sanitize(output);

	FILE* output_file = stdout;
	save_file(output_file, output);
	//fclose(output_file);
	free(output);

	return 0;
}