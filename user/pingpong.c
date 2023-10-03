#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char *argv[])
{
    int p1[2],p2[2];
    pipe(p1);
    pipe(p2);
    char buf[5];
    int pid=fork();

    if(pid==0){
        read(p1[0],buf,4);
        close(p1[0]);
        printf("%d: received %s\n",getpid(),buf);
        write(p2[1],"pong",4);
        close(p2[1]);
    }else if(pid>0){
        write(p1[1],"ping",4);
        close(p1[1]);
        read(p2[0],buf,4);
        close(p2[0]);
        printf("%d: received %s\n",getpid(),buf);
    }
    exit(0);
}