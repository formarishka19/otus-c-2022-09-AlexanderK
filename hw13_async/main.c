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
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

// #define FILENAME "23_async_homework-12926-11d3b1.pdf"
#define FILENAME "1.txt"

#define IPV4_ADDR_LEN 16
#define MAX_EPOLL_EVENTS 128
#define BACKLOG 128
#define MTU_SIZE 1024

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BOLD    "\033[1m"

enum STATUS {
    INIT,
    READ,
    WRITE
};

typedef struct HTTP_HEADER {
    char method[10];
    char query[10240];
    char version[10];
} header;

typedef struct HTTP_RESPONSE {
    unsigned int status;
    char protocol[10];
    char datetime[100];
    char content_type[50];
    char server[20];
} response;

typedef struct SESSION {
    int fd;
    int status; //init/read/write
} session;

static char buffer[2048];

size_t getFileSize(char* filename) {
    struct stat finfo;
    int status;
    status = stat(filename, &finfo);
    if(status == 0) {
        return finfo.st_size;
    }
    else {
        printf("Нельзя найти файл %s или получить о нем информацию\n", filename);
        exit(EXIT_FAILURE);
    }
}

void parse_http(char* buffer, header* h) {
    gchar** headers_split;
    gchar** title_split;

    headers_split = g_strsplit((const gchar *)buffer, "\r\n", -1);
    char* title = (char*)headers_split[0];
    //написать цикл разобра остальных заголовков (while headers_split[x], x++)
    title_split = g_strsplit((const gchar *)title, " ", -1);
    if ((char*)title_split[0] && (char*)title_split[1] && (char*)title_split[2]) {
        strncpy(h->method, (char*)title_split[0], 10);
        strncpy(h->query, (char*)title_split[1], 10240);
        strncpy(h->version, (char*)title_split[2], 10);
    }
    else {
        puts("Invalid method or query or protocol");
        exit(EXIT_FAILURE);
    }
    g_strfreev(headers_split);
    g_strfreev(title_split);
}

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

void do_read(int fd, session* ses)
{
    int rc = recv(fd, buffer, sizeof(buffer), 0);
    if (rc < 0) {
        perror("read");
        return;
    }
    buffer[rc] = 0;
    printf("read: %s\n", buffer);
    header http_h = {"", "", ""};
    //ограничить буфер, следить за его переполнением
    parse_http(buffer, &http_h);
    printf("HTTP version" ANSI_COLOR_GREEN " %s\n" ANSI_COLOR_RESET, http_h.version);
    printf("HTTP method" ANSI_COLOR_GREEN " %s\n" ANSI_COLOR_RESET, http_h.method);
    printf("HTTP query" ANSI_COLOR_GREEN " %s\n" ANSI_COLOR_RESET, http_h.query);
    ses->status = WRITE;
}

void do_write(int fd, session* ses)
{
    response res;

    char buf[100];
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    char filename[255] = FILENAME;
    size_t fsize = getFileSize(filename);

    printf("file size %lu\n", fsize);

    strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    snprintf(res.datetime, 100, "%s", buf);
    res.status = 200;
    strncpy(res.content_type, "application/octet-stream", 50);

    FILE *fp;
    if ((fp=fopen(filename, "rb") ) == NULL) {
        printf("Error opening file %s\n", filename);
        exit (1);
    }
    int headers_len = snprintf(NULL, 0, "HTTP/1.1 %d OK\r\nDate: %s\r\nContent-Type: %s\r\nContent-Disposition: attachment; filename=\"%s\"\r\nServer: sasha/1.0\r\n\r\n", res.status, res.datetime, res.content_type, filename);
    char* headers = malloc(headers_len+1);
    if(!headers) {
        puts("memory error");
        exit(EXIT_FAILURE);
    }
    snprintf(headers, headers_len, "HTTP/1.1 %d OK\r\nDate: %s\r\nContent-Type: %s\r\nContent-Disposition: attachment; filename=\"%s\"\r\nServer: sasha/1.0\r\n\r\n", res.status, res.datetime, res.content_type, filename);
    int rc;
    /* Sending headers */
    rc = send(fd, headers, strlen(headers), 0);
    if (rc < 0) {
        perror("write");
        puts("error while sending to socket");
        fclose(fp);
        shutdown(fd, SHUT_RDWR);
        exit(EXIT_FAILURE);
    }
    free(headers);
   
    char file_data[MTU_SIZE];
    size_t nbytes = 0;
    int offset;
    int sent;
    while ( (nbytes = fread(file_data, sizeof(char), MTU_SIZE, fp)) > 0)
    {
        printf("bytes read from file %lu\n", nbytes);
        offset = 0;
        sent = 0;
        while (nbytes > 0) {
            sent = send(fd, file_data + offset, nbytes, MSG_DONTWAIT);
            if (sent >= 0) {
                offset += sent;
                nbytes -= sent;
                printf("sent bytes %d\n", sent);
            }

            else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            else if (sent == -1) {
                printf("errno %d\nreseting..\n", errno);
                fclose(fp);
                shutdown(fd, SHUT_RDWR);
                exit(EXIT_FAILURE);
            }
        }
    }
    // send(fd, "\r\n\r\n", 5, MSG_DONTWAIT);
    puts("closed");
    fclose(fp);
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
    session sessions[MAX_EPOLL_EVENTS];

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
                sessions[connfd].fd = connfd;
                sessions[connfd].status = READ;
                connev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
                if (epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &connev) < 0) {
                    perror("epoll_ctl");
                    sessions[connfd].status = INIT;
                    close(connfd);
                    continue;
                }
                events_count++;
            }
            else {
                int fd = events[i].data.fd;

                if ((events[i].events & EPOLLIN) && sessions[fd].status == READ) {
                    do_read(fd, &sessions[fd]);
                }

                if ((events[i].events & EPOLLOUT) && sessions[fd].status == WRITE) {
                    do_write(fd, &sessions[fd]);
                    sessions[fd].status = INIT;
                }

                if (events[i].events & EPOLLRDHUP) {
                    process_error(fd);
                }

                epoll_ctl(efd, EPOLL_CTL_DEL, fd, &connev);
                shutdown(fd, SHUT_RDWR);
                close(fd);
                events_count--;
            }
        }
    }
 return 0;
}