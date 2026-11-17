# BSDSF24A029-OS-A03

A custom UNIX shell built in C for the Operating Systems programming assignment.

## Build

Requires GCC, Make, and GNU Readline development files.

```bash
make
```

## Run

```bash
./bin/myshell
```

Features include built-in commands, command history with `!n`, Readline editing and completion, I/O redirection, pipes, background jobs, `if-then-else-fi`, and shell variables.

## Clean

```bash
make clean
```

## Layout

- `src/` - C source files
- `include/` - headers
- `bin/` - compiled shell
- `obj/` - object files
