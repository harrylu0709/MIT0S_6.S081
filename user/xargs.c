#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"
int main(int argc, char *argv[])
{
    sleep(10);
    char buf[512];
    memset(buf,0,sizeof(buf));

    char *xargv[MAXARG];
    int xargc=0;
    int status;
    for(int i=1;i<argc;i++){
        xargv[xargc]=argv[i];
        xargc++;
    }
    //xargv[xargc++]=0;
    

    
    int n=read(0,buf,sizeof(buf));
    int buf_idx=0;
    char *p=buf;
    while(n>0){
        for(int i=0;i<strlen(buf);++i){
            if(buf[i]=='\n'){
                int pid=fork();
                if(pid>0){
                    
                    wait(&status);
                    //p2=strchr(buf,'\n');
                    p=&buf[i+1];
                    printf("pp=%s\n",p);
                }else if (pid==0){
                    //p=&buf[i+1];
                    buf[i]=0;
                    printf("p=%s\n",p);
                    xargv[xargc++]=p;
                    xargv[xargc++]=0;
                    exec(xargv[0], xargv);
                    exit(0);
                }
                buf_idx+=n;
                n=read(0,&buf[buf_idx],sizeof(buf));
                //printf("n=%d\n",n);
            }
        }

    }
    exit(0);
}