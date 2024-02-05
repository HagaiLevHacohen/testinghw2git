#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>


void mySignalHandlerZombie(int signum) {
    int wait_err, status;
    wait_err = waitpid(-1, &status, WNOHANG);
    if (wait_err == -1 && errno != ECHILD && errno != EINTR){
        perror("error in wait");
        exit(1);
    }
    return;
}

int prepare(void){
    // Overwrite default behavior for SIGINT to ignore
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("Signal handle registration failed");
        return 1;
    }
    // Overwrite default behavior for ECHILD
    // setting SA_RESTART as a flag, incase the SIGCHLD 
    // is sent while the parent proccess is in the middle of waitpid
    struct sigaction newActionZombie = {.sa_handler = mySignalHandlerZombie, .sa_flags = SA_RESTART};
    if (sigaction(SIGCHLD, &newActionZombie, NULL) == -1) {
        perror("Signal handle registration failed");
        return 1;
    }
    return 0;
}

int finalize(void){
    return 0;
}

int execute_command_foreground(char** arglist){
    int status;
    pid_t cpid, wait_err;
    int exe_err;
    // initializing status
    status = 0;
    cpid = fork();
    if (cpid == -1){
        // Fork has failed
        perror("Fork error.");
        return 0;
    }
    else if (cpid == 0){
        // Child proccess
        // setting SIGINT to default behavior
        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
            perror("Signal handle registration failed");
            return 1;
        }
        // executing command
        exe_err = execvp(arglist[0], arglist);
        if (exe_err == -1){
            // Execvp has failed
            perror("Execvp error - Invalid command.");
            exit(1);
        }
    }
    else {
        // Parent process
        wait_err = waitpid(cpid, &status, 0);
        if (wait_err == -1){
            if (errno == ECHILD || errno == EINTR) {
                // do nothing, and continue the program flow
            }
            else {
                perror("Waitpid error.");
                return 0;
            }
        }
        return 1;
    }
    return 0;
}
int execute_command_background(int count, char**  arglist){
    pid_t cpid;
    int exe_err;
    // cut the & element from the argument list
    arglist[count - 1] = NULL;
    cpid = fork();
    if (cpid == -1){
        // Fork has failed
        perror("Fork error.");
        return 0;
    }
    else if (cpid == 0){
        // Child proccess
        // executing command
        exe_err = execvp(arglist[0], arglist);
        if (exe_err == -1){
            // Execvp has failed
            perror("Execvp error - Invalid command.");
            exit(1);
        }
    }
    else{
        // Parent process
        return 1;
    }
    return 0;
}

int input_redirection(int count, char** arglist){
    int status;
    pid_t cpid, wait_err;
    int dup2_err, exe_err;
    int fd;
    // initializing status
    status = 0;
    // opening the input file
    fd = open(arglist[count - 1], O_RDONLY);
    if (fd == -1){
        perror("Open file error");
        return 0;
    }
    cpid = fork();
    if (cpid == -1){
        // Fork has failed
        perror("Fork error.");
        return 0;
    }
    else if (cpid == 0){
        // Child proccess
        // setting SIGINT to default behavior
        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
            perror("Signal handle registration failed");
            return 1;
        }
        // standart input file descriptor will now become a file descriptor for the file
        dup2_err = dup2(fd, STDIN_FILENO);
        if (dup2_err == -1){
            // dup2 systemcall failed
            perror("dup2 error");
            exit(1);
        }
        arglist[count - 2] = NULL;
        exe_err = execvp(arglist[0], arglist);
        if (exe_err == -1){
            // Execvp has failed
            perror("Execvp error - Invalid command.");
            exit(1);
        }    
    }
    else{
        // Parent process
        wait_err = waitpid(cpid, &status, 0);
        if (wait_err == -1){
            if (errno == ECHILD || errno == EINTR) {
                // do nothing, and continue the program flow
            }
            else {
                perror("Waitpid error.");
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

int output_redirection(int count, char** arglist){
    int status;
    pid_t cpid, wait_err;
    int dup2_err, exe_err;
    int fd;
    arglist[count - 2] = NULL;
    // initializing status
    status = 0;
    // opening the input file
    fd = open(arglist[count - 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1){
        perror("Open file error");
        return 0;
    }
    cpid = fork();
    if (cpid == -1){
        // Fork has failed
        perror("Fork error.");
        return 0;
    }
    else if (cpid == 0){
        // Child proccess
        // setting SIGINT to default behavior
        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
            perror("Signal handle registration failed");
            return 1;
        }
        // standart output file descriptor will now become a file descriptor for the file
        dup2_err = dup2(fd, STDOUT_FILENO);
        if (dup2_err == -1){
            // dup2 systemcall failed
            perror("dup2 error");
            exit(1);
        }
        arglist[count - 2] = NULL;
        exe_err = execvp(arglist[0], arglist);
        if (exe_err == -1){
            // Execvp has failed
            perror("Execvp error - Invalid command.");
            exit(1);
        }
    }
    else{
        // Parent process
        wait_err = waitpid(cpid, &status, 0);
        if (wait_err == -1){
            if (errno == ECHILD || errno == EINTR) {
                // do nothing, and continue the program flow
            }
            else {
                perror("Waitpid error.");
                return 0;
            }
        }
        return 1; 
    }
    return 0;
}

int index_of_pipe(int count, char** arglist){
    // finding the index symbol | in arglist. if it doesn't exists
    // the function returns -1
    int index, i;
    index = -1;
    for (i = 0; i < count; i++){
        if (arglist[i][0] == '|'){
            index = i;
            return index;
        }
    }
    return index;
}

int piping(int count, char** arglist, int index){
    int status1, status2;
    pid_t cpid1, cpid2, wait_err, pipe_err;
    int dup2_err, exe_err;
    int pipefd[2];
    char** arglist2;
    // initializing status1 and status2
    status1 = 0;
    status2 = 0;
    // creating a pipe
    pipe_err = pipe(pipefd);
    if (pipe_err == -1){
        perror("pipe error");
        return 0;
    }
    cpid1 = fork();
    if (cpid1 == -1){
        // Fork has failed
        perror("Fork error.");
        return 0;
    }
    else if (cpid1 == 0){
        // Child 1 proccess
        // setting SIGINT to default behavior
        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
            perror("Signal handle registration failed");
            return 1;
        }
        // closing the read end of the pipe
        close(pipefd[0]);
        // standart output file descriptor will now become a file descriptor for the write end of the pipe
        dup2_err = dup2(pipefd[1], STDOUT_FILENO);
        if (dup2_err == -1){
            // dup2 systemcall failed
            perror("dup2 error");
            exit(1);
        }
        // cutting the arglist from the | symbol onwards
        arglist[index] = NULL;
        exe_err = execvp(arglist[0], arglist);
        if (exe_err == -1){
            // Execvp has failed
            perror("Execvp error - Invalid command.");
            exit(1);
        }
    }
    else{
        // Parent process
        cpid2 = fork();
        if (cpid2 == -1){
            // Fork has failed
            perror("Fork error.");
            return 0;
        }
        else if (cpid2 == 0){
            // Child 2 proccess
            // setting SIGINT to default behavior
            if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
                perror("Signal handle registration failed");
                return 1;
            }
            // closing the write end of the pipe
            close(pipefd[1]);
            // standart input file descriptor will now become a file descriptor for the read end of the pipe
            dup2_err = dup2(pipefd[0], STDIN_FILENO);
            if (dup2_err == -1){
                // dup2 systemcall failed
                perror("dup2 error");
                exit(1);
            }
            // cutting the arglist until the | symbol (included)
            arglist2 = arglist + (index + 1);
            exe_err = execvp(arglist2[0], arglist2);
            if (exe_err == -1){
                // Execvp has failed
                perror("Execvp error - Invalid command.");
                exit(1);
            }
        }
        else{
            // Parent proccess
            // close both read and write end of the pipe
            close(pipefd[0]);
            close(pipefd[1]);
            // wait both children processes
            wait_err = waitpid(cpid1, &status1, 0);
            if (wait_err == -1){
                if (errno == ECHILD || errno == EINTR) {
                    // do nothing, and continue the program flow
                }
                else {
                    perror("Waitpid error.");
                    return 0;
                }
            }
            wait_err = waitpid(cpid2, &status2, 0);
            if (wait_err == -1){
                if (errno == ECHILD || errno == EINTR) {
                    // do nothing, and continue the program flow
                }
                else {
                    perror("Waitpid error.");
                    return 0;
                }
            }
            return 1;
        }
    }
    return 0;
}

int process_arglist(int count, char** arglist){
    int index;
    if (arglist[count - 1][0] == '&'){
        return execute_command_background(count, arglist);
    }
    else if ((count > 1) && (arglist[count - 2][0] == '<')){
        return input_redirection(count, arglist);
    }
    else if ((count > 1) && (arglist[count - 2][0]  == '>')){
        return output_redirection(count, arglist);
    }
    else if ((index = index_of_pipe(count, arglist)) != -1){
        return piping(count, arglist, index);
    }
    else{
        return execute_command_foreground(arglist);
    }
}