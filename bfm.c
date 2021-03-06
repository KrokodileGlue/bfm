#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

void check_errors();

int verbose = 0;
void init_errors(char*, char*);
void fatal_error(int, const char*, ...);
int get_line_num(int);
int get_column_num(int index);
int count_leading_whitespace(int);
char* get_line_from_index(int);
void errprint(char*);
void print_location(int);
void push_error(int errloc, int is_suppressable, int is_fatal, const char* message, ...);
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

	if (!file) {
		fatal_error(-1, "could not open output file.");
		return;
	}

	int index = 0;
	while (str[index] != '\0') {
		if (index % 80 == 0 && index != 0) {
			fputc('\n', file);
		}
		fputc(str[index], file);
		index++;
	}
}

typedef struct {
	int errloc, is_suppressable, is_fatal;
	char* error;
} Error;
Error* errors = NULL;

char *raw = NULL, *input_path = NULL, *output_path = NULL;
int num_errors;

void check_errors()
{
	if (num_errors) {
		list_errors();
		for (int i = 0; i < num_errors; i++) {
			if (errors[i].is_fatal) {
				exit(EXIT_FAILURE);
			}
		}
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

__attribute__ ((noreturn))
void fatal_error(int errloc /* the location of the error */, const char* message, ...)
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

void push_error(int errloc /* the location of the error */, int is_suppressable, int is_fatal, const char* message, ...)
{
	if (!errors) {
		errors = bfm_malloc(sizeof(Error));
	} else {
		errors = bfm_realloc(errors, (num_errors + 1) * sizeof(Error));
	}

	errors[num_errors].errloc = errloc;
	errors[num_errors].is_suppressable = is_suppressable;
	errors[num_errors].is_fatal = is_fatal;
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
	int i, suppressed_count = 0;
	for (i = 0; i < num_errors; i++) {
		if (!verbose) {
			if (i > 0 && get_line_num(errors[i].errloc) == get_line_num(errors[i - 1].errloc) && errors[i].is_suppressable) {
				suppressed_count++;
				continue; /* only print one error for every line */
			}
		}
		int errloc = errors[i].errloc;
		char* error = errors[i].error;
		
		if (errloc >= 0) {
			printf("%s:%d:%d: ", input_path,
				get_line_num(errloc) + 1,
				get_column_num(errloc) + 1);
		}
		
		if (errors[i].is_fatal) {
			printf("error: ");
		} else {
			printf("warning: ");
		}

		errprint(error);
		putchar('\n');
		
		if (errloc >= 0) /* if errloc is positive we will print out the line that caused the error */
			print_location(errloc);
	}

	if (!verbose)
		printf("\tnote: %d warning(s) suppressed.\n", suppressed_count);
}

#define NUM_KEYWORDS 14
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
	"write",
	"decimal",
	"macro"
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
	KYWRD_WRITE,
	KYWRD_DECIM,
	KYWRD_MACRO
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
	MOP_MUL   , MOP_DIV   ,
	MOP_LBRACE, MOP_RBRACE,
	MOP_SEMICOLON,
	MOP_LBRACK, MOP_RBRACK,
	MOP_COMMA
};

#define NUM_OPERATORS 28
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
	"*" , "/",
	"(" , ")",
	";" , "[",
	"]" , ","
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

int parse_escape_characters(char* str, int location)
{
	char* buf = bfm_malloc(strlen(str) + 1);
	
	int i = 0;
	char* c = buf;
	while (str[i]) {
		if (str[i] == '\\') {
			i++;
			
			switch (str[i]) {
				case 't': *c = '\t'; break;
				case 'n': *c = '\n'; break;
				case 'b': *c = '\b'; break;
				case 'f': *c = '\f'; break;
				case 'r': *c = '\r'; break;
				case 'x':
					if (strlen(&str[i]) < 3)
						push_error(location + i, 1, 1, "malformed escape sequence.");

					char num[3];
					strncpy(num, &str[i + 1], 2);
					num[2] = '\0';

					if (!is_hex_num(num))
						push_error(location + i, 1, 1, "malformed escape sequence.");

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
	memcpy(str, buf, (c - buf) + 1);
	free(buf);

	return (int)(c - buf);
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
char token_types[][11] = {
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
			push_error(tok_origin, 0, 1, "comment terminator has no intializer.");
			
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
				push_error(tok_origin, 0, 1, "unterminated comment.");
				
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
					push_error(tok_origin, 0, 1, "unmatched \" character.");
					
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
				push_error(tok_origin, 0, 1, "unmatched \" character.");
				
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
					push_error(tok_origin, 0, 1, "unmatched ' character.");
					
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
				push_error(tok_origin, 0, 1, "unmatched ' character.");
				
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
			if (is_number(current->value)) {
				current->type = TOK_NUMBER;
			} else if (get_keyword(current->value) != -1) {
				current->type = TOK_KYWRD;
				current->data = get_keyword(current->value);
			}
		}
		
		if (current->type == TOK_OPERATOR) {
			current->data = get_operator_type(current->value);
		}

		if (current->type == TOK_STRING || current->type == TOK_CHAR) {
			current->data = parse_escape_characters(current->value, current->origin);
		}

		if (current->type == TOK_NUMBER) {
			current->data = strtol(current->value, NULL, 0);
		}

		if (current->type == TOK_CHAR) {
			if (strlen(current->value) > 1) {
				push_error(tok_origin, 0, 1, "multi-character chars are not permitted.");
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

	char* buf = bfm_malloc(strlen(str) + 1);

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

#define NUM_TEMP_CELLS 7
int cell_pointer = 0, temp_cells = 0, temp_x = 0, temp_x_index = 0, temp_y = 0, temp_y_index = 0,
	arrays = 0, if_cell;

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
	ALGO_OR, ALGO_DECIM,
	ALGO_AND, ALGO_LESS
};

#define NUM_ALGORITHMS 16
char algorithms[NUM_ALGORITHMS][256] = {
	"0[-]1[-]2[-]3[-]x[0+x-]0[y[1+2+y-]2[y+2-]1[2+0-[2[-]3+0-]3[0+3-]2[1-[x-1[-]]+2-]1-]x+0]", /* x / y */
	"0[-]1[-]x[1+x-]1[y[x+0+y-]0[y+0-]1-]", /* x * y */
	"0[-]y[x+0+y-]0[y+0-]", /* x + y */
	"0[-]y[x-0+y-]0[y+0-]", /* x - y */
	"0[-]x[-]y[x+0+y-]0[y+0-]", /* x = y */
	"1[-]2[-]3[-]4[-]5[-]6[-]x[1+x-]y[2+3+y-]3[y+3-]1[>->+<[>]>[<+>-]<<[<]>-]3[x+3-]x", /* x % y */
	"0[-]1[-]2[-]5[-]6[-]y[5+6+y-]6[y+6-]x[0+y[-0[-]1+y]0[-2+0]1[-y+1]y-x-]2[x+2-]y[-]5[y+5-]", /* x > y */
	"0[-]x[0+x[-]]+0[x-0-]", /* logical not */
	"0[-]1[-]x[1+x-]+y[1-0+y-]0[y+0-]1[x-1[-]]", /* x == y */
	"z[-x+x>>>+<<<z]x[-z+x]y[-x+x>+<y]x[-y+x]y[-x+x>>+<<y]x[-y+x]>[>>>[-<<<<+>>>>]<[->+<]<[->+<]<[->+<]>-]>>>[-]<[->+<]<[[-<+>]<<<[->>>>+<<<<]>>-]<<", /* x(y) = z (array write) */
	"z[-y+y>+<z]y[-z+y]z[-y+y>>+<<z]y[-z+y]>[>>>[-<<<<+>>>>]<<[->+<]<[->+<]>-]>>>[-<+<<+>>>]<<<[->>>+<<<]>[[-<+>]>[-<+>]<<<<[->>>>+<<<<]>>-]<<x[-]y>>>[-<<<x+y>>>]<<<", /* x = y(z) (array read) */
	"0[-]1[-]2[-]3[-]4[-]5[-]6[-]7[-]x[0+1+x-]1[x+1-]0[>>+>+<<<-]>>>[<<<+>>>-]<<+>[<->[>++++++++++<[->-[>+>>]>[+[-<+>]>+>>]<<<<<]>[-]++++++++[<++++++>-]>[<<+>>-]>[<<+>>-]<<]>]<[->>++++++++[<++++++>-]]<[.[-]<]<", /* printv */
	"0[-]1[-]x[1+x-]1[x-1[-]]y[1+0+y-]0[y+0-]1[x[-]-1[-]]", /* logical or */
	"0[-]>[-]+[[-]>[-],[+[-----------[>[-]++++++[<------>-]<--<<[->>++++++++++<<]>>[-<<+>>]<+>]]]<]<0[x+0-]", /* decimal input */
	"0[-]1[-]x[1+x-]1[1[-]y[1+0+y-]0[y+0-]1[x+1[-]]]", /* logical and */
	
	// this algoritm is from http://stackoverflow.com/a/13327857
	// it fails if x is 255, it should be replaced
	"0[-]1[-]+2[-]3[-]x[3+x-]4[-]5[-]y[4+5+y-]5[y+5-]3+>+<[->-[>]<<]<[-]<[-<>>>x+0]", /* x < y */
};

void emit_algo(int algo, int x, int y, int z)
{
	int i = 0;
	while (algorithms[algo][i] != '\0') {
		if (IS_BF_COMMAND(algorithms[algo][i])) {
			emit_char(algorithms[algo][i]);
		} else {
			switch (algorithms[algo][i]) {
				case 'x': move_pointer_to(x); break;
				case 'y': move_pointer_to(y); break;
				case 'z': move_pointer_to(z); break;
				default: move_pointer_to(temp_cells + (algorithms[algo][i] - '0')); break;
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

void emit_print_string(Token* tok)
{
	move_pointer_to(temp_cells);
	emit("[-]"), add(tok->value[0]), emit(".");
	
	for (int i = 1; i < tok->data; i++) {
		add(tok->value[i] - tok->value[i - 1]);
		emit(".");
	}
}

void emit_write_string(Token* tok)
{
	emit("[-]"), add(tok->value[0]);

	if (tok->data) {
		int i;
		for (i = 1; i < tok->data - 1; i++) {
			emit(">[-]>[-]<<[>+>+<<-]>>[<<+>>-]<"), add(tok->value[i] - tok->value[i - 1]);
		}
		emit(">[-]"), add(tok->value[i]);

		for (i = 0; i < tok->data - 1; i++) {
			emit("<");
		}
	}
}

#define SYNTAX_ASSERT(err_cond, err_str)                     \
	do {                                                 \
		if (err_cond) {                              \
			push_error(tok->origin, 1, 1, err_str); \
			*token = tok;                        \
			return;                              \
		}                                            \
	} while(0);

typedef struct {
	char* name;
	int data;
} Definition;
Definition definitions[4096];
int num_definitions = 0;

void add_definition(char* name, int data)
{
	definitions[num_definitions].name = name;
	definitions[num_definitions++].data = data;
}

int get_definition_index(char* name)
{
	for (int i = 0; i < num_definitions; i++) {
		if (!strcmp(definitions[i].name, name))
			return i;
	}
	return -1;
}

typedef struct {
	char* name;
	char** args;
	int num_args, origin, *origins /* the locations of the arguments */;
	Token* body;
} Macro;
Macro macros[4096];
int num_macros = 0;

void add_macro(char* name, char** args, int num_args, Token* body, int origin, int* origins)
{
	macros[num_macros].name = name;
	macros[num_macros].args = args;
	macros[num_macros].num_args = num_args;
	macros[num_macros].body = body;
	macros[num_macros].origin = origin;
	macros[num_macros].origins = origins;
	num_macros++;
}

int get_macro_index(char* name)
{
	for (int i = 0; i < num_macros; i++) {
		if (!strcmp(macros[i].name, name))
			return i;
	}
	return -1;
}

typedef struct {
	char* name;
	int location, scope, num_elements, ctx, used, origin;
	enum {
		VAR_CELL, VAR_ARRAY
	} type;
} Variable;
Variable variables[4096];
int num_variables = 0,
	scope = 0, used_array_cells = 0,
	used_variable_cells = 0,
	context = 0;

int get_variable_index(char* varname)
{
	for (int i = 0; i < num_variables; i++) {
		if (!strcmp(variables[i].name, varname) && (variables[i].ctx == context || variables[i].ctx == -1)) {
			variables[i].used = 1;
			return i;
		}
	}
	return -1;
}

void add_variable(char* varname, int num_elements, int type, int location, int ctx, int origin, int var_scope)
{
	if (get_variable_index(varname) != -1 && context == ctx) {
		push_error(origin, 0, 1, "variable already defined.");
	}

	if (get_definition_index(varname) != -1) {
		push_error(origin, 0, 1, "variable name conflicts with a constant definition.");
	}

	if (type == VAR_CELL) {
		variables[num_variables].location = location;
	} else {
		variables[num_variables].location = location;
		used_array_cells += num_elements + 5;
	}

	variables[num_variables].scope = scope;
	variables[num_variables].type = type;
	variables[num_variables].num_elements = num_elements;
	variables[num_variables].ctx = ctx;
	variables[num_variables].used = 0;
	variables[num_variables].origin = origin;
	variables[num_variables].scope = var_scope;
	variables[num_variables++].name = varname;
}

void kill_variables_of_context(int killcontext)
{
	for (int i = 0; i < num_variables; i++) {
		if (variables[i].used == 0 && variables[i].ctx == killcontext) {
			push_error(variables[i].origin, 0, 0, "unused variable %s.", variables[i].name);
		}
	}
	for (int i = num_variables - 1; i >= 0; i--) {
		if (variables[i].ctx == killcontext) {
			// printf("killing variable %s\n", variables[i].name);
			num_variables--;
		}
	}
}

void kill_variables_of_scope(int killscope)
{
	for (int i = 0; i < num_variables; i++) {
		if (variables[i].used == 0 && variables[i].scope == killscope) {
			push_error(variables[i].origin, 0, 0, "unused variable %s.", variables[i].name);
		}
	}
	for (int i = num_variables - 1; i >= 0; i--) {
		if (variables[i].scope == killscope) {
			// printf("killing variable %s\n", variables[i].name);
			num_variables--;
			if (variables[i].type == VAR_CELL) {
				used_variable_cells--;
			} else {
				used_array_cells -= variables[i].num_elements + 5;
			}
		}
	}
}

/* TODO: clean up these macros */
#define NEXT_TOKEN(t)                                                               \
	do {                                                                        \
		if (!(t->next)) {                                                   \
			push_error(t->origin, 1, 1, "expected a valid token to follow."); \
			return;                                                     \
		} else t = t->next;                                                 \
	} while (0);

#define EXPECT_TOKEN(t, ttype, str)                                                                          \
	do {                                                                                                 \
		NEXT_TOKEN(t)                                                                                \
		if (strcmp(t->value,str) || t->type != ttype) {                                              \
			push_error(t->origin, 1, 1, "unexpected token \"%s\", expected \"%s\".\n", t->value, str); \
		}                                                                                            \
	} while (0);

#define PARSE_EXPECT_TOKEN(t, ttype, str)                                                                          \
	do {                                                                                                 \
		PARSE_NEXT_TOKEN(t)                                                                                \
		if (strcmp(t->value,str) || t->type != ttype) {                                              \
			push_error(t->origin, 1, 1, "unexpected token \"%s\", expected \"%s\".\n", t->value, str); \
		}                                                                                            \
	} while (0);

#define PARSE_SYNTAX_ASSERT(err_cond, err_str)            \
	do {                                              \
		if (err_cond) {                           \
			push_error(tok->origin, 1, 1, err_str); \
			*token = tok;                     \
			return - 1;                       \
		}                                         \
	} while(0);

#define PARSE_NEXT_TOKEN(t)                                                         \
	do {                                                                        \
		if (!(t->next)) {                                                   \
			push_error(t->origin, 1, 1, "expected a valid token to follow."); \
			return -1;                                                  \
		} else t = t->next;                                                 \
	} while (0);

double term(Token** token);
double expression(Token** token);
double primary(Token** token)
{
	Token* tok = *token;

	int data = tok->data, type = tok->type;

	int def_idx = get_definition_index(tok->value);
	if (type == TOK_IDENTIFIER && def_idx != -1) {
		data = definitions[def_idx].data;
		type = TOK_NUMBER;
	}

	if (type == TOK_OPERATOR && data == MOP_LBRACE) {
		PARSE_NEXT_TOKEN(tok)
		double d = expression(&tok);
		PARSE_NEXT_TOKEN(tok)

		/* TODO: replace this with EXPECT_TOKEN */
		PARSE_SYNTAX_ASSERT(tok->type != TOK_OPERATOR, "unmatched lbrace.")
		PARSE_SYNTAX_ASSERT(tok->data != MOP_RBRACE, "unmatched lbrace.")

		*token = tok;
		return d;
	} else if (type == TOK_NUMBER) {
		return (double)data;
	}

	PARSE_SYNTAX_ASSERT(1, "unexpected token, expected a number or operator.");
}

double term(Token** token)
{
	Token* tok = *token;
	double left = primary(&tok);
	PARSE_NEXT_TOKEN(tok)

	while (1) {
		if (tok->type == TOK_OPERATOR) {
			switch (tok->data) {
				case MOP_MUL:
					PARSE_NEXT_TOKEN(tok)
					left *= primary(&tok);
					PARSE_NEXT_TOKEN(tok)
					break;
				case MOP_DIV:
					PARSE_NEXT_TOKEN(tok)
					left /= primary(&tok);
					PARSE_NEXT_TOKEN(tok)
					break;
				default:
					*token = tok->prev;
					return left;
			}
		} else {
			*token = tok->prev;
			return left;
		}
	}
}

double expression(Token** token)
{
	Token* tok = *token;

	double left = term(&tok);
	PARSE_NEXT_TOKEN(tok)

	while (1) {
		if (tok->type == TOK_OPERATOR) {
			if (tok->data == MOP_ADD) {
				PARSE_NEXT_TOKEN(tok)
				left += term(&tok);
				PARSE_NEXT_TOKEN(tok)
			} else if (tok->data == MOP_SUB) {
				PARSE_NEXT_TOKEN(tok)
				left -= term(&tok);
				PARSE_NEXT_TOKEN(tok)
			} else {
				*token = tok->prev;
				return left;
			}
		} else {
			*token = tok->prev;
			return left;
		}
	}
}

typedef struct {
	char* code;
	int cells_required;
} BFConstant;
BFConstant bf_constants[256] = {
/* 0   */	{ "",                     1 },
/* 1   */	{ "+",                    1 },
/* 2   */	{ "++",                   1 },
/* 3   */	{ "+++",                  1 },
/* 4   */	{ "++++",                 1 },
/* 5   */	{ "+++++",                1 },
/* 6   */	{ "++++++",               1 },
/* 7   */	{ "+++++++",              1 },
/* 8   */	{ "++++++++",             1 },
/* 9   */	{ "+++++++++",            1 },
/* 10  */	{ "++++++++++",           1 },
/* 11  */	{ "+++++++++++",          1 },
/* 12  */	{ "++++++++++++",         1 },
/* 13  */	{ "+++++++++++++",        1 },
/* 14  */	{ "++++++++++++++",       1 },
/* 15  */	{ "+++[>+++++<-]>",       2 },
/* 16  */	{ "++++[>++++<-]>",       2 },
/* 17  */	{ "++++[>++++<-]>+",      2 },
/* 18  */	{ "+++[>++++++<-]>",      2 },
/* 19  */	{ "+++[>++++++<-]>+",     2 },
/* 20  */	{ "++++[>+++++<-]>",      2 },
/* 21  */	{ "-[+[-<]>>++++]<",      6 },
/* 22  */	{ ">++[++[<]>->+]<",      6 },
/* 23  */	{ ">-[++++[<]>->+]<",     5 },
/* 24  */	{ "++++[>++++++<-]>",     2 },
/* 25  */	{ "+++++[>+++++<-]>",     2 },
/* 26  */	{ ">++[[+<]>+>++]<-",     5 },
/* 27  */	{ ">++[[+<]>+>++]<",      5 },
/* 28  */	{ ">+[--[>+<<]>-]>",      4 },
/* 29  */	{ ">+[--[<]>>+<-]>",      4 },
/* 30  */	{ ">+[--[<]>>+<-]>+",     4 },
/* 31  */	{ ">-[-[-<]>>+<]>--",     5 },
/* 32  */	{ ">-[-[-<]>>+<]>-",      5 },
/* 33  */	{ ">-[-[-<]>>+<]>",       5 },
/* 34  */	{ ">-[-[-<]>>+<]>+",      5 },
/* 35  */	{ ">+[++<[-<]>>+]<",      6 },
/* 36  */	{ "+[->-[---<]>-]>",      6 },
/* 37  */	{ "---[>+<+++++++]>",     2 },
/* 38  */	{ ">--[>+<--[-<]>]>",     5 },
/* 39  */	{ ">+[--[-<]>>+<]>",      5 },
/* 40  */	{ ">+[--[-<]>>+<]>+",     5 },
/* 41  */	{ "--[>+<++++++]>--",     2 },
/* 42  */	{ "--[>+<++++++]>-",      2 },
/* 43  */	{ "--[>+<++++++]>",       2 },
/* 44  */	{ "--[>+<++++++]>+",      2 },
/* 45  */	{ "-[+>+[+<]>+]>-",       5 },
/* 46  */	{ "-[+>+[+<]>+]>",        5 },
/* 47  */	{ "+[-[--<]>>--]<",       4 },
/* 48  */	{ "-[>+<-----]>---",      2 },
/* 49  */	{ "-[>+<-----]>--",       2 },
/* 50  */	{ "-[>+<-----]>-",        2 },
/* 51  */	{ "-[>+<-----]>",         2 },
/* 52  */	{ "-[>+<-----]>+",        2 },
/* 53  */	{ "-[>+<-----]>++",       2 },
/* 54  */	{ "-[>+<-----]>+++",      2 },
/* 55  */	{ ">-[++>+[+<]>]>-",      4 },
/* 56  */	{ ">-[++>+[+<]>]>",       5 },
/* 57  */	{ ">-[++>+[+<]>]>+",      4 },
/* 58  */	{ ">-[++[+<]>>+<]>",      4 },
/* 59  */	{ ">-[++[+<]>>+<]>+",     4 },
/* 60  */	{ "-[+>+[+++<]>-]>",      4 },
/* 61  */	{ "+[-[>+<<]>-]>--",      4 },
/* 62  */	{ "+[-[>+<<]>-]>-",       4 },
/* 63  */	{ "+[-[>+<<]>-]>",        4 },
/* 64  */	{ "-[+[>+<<]>+]>",        4 },
/* 65  */	{ ">+[+[<]>>+<+]>",       4 },
/* 66  */	{ "--[++>+[<]>+]>",       5 },
/* 67  */	{ "+[->-[--<]>-]>",       3 },
/* 68  */	{ "+[->-[--<]>-]>+",      3 },
/* 69  */	{ "+[->-[--<]>-]>++",     3 },
/* 70  */	{ "-[+[>---<<]>+]>",      4 },
/* 71  */	{ "-[>+<-------]>--",     2 },
/* 72  */	{ "-[>+<-------]>-",      2 },
/* 73  */	{ "-[>+<-------]>",       2 },
/* 74  */	{ "-[>+<-------]>+",      2 },
/* 75  */	{ "-[>+<-------]>++",     2 },
/* 76  */	{ ">+++[[-<]>>--]<-",     4 },
/* 77  */	{ ">+++[[-<]>>--]<",      4 },
/* 78  */	{ "-[+>++[++<]>]>--",     3 },
/* 79  */	{ "-[+>++[++<]>]>-",      3 },
/* 80  */	{ "-[+>++[++<]>]>",       2 },
/* 81  */	{ "-[>+<---]>----",       2 },
/* 82  */	{ "-[>+<---]>---",        2 },
/* 83  */	{ "-[>+<---]>--",         2 },
/* 84  */	{ "-[>+<---]>-",          2 },
/* 85  */	{ "-[>+<---]>",           2 },
/* 86  */	{ "-[>+<---]>+",          2 },
/* 87  */	{ "-[>+<---]>++",         2 },
/* 88  */	{ "-[+[+<]>>+]<",         4 },
/* 89  */	{ "-[+[+<]>>+]<+",        4 },
/* 90  */	{ "-[+[+<]>>+]<++",       4 },
/* 91  */	{ "-[+[+<]>>+]<+++",      4 },
/* 92  */	{ "+[->---[-<]>-]>",      3 },
/* 93  */	{ "-->+[[+++>]<+<]>",     3 },
/* 94  */	{ "-->+[[+++>]<+<]>+",    3 },
/* 95  */	{ "+[-[---<]>>-]<---",    3 },
/* 96  */	{ ">-[-[--->]<<-]>--",    3 },
/* 97  */	{ ">-[-[--->]<<-]>-",     3 },
/* 98  */	{ "-[>++<-----]>----",    2 },
/* 99  */	{ "-[>++<-----]>---",     2 },
/* 100 */	{ "-[>++<-----]>--",      2 },
/* 101 */	{ "-[>++<-----]>-",       2 },
/* 102 */	{ "-[>++<-----]>",        2 },
/* 103 */	{ "-[>++<-----]>+",       2 },
/* 104 */	{ "-[>++<-----]>++",      2 },
/* 105 */	{ "-[>++<-----]>+++",     2 },
/* 106 */	{ "-[>++<-----]>++++",    2 },
/* 107 */	{ "-[>++<-----]>+++++",   2 },
/* 108 */	{ "-[>--<-------]>--",    2 },
/* 109 */	{ "-[>--<-------]>-",     2 },
/* 110 */	{ "-[>--<-------]>",      2 },
/* 111 */	{ "-[>--<-------]>+",     2 },
/* 112 */	{ "-[>--<-------]>++",    2 },
/* 113 */	{ "-[>++<+++++++++]>-",   2 },
/* 114 */	{ "-[>++<+++++++++]>",    2 },
/* 115 */	{ "-[>++<+++++++++]>+",   2 },
/* 116 */	{ "--------[>+++<--]>",   2 },
/* 117 */	{ "----[>+++++<--]>-",    2 },
/* 118 */	{ "----[>+++++<--]>",     2 },
/* 119 */	{ "------[>+++<--]>",     2 },
/* 120 */	{ "----[>+++<--]>--",     2 },
/* 121 */	{ "----[>+++<--]>-",      2 },
/* 122 */	{ "----[>+++<--]>",       2 },
/* 123 */	{ "--[>+<--]>----",       2 },
/* 124 */	{ "--[>+<--]>---",        2 },
/* 125 */	{ "--[>+<--]>--",         2 },
/* 126 */	{ "--[>+<--]>-",          2 },
/* 127 */	{ "--[>+<--]>",           2 },
/* 128 */	{ "--[>-<--]>-",          2 },
/* 129 */	{ "--[>-<--]>",           2 },
/* 130 */	{ "--[>-<--]>+",          2 },
/* 131 */	{ "--[>-<--]>++",         2 },
/* 132 */	{ "--[>-<--]>+++",        2 },
/* 133 */	{ "--[>-<--]>++++",       2 },
/* 134 */	{ "----[>---<--]>",       2 },
/* 135 */	{ "----[>---<--]>+",      2 },
/* 136 */	{ "----[>---<--]>++",     2 },
/* 137 */	{ "------[>---<--]>",     2 },
/* 138 */	{ "----[>-----<--]>",     2 },
/* 139 */	{ "----[>-----<--]>+",    2 },
/* 140 */	{ "--------[>---<--]>",   2 },
/* 141 */	{ "-[>--<+++++++++]>-",   2 },
/* 142 */	{ "-[>--<+++++++++]>",    2 },
/* 143 */	{ "-[>--<+++++++++]>+",   2 },
/* 144 */	{ "-[>++<-------]>--",    2 },
/* 145 */	{ "-[>++<-------]>-",     2 },
/* 146 */	{ "-[>++<-------]>",      2 },
/* 147 */	{ "-[>++<-------]>+",     2 },
/* 148 */	{ "-[>++<-------]>++",    2 },
/* 149 */	{ "-[>--<-----]>-----",   2 },
/* 150 */	{ "-[>--<-----]>----",    2 },
/* 151 */	{ "-[>--<-----]>---",     2 },
/* 152 */	{ "-[>--<-----]>--",      2 },
/* 153 */	{ "-[>--<-----]>-",       2 },
/* 154 */	{ "-[>--<-----]>",        2 },
/* 155 */	{ "-[>--<-----]>+",       2 },
/* 156 */	{ "-[>--<-----]>++",      2 },
/* 157 */	{ "-[>--<-----]>+++",     2 },
/* 158 */	{ "-[>--<-----]>++++",    2 },
/* 159 */	{ "-[>--<-----]>+++++",   2 },
/* 160 */	{ "-[>--<-----]>++++++",  2 },
/* 161 */	{ "-[>--<-----]>+++++++", 2 },
/* 162 */	{ "-[>-<---]>---------",  2 },
/* 163 */	{ "-[>-<---]>--------",   2 },
/* 164 */	{ "-[>-<---]>-------",    2 },
/* 165 */	{ "-[>-<---]>------",     2 },
/* 166 */	{ "-[>-<---]>-----",      2 },
/* 167 */	{ "-[>-<---]>----",       2 },
/* 168 */	{ "-[>-<---]>---",        2 },
/* 169 */	{ "-[>-<---]>--",         2 },
/* 170 */	{ "-[>-<---]>-",          2 },
/* 171 */	{ "-[>-<---]>",           2 },
/* 172 */	{ "-[>-<---]>+",          2 },
/* 173 */	{ "-[>-<---]>++",         2 },
/* 174 */	{ "-[>-<---]>+++",        2 },
/* 175 */	{ "-[>-<---]>++++",       2 },
/* 176 */	{ "-[>-<---]>+++++",      2 },
/* 177 */	{ "-[>-<---]>++++++",     2 },
/* 178 */	{ "-[>-<---]>+++++++",    2 },
/* 179 */	{ "-[>-<---]>++++++++",   2 },
/* 180 */	{ "-[>-<-------]>---",    2 },
/* 181 */	{ "-[>-<-------]>--",     2 },
/* 182 */	{ "-[>-<-------]>-",      2 },
/* 183 */	{ "-[>-<-------]>",       2 },
/* 184 */	{ "-[>-<-------]>+",      2 },
/* 185 */	{ "-[>-<-------]>++",     2 },
/* 186 */	{ "-[>-<-------]>+++",    2 },
/* 187 */	{ "----[>+++<----]>--",   2 },
/* 188 */	{ "----[>+++<----]>-",    2 },
/* 189 */	{ "----[>+++<----]>",     2 },
/* 190 */	{ "----[>-<----]>---",    2 },
/* 191 */	{ "----[>-<----]>--",     2 },
/* 192 */	{ "----[>-<----]>-",      2 },
/* 193 */	{ "----[>-<----]>",       2 },
/* 194 */	{ "----[>-<----]>+",      2 },
/* 195 */	{ "----[>-<----]>++",     2 },
/* 196 */	{ "----[>-<----]>+++",    2 },
/* 197 */	{ "----[>-<----]>++++",   2 },
/* 198 */	{ "-[>-<+++++++++]>-",    2 },
/* 199 */	{ "-[>-<+++++++++]>",     2 },
/* 200 */	{ "-[>-<-----]>-----",    2 },
/* 201 */	{ "-[>-<-----]>----",     2 },
/* 202 */	{ "-[>-<-----]>---",      2 },
/* 203 */	{ "-[>-<-----]>--",       2 },
/* 204 */	{ "-[>-<-----]>-",        2 },
/* 205 */	{ "-[>-<-----]>",         2 },
/* 206 */	{ "-[>-<-----]>+",        2 },
/* 207 */	{ "-[>-<-----]>++",       2 },
/* 208 */	{ "-[>-<-----]>+++",      2 },
/* 209 */	{ "-[>-<-----]>++++",     2 },
/* 210 */	{ "--[>-<++++++]>---",    2 },
/* 211 */	{ "--[>-<++++++]>--",     2 },
/* 212 */	{ "--[>-<++++++]>-",      2 },
/* 213 */	{ "--[>-<++++++]>",       2 },
/* 214 */	{ "--[>-<++++++]>+",      2 },
/* 215 */	{ "--[>-<++++++]>++",     2 },
/* 216 */	{ "--[>-<++++++]>+++",    2 },
/* 217 */	{ "---[>-<+++++++]>--",   2 },
/* 218 */	{ "---[>-<+++++++]>-",    2 },
/* 219 */	{ "---[>-<+++++++]>",     2 },
/* 220 */	{ "--[>++<+++++++]>",     2 },
/* 221 */	{ "--[>++<+++++++]>+",    2 },
/* 222 */	{ "--[>++<+++++++]>++",   2 },
/* 223 */	{ "----[>--------<+]>-",  2 },
/* 224 */	{ "----[>--------<+]>",   2 },
/* 225 */	{ "-----[>------<+]>-",   2 },
/* 226 */	{ "-----[>------<+]>",    2 },
/* 227 */	{ "----[>-------<+]>-",   2 },
/* 228 */	{ "----[>-------<+]>",    2 },
/* 229 */	{ "---[>---------<+]>",   2 },
/* 230 */	{ "-----[>-----<+]>-",    2 },
/* 231 */	{ "-----[>-----<+]>",     2 },
/* 232 */	{ "----[>------<+]>",     2 },
/* 233 */	{ "----[>------<+]>+",    2 },
/* 234 */	{ "---[>-------<+]>-",    2 },
/* 235 */	{ "---[>-------<+]>",     2 },
/* 236 */	{ "----[>-----<+]>",      2 },
/* 237 */	{ "---[>------<+]>-",     2 },
/* 238 */	{ "---[>------<+]>",      2 },
/* 239 */	{ "----[>----<+]>-",      2 },
/* 240 */	{ "----[>----<+]>",       2 },
/* 241 */	{ "---[>-----<+]>",       2 },
/* 242 */	{ "--------------",       1 },
/* 243 */	{ "-------------",        1 },
/* 244 */	{ "------------",         1 },
/* 245 */	{ "-----------",          1 },
/* 246 */	{ "----------",           1 },
/* 247 */	{ "---------",            1 },
/* 248 */	{ "--------",             1 },
/* 249 */	{ "-------",              1 },
/* 250 */	{ "------",               1 },
/* 251 */	{ "-----",                1 },
/* 252 */	{ "----",                 1 },
/* 253 */	{ "---",                  1 },
/* 254 */	{ "--",                   1 },
/* 255 */	{ "-",                    1 }
};

/* a cute little interpreter */
int pointer_offset_interpreter(char* code)
{
	char mem[64], stack[64];
	int ip = 0, mp = 0, sp = 0;

	for (int i = 0; i < 64; i++) mem[i] = 0, stack[i] = 0;

	while (code[ip]) {
		switch (code[ip]) {
			case '-': mem[mp]--; break;
			case '+': mem[mp]++; break;
			case '[': {
				if (mem[mp]) {
					stack[sp++] = ip;
				} else {
					int depth = 1;
					do {
						ip++;
						if (code[ip] == '[') depth++;
						else if (code[ip] == ']') depth--;
					} while (depth);
				}
			} break;
			case ']': ip = stack[--sp] - 1; break;
			case '<': mp--; break;
			case '>': mp++; break;
		}

		ip++;
	}

	return mp;
}

void clear_cells_right(int num)
{
	for (int i = 0; i < num; i++) {
		emit("[-]>");
	}

	for (int i = 0; i < num; i++) {
		emit("<");
	}
}

void set_cell_to_constant(int cell, int value)
{
	value %= 256;

	if (value > 15) {
		move_pointer_to(temp_cells);
		clear_cells_right(bf_constants[value].cells_required);

		emit(bf_constants[value].code);
		int offset = pointer_offset_interpreter(bf_constants[value].code);

		for (int i = 0; i < offset; i++) emit("<");

		move_pointer_to(cell), emit("[-]"), move_pointer_to(temp_cells + offset);
		emit("["), move_pointer_to(cell), emit("+"), move_pointer_to(temp_cells + offset), emit("-]");
	} else {
		move_pointer_to(cell), emit("[-]"), add(value);
	}
}

void parse_lefthand_side(Token** token, int* left, int* left_index, int* array)
{
	Token* tok = *token;

	*left_index = get_variable_index(tok->value);
	SYNTAX_ASSERT(*left_index == -1, "expected an a variable identifier.")

	if (variables[*left_index].type == VAR_ARRAY) {
		*array = 1;
		EXPECT_TOKEN(tok, TOK_OPERATOR, "[")
		NEXT_TOKEN(tok)

		/* now get the index value, we must account for variables and constants */
		int subscript_index = get_variable_index(tok->value);
		if (tok->type == TOK_IDENTIFIER && subscript_index != -1) {
			emit_algo(ALGO_EQU, temp_x_index, variables[subscript_index].location, -1);
		} else {
			int a = expression(&tok);
			set_cell_to_constant(temp_x_index, a);
		}

		EXPECT_TOKEN(tok, TOK_OPERATOR, "]")

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
	SYNTAX_ASSERT(tok->type != TOK_OPERATOR, "expected a valid operator.")

	operation = tok->data;

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
		case MOP_ANDAND: algo = ALGO_AND;  break;
		case MOP_LESS:   algo = ALGO_LESS; break;
		case MOP_MORE:   algo = ALGO_GRT;  break;
		default:
			push_error(tok->origin, 1, 1, "unrecognized operator.");
			return;
	}

#define FERRY_ARRAY_BACK                                                                           \
	if (array) {                                                                               \
		/* x(y) = z (array write) */                                                       \
		emit_algo(ALGO_ARRAY_WRITE, variables[left_index].location, temp_x_index, temp_x); \
	}

	NEXT_TOKEN(tok)

	int right_index = get_variable_index(tok->value);

	if (tok->type == TOK_IDENTIFIER && right_index != -1) {
		if (variables[right_index].type == VAR_ARRAY) {
			EXPECT_TOKEN(tok, TOK_OPERATOR, "[")
			NEXT_TOKEN(tok)

			int subscript_index = get_variable_index(tok->value);
			if (tok->type == TOK_IDENTIFIER && subscript_index != -1) {
				emit_algo(ALGO_EQU, temp_y_index, variables[subscript_index].location, -1);
			} else {
				int num = expression(&tok);
				set_cell_to_constant(temp_y_index, num);
			}

			EXPECT_TOKEN(tok, TOK_OPERATOR, "]")

			emit_algo(ALGO_ARRAY_READ, temp_y, variables[right_index].location, temp_y_index); /* x = y(z) */
			right = temp_y;
		} else {
			if (left_index == right_index) {
				emit_algo(ALGO_EQU, temp_y, variables[right_index].location, -1);
				right = temp_y;
			} else {
				right = variables[right_index].location;
			}
		}
	} else {
		int a = expression(&tok);
		EXPECT_TOKEN(tok, TOK_OPERATOR, ";")

		if (operation == MOP_SUB) {
			set_cell_to_constant(temp_y, a);
			move_pointer_to(temp_y), emit("["), move_pointer_to(left), emit("-"), move_pointer_to(temp_y), emit("-]");
			*token = tok;
			FERRY_ARRAY_BACK
			return;
		} else if (operation == MOP_ADD) {
			set_cell_to_constant(temp_y, a);
			move_pointer_to(temp_y), emit("["), move_pointer_to(left), emit("+"), move_pointer_to(temp_y), emit("-]");
			*token = tok;
			FERRY_ARRAY_BACK
			return;
		} else if (operation == MOP_EQU) {
			set_cell_to_constant(left, a);

			*token = tok;
			FERRY_ARRAY_BACK
			return;
		} else {
			set_cell_to_constant(temp_y, a);

			right = temp_y;
		}
	}

	emit_algo(algo, left, right, -1);
	FERRY_ARRAY_BACK

	*token = tok;
}

enum {
	STACK_WHILE,
	STACK_IF,
	STACK_MACRO
};

int stack[4096], stack_ptr = 0;

char** parse_list(Token** token, int* count, int** origins)
{
	Token* tok = *token;

	*count = 0;
	char** args = bfm_malloc(sizeof(char*) * 2);
	while (tok->type == TOK_IDENTIFIER && tok->next != NULL && tok->next->type == TOK_OPERATOR) {
		args = bfm_realloc(args, sizeof(char*) * (*count + 1));
		args[*count] = tok->value;
		if (origins) {
			*origins = bfm_realloc(*origins, sizeof(int) * (*count + 1));
			(*origins)[*count] = tok->origin;
		}

		(*count)++;
		
		if (!tok->next) {
			push_error(tok->origin, 1, 1, "expected a valid token to follow.");
			*token = tok;
			return NULL;
		}
		tok = tok->next;

		if (tok->data == MOP_RBRACE) {
			break;
		} else if (tok->data != MOP_COMMA) {
			push_error(tok->origin, 1, 1, "expected a \",\".");
			*token = tok;
			break;
		} else {
			if (!tok->next) {
				push_error(tok->origin, 1, 1, "expected a valid token to follow.");
				*token = tok;
				return NULL;
			}
			tok = tok->next;
		}
	}

	*token = tok;
	return args;
}

Token* tok_stack[4096];
int tok_sp = 0;

void parse_keyword(Token** token)
{
	Token* tok = *token;

	switch (tok->data) {
		case KYWRD_VAR:
			NEXT_TOKEN(tok)

			SYNTAX_ASSERT(tok->type != TOK_IDENTIFIER, "expected an identifier.")
			SYNTAX_ASSERT(get_keyword(tok->value) != -1, "variable names must not be keywords.")

			add_variable(tok->value, -1, VAR_CELL, used_variable_cells++, context, tok->origin, scope);
			break;
		case KYWRD_WHILE: {
			NEXT_TOKEN(tok)

			int var_index = get_variable_index(tok->value);
			
			SYNTAX_ASSERT(var_index == -1, "invalid identifier.")
			SYNTAX_ASSERT(variables[var_index].type != VAR_CELL, "arguments for while statements must not be arrays.")

			int variable_location = variables[var_index].location;
			move_pointer_to(variable_location);
			emit("[");

			stack[stack_ptr++] = variable_location;
			stack[stack_ptr++] = STACK_WHILE;

			scope++;
			
		} break;
		case KYWRD_END: {
			SYNTAX_ASSERT(stack_ptr < 1, "unmatched end statement.")

			kill_variables_of_scope(scope--);
			switch (stack[--stack_ptr]) {
				case STACK_WHILE:
					move_pointer_to(stack[--stack_ptr]);
					emit("]");
					break;
				case STACK_IF:
					move_pointer_to(stack[--stack_ptr]);
					emit("[-]]");
					break;
				case STACK_MACRO:
					tok = tok_stack[--tok_sp];
					stack_ptr--;
					kill_variables_of_context(context--);
					break;
			}
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
			SYNTAX_ASSERT(variables[var_index].type != VAR_CELL, "arguments for if statements must not be arrays.")

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
			SYNTAX_ASSERT(variables[var_index].type != VAR_CELL, "arguments for not statements must not be arrays.")

			int variable_location = variables[var_index].location;
			emit_algo(ALGO_NOT, variable_location, -1, -1);
		} break;
		case KYWRD_PRINT: {
				NEXT_TOKEN(tok)

				int var_index = get_variable_index(tok->value);
				if (tok->type == TOK_STRING) {
					emit_print_string(tok);
				} else if (tok->type == TOK_IDENTIFIER && var_index != -1) {
					int left = 0;
					if (variables[var_index].type == VAR_ARRAY) {
						EXPECT_TOKEN(tok, TOK_OPERATOR, "[")
						NEXT_TOKEN(tok)

						int subscript_index = get_variable_index(tok->value);
						if (tok->type == TOK_IDENTIFIER && subscript_index != -1) {
							emit_algo(ALGO_EQU, temp_y_index, variables[subscript_index].location, -1);
						} else {
							int num = expression(&tok);
							move_pointer_to(temp_y_index);
							emit("[-]");
							add(num);
						}

						EXPECT_TOKEN(tok, TOK_OPERATOR, "]")

						emit_algo(ALGO_ARRAY_READ, temp_y, variables[var_index].location, temp_y_index); /* x = y(z) */
						left = temp_y;
					} else {
						left = variables[var_index].location;
					}

					emit_algo(ALGO_PRINTV, left, -1, -1);
				} else {
					int a = expression(&tok);
					EXPECT_TOKEN(tok, TOK_OPERATOR, ";")
					
					move_pointer_to(temp_cells);
					emit("[-]"), add(a), emit(".");
				}
			} break;
		case KYWRD_ARRAY: {
			NEXT_TOKEN(tok)
			SYNTAX_ASSERT(tok->type != TOK_IDENTIFIER, "expected an identifier.")
			char* name = tok->value;
			NEXT_TOKEN(tok)

			int a = expression(&tok);
			EXPECT_TOKEN(tok, TOK_OPERATOR, ";")
			add_variable(name, a, VAR_ARRAY, arrays + used_array_cells, context, tok->origin, scope);
		} break;
		case KYWRD_BF: {
			NEXT_TOKEN(tok)
			SYNTAX_ASSERT(tok->type != TOK_STRING, "expected a string literal.")
			emit(tok->value);
		} break;
		case KYWRD_DEFINE: {
			NEXT_TOKEN(tok)
			SYNTAX_ASSERT(tok->type != TOK_IDENTIFIER, "expected an identifier.")

			char* name = tok->value;
			NEXT_TOKEN(tok)
			SYNTAX_ASSERT(tok->type != TOK_NUMBER, "expected a number.")

			double data = expression(&tok);
			EXPECT_TOKEN(tok, TOK_OPERATOR, ";")

			add_definition(name, (int)data);
		} break;
		case KYWRD_INPUT: {
			NEXT_TOKEN(tok)

			int var_index = get_variable_index(tok->value);

			SYNTAX_ASSERT(var_index == -1, "invalid identifier.")
			SYNTAX_ASSERT(variables[var_index].type != VAR_CELL, "arguments for input statements must not be arrays.")
			
			move_pointer_to(variables[var_index].location);
			emit(",");
		} break;
		case KYWRD_WRITE: {
			NEXT_TOKEN(tok)
			SYNTAX_ASSERT(tok->type != TOK_STRING, "expected a string.")

			emit_write_string(tok);
		} break;
		case KYWRD_DECIM: {
			NEXT_TOKEN(tok)
			SYNTAX_ASSERT(tok->type != TOK_IDENTIFIER, "expected an identifier.")
			int var_index = get_variable_index(tok->value);
			SYNTAX_ASSERT(var_index == -1, "invalid identifier.")
			SYNTAX_ASSERT(variables[var_index].type != VAR_CELL, "arguments for decimal statements must not be arrays.")

			emit_algo(ALGO_DECIM, variables[var_index].location, -1, -1);
		} break;
		case KYWRD_MACRO: {
			NEXT_TOKEN(tok)
			
			if (tok->type != TOK_IDENTIFIER)
				push_error(tok->origin, 1, 1, "expected an identifier.");

			char* name = tok->value;
			int origin = tok->origin;
			EXPECT_TOKEN(tok, TOK_OPERATOR, "(")
			NEXT_TOKEN(tok)

			int count = 0;
			int* origins = bfm_malloc(2);
			char** args = parse_list(&tok, &count, &origins);
			SYNTAX_ASSERT(!args, "malformed argument list.")

			if (tok->type != TOK_OPERATOR && strcmp(tok->value, ")")) {
				push_error(tok->origin, 1, 1, "expected \")\".");
			}
			NEXT_TOKEN(tok)
			Token* body = tok;

			add_macro(name, args, count, body, origin, origins);

			int depth = 1;
			while (tok) {
				if (tok->type == TOK_KYWRD) {
					if (tok->data == KYWRD_IF || tok->data == KYWRD_WHILE) {
						depth++;
					} else if (tok->data == KYWRD_END) {
						depth--;
					}
				} else if (tok->type == TOK_KYWRD && tok->data == KYWRD_MACRO) {
					depth++;
				}

				if (!depth)
					break;

				tok = tok->next;
			}

			if (depth) {
				push_error(origin, 0, 1, "no terminating end statement to macro definition.");
				tok = body;
				*token = tok;
				return;
			}

			// printf("found a macro called %s with %d arguments.\n", name, count);
		} break;
	}

	*token = tok;
}

/* TODO: change the error system so the we can report both the location of a problematic
 * macro and the expansion that caused the issue. */
void parse_macro(Token** token)
{
	Token* tok = *token;

	int macro_idx = get_macro_index(tok->value);

	for (int i = stack_ptr - 1; i >= 1; i -= 2) {
		if (stack[i] == STACK_MACRO && stack[i - 1] == macro_idx) {
			push_error(macros[macro_idx].origin, 0, 1, "recursive macro definition.");
			int num_args = 0;
			EXPECT_TOKEN(tok, TOK_OPERATOR, "(")
			NEXT_TOKEN(tok)
			char** args = parse_list(&tok, &num_args, NULL);

			if (tok->type != TOK_OPERATOR && strcmp(tok->value, ")")) {
				push_error(tok->origin, 0, 1, "expected \")\".");
			}

			free(args);
			*token = tok->next;

			return;
		}
	}

	EXPECT_TOKEN(tok, TOK_OPERATOR, "(")
	NEXT_TOKEN(tok)
	
	int num_args = 0;
	char** args = parse_list(&tok, &num_args, NULL);

	if (tok->type != TOK_OPERATOR && strcmp(tok->value, ")")) {
		push_error(tok->origin, 1, 1, "expected \")\".");
	}

	SYNTAX_ASSERT(!args, "malformed argument list.")
	SYNTAX_ASSERT(num_args != macros[macro_idx].num_args, "incorrect number of arguments to macro.")
	tok_stack[tok_sp++] = tok;
	stack[stack_ptr++] = macro_idx;
	stack[stack_ptr++] = STACK_MACRO;
	scope++;

	int failed = 0;
	for (int i = 0; i < num_args; i++) {
		int arg_idx = get_variable_index(args[i]);
		if (arg_idx == -1) {
			push_error(tok->origin, 0, 1, "unrecognized variable.");
			failed = 1;
			continue;
		}
		add_variable(macros[macro_idx].args[i], -1, variables[arg_idx].type, variables[arg_idx].location, context + 1, macros[macro_idx].origins[i], -1);
	}

	if (failed) {
		*token = tok;
		return;
	}

	context++;
	tok = macros[macro_idx].body;

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
		} else if (tok->type == TOK_IDENTIFIER && get_macro_index(tok->value) != -1) {
			parse_macro(&tok);
		} else {
			push_error(tok->origin, 1, 1, "invalid statement.", tok->value);
			tok = tok->next;
		}
	}
	kill_variables_of_scope(scope--);

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

int scopes[4096], scope_ptr;
int estimate_variables(Token** token)
{
	Token* tok = *token;
	int top = 0;

	while (tok) {
		int living_cells = 0;
		for (int i = 0; i <= scope_ptr; i++) {
			living_cells += scopes[i];
		}

		top = (living_cells > top) ? living_cells : top;

		if (tok->type == TOK_KYWRD && tok->data == KYWRD_VAR) {
			scopes[scope_ptr]++;
		} else if (tok->type == TOK_KYWRD && tok->data == KYWRD_MACRO) {
			PARSE_NEXT_TOKEN(tok)
			
			if (tok->type != TOK_IDENTIFIER)
				push_error(tok->origin, 1, 1, "expected an identifier.");

			char* name = tok->value;
			int origin = tok->origin;
			PARSE_EXPECT_TOKEN(tok, TOK_OPERATOR, "(")
			PARSE_NEXT_TOKEN(tok)

			int count = 0;
			int* origins = bfm_malloc(2);
			char** args = parse_list(&tok, &count, &origins);
			PARSE_SYNTAX_ASSERT(!args, "malformed argument list.")

			if (tok->type != TOK_OPERATOR && strcmp(tok->value, ")")) {
				push_error(tok->origin, 1, 1, "expected \")\".");
			}
			PARSE_NEXT_TOKEN(tok)
			Token* body = tok;

			add_macro(name, args, count, body, origin, origins);

			int depth = 1;
			while (tok) {
				if (tok->type == TOK_KYWRD) {
					if (tok->data == KYWRD_IF || tok->data == KYWRD_WHILE) {
						depth++;
					} else if (tok->data == KYWRD_END) {
						depth--;
					}
				} else if (tok->type == TOK_KYWRD && tok->data == KYWRD_MACRO) {
					depth++;
				}

				if (!depth)
					break;

				tok = tok->next;
			}

			if (depth) {
				push_error(origin, 0, 1, "no terminating end statement to macro definition.");
				tok = body;
				*token = tok;
				return -1;
			}
		} else if (tok->type == TOK_IDENTIFIER && get_macro_index(tok->value) != -1) {
			int macro_idx = get_macro_index(tok->value);
			
			tok_stack[tok_sp++] = tok;
			tok = macros[macro_idx].body->prev;

			stack[stack_ptr++] = macro_idx;
			stack[stack_ptr++] = STACK_MACRO;
			
			scopes[++scope_ptr] = 0;
		} else if (tok->type == TOK_KYWRD && (tok->data == KYWRD_WHILE || tok->data == KYWRD_IF)) {
			stack[stack_ptr++] = 0;
			stack[stack_ptr++] = STACK_WHILE;
			
			scopes[++scope_ptr] = 0;
		} else if (tok->type == TOK_KYWRD && tok->data == KYWRD_END) {
			switch (stack[--stack_ptr]) {
				case STACK_MACRO: {
					tok = tok_stack[--tok_sp];
					stack_ptr--;
				} break;
				case STACK_WHILE: {
					stack_ptr--;
				} break;
			}

			scope_ptr--;
		}

		tok = tok->next;
	}

	num_macros = 0;
	num_variables = 0;
	num_definitions = 0;
	stack_ptr = 0;
	tok_sp = 0;

	// printf("top: %d\n", top);
	check_errors();

	return top;
}

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "-o", 2)) {
			if (output_path) {
				fatal_error(-1, "Usage: bfm INPUT_PATH -oOUTPUT_PATH");
			}

			output_path = &argv[i][2];
		} else if (!strcmp(argv[i], "-v")) {
			verbose = 1;
		} else {
			input_path = argv[i];
		}
	}

	if (!input_path || !output_path)
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

	temp_cells   = estimate_variables(&tok) + 4;
	temp_x       = temp_cells - 4,   temp_y = temp_cells - 2;
	temp_x_index = temp_x + 1,       temp_y_index = temp_y + 1;
	arrays = temp_cells + NUM_TEMP_CELLS;

	add_variable("null", -1, VAR_CELL, temp_cells + NUM_TEMP_CELLS - 1, -1, -1, -1);
	parse(tok);
	free(raw);
	delete_list(tok);

	sanitize(output);

	FILE* output_file = fopen(output_path, "w");
	save_file(output_file, output);
	fclose(output_file);

	free(output);

	return 0;
}
