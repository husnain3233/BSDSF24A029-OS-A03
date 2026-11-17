#include "shell.h"

int main() {
    char* raw;
    char* cmdline;
    Pipeline pipeline;

    initialize_readline();

    while ((raw = read_cmd(PROMPT, stdin)) != NULL) {
        cmdline = resolve_history(raw);
        free(raw);

        if (cmdline == NULL) {
            continue;
        }

        add_to_history(cmdline);

        if (parse_pipeline(cmdline, &pipeline)) {
            int handled = 0;
            Command* single = &pipeline.commands[0];
            if (pipeline.count == 1 && single->infile == NULL && single->outfile == NULL) {
                handled = handle_builtin(single->argv);
            }
            if (!handled) {
                execute_pipeline(&pipeline);
            }
            free_pipeline(&pipeline);
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
