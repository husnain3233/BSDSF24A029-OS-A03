#include "shell.h"
#include <fcntl.h>

static void close_pipes(int pipes[MAX_PIPES - 1][2], int stages) {
    for (int i = 0; i < stages - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

static void apply_redirection(Command* cmd) {
    if (cmd->infile != NULL) {
        int fd = open(cmd->infile, O_RDONLY);
        if (fd < 0) {
            perror(cmd->infile);
            _exit(1);
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2");
            _exit(1);
        }
        close(fd);
    }

    if (cmd->outfile != NULL) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->outfile, flags, 0644);
        if (fd < 0) {
            perror(cmd->outfile);
            _exit(1);
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            _exit(1);
        }
        close(fd);
    }
}

int execute_pipeline(Pipeline* pipeline) {
    int pipes[MAX_PIPES - 1][2];
    pid_t pids[MAX_PIPES];
    int stages = pipeline->count;
    int status = 0;

    if (stages <= 0) {
        return 0;
    }

    for (int i = 0; i < stages - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe failed");
            close_pipes(pipes, stages);
            return 1;
        }
    }

    fflush(stdout);

    for (int i = 0; i < stages; i++) {
        pids[i] = fork();

        if (pids[i] == 0) {
            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                perror("dup2");
                _exit(1);
            }
            if (i < stages - 1 && dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                perror("dup2");
                _exit(1);
            }

            close_pipes(pipes, stages);
            apply_redirection(&pipeline->commands[i]);

            execvp(pipeline->commands[i].argv[0], pipeline->commands[i].argv);
            perror("Command not found");
            _exit(127);
        }

        if (pids[i] < 0) {
            perror("fork failed");
            close_pipes(pipes, stages);
            for (int j = 0; j < i; j++) {
                waitpid(pids[j], NULL, 0);
            }
            return 1;
        }
    }

    close_pipes(pipes, stages);

    for (int i = 0; i < stages; i++) {
        int child_status;
        while (waitpid(pids[i], &child_status, 0) < 0 && errno == EINTR) {
            continue;
        }
        if (i == stages - 1) {
            status = child_status;
        }
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}
