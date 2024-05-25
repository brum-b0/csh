#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>

typedef struct {
    char *cmd;
    char *args[1000];
} Command;

//global 
char path[1000] = "/bin /usr/bin";//path for execv

void ccshError() {//generic error requested by guidelines as the only error message
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

void resetCommands(Command commands[]) {
    for (int i = 0; i < 1000; i++) {
        commands[i].cmd = NULL;
        for (int j = 0; j < 1000; j++) {
            commands[i].args[j] = NULL;
        }
    }
}

void parse(char *line, Command commands[]) {
    char *ampersand = strchr(line, '&');

    if (ampersand == NULL) {// No ampersand, parse single command
        
        int argsIndex = 0;
        char *token = NULL;
        char *rest = line;

        while ((token = strsep(&rest, " ")) != NULL) {
            if (commands[0].cmd == NULL) {
                commands[0].cmd = token;// First token is cmd
                commands[0].args[argsIndex++] = token;//first arg should also be the cmd??? 
            } else {
                
                commands[0].args[argsIndex++] = token;//all other tokens are args
            }
        }

        commands[0].args[argsIndex] = NULL;  // Null terminate args

    } else {// Ampersand found, parse parallel commands

        // Parse line into cmd and args for parallel commands
        int cmdIndex = 0;
        int argsIndex = 0;

        char *token = NULL;
        char *rest = line;

        while ((token = strsep(&rest, " ")) != NULL) {//parse line left to right

            if (strcmp(token, "&") == 0) {//when it reaches the ampersand
                commands[cmdIndex].args[argsIndex] = NULL;  // Null terminate the args
                cmdIndex++;  // Move to the next command
                argsIndex = 0;  // Reset args index
            } else {//not reached ampersand yet
                if (commands[cmdIndex].cmd == NULL) {
                    commands[cmdIndex].cmd = token;  // First token is cmd
                    commands[cmdIndex].args[argsIndex++] = token;//first arg should also be the cmd???
                } else {
                    
                    commands[cmdIndex].args[argsIndex++] = token;//all other tokens are args
                }
            }
        }

        commands[cmdIndex].args[argsIndex] = NULL;  // Null terminate args for the last command
        commands[cmdIndex + 1].cmd = NULL;  // Null terminate the array of commands
    }
}


void execBuiltIn(Command command) {

    if (strcmp(command.cmd, "exit") == 0) {//exit
        exit(0);
        
    } else if (strcmp(command.cmd, "cd") == 0) {//cd
        
        if (command.args[0] == NULL) {//no dir specified
            //printf("no dir specified");
            ccshError();//write error
        }
        else {
            if (chdir(command.args[1]) != 0) {//change directory
                //printf("no such directory");
                ccshError();//write error
            }
        }
    } else if (strcmp(command.cmd, "path") == 0) {//path
        // Clear the existing path
        path[0] = '\0';

        // Concatenate all directories from args to path
        for (int i = 1; command.args[i] != NULL; i++) {
            strcat(path, command.args[i]);
            if (command.args[i + 1] != NULL) {
                strcat(path, " ");
            }
        }
}

}

void execCmd(Command commands[]) {
    pid_t children[1000];  // array to hold children pids
    int numChildren = 0;   // number of children

    for (int i = 0; commands[i].cmd != NULL; i++) {
        pid_t pid = fork();  // fork for each command

        if (pid < 0) {  // fork failed
            //printf("fork failed\n");
            ccshError();  // write error
        } else if (pid == 0) {  // child
            char *pathCopy = strdup(path);  // copy path to avoid modifying global path
            char *dir = NULL;  // dir to be concatenated with cmd

            while ((dir = strsep(&pathCopy, " ")) != NULL) {
                char xcmd[1000];
                sprintf(xcmd, "%s/%s", dir, commands[i].cmd);  // concatenate dir and cmd
                //printf("Executing: %s with args %s\n", xcmd, commands[i].args[1]);

                if (access(xcmd, X_OK) == 0) {  // check access
                    execv(xcmd, commands[i].args);  // execute cmd, should replace the current child
                }
                dir = strchr(dir, ' ');  // get next dir
                if (dir != NULL) {
                    *dir = '\0';  // replace space with null terminator
                    dir++;  // move to the next character
                }
            }

            // if we get here, execv failed
            //printf("command not found, execv failed\n");
            ccshError();  // write error
            free(pathCopy);  // free pathCopy
            exit(1);  // exit child
        } else {  // parent
            children[numChildren++] = pid;  // add child to children array
        }
    }

    // Wait for all children to finish
    for (int j = 0; j < numChildren; j++) {
        waitpid(children[j], NULL, 0);
    }
}



int main(int argc, char *argv[]) {

    if (argc > 2) {//too many args
        printf("Usage: ./ccsh or ./ccsh [batchFile]\n");
        exit(1);
    }

    FILE *input = stdin;//default input is stdin

    if (argc == 2) {//batch mode
        input = fopen(argv[1], "r");// if a batch file is specified input is changed to the opened file
        if (input == NULL) {//file not found
            //printf("file not found\n");
            ccshError();//write error
            exit(1);
        }
    }

   
    char *line = NULL;//line to be read
    size_t size = 0;//size of line

    while (1) {
        if (argc == 1) {//only print prompt for interactive mode
            printf("ccsh> ");
            
        }

        //read cmd from input
        if (getline(&line, &size, input) == -1) {//end of file
            if (argc == 1) {
                printf("\n");//looks better for interactive mode
            }

            break;
        }

        //remove newline 
        if (line[strlen(line) - 1] == '\n'){
            line[strlen(line) - 1] = '\0';
        }
        

       //parse cmds
       Command commands[1000];
       parse(line, commands);

       if (commands[0].cmd == NULL) {//empty line
           continue;//go next
        }

        ///////////begin executing cmds
        //check if built in, should be the only command struct stored in the array if so, can't imagine anyone actually parallellizing the builtins
        bool builtIn = false;
        if (strcmp(commands[0].cmd, "exit") == 0 || strcmp(commands[0].cmd, "cd") == 0 || strcmp(commands[0].cmd, "path") == 0) {
            builtIn = true;

        }

        //execute cmds
        if (builtIn) {//if a builtin, call execBuiltIn
            execBuiltIn(commands[0]);//this can be put here or inside execCmd, I chose here because it's more readable
        }
        else {
            execCmd(commands);//else call outside
        }
        
        resetCommands(commands);//reset commands array for next line

        //free memory so no leaks
        if (line) {
            free(line);
            
            line = NULL;

        }
        

    }
    //close input for batch mode
    if (input != stdin) {
        fclose(input);
    }

    return 0;
}