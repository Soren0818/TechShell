// Name: Soren Grunzinger
// Description:

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
 
#define MAX_INPUT 4096
 
// Holds everything we learn from parsing one command line
struct ShellCommand{
    char** args;     // NULL-terminated argument vector (args[0] is the command)
    int argc;        // number of arguments
    char* inFile;    // file for < redirect, or NULL
    char* outFile;   // file for > redirect, or NULL
    int syntaxError; // nonzero if the line was malformed (e.g. "ls >")
};

//////// Some function ideas: ////////////
// Note: Some code is reflected in main that represents these functions,
// but it is up to you to determine how you want to organize your code.

// Print an error: Error # (MESSAGE)
static void printError(int fd, int err){
    dprintf(fd, "Error %d (%s)\n", err, strerror(err));
}

/* 
    A function that causes the prompt to 
    display in the terminal
*/

void displayPrompt(void){
    char cwd[MAX_INPUT];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strcpy(cwd, "?");
    printf("%s$ ", cwd);
    fflush(stdout);
}

/*
    A function that takes input from the user.
    It may return return the input to the calling statement or 
    store it at some memory location using a pointer.
*/ 

char* getInput(void){
    char* line = malloc(MAX_INPUT);
    if (line == NULL){
        perror("malloc");
        exit(1);
    }
    if (fgets(line, MAX_INPUT, stdin) == NULL){
        free(line);
        printf("\n");
        exit(0);
    }
    line[strcspn(line, "\n")] = '\0';
    return line;
}

// Read one word starting at line[*i], handling quotes and backslash escapes.
// Stops at unquoted whitespace, '<' or '>'. Advances *i. Returns malloc'd word.
static char* readWord(const char* line, int* i){
    char* buf = malloc(strlen(line) + 1);
    int len = 0;
    char quote = 0;

    if (buf == NULL){
        perror("malloc");
        exit(1);
    }

    while (line[*i] != '\0'){
        char c = line[*i];

        if (quote){

            if (c == quote)
                quote = 0;
            else if (quote == '"' && c == '\\' &&
                     (line[*i + 1] == '"' || line[*i + 1] == '\\')){
                (*i)++;
                buf[len++] = line[*i];
            }
            else
                buf[len++] = c;
        }
        else{

            if (c == '\\' && line[*i + 1] != '\0'){
                (*i)++;
                buf[len++] = line[*i];
            }
            else if (c == '"' || c == '\'')
                quote = c;
            else if (isspace((unsigned char)c) || c == '<' || c == '>')
                break;
            else
                buf[len++] = c;
        }
        (*i)++;
    }
    buf[len] = '\0';
    return buf;
}

/*
    A function that parses through the user input.
    Consider having this function return a struct that stores vital
    information about the parsed instruction such as:
    - The command itself
    - The arguments that come after the command 
        Hint: When formatting your data, 
        look into execvp and how it takes in args.
    - Information about if a redirect was detected such as >, <, or |
    - Information about whether or not a new file 
        needs to be created and what that filename may be.
    

    Some helpful functions when doing this come from string.h and stdlib.h, such as
    strtok, strcmp, strcpy, calloc, malloc, realloc, free, and more

    Be sure to consider/test for situations when a backslash is used to escape the space char
    and when quotes are used to group together various tokens.
*/

struct ShellCommand parseInput(char* line){
    struct ShellCommand cmd;
    int i = 0;
    size_t cap = strlen(line) + 2;

    cmd.args = calloc(cap, sizeof(char*));
    cmd.argc = 0;
    cmd.inFile = NULL;
    cmd.outFile = NULL;
    cmd.syntaxError = 0;
    if (cmd.args == NULL){
        perror("calloc");
        exit(1);
    }

    while (line[i] != '\0'){
        while (isspace((unsigned char)line[i]))
            i++;

        if (line[i] == '\0')
            break;

        if (line[i] == '<' || line[i] == '>'){
            char which = line[i++];
            while (isspace((unsigned char)line[i]))
                i++;

            // a filename must follow the redirect symbol
            if (line[i] == '\0' || line[i] == '<' || line[i] == '>'){
                cmd.syntaxError = 1;
                break;
            }
            char* name = readWord(line, &i);
            if (which == '<'){
                free(cmd.inFile);
                cmd.inFile = name;
            }
            else{
                free(cmd.outFile);
                cmd.outFile = name;
            }
        }
        else{
            cmd.args[cmd.argc++] = readWord(line, &i);
        }
    }
    cmd.args[cmd.argc] = NULL;

    return cmd;
}

// Free memory owned by a ShellCommand
static void freeCommand(struct ShellCommand* cmd){
    for (int k = 0; k < cmd->argc; k++)
        free(cmd->args[k]);
    free(cmd->args);
    free(cmd->inFile);
    free(cmd->outFile);
}

/*
    A function that executes the command. 
    This function might take in a struct that represents the shell command.

    Be sure to consider each of the following:
    1. The execvp() function. 
        This can execute commands that already exist, that is, 
        you don't need to recreate the functionality of 
        the commands on your computer, just the shell.
        Keep in mind that execvp takes over the current process.
    2. The fork() function. 
        This can create a process for execvp to take over.
    3. cd is not a command like ls and mkdir. 
        cd is a toold provided by the shell, 
        so you WILL need to recreate the functionality of cd.
    4. Be sure to handle standard output redirect and standard input redirects here 
        That is, there symbols: > and <. 
        Pipe isn't required but could be a nice addition.
*/

void executeCommand(struct ShellCommand cmd){
    if (cmd.syntaxError){
        printError(STDOUT_FILENO, EINVAL);
        return;
    }

    if (cmd.argc == 0)
        return;

    // built-in: cd (must change the shell's own directory)
    if (strcmp(cmd.args[0], "cd") == 0){
        const char* target = cmd.args[1];
        if (target == NULL)
            target = getenv("HOME");
        if (target == NULL || chdir(target) != 0)
            printError(STDOUT_FILENO, target == NULL ? ENOENT : errno);
        return;
    }

    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0){
        printError(STDOUT_FILENO, errno);
        return;
    }

    if (pid == 0){
        // Child: keep a copy of the real stdout for error messages so they still show on screen even when stdout is redirected to a file.
        int errfd = dup(STDOUT_FILENO);
        if (errfd >= 0)
            fcntl(errfd, F_SETFD, FD_CLOEXEC);
        else
            errfd = STDERR_FILENO;

        if (cmd.inFile != NULL){
            int fd = open(cmd.inFile, O_RDONLY);
            if (fd < 0){
                printError(errfd, errno);
                _exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (cmd.outFile != NULL){
            int fd = open(cmd.outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0){
                printError(errfd, errno);
                _exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(cmd.args[0], cmd.args);
        // only reached if exec failed
        printError(errfd, errno);
        _exit(1);
    }

    // Parent: wait for the child to finish
    int status;
    waitpid(pid, &status, 0);
}

int main(){ // MAIN
	char* input;
	struct ShellCommand command;
		
	// repeatedly prompt the user for input
	for (;;){
        // display the prompt
        displayPrompt();

	    // get the user's input
	    input = getInput();
	    
	    // parse the command line
	    command = parseInput(input);

         // exit is handled here so the shell itself quits
	    if (command.argc > 0 && strcmp(command.args[0], "exit") == 0){
	        freeCommand(&command);
	        free(input);
	        exit(0);
	    }
	    
	    // execute the command
	    executeCommand(command);

        freeCommand(&command);
	    free(input);
	}

	exit(0);
}