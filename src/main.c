#include "shell.h"

int main() {
    char* raw;
    char* cmdline;
    char** arglist;

    initialize_readline();

    while ((raw = read_cmd(PROMPT, stdin)) != NULL) {
        cmdline = resolve_history(raw);
        free(raw);

        if (cmdline == NULL) {
            continue;
        }

        add_to_history(cmdline);

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (!handle_builtin(arglist)) {
                execute(arglist);
            }

            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
