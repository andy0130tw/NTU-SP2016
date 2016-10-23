#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/stat.h>
int same_file(int fd1, int fd2) {
    struct stat stat1, stat2;
    if(fstat(fd1, &stat1) < 0) return -1;
    if(fstat(fd2, &stat2) < 0) return -1;
    return (stat1.st_dev == stat2.st_dev) && (stat1.st_ino == stat2.st_ino);
}

#ifdef COLOR
#define CTRLSEQ_RED     "\x1b[31m"
#define CTRLSEQ_GREEN   "\x1b[32m"
#define CTRLSEQ_YELLOW  "\x1b[33m"
#define CTRLSEQ_BLUE    "\x1b[34m"
#define CTRLSEQ_GRAY    "\033[90m"
#define CTRLSEQ_RESET   "\x1b[0m"
#else
#define CTRLSEQ_RED     ""
#define CTRLSEQ_GREEN   ""
#define CTRLSEQ_YELLOW  ""
#define CTRLSEQ_BLUE    ""
#define CTRLSEQ_GRAY    ""
#define CTRLSEQ_RESET   ""
#endif

#define ERR_EXIT(a) { perror(CTRLSEQ_RED a CTRLSEQ_RESET); exit(1); }

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
    char* filename;  // filename set in header, end with '\0'.
    int header_done;  // used by handle_read to know if the header is read or not.
} request;

typedef struct {
   short l_type;    /* Type of lock: F_RDLCK,
                       F_WRLCK, F_UNLCK */
   short l_whence;  /* How to interpret l_start:
                       SEEK_SET, SEEK_CUR, SEEK_END */
   off_t l_start;   /* Starting offset for lock */
   off_t l_len;     /* Number of bytes to lock */
   pid_t l_pid;     /* PID of process blocking our lock
                       (set by F_GETLK and F_OFD_GETLK) */
} flock;


server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_header = "ACCEPT\n";
const char* reject_header = "REJECT\n";

// Forwards

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

static int handle_read(request* reqP);
// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error

int main(int argc, char** argv) {
    int i, ret;

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Get file descripter table size and initize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    fd_set readset;
    FD_ZERO(&readset);

#ifdef HEARTBEAT
    int k = 0;
#endif

#ifdef READ_SERVER
    int lock_ok;
#endif

#ifndef READ_SERVER
    int write_fd[maxfd];
    for (i = 0; i < maxfd; i++)
        write_fd[i] = -1;
#endif

    while (1) {
        // TODO: Add IO multiplexing
        // Check new connection
        clilen = sizeof(cliaddr);

#ifdef HEARTBEAT
        fprintf(stderr, CTRLSEQ_GRAY "active fd");
#endif
        for (i = 0; i < maxfd; i++) {
            if (requestP[i].conn_fd >= 0) {
#ifdef HEARTBEAT
                fprintf(stderr, " %d", requestP[i].conn_fd);
#endif
                FD_SET(requestP[i].conn_fd, &readset);
            }
        }
#ifdef HEARTBEAT
        fprintf(stderr, " - %d\n" CTRLSEQ_RESET, k++);
#endif

#ifdef HEARTBEAT
        struct timeval timeout = (struct timeval) {0, 80000};
        int totfds = select(maxfd, &readset, NULL, NULL, &timeout);
#else
        int totfds = select(maxfd, &readset, NULL, NULL, NULL);
#endif

        if (totfds == 0) {
            continue;
        } else if (totfds < 0) {
            ERR_EXIT("select");
        }

        // check the listening port first
        int listen_ok = 0;
        if (FD_ISSET(svr.listen_fd, &readset)) {
            fprintf(stderr, CTRLSEQ_BLUE "Ready to accept socket fd %d\n" CTRLSEQ_RESET, svr.listen_fd);
            listen_ok = 1;
            FD_CLR(svr.listen_fd, &readset);
        }

        // then check for other ports
        for (i = 0; i < maxfd; i++) {
            if (FD_ISSET(i, &readset)) {
                fprintf(stderr, "Ready to read from file fd %d\n", i);
                conn_fd = i;
                break;
            }
        }

        // to be simple, re-enter the loop after accepting a connection
        if (listen_ok) {
            conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
            if (conn_fd < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;  // try again
                if (errno == ENFILE) {
                    (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                    continue;
                }
                ERR_EXIT("accept");
            }
            requestP[conn_fd].conn_fd = conn_fd;
            strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
            fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
            continue;
        }

        file_fd = -1;

#ifdef READ_SERVER
        lock_ok = 0;
        ret = handle_read(&requestP[conn_fd]);

        if (ret < 0) {
            fprintf(stderr, CTRLSEQ_RED "Bad request from %s\n" CTRLSEQ_RESET, requestP[conn_fd].host);
            continue;
        }
        // requestP[conn_fd]->filename is guaranteed to be successfully set.
        if (file_fd == -1) {
            // open the file here.
            fprintf(stderr, CTRLSEQ_BLUE "Opening file [%s]\n" CTRLSEQ_RESET, requestP[conn_fd].filename);
            file_fd = open(requestP[conn_fd].filename, O_RDONLY, 0);
            struct flock read_lock = {
                .l_type = F_RDLCK,
                .l_whence = SEEK_SET
                // other fields are initialized to zero
            };
            if (fcntl(file_fd, F_SETLK, &read_lock) == 0) {
                lock_ok = 1;
                write(requestP[conn_fd].conn_fd, accept_header, sizeof(accept_header));
            } else {
                write(requestP[conn_fd].conn_fd, reject_header, sizeof(reject_header));
                fprintf(stderr, CTRLSEQ_YELLOW "Rejected fd %d, cannot obtain read lock.\n" CTRLSEQ_RESET, requestP[conn_fd].conn_fd);
                perror("fnctl");
            }
        }

        if (lock_ok) {
            if (ret == 0) break;
            while (1) {
                ret = read(file_fd, buf, sizeof(buf));
                if (ret < 0) {
                    fprintf(stderr, CTRLSEQ_RED "Error when reading file %s\n" CTRLSEQ_RESET, requestP[conn_fd].filename);
                    break;
                } else if (ret == 0) break;
                write(requestP[conn_fd].conn_fd, buf, ret);
            }
            fprintf(stderr, CTRLSEQ_GREEN "Done reading file [%s]\n" CTRLSEQ_RESET, requestP[conn_fd].filename);
        }

        if (file_fd >= 0) close(file_fd);
        fprintf(stderr, "End request fd %d\n", requestP[conn_fd].conn_fd);
        FD_CLR(requestP[conn_fd].conn_fd, &readset);
        close(requestP[conn_fd].conn_fd);
        free_request(&requestP[conn_fd]);
#endif

#ifndef READ_SERVER
        ret = handle_read(&requestP[conn_fd]);
        if (ret < 0) {
            fprintf(stderr, CTRLSEQ_RED "Bad request from %s\n" CTRLSEQ_RESET, requestP[conn_fd].host);
            continue;
        }

        file_fd = write_fd[requestP[conn_fd].conn_fd];
        // requestP[conn_fd]->filename is guaranteed to be successfully set.
        if (file_fd < 0) {
            // open the file here.
            fprintf(stderr, CTRLSEQ_BLUE "Opening file [%s]\n" CTRLSEQ_RESET, requestP[conn_fd].filename);
            file_fd = open(requestP[conn_fd].filename, O_WRONLY | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

            struct flock write_lock = {
                .l_type = F_WRLCK,
                .l_whence = SEEK_SET
            };

            if (fcntl(file_fd, F_SETLK, &write_lock) == 0) {
                for (i = 0; i < maxfd; i++) {
                    if (write_fd[i] >= 0 && same_file(write_fd[i], file_fd)) {
                        write(requestP[conn_fd].conn_fd, reject_header, sizeof(reject_header));
                        fprintf(stderr, CTRLSEQ_YELLOW "Rejected fd %d, already opened by fd %d as fd %d\n" CTRLSEQ_RESET, requestP[conn_fd].conn_fd, i, write_fd[i]);
                        break;
                    }
                }
                if (i == maxfd) {
                    // distinct in file descriptors, success
                    write_fd[requestP[conn_fd].conn_fd] = file_fd;
                    write(requestP[conn_fd].conn_fd, accept_header, sizeof(accept_header));
                }
            } else {
                write(requestP[conn_fd].conn_fd, reject_header, sizeof(reject_header));
                fprintf(stderr, CTRLSEQ_YELLOW "Rejected fd %d, cannot obtain write lock.\n" CTRLSEQ_RESET, requestP[conn_fd].conn_fd);
                perror("fnctl");
            }
        }

        // after accepting, the file_fd is copied to write_fd array
        if (write_fd[requestP[conn_fd].conn_fd] >= 0 && ret > 0) {
            fprintf(stderr, CTRLSEQ_BLUE "Writing to fd %d\n" CTRLSEQ_RESET, file_fd);
            write(file_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len);
            continue;
        }

        // implies ret == 0
        if (write_fd[requestP[conn_fd].conn_fd] >= 0) {
            fprintf(stderr, CTRLSEQ_GREEN "Done writing file [%s]\n" CTRLSEQ_RESET, requestP[conn_fd].filename);
            write_fd[requestP[conn_fd].conn_fd] = -1;
            close(file_fd);
        }

        fprintf(stderr, "End request fd %d\n", requestP[conn_fd].conn_fd);
        FD_CLR(requestP[conn_fd].conn_fd, &readset);
        close(requestP[conn_fd].conn_fd);
        free_request(&requestP[conn_fd]);
#endif
    }

    free(requestP);
    return 0;
}


// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void* e_malloc(size_t size);


static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->filename = NULL;
    reqP->header_done = 0;
}

static void free_request(request* reqP) {
    if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }
    init_request(reqP);
}

// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error
static int handle_read(request* reqP) {
    int r;
    char buf[512];

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    if (reqP->header_done == 0) {
        char* p1 = strstr(buf, "\015\012");
        int newline_len = 2;
        // be careful that in Windows, line ends with \015\012
        if (p1 == NULL) {
            p1 = strstr(buf, "\012");
            newline_len = 1;
            if (p1 == NULL) {
                // This would not happen in testing, but you can fix this if you want.
                ERR_EXIT("header not complete in first read...");
            }
        }
        size_t len = p1 - buf + 1;
        reqP->filename = (char*)e_malloc(len);
        memmove(reqP->filename, buf, len);
        reqP->filename[len - 1] = '\0';
        p1 += newline_len;
        reqP->buf_len = r - (p1 - buf);
        memmove(reqP->buf, p1, reqP->buf_len);
        reqP->header_done = 1;
    } else {
        reqP->buf_len = r;
        memmove(reqP->buf, buf, r);
    }
    return 1;
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }
}

static void* e_malloc(size_t size) {
    void* ptr;

    ptr = malloc(size);
    if (ptr == NULL) ERR_EXIT("out of memory");
    return ptr;
}
