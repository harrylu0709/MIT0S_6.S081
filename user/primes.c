#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
void process(int p){
    int fd[2];
    int prime, n, pid, status;
    
    if(read(p,&prime,4)==0) return;
    printf("prime: %d\n",prime);
    pipe(fd);
    pid=fork();
    if(pid>0){
        close(fd[0]);
        while(read(p,&n,4)!=0){
            if(n%prime){
                write(fd[1],&n,4);
            }
        }
        close(fd[1]);
    }else if (pid==0){
        close(fd[1]);
        process(fd[0]);
        close(fd[0]);
        exit(0);
    }
    wait(&status);
}

int main(int argc, char *argv[])
{
    int fd[2];
    pipe(fd);
    int i;

    for(i=2;i<=35;i++){
        write(fd[1],&i,4);
    }
    close(fd[1]);
    process(fd[0]);

    exit(0);
}