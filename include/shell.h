#ifndef SHELL_H
#define SHELL_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 2048
#define MAXARGS 64
#define MAX_PIPES 8
#define ARGLEN 256
#define PROMPT "FCIT> "

typedef struct {
    char* argv[MAXARGS];
    char* infile;
    char* outfile;
    int append;
} Command;

typedef struct {
    Command commands[MAX_PIPES];
    int count;
} Pipeline;

char* read_cmd(char* prompt, FILE* fp);
void initialize_readline(void);
void add_to_history(const char* cmdline);
void print_history(void);
char* resolve_history(const char* line);

int parse_pipeline(const char* line, Pipeline* pipeline);
void free_pipeline(Pipeline* pipeline);
int execute_pipeline(Pipeline* pipeline);

int handle_builtin(char** arglist);

#endif
