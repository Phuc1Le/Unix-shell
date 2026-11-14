#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>
#include<fcntl.h>

int main(void){
    char command[1024];
    while(1){
        printf("minishell> ");
        fflush(stdout);
        if(fgets(command, sizeof(command), stdin) == NULL){
            break;
        }
        command[strcspn(command, "\n")] = '\0';
        char* args[64];
        int arg_c = 0;
        char* token = strtok(command, " \t");
        while(token != NULL && arg_c < 64){
            args[arg_c++] = token;
            token = strtok(NULL, " \t");
        }
        args[arg_c] = NULL;
        // Detect filename and redirection
        char *o_file = NULL;
        bool b = true;
        for(int i=0;i<arg_c;i++){
            if(strcmp(args[i], ">") == 0){
                if(i == 0){
                    fprintf(stderr, "minishell: missing command before >\n");
                    b = false;
                    break;
                }
                if(i == arg_c - 1){
                    fprintf(stderr, "minishell: missing filename after >\n");
                    b = false;
                    break;
                }
                if(i+1 != arg_c - 1){
                    fprintf(stderr, "minishell: too many arguments\n");
                    b = false;
                    break;
                }
                o_file = args[i+1];
                args[i] = NULL;
                break;
            }
        }
        if(!b) continue;
        if (strcmp(args[0], "exit") == 0) {
            if(o_file != NULL){
                fprintf(stderr, "minishell: Unsupported redirection error\n");
            }
            break;
        }
        // Fin
        if(strcmp(args[0], "cd") == 0){
            if(o_file != NULL){
                fprintf(stderr, "minishell: Unsupported redirection error\n");
                continue;
            }
            const char* path = arg_c > 1 ? args[1] : getenv("HOME");
            if(path == NULL || chdir(path) == -1){
                perror("cd");
            }
            continue;
        }
        pid_t pid = fork();
        if(pid < 0){
            perror("fork");
            continue;
        }
        else if(pid == 0){
            sleep(1);
            if(o_file != NULL){
                int fd = open(o_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
                if(fd < 0){
                    perror("open");
                    _exit(1);
                }
                if(dup2(fd, STDOUT_FILENO) == -1){
                    perror("dup2");
                    close(fd);
                    _exit(1);
                }
                close(fd);
            }
            execvp(args[0], args);
            int saved_errno = errno;
            perror("execvp");
            _exit(saved_errno == ENOENT ? 127 : 126);
        }
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            continue;
        }
    }
    return 0;
}