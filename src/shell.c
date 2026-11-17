#include "shell.h"

#define HISTORY_SIZE 20

static char* history[HISTORY_SIZE];
static int history_count = 0;
static int history_start = 0;

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*)malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        if (pos < MAX_LEN - 1) cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL;
    }

    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        memset(arglist[i], 0, ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;

        if (*cp == '\0') break;

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) {
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
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

static void print_help(void) {
    printf("Built-in commands:\n");
    printf("  exit          Exit the shell\n");
    printf("  cd <dir>      Change the current working directory\n");
    printf("  help          Show this help message\n");
    printf("  jobs          List background jobs (placeholder)\n");
    printf("  history       Show previously entered commands\n");
    printf("  !n            Re-execute command number n from history\n");
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
        printf("Job control not yet implemented.\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    return 0;
}
