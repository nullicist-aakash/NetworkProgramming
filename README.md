# Network Programming

## About the projects
This repo contains my labs and assignment submissions for the network programming course, BITS Pilani, second semester, 2021-22.
We were given two assignments, containing two questions each. We were required to do any three questions out of 4 questions. I did both of assignment 1 and first of assignment 2.
After taking permission from instructor, I was allowed to use C++ as long as I don't use any system call wrappers.

## Labs
The lab submissions and their corresponding questions are present in Labs folder.

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


### Question 2
#### Client and Server in Ring Topology

In this problem let us extend Message Queues network wide for the following characteristics.
1. One who writes a message is called a publisher and one who reads is called as subscriber. A
publisher tags a message with a topic. Anyone who subscribed to that topic can read that
message. There can be many subscribers and publishers for a topic but there can only be one
publisher for a given message.
2. Publisher program should provide an interface for the user to (i) create a topic. Publisher
also provides commands for (ii) sending a message, (iii) taking a file and send it as a series
of messages. When sending a message, topic must be specified. Each message can be up to
512 bytes.
3. Publisher program takes address of a Broker server as CLA. There can be several broker
servers on separate machines or on a single machine. The role of a broker server is to receive
messages from a publisher and store them on disk and send messages to a subscriber when
requested,
4. Publishers and subscribers may be connected to different brokers. The messages should reach
the right subscriber.
5. Subscriber program takes the address of a broker server as CLA at the startup. It allows a
user to (i) subscribe to a topic (ii) retrieve next message (iii) retrieve continuously all
messages. Subscriber should print the message id, and the message.
6. All brokers are connected in a circular topology. For message routing, the broker connected
to a subscriber, queries its neighbor brokers and they query further and so on. Each query
retrieves a bulk of messages limited by `BULK_LIMIT` (default=10).
7. Brokers store messages for a period of `MESSAGE_TIME_LIMIT` (default=1minute)
8. This system doesn’t guarantee FIFO order of messages. Think and propose any mechanism
that can guarantee FIFO order.

#### Deliverables
1. `Publisher.c`, `Subscriber.c`, `Broker.c`.
2. PDF file explaining design decisions and documentation.


## Assignment 2
### Question 1
#### Longest Common router path
A file contains `N` (>1000) URLs of webpages. A programmer wants to find out the longest
path common to all the given webpages. A path consists of intermediate router interface addresses
through which the packet travels. Using I/O multiplexing, conceive and implement a program which
finds out path for each of the URLs and the longest path common to them.
#### Deliverables
1. `pathfinder.c`.
2. PDF file explaining design decisions and documentation.

### Question 2
#### Coordination in TCP
Consider the following paragraph given in section 16.5 of the textbook.

>We provide this example using simultaneous connects because it is a nice example using
nonblocking I/O and one whose performance impact can be measured. It is also a feature
used by a popular Web application, the Netscape browser. There are pitfalls in this technique
if there is any congestion in the network. Chapter 21 of TCPv1 describes TCP's slow-start
and congestion avoidance algorithms in detail. __When multiple connections are established
from a client to a server, there is no communication between the connections at the TCP
layer. That is, if one connection encounters a packet loss, the other connections to the same
server are not notified, and it is highly probable that the other connections will soon encounter
packet loss unless they slow down. These additional connections are sending more packets
into an already congested network. This technique also increases the load at any given time
on the server.__

Consider the bold lines. It tells about the missing coordination among the TCP connections or
clients accessing the same web server. Design a solution that can enable this coordination considering
the fact that TCP doesn't tell the application immediately about the packet loss. Generally TCP
tries for a few times before it concludes about packet loss. You design should be able to detect
packet losses as soon as the host comes to know about it and notify the client who are accessing
that server. Your program should take care of accepting requests from clients, detecting packet
losses, and notifying the clients. Implement the solution designed.

#### Deliverables
1. `tcpclient.c`, `error_detector.c`.
2. PDF file explaining design decisions and documentation.
