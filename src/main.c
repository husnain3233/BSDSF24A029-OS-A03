#include "shell.h"
#include <ctype.h>

#define MAX_IF_LINES 256

static const char* skip_spaces(const char* text) {
    while (text != NULL && isspace((unsigned char)*text)) text++;
    return text;
}

static char* trim(char* text) {
    char* end;
    while (isspace((unsigned char)*text)) text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
    return text;
}

static int is_keyword(const char* line, const char* keyword) {
    const char* p = skip_spaces(line);
    size_t len = strlen(keyword);
    return strncmp(p, keyword, len) == 0 &&
           (p[len] == '\0' || isspace((unsigned char)p[len]));
}

static int run_line(const char* line) {
    ChainItem items[MAX_CHAIN];
    int count = split_chain(line, items, MAX_CHAIN);
    int status = 0;

    if (count <= 0) {
        return count < 0 ? 1 : 0;
    }

    for (int i = 0; i < count; i++) {
        Pipeline pipeline;
        if (parse_pipeline(items[i].text, &pipeline)) {
            int handled = 0;
            Command* single = &pipeline.commands[0];
            if (pipeline.count == 1 && single->infile == NULL && single->outfile == NULL) {
                handled = handle_builtin(single->argv);
            }
            if (!handled) {
                status = execute_pipeline(&pipeline, items[i].background, items[i].text);
            }
            free_pipeline(&pipeline);
        }
    }

    free_chain(items, count);
    return status;
}

static void run_block(char** lines, int start, int end) {
    for (int i = start; i < end; i++) {
        if (lines[i][0] != '\0') {
            run_line(lines[i]);
        }
    }
}

static int execute_if_block(const char* block) {
    char* copy = strdup(block);
    char* lines[MAX_IF_LINES];
    int nlines = 0;
    int then_index = -1;
    int else_index = -1;
    int fi_index = -1;
    int condition_status = 1;
    char* saveptr = NULL;

    if (copy == NULL) {
        return 1;
    }

    for (char* p = strtok_r(copy, "\n", &saveptr);
         p != NULL && nlines < MAX_IF_LINES;
         p = strtok_r(NULL, "\n", &saveptr)) {
        lines[nlines++] = strdup(p);
    }

    if (nlines < 3) {
        fprintf(stderr, "if: incomplete block\n");
        goto cleanup;
    }

    for (int i = 1; i < nlines; i++) {
        char* probe = strdup(lines[i]);
        char* keyword = trim(probe);
        if (then_index < 0 && is_keyword(keyword, "then")) {
            then_index = i;
        } else if (else_index < 0 && is_keyword(keyword, "else")) {
            else_index = i;
        } else if (is_keyword(keyword, "fi")) {
            fi_index = i;
        }
        free(probe);
    }

    if (then_index < 0 || fi_index < 0 || fi_index <= then_index) {
        fprintf(stderr, "if: expected then and fi\n");
        goto cleanup;
    }

    {
        char* allocated = strdup(lines[0]);
        char* condition = trim(allocated);
        if (is_keyword(condition, "if")) {
            condition += 2;
        }
        condition = trim(condition);
        if (*condition == '\0') {
            fprintf(stderr, "if: missing condition\n");
            free(allocated);
            goto cleanup;
        }
        condition_status = run_line(condition);
        free(allocated);
    }

    if (condition_status == 0) {
        int end = (else_index > then_index) ? else_index : fi_index;
        run_block(lines, then_index + 1, end);
    } else if (else_index > then_index) {
        run_block(lines, else_index + 1, fi_index);
    }

cleanup:
    for (int i = 0; i < nlines; i++) {
        free(lines[i]);
    }
    free(copy);
    return condition_status;
}

int main() {
    char* raw;
    char* cmdline;
    ChainItem items[MAX_CHAIN];
    int item_count;

    initialize_readline();

    while (1) {
        reap_jobs();

        raw = read_full_command(PROMPT);
        if (raw == NULL) {
            break;
        }

        cmdline = resolve_history(raw);
        free(raw);
        if (cmdline == NULL) {
            continue;
        }

        add_to_history(cmdline);

        if (is_keyword(cmdline, "if")) {
            execute_if_block(cmdline);
        } else {
            item_count = split_chain(cmdline, items, MAX_CHAIN);
            if (item_count > 0) {
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
            }
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
