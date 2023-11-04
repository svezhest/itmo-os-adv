#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int p[2];
    pipe(p);
    int x = fork();
    char *buf[256];
    if (x != 0) {
        write(p[1], "I go ping", 9);
        wait();
        read(p[0], buf, 256);
    } else {
        while(0 == read(p[0], buf, 256)) {
            sleep(10);
//             right?
        }
        write(p[1], "I go pong and then die", 23);
    }
    printf("Process %d: got message '%s'\n", getpid(), buf);
    close(p[0]);
    close(p[1]);
    exit();
}   
