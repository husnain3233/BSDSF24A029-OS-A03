#include "shell.h"
#include <ctype.h>

#define HISTORY_SIZE 20

static char* history[HISTORY_SIZE];
static int history_count = 0;
static int history_start = 0;

typedef struct {
    pid_t pid;
    char* command;
    int in_use;
} Job;

static Job jobs[MAX_JOBS];

static const char* builtin_names[] = {
    "cd", "exit", "help", "jobs", "history", NULL
};

typedef enum {
    TOK_WORD,
    TOK_PIPE,
    TOK_IN,
    TOK_OUT,
    TOK_APPEND
} TokenType;

typedef struct {
    TokenType type;
    char* text;
} Token;

char* read_cmd(char* prompt, FILE* fp) {
    (void)fp;
    char* line = readline(prompt);
    if (line != NULL && line[0] != '\0') {
        add_history(line);
    }
    return line;
}

static char* command_generator(const char* text, int state) {
    static int index = 0;
    static size_t len = 0;

    if (state == 0) {
        index = 0;
        len = strlen(text);
    }

    while (builtin_names[index] != NULL) {
        const char* name = builtin_names[index++];
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }
    return NULL;
}

static char** shell_completion(const char* text, int start, int end) {
    (void)end;
    rl_attempted_completion_over = 1;
    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }
    return rl_completion_matches(text, rl_filename_completion_function);
}

void initialize_readline(void) {
    rl_attempted_completion_function = shell_completion;
}

void add_to_history(const char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0') {
        return;
    }

    if (history_count < HISTORY_SIZE) {
        history[(history_start + history_count) % HISTORY_SIZE] = strdup(cmdline);
        history_count++;
        return;
    }

    free(history[history_start]);
    history[history_start] = strdup(cmdline);
    history_start = (history_start + 1) % HISTORY_SIZE;
}

void print_history(void) {
    for (int i = 0; i < history_count; i++) {
        int idx = (history_start + i) % HISTORY_SIZE;
        printf("%4d  %s\n", i + 1, history[idx]);
    }
}

char* resolve_history(const char* line) {
    if (line == NULL) {
        return NULL;
    }

    if (line[0] != '!') {
        return strdup(line);
    }

    const char* digits = line + 1;
    if (*digits == '\0') {
        fprintf(stderr, "history: missing event number\n");
        return NULL;
    }

    for (const char* p = digits; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            fprintf(stderr, "history: invalid event designator\n");
            return NULL;
        }
    }

    int n = atoi(digits);
    if (n < 1 || n > history_count) {
        fprintf(stderr, "history: event not found: %d\n", n);
        return NULL;
    }

    int idx = (history_start + n - 1) % HISTORY_SIZE;
    printf("%s\n", history[idx]);
    return strdup(history[idx]);
}

void add_job(pid_t pid, const char* command) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].in_use) {
            jobs[i].pid = pid;
            jobs[i].command = strdup(command != NULL ? command : "");
            jobs[i].in_use = 1;
            return;
        }
    }
    fprintf(stderr, "jobs: job table is full\n");
}

void reap_jobs(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_JOBS; i++) {
            if (jobs[i].in_use && jobs[i].pid == pid) {
                free(jobs[i].command);
                memset(&jobs[i], 0, sizeof(Job));
                break;
            }
        }
    }
}

void print_jobs(void) {
    int found = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use) {
            printf("[%d] %ld  %s\n", i + 1, (long)jobs[i].pid, jobs[i].command);
            found = 1;
        }
    }
    if (!found) {
        printf("No active background jobs.\n");
    }
}

void free_chain(ChainItem* items, int count) {
    for (int i = 0; i < count; i++) {
        free(items[i].text);
        items[i].text = NULL;
        items[i].background = 0;
    }
}

int split_chain(const char* line, ChainItem* items, int max_items) {
    if (line == NULL || line[0] == '\0') {
        return 0;
    }

    char buffer[MAX_LEN];
    int len = 0;
    int count = 0;
    char quote = 0;
    int background = 0;

    for (const char* p = line; *p != '\0'; p++) {
        char c = *p;

        if (quote != 0) {
            if (c == quote) {
                quote = 0;
            } else if (len < MAX_LEN - 1) {
                buffer[len++] = c;
            }
            continue;
        }

        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }

        if (c == ';' || c == '&') {
            if (c == '&') background = 1;
            while (len > 0 && isspace((unsigned char)buffer[len - 1])) len--;
            if (len > 0) {
                if (count >= max_items) {
                    fprintf(stderr, "shell: too many commands\n");
                    free_chain(items, count);
                    return -1;
                }
                buffer[len] = '\0';
                items[count].text = strdup(buffer);
                items[count].background = background;
                count++;
            }
            len = 0;
            background = 0;
            continue;
        }

        if (len < MAX_LEN - 1) buffer[len++] = c;
    }

    if (quote != 0) {
        fprintf(stderr, "shell: unmatched quote\n");
        free_chain(items, count);
        return -1;
    }

    while (len > 0 && isspace((unsigned char)buffer[len - 1])) len--;
    if (len > 0) {
        if (count >= max_items) {
            fprintf(stderr, "shell: too many commands\n");
            free_chain(items, count);
            return -1;
        }
        buffer[len] = '\0';
        items[count].text = strdup(buffer);
        items[count].background = background;
        count++;
    }

    return count;
}

static int is_operator(char c) {
    return c == '|' || c == '<' || c == '>';
}

static void free_tokens(Token* tokens, int count) {
    for (int i = 0; i < count; i++) {
        free(tokens[i].text);
    }
}

static int lex_line(const char* line, Token* tokens, int max_tokens) {
    int n = 0;
    const char* p = line;

    while (*p != '\0') {
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') break;

        if (n >= max_tokens) {
            fprintf(stderr, "shell: too many tokens\n");
            free_tokens(tokens, n);
            return -1;
        }

        if (*p == '|') {
            tokens[n].type = TOK_PIPE;
            tokens[n].text = NULL;
            n++;
            p++;
            continue;
        }

        if (*p == '<') {
            tokens[n].type = TOK_IN;
            tokens[n].text = NULL;
            n++;
            p++;
            continue;
        }

        if (*p == '>') {
            if (*(p + 1) == '>') {
                tokens[n].type = TOK_APPEND;
                p += 2;
            } else {
                tokens[n].type = TOK_OUT;
                p++;
            }
            tokens[n].text = NULL;
            n++;
            continue;
        }

        char buf[MAX_LEN];
        int len = 0;
        char quote = 0;

        while (*p != '\0') {
            char c = *p;
            if (quote != 0) {
                if (c == quote) {
                    quote = 0;
                } else if (len < MAX_LEN - 1) {
                    buf[len++] = c;
                }
                p++;
                continue;
            }
            if (c == '\'' || c == '"') {
                quote = c;
                p++;
                continue;
            }
            if (isspace((unsigned char)c) || is_operator(c)) break;
            if (len < MAX_LEN - 1) buf[len++] = c;
            p++;
        }

        if (quote != 0) {
            fprintf(stderr, "shell: unmatched quote\n");
            free_tokens(tokens, n);
            return -1;
        }

        buf[len] = '\0';
        tokens[n].type = TOK_WORD;
        tokens[n].text = strdup(buf);
        n++;
    }

    return n;
}

void free_pipeline(Pipeline* pipeline) {
    for (int c = 0; c < pipeline->count; c++) {
        Command* cmd = &pipeline->commands[c];
        for (int i = 0; i < MAXARGS; i++) {
            free(cmd->argv[i]);
        }
        free(cmd->infile);
        free(cmd->outfile);
        memset(cmd, 0, sizeof(Command));
    }
    pipeline->count = 0;
}

int parse_pipeline(const char* line, Pipeline* pipeline) {
    Token tokens[MAXARGS * MAX_PIPES];
    int ntokens;
    int argc = 0;
    int ok = 1;

    memset(pipeline, 0, sizeof(Pipeline));

    if (line == NULL || line[0] == '\0') {
        return 0;
    }

    ntokens = lex_line(line, tokens, MAXARGS * MAX_PIPES);
    if (ntokens < 0) {
        return 0;
    }
    if (ntokens == 0) {
        return 0;
    }

    Command* current = &pipeline->commands[0];

    for (int i = 0; i < ntokens && ok; i++) {
        Token* token = &tokens[i];

        switch (token->type) {
            case TOK_WORD:
                if (argc >= MAXARGS - 1) {
                    fprintf(stderr, "shell: too many arguments\n");
                    ok = 0;
                } else {
                    current->argv[argc++] = token->text;
                    token->text = NULL;
                }
                break;

            case TOK_PIPE:
                if (argc == 0) {
                    fprintf(stderr, "shell: syntax error near '|'\n");
                    ok = 0;
                    break;
                }
                current->argv[argc] = NULL;
                pipeline->count++;
                if (pipeline->count >= MAX_PIPES) {
                    fprintf(stderr, "shell: too many pipeline stages\n");
                    ok = 0;
                    break;
                }
                current = &pipeline->commands[pipeline->count];
                argc = 0;
                break;

            case TOK_IN:
            case TOK_OUT:
            case TOK_APPEND:
                if (i + 1 >= ntokens || tokens[i + 1].type != TOK_WORD) {
                    fprintf(stderr, "shell: syntax error near redirection\n");
                    ok = 0;
                    break;
                }
                i++;
                if (token->type == TOK_IN) {
                    free(current->infile);
                    current->infile = tokens[i].text;
                } else {
                    free(current->outfile);
                    current->outfile = tokens[i].text;
                    current->append = (token->type == TOK_APPEND);
                }
                tokens[i].text = NULL;
                break;
        }
    }

    free_tokens(tokens, ntokens);

    if (!ok) {
        free_pipeline(pipeline);
        return 0;
    }

    if (argc == 0) {
        fprintf(stderr, "shell: syntax error near end of line\n");
        free_pipeline(pipeline);
        return 0;
    }

    current->argv[argc] = NULL;
    pipeline->count++;

    return 1;
}

static void print_help(void) {
    printf("Built-in commands:\n");
    printf("  exit          Exit the shell\n");
    printf("  cd <dir>      Change the current working directory\n");
    printf("  help          Show this help message\n");
    printf("  jobs          List active background jobs\n");
    printf("  history       Show previously entered commands\n");
    printf("  !n            Re-execute command number n from history\n");
    printf("  Tab           Complete commands and filenames\n");
    printf("  Up/Down       Browse Readline command history\n");
    printf("Operators: < input  > output  >> append  | pipe  ; chain  & background\n");
}

int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
    }

    if (strcmp(arglist[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        const char* target = (arglist[1] != NULL) ? arglist[1] : getenv("HOME");
        if (target == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
        if (chdir(target) != 0) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        print_help();
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    return 0;
}
