#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define MAX_ERROR_LENGTH 4096
void fatal_error(const char* message, ...)
{
	char error_buf[MAX_ERROR_LENGTH + 1];
	
	printf("error: ");
	
	if (message) {
		va_list args;
		va_start(args, message);
		vsnprintf(error_buf, MAX_ERROR_LENGTH, message, args);
		va_end(args);
		printf("%s\n", error_buf);
	}
	
	exit(EXIT_FAILURE);
}

char *load_file(const char *path)
{
	char *buf = NULL;
	FILE *file = fopen(path, "r");
	
	if (file) {
		if (fseek(file, 0L, SEEK_END) == 0) {
			long len = ftell(file);
			if (len == -1) return NULL;
			
			buf = malloc(len + 1);

			if (fseek(file, 0L, SEEK_SET) != 0)
				return NULL;

			size_t new_len = fread(buf, 1, len, file);
			if (ferror(file) != 0) {
				fatal_error("Input file could not be read.\n");
			} else {
				buf[new_len++] = '\0';
			}
		}
		
		fclose(file);
	}
	
	return buf;
}

char **tokenize(char *str)
{
	int toks_allocated = 128, num_tokens = 0;
	char **tokens = malloc(256 * sizeof(char*));

	char *tok = strtok(str, "\t\r\n ");
	while (tok) {
		if (num_tokens >= toks_allocated)
			tokens = realloc(tokens, (toks_allocated + 256) * sizeof(char*));

		tokens[num_tokens] = malloc(strlen(tok) + 1);
		strcpy(tokens[num_tokens], tok);
		num_tokens++;

		tok = strtok(NULL, "\t\r\n ");
	}

	tokens = realloc(tokens, (num_tokens + 1) * sizeof(char*));
	tokens[num_tokens] = NULL;

	return tokens;
}

char **tok;

enum {
	KEYWORD_VAR,
	KEYWORD_WHILE,
	KEYWORD_IF,
	KEYWORD_ENDIF,
	KEYWORD_ENDWHILE,
	KEYWORD_PRINTC,
	KEYWORD_PRINT,
	KEYWORD_PRINTV
};

#define NUM_KEYWORDS 8
char keywords[][9] = {
	"var",
	"while",
	"if",
	"endif",
	"endwhile",
	"printc",
	"print",
	"printv"
};

int get_keyword(char *str)
{
	for (int i = 0; i < NUM_KEYWORDS; i++)
		if (!strcmp(str, keywords[i]))
			return i;
	return -1;
}

struct {
	char *name;
	int location, scope;
} variables[4096]; /* no one will ever need more than this */
int num_variables, scope;

void add_variable(char *name)
{
	variables[num_variables].location = num_variables;
	variables[num_variables].scope = scope;
	variables[num_variables].name = malloc(strlen(name) + 1);
	strcpy(variables[num_variables].name, name);

	num_variables++;
}

void kill_variables(int kill_scope)
{
	for (int i = num_variables - 1; i > 0; i--) {
		if (variables[i].scope == kill_scope) {
			printf("killing variable %s\n", variables[i].name);
			free(variables[i].name);
			num_variables--;
		}
	}
}

int find_variable(const char* name)
{
	for (int i = 0; i < num_variables; i++)
		if (!strcmp(variables[i].name, name))
			return i;
	return -1;
}

void parse_operation()
{
	tok += 3;
}

void parse()
{
	while (*tok) {
		switch (get_keyword(*tok)) {
			case KEYWORD_VAR:
				tok++;
				add_variable(*tok++);
				break;
			case KEYWORD_WHILE: scope++; tok++; break;
			case KEYWORD_IF: tok++; break;
			case KEYWORD_ENDIF: tok++; break;
			case KEYWORD_ENDWHILE:
				kill_variables(scope);
				scope++;
				tok++;
				break;
			case KEYWORD_PRINTC:
				tok++;
				break;
			case KEYWORD_PRINT:
				tok++;
				break;q
			case KEYWORD_PRINTV:
				tok++;
				break;
			default:
				if (find_variable(*tok) != -1)
					parse_operation();
				else {
					/* fatal_error("unrecognized token %s.\n", *tok++); */
					printf("uh oh: %s\n", *tok);
					tok++;
				}

				break;
		}
	}
}

int main(int argc, char **argv)
{
	char *input_path = NULL, *output_path = NULL;
	
	for (int i = 0; i < argc; i++) {
		if (!strncmp(argv[i], "-o", 2))
			output_path = &argv[i][2];
		else
			input_path = argv[i];
	}

	if (!input_path)
		fatal_error("Usage: bfm INPUT_PATH -oOUTPUT_PATH");

	char *src = load_file(input_path);
	tok = tokenize(src);
	char **tokens = tok;
	parse();

#ifdef DEBUG
	for (int i = 0; tokens[i]; i++)
		printf("[%s]\n", tokens[i]);

	puts("variables in scope at the end of the program:");
	for (int i = 0; i < num_variables; i++)
		printf("variable: [%s]\n", variables[i].name);
#endif

	return 0;
}