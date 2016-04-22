#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

void sighandler(int signum)
{
    printf("Process %d got signal %d\n", getpid(), signum);
    signal(signum, SIG_DFL);
    kill(getpid(), signum);
}

int main()
{
    signal(SIGSEGV, sighandler);
    printf("Process %d waits for someone to send it SIGSEGV\n",
        getpid());
    sleep(1000);

    return 0;
}
