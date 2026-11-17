#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>
#include<fcntl.h>

typedef struct{
    char *argv[128];
    char *i_file;
    char *o_file;
    bool append;
} Command;

bool is_operator(char *token){
    if(token == NULL) return false;
    if(strcmp(token, ">") == 0 || strcmp(token, "<") == 0|| strcmp(token, ">>") == 0 || strcmp(token, "|") == 0) return true;
    return false;
}

int setup_redirection(const Command *cmd) {
    if(cmd->i_file != NULL){
        int fd = open(cmd->i_file, O_RDONLY);
        if(fd < 0){
            perror("open");
            return 1;
        }
        if(dup2(fd, STDIN_FILENO) == -1){
            perror("dup2");
            close(fd);
            return 1;
        }
        if(fd != STDIN_FILENO) close(fd);
    }
    if(cmd->o_file != NULL){
        int flags = O_WRONLY | O_CREAT;
        if(cmd->append) flags = flags | O_APPEND;
        else flags = flags | O_TRUNC;
        int fd = open(cmd->o_file, flags, 0644);
        if(fd < 0){
            perror("open");
            return 1;
        }
        if(dup2(fd, STDOUT_FILENO) == -1){
            perror("dup2");
            close(fd);
            return 1;
        }
        if(fd != STDOUT_FILENO) close(fd);
    }
    return 0;
}
int main(void){
    char command[1024];
    
    while(1){
        Command commands[2] = {0};
        printf("minishell> ");
        fflush(stdout);
        if(fgets(command, sizeof(command), stdin) == NULL){
            break;
        }
        command[strcspn(command, "\n")] = '\0';
        // char* args[64];
        int arg_c = 0;
        char* token = strtok(command, " \t");
        int is_next = -1;
        int cmd_idx = 0;
        bool mul_pipe = true;
        bool mis_cmd = false;
        bool is_ope = false;
        while(token != NULL){
            if(is_next == 0){
                if(is_operator(token)){
                    is_ope = true;
                    break;
                }
                commands[cmd_idx].i_file = token;
                is_next = -1;
            }
            else if(is_next == 1){
                if(is_operator(token)){
                    is_ope = true;
                    break;
                }
                commands[cmd_idx].o_file = token;
                commands[cmd_idx].append = false;
                is_next = -1;
            }
            else if(is_next == 2){
                if(is_operator(token)){
                    is_ope = true;
                    break;
                }
                commands[cmd_idx].o_file = token;
                commands[cmd_idx].append = true;
                is_next = -1;
            }
            else{
                if(strcmp(token, ">") == 0) is_next = 1; 
                else if(strcmp(token, ">>") == 0) is_next = 2;
                else if(strcmp(token, "<") == 0) is_next = 0;
                else if(strcmp(token, "|") == 0 ){
                    if (commands[cmd_idx].argv[0] == NULL) {
                        mis_cmd = true;
                        break;
                    }
                    if(cmd_idx == 1){
                        mul_pipe = false;
                        break;
                    }
                    cmd_idx++;
                    arg_c = 0;
                }
                else{
                    if (arg_c >= 127) {
                        fprintf(stderr, "minishell: too many arguments\n");
                        break;
                    }
                    commands[cmd_idx].argv[arg_c++] = token;
                }  
            }
            token = strtok(NULL, " \t");
        }
        if(is_next != -1){
            fprintf(stderr, "minishell: missing input or output file\n");
            continue;
        } 
        if(!mul_pipe){
            fprintf(stderr, "minishell: multiple pipes not supported\n");
            continue;
        }
        if(is_ope){
            fprintf(stderr, "minishell: wrong file format\n");
            continue;
        }
        for(int i=0;i<=cmd_idx;i++){
            if(commands[i].argv[0] == NULL){
                mis_cmd = true;
                break;
            }
        }
        if(mis_cmd){
            fprintf(stderr, "minishell: missing command\n");
            continue;
        }
        if (cmd_idx == 0 && strcmp(commands[0].argv[0], "exit") == 0){
            if (commands[0].i_file != NULL || commands[0].o_file != NULL) {
                fprintf(stderr, "minishell: redirection not supported for cd\n");
                continue;
            }
            break;
        }
        if (cmd_idx == 0 && strcmp(commands[0].argv[0], "cd") == 0) {
            if (commands[0].i_file != NULL || commands[0].o_file != NULL) {
                fprintf(stderr, "minishell: redirection not supported for cd\n");
                continue;
            }
            const char *path = commands[0].argv[1] ? commands[0].argv[1] : getenv("HOME");
            if (path == NULL || chdir(path) == -1) {
                perror("cd");
            }
            continue;
        }
        // Handle 2-command pipeline
        if(cmd_idx == 1){
            int pipefd[2];
            if(pipe(pipefd) == -1){
                perror("pipe");
                continue;
            }
            pid_t pid1 = fork();
            if(pid1 < 0){
                perror("fork");
                close(pipefd[0]);
                close(pipefd[1]);
                continue;
            }
            else if(pid1 == 0){
                close(pipefd[0]);
                if(dup2(pipefd[1], STDOUT_FILENO) == -1){
                    perror("dup2");
                    _exit(1);
                }
                if(pipefd[1] != STDOUT_FILENO) close(pipefd[1]);
                if (setup_redirection(&commands[0]) == -1) _exit(1);
                execvp(commands[0].argv[0], commands[0].argv);
                int saved_errno = errno;
                perror("execvp");
                _exit(saved_errno == ENOENT ? 127 : 126);
            }

            pid_t pid2 = fork();
            if(pid2 < 0){
                perror("fork");
                close(pipefd[0]);
                close(pipefd[1]);
                kill(pid1, SIGTERM);
                if(waitpid(pid1, NULL, 0) == -1){
                    perror("waitpid");
                    continue;
                }
                continue;
            }
            else if(pid2 == 0){
                close(pipefd[1]);
                if(dup2(pipefd[0], STDIN_FILENO) == -1){
                    perror("dup2");
                    _exit(1);
                }
                if(pipefd[0] != STDIN_FILENO) close(pipefd[0]);
                if (setup_redirection(&commands[1]) == -1) _exit(1);
                execvp(commands[1].argv[0], commands[1].argv);
                int saved_errno = errno;
                perror("execvp");
                _exit(saved_errno == ENOENT ? 127 : 126);
            }
            close(pipefd[0]);
            close(pipefd[1]);
            if(waitpid(pid1, NULL, 0) == -1){
                perror("waitpid");
            }
            if(waitpid(pid2, NULL, 0) == -1){
                perror("waitpid");
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
            if (setup_redirection(&commands[0]) == -1)
                _exit(1);
            execvp(commands[0].argv[0], commands[0].argv);
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