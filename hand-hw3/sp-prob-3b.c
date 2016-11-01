#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>

int main(int argc, char *argv[]) {
    int fd1, fd2;
    fd1 = open("/tmp/infile", O_RDONLY );
    fd2 = open("/tmp/outfile", O_WRONLY | O_CREAT, 0666);

    // code start...
    close(0);      // close stdin first
    dup(fd1);      // 0, stdin (reopen)
    dup2(fd2, 1);  // 1, stdout
    dup2(fd2, 2);  // 2, stderr
    // code end...

    int rtn = execlp("./a.out", "./a.out", (char *)0);
    if (rtn < 0) {
        perror("execlp");
        return 1;
    }
    return 0;
}
