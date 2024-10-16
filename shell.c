#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_COMMAND_LINE_LEN 1024
#define MAX_COMMAND_LINE_ARGS 128

#define CHILD_FINISHED_SUCCESSFULLY -1

// Global variables
char prompt[] = "> ";           // prints "> " before each command line entry
char delimiters[] = " \t\r\n";  // delimiters to check for in each command line entry
char *cur_dir;                  // the current directory the shell is in
pid_t pid = 0, pid1 = 0;        // pids for the child and grandchild, respectively
extern char **environ;          // stores all of the environment variables

// Additional functions

// Handles the CTRL-C signal input for our shell; just clears stdin and stdout
void sigint_handler(int signo) {
    fflush(stdout);
    fflush(stdin);
}

// Kills the child if the executed command takes longer than 10 seconds
void kill_child(int signo) {
    if (pid > 0) {
        kill(pid, SIGTERM);
    }
}

// Main
int main() {
    signal(SIGINT, sigint_handler);           // signal to handle CTRL-C
    char command_line[MAX_COMMAND_LINE_LEN];  // stores the full command line entry
    long dir_size;                            // size of the current directory
    char *buf;                                // buffer that helps get size of cwd
    char *comm, *arguments, *en_vars;         // stores the tokenized command line input

    while (true) {
        // Get current working directory (cwd)
        dir_size = pathconf(".", _PC_PATH_MAX);
        if ((buf = (char *)malloc((size_t)dir_size)) != NULL) {
            cur_dir = getcwd(buf, (size_t)dir_size);
        }

        do {
            // CHECK 0. Modify the prompt to print the current working directory
            // Print the shell prompt.
            printf("%s %s", cur_dir, prompt);
            fflush(stdout);
            fflush(stdin);

            // Read input from stdin and store it in command_line. If there's an
            // error, exit immediately. (If you want to learn more about this line,
            // you can Google "man fgets")
            if ((fgets(command_line, MAX_COMMAND_LINE_LEN, stdin) == NULL && ferror(stdin))) {
                fprintf(stderr, "fgets error");
                exit(0);
            }

            // If user just presses enter, keep shell alive
            if (command_line[0] == '\n') break;

            // CHECK 1. Tokenize the command line input (split it on whitespace)
            // Tokenize input
            arguments = strtok(command_line, delimiters);
            comm = arguments;
            arguments = strtok(NULL, delimiters);  // in order to skip the command itself

            // CHECK 2. Implement Built-In Commands

            // Change directory (cd)
            if (strcmp(comm, "cd") == 0) {
                if (chdir(arguments) == 0) {
                    cur_dir = getcwd(buf, (size_t)dir_size);
                } else
                    printf("Changing directory to '%s' failed: %s\n", arguments, strerror(errno));
            }

            // Print working directory (pwd)
            else if (strcmp(comm, "pwd") == 0) {
                printf("%s\n", cur_dir);
            }

            // Print rest of arguments (echo)
            else if (strcmp(comm, "echo") == 0) {
                while (arguments != NULL) {
                    if (arguments[0] == '$') {
                        arguments++;  // remove '$' character
                        if (getenv(arguments) != NULL)
                            printf("%s ", getenv(arguments));
                        else
                            printf("'%s' does not exist", arguments);
                    } else {
                        printf("%s ", arguments);
                    }

                    arguments = strtok(NULL, delimiters);

                    if (arguments == NULL) printf("\n");
                }
            }

            // Exit shell (exit)
            else if (strcmp(comm, "exit") == 0) {
                fflush(stdin);
                fflush(stdout);
                fflush(stderr);
                exit(0);
            }

            // View environment variables (env)
            else if (strcmp(comm, "env") == 0) {
                if (arguments == '\0') {
                    int i = 0;
                    while (environ[i] != NULL) {
                        printf("%s\n", environ[i]);
                        i++;
                    }
                } else {
                    if (getenv(arguments) != NULL)
                        printf("%s\n", getenv(arguments));
                    else
                        printf("'%s' does not exist\n", arguments);
                }
            }

            // Set environment variables (setenv)
            else if (strcmp(comm, "setenv") == 0) {
                if (arguments == '\0') {
                    printf("No args!\n");
                } else {
                    if (index(arguments, '=') != NULL) {  // if '=' is in the argument
                        char *left, *right;
                        en_vars = strtok(arguments, "=");
                        left = en_vars;
                        en_vars = strtok(NULL, "\n");
                        right = en_vars;

                        if (left && right) {
                            if (setenv(left, right, 1) != 0) {
                                printf("Error creating variable: %s\n", strerror(errno));
                            }
                        } else {
                            printf("Badly formatted variable\n");
                        }
                    } else {
                        printf("Badly formatted variable\n");
                    }
                }
            }

            // Handles rest of cmds and false cmds (via forking)
            else {
                // CHECK 3. Create a child process which will execute the command line input

                // Create child process
                pid = fork();

                // Child -- executes command
                if (pid == 0) {
                    // Get proper args format
                    char *max_commands[MAX_COMMAND_LINE_ARGS];
                    int i = 0;
                    max_commands[i++] = comm;
                    while (arguments != NULL || i >= MAX_COMMAND_LINE_ARGS) {
                        max_commands[i++] = arguments;
                        arguments = strtok(NULL, delimiters);
                    }
                    char *argsv[i + 1];
                    int j;
                    for (j = 0; j < i; j++) {
                        argsv[j] = max_commands[j];
                    }
                    argsv[j] = '\0';

                    // Signal to handle CTRL-C
                    signal(SIGINT, SIG_DFL);

                    // Check for background process; creates grandchild if bg process

                    // bg process
                    if (strcmp(argsv[j - 1], "&") == 0) {
                        // create grandchild process
                        pid1 = fork();

                        // Grandchild -- executes command
                        if (pid1 == 0) {
                            signal(SIGINT, SIG_IGN);  // ignores CTRL-C; have to kill manually
                            if (execvp(comm, argsv) == -1) {
                                printf("Extend Err: %s\n", strerror(errno));
                            }
                        }

                        // Error
                        else if (pid1 < 0) {
                            printf("Forking error\n");
                            exit(1);
                        }

                        // CHECK 4. The "parent" process should wait for the "child" to complete unless its a background process
                        // Child (as a parent) -- exits (no waiting)
                        else {
                            exit(0);
                        }
                    }

                    // fg process
                    else {
                        if (execvp(comm, argsv) == -1) {
                            printf("Extend Err: %s\n", strerror(errno));
                        }
                    }
                }

                // Error -- handles fork error
                else if (pid < 0) {
                    printf("Forking error\n");
                    exit(1);
                }

                // CHECK 4. The parent process should wait for the child to complete unless its a background process
                // Parent -- waits for executed command
                else {
                    signal(SIGALRM, kill_child);        // listens for alarm
                    alarm(10);                          // sets alarm for 10 seconds
                    wait(NULL);                         // waits for child to finish
                    pid = CHILD_FINISHED_SUCCESSFULLY;  // condition in kill_child to check for; only occurs if process finishes before 10 seconds
                }
            }

        } while (command_line[0] == 0x0A);  // while just ENTER pressed

        // If the user input was EOF (ctrl+d), exit the shell.
        if (feof(stdin)) {
            printf("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;
        }

    }
    // This should never be reached.
    return -1;
}