# MiniShell — A Unix Shell in C

A lightweight Unix shell implemented from scratch in C to explore fundamental operating system concepts, including process creation, file descriptors, inter-process communication, and system calls.

## Features

- **Command execution:** Run external programs using `fork()`, `execvp()`, and `waitpid()`.
- **Built-in commands:** Support for `cd` and `exit`.
- **I/O redirection:** Redirect standard input and output using `<`, `>`, and `>>`.
- **Pipelines:** Connect commands using `|`.
- **Combined operations:** Support pipes and file redirection in the same command.
- **Error handling:** Handle failures in process creation, execution, and file operations.

## Getting Started
Compile and run (assuming the source file is `main.c`):

```bash
gcc -Wall -Wextra -o minishell main.c
./minishell
```
## Example Commands

```bash
ls -l
cd ..
echo hello
ls | wc -l
cat < input.txt
echo hello > output.txt
echo world >> output.txt
cat < input.txt | sort > output.txt
exit
```
## Purpose

Built as a hands-on systems programming project to understand how Unix shells interact with the operating system, rather than relying on higher-level process management abstractions.
