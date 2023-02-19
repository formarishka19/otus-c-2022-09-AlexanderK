#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <glib.h>

// #define DEFAULT_PORT 80
// #define DEFAULT_HOST "127.0.0.1"
#define IPV4_ADDR_LEN 16
#define MAX_EPOLL_EVENTS 128
#define BACKLOG 128
#define CHUNK_SIZE 2048
#define BUF_SIZE 20480
#define MAX_MSG_SIZE 100
#define RCV_RETRY_TIMEOUT 100000 //0.1 sec
#define RCV_TIMEOUT 5 //sec

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BOLD    "\033[1m"


static char buffer[2048];

int setnonblocking(int sock)
{
    int opts;
    opts = fcntl(sock,F_GETFL);
    if (opts < 0) {
        perror("fcntl(F_GETFL)");
        return -1;
    }
    opts = (opts | O_NONBLOCK);
    if (fcntl(sock, F_SETFL, opts) < 0)
    {
        perror("fcntl(F_SETFL)");
        return -1;
    }
 return 0;
}

void do_read(int fd)
{
    int rc = recv(fd, buffer, sizeof(buffer), 0);
    if (rc < 0) {
        perror("read");
        return;
    }
    buffer[rc] = 0;
    printf("read: %s\n", buffer);
}

void do_write(int fd)
{
    static const char* greeting = "Hello world!\n";
    int rc = send(fd, greeting, strlen(greeting), 0);
    if (rc < 0) {
        perror("write");
        return;
    }
}

void process_error(int fd)
{
    printf("fd %d error!\n", fd);
}


static struct epoll_event events[MAX_EPOLL_EVENTS];

int main(int argc, char** argv) {
    if (argc != 3) {
        printf(ANSI_COLOR_RED ANSI_COLOR_BOLD "Usage:" ANSI_COLOR_RESET " %s </directory/with/files/> <address:port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int port;
    char* p;
    char temp[6] = {0};
    char address[IPV4_ADDR_LEN];

    gchar** arg_split = g_strsplit((const gchar *)argv[2], ":", -1);
    if ((char*)arg_split[0] && (char*)arg_split[1]) {
        strncpy(address, (char*)arg_split[0], IPV4_ADDR_LEN);
        strncpy(temp, (char*)arg_split[1], 6);
    }
    else {
        puts("Invalid port number or address");
        return EXIT_FAILURE;
    }
    g_strfreev(arg_split);
    
    port = strtol(temp, &p, 10);
    if (*p) {
        printf("Invalid port number %s\n", temp);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    int efd = epoll_create(MAX_EPOLL_EVENTS);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    setnonblocking(listenfd);

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, address, &(servaddr.sin_addr));
    servaddr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(listenfd, BACKLOG) < 0) { //BACKLOG - кол-во коннектов, которые будут приниматься сервером
        perror("listen");
        return EXIT_FAILURE;
    }

    struct epoll_event listenev;
    listenev.events = EPOLLIN | EPOLLET;
    listenev.data.fd = listenfd;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &listenev) < 0) {
        perror("epoll_ctl");
        return EXIT_FAILURE;
    }

    struct epoll_event connev;
    int events_count = 1;

    for (;;) {
        int nfds = epoll_wait(efd, events, MAX_EPOLL_EVENTS, -1);

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listenfd) {
                int connfd = accept(listenfd, NULL, NULL);
                if (connfd < 0) {
                    perror("accept");
                    continue;
                }
                if (events_count == MAX_EPOLL_EVENTS-1) {
                    printf("Event array is full\n");
                    close(connfd);
                    continue;
                }

                setnonblocking(connfd);
                connev.data.fd = connfd;
                connev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
                if (epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &connev) < 0) {
                    perror("epoll_ctl");
                    close(connfd);
                    continue;
                }
                events_count++;
            }
            else {
                int fd = events[i].data.fd;

                if (events[i].events & EPOLLIN) {
                    do_read(fd);
                }

                if (events[i].events & EPOLLOUT) { // срабатывает быстрее, чем функция do_read
                    do_write(fd);
                }

                if (events[i].events & EPOLLRDHUP) {
                    process_error(fd);
                }

                epoll_ctl(efd, EPOLL_CTL_DEL, fd, &connev);
                close(fd);
                events_count--;
            }
        }
    }
 return 0;
}