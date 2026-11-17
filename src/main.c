#include "shell.h"

int main() {
    char* raw;
    char* cmdline;
    ChainItem items[MAX_CHAIN];
    int item_count;

    initialize_readline();

    while (1) {
        reap_jobs();

        raw = read_cmd(PROMPT, stdin);
        if (raw == NULL) {
            break;
        }

        cmdline = resolve_history(raw);
        free(raw);

        if (cmdline == NULL) {
            continue;
        }

        add_to_history(cmdline);

        item_count = split_chain(cmdline, items, MAX_CHAIN);
        if (item_count <= 0) {
            free(cmdline);
            if (item_count < 0) continue;
            continue;
        }

        for (int i = 0; i < item_count; i++) {
            Pipeline pipeline;
            if (parse_pipeline(items[i].text, &pipeline)) {
                int handled = 0;
                Command* single = &pipeline.commands[0];
                if (pipeline.count == 1 && single->infile == NULL && single->outfile == NULL) {
                    handled = handle_builtin(single->argv);
                }
                if (!handled) {
                    execute_pipeline(&pipeline, items[i].background, items[i].text);
                }
                free_pipeline(&pipeline);
            }
        }

        free_chain(items, item_count);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
