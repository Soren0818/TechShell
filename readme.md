Name: Soren Grunzinger

Description:

This program is a bash-like shell written in C. It shows a prompt with the
current working directory followed by $, reads a command line, parses it,
and runs it in a forked child process using execvp. The parser understands
double/single quotes, backslash-escaped spaces, and < / > redirection.
cd and exit are built in. Errors print as Error # (MESSAGE).

How to clone, compile, and run the program:

git clone https://github.com/Soren0818/TechShell

cd TechShell

gcc techshell.c -o techshell

./techshell



All required parts are implemented.
