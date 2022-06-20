/* Creates a UNIX shell interface that accepts and executes user commands.
 * Features:
 * 1) The ability for the parent process and child processes to run concurrently
 * 2) A history feature (!!) that repeats the last user command
 * 3) Input and output redirection with files
 * 4) Execution of up to two commands (with communication between processes via a pipe)
 * 5) Basic error handling
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#define MAX_LENGTH 80 // maxmimum length of a command

/** breaks the command into arguments
 * @pre none
 * @post none
 * @param args : array of command arguments
 * @param line : current user command
 * @param mods : array of command modifiers
 * @return none
 */
void parseLine(char* line, char* args[], char* mods[]) {
    // check for ampersand
    if (strcmp(&line[strlen(line) - 1], "&") == 0) {
        mods[0] = "1";
        line[strlen(line) - 1] = '\0'; // truncate ampersand
    }
    int counter = 0;
    char *token = strtok(line, " ");
    while (token != NULL) {
        if (strcmp(token, ">") == 0) {
            mods[1] = "1";
            mods[4] = strtok(NULL, " "); // get file name
        } else if (strcmp(token, "<") == 0) {
            mods[2] = "1";
            mods[4] = strtok(NULL, " "); // store file
        } else if (strcmp(token, "|") == 0) {
            mods[3] = "1";
            mods[5] = strtok(NULL, "\0"); // store second command
        } else {
            args[counter++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[counter] = NULL; // last element of array must be NULL for execvp()
}

int main(void) {
    // variables that remain constant throughout program
    int should_run = 1; 
    char history[MAX_LENGTH + 1] = {'\0'}; // history buffer, contains last user command

    while (should_run) {
        // variables that reset with each command
        char nextLine[MAX_LENGTH + 1]; // user input
        char* args[MAX_LENGTH/2 + 1]; // array of arguments
        char* mods[6] = {"0", "0", "0", "0", NULL, NULL}; // array of command modifiers ([0]&, [1]>, [2]<, [3]|, [4] file, [5] 2nd command) 
        int pipeFD[2]; // array of file descriptors for pipe

        fflush(stdout);
        printf("osh>");

        fgets(nextLine, MAX_LENGTH + 1, stdin);
        nextLine[strcspn(nextLine, "\n")] = 0; // delete trailing newline
        if (strcmp(nextLine, "exit") == 0) { // exits program
            should_run = 0;
        } else if (strcmp(nextLine, "\0") == 0) { // for empty user inputs
        } else if ((strcmp(nextLine, "!!") == 0) && (strcmp(history, "\0") == 0)) { // history feature
            printf("Error: No commands in history\n");
        } else {
            if ((strcmp(nextLine, "!!") == 0) && (strcmp(history, "\0") != 0)) { // history feature
                strncpy(nextLine, history, MAX_LENGTH + 1); // user input is copied from history buffer
            } else {
                strncpy(history, nextLine, MAX_LENGTH + 1); // history buffer is copied from user input
            }
            parseLine(nextLine, args, mods); // parse command line into arguments and modifiers

            if (args[0] == NULL) { // handles cases, like "<", ">" and "|"
                printf("Error: Syntax error near unexpected token '%s'.\n", nextLine);
            } else {
                // execute command(s)
                if (strcmp(mods[3], "1") == 0) { // create pipe, if necessary
                    if (pipe(pipeFD) < 0) {
                        printf("Error in pipe creation.\n");
                        break;
                    }
                }
                // create child process
                int pid = fork();
                if (pid < 0) {
                    printf("Error in process creation.\n");
                    break;
                } else if (pid == 0) { // child process
                    if (strcmp(mods[3], "1") == 0) { // set up communication via pipe
                        char* args2[MAX_LENGTH/2 + 1]; // argument array for second command
                        char* mods2[6]; // modifier array for second command
                        parseLine(mods[5], args2, mods2); 
                        
                        int pid2 = fork();
                        if (pid2 < 0) {
                            printf("Error in process creation.\n");
                            break;
                        } else if (pid2 == 0) { // grandchild process
                            close(pipeFD[0]); // close read end
                            dup2(pipeFD[1], STDOUT_FILENO); // duplicate stdout to write end
                            execvp(args[0], args);
                            // if command isn't recognized
                            printf("Error: Command '%s' not found.\n", args[0]);
                            exit(1);
                        } else { // parent process
                            wait(NULL);
                            close(pipeFD[1]); // close write end
                            dup2(pipeFD[0], STDIN_FILENO); // duplicate read end to stdin
                            execvp(args2[0], args2);
                            // if command isn't recognized
                            printf("Error: Command '%s' not found.\n", args2[0]);
                            exit(1);
                        }
                    } else {
                        if (strcmp(mods[1], "1") == 0) { // output redirection
                            int fd = open(mods[4], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU); // open/create file
                            if (fd > 0) {
                                dup2(fd, STDOUT_FILENO); // duplicate stdout into file
                            } else {
                                printf("Error in file open.\n");
                                break;
                            }
                        } else if (strcmp(mods[2], "1") == 0) { // input redirection
                            int fd = open(mods[4], O_RDONLY); // open file
                            if (fd > 0) {
                                dup2(fd, STDIN_FILENO); // duplicate file into stdin
                            } else {
                                printf("Error in file open.\n");
                                break;
                            }
                        }
                        execvp(args[0], args);
                        // if command isn't recognized
                        printf("Errdor: Command '%s' not found.\n", args[0]);
                        exit(1);
                    }
                } else { // parent process
                    if (strcmp(mods[3], "1") == 0) {
                        close(pipeFD[0]); // close read end
                        close(pipeFD[1]); // close write end
                    }
                    while (waitpid(-1, NULL, WNOHANG) != 0);
                    if (strcmp(mods[0], "0") == 0) { // parent process waits
                        wait(NULL);
                    } else {
                        waitpid(pid, NULL, WNOHANG);
                    }
                }
            }
        }
    }
    return 0;
}