# Network Programming Assignment

## About the project
This repo contains my assignment submissions for the network programming course, BITS Pilani, second semester, 2021-22.

We were given two assignments, containing two questions each. We were required to do any three questions out of 4 questions. I did both of assignment 1 and first of assignment 2.
After taking permission from instructor, I was allowed to use C++ as long as I don't use any system call wrappers.

## Assignment 1
### Question 1
#### Shell

You are required to build a bash-like shell for the following requirements. Your program
should not use temporary files, `popen()`, `system()` library calls. It should only use system-call wrappers
from the library. It should not use sh or bash shells to execute a command.

1. Shell should wait for the user to enter a command. User can enter a command with multiple
arguments. Program should parse these arguments and pass them to `execv()` call. For every
command, shell should search for the file in PATH and print any error. Shell should also print
the pid, status of the process before asking for another command.

2. Shell should create a new process group for every command. When a command is run with
`&` at end, it is counted as background process group. Otherwise it should be run as fore-
ground process group (look at `tcsetpgrp()`). That means any signal generated in the terminal
should go only to the command running, not to the shell process. `fg` command should bring
the background job to fore ground. `bg` command starts the stopped job in the background.

3. Shell should support any number of commands in the pipeline. e.g. `ls|wc|wc|wc`. Print
details such as pipe fds, process pids and the steps. Redirection operators can be used in
combination with pipes.

4. Shell should support `#` operator. The meaning of this: it carries same semantics as pipe but
use message queue instead of pipe. The operator `##` works in this way: `ls ## wc , sort`.
output of `ls` should be replicated to both `wc` and `sort` using message queues

5. Shell should support `S` operator. The meaning of this: it carries same semantics as pipe but
use shared memory instead of pipe. The operator `SS` works in this way: Using example, `ls
SS wc, sort`. Output of `ls` should be replicated to both `wc` and `sort` using shared memory

6. Shell should support a command `daemonize` which takes the form `daemonize <program>`
and converts the program into a daemon process.

7. Shell should support `<`, `>`, and `>>` redirection operators. Print details such as fd of the file,
remapped fd.

#### Deliverables
1. Brief Design Document (.pdf).
2. `shell.c`.
