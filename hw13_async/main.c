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
#include <linux/limits.h>
#include <unistd.h>

#define S_IFMT 0170000
#define S_IFREG 0100000

#define RESPONSE_TEMPLATE "HTTP/1.1 %d %s\r\nDate: %s\r\nContent-Type: %s; charset=UTF-8\r\nServer: sasha/1.0\r\nContent-Length: %u\r\nConnection: close\r\n\r\n"
#define RESPONSE_ERROR "HTTP/1.1 %d %s\r\nDate: %s\r\nConnection: close\r\n\r\n"
#define CONTENT_TYPE "application/octet-stream"

#define IPV4_ADDR_LEN 16
#define MAX_EPOLL_EVENTS 128

#define MAX_REQUEST_LEN 2048
#define MAX_QUERY_LEN 10240

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BOLD    "\033[1m"

enum SOCK_STATUS {
    READ,
    WRITE
};

enum RESPONE_STATUS {
    OK = 200,
    NOT_FOUND = 404,
    FORBIDDEN = 403,
    UNAVAILABLE = 500
};

typedef struct HTTP_RESPONSE {
    unsigned int status;
    char* datetime;
    char* content_type;
    unsigned int content_length;
} response;

typedef struct SESSION {
    int fd;
    int status; //read/write
    char* query;
} session;


static char buffer[MAX_REQUEST_LEN];
static struct epoll_event events[MAX_EPOLL_EVENTS];
char directory[PATH_MAX];


void send_error(int fd, response* resp) {
    char status_verbose[15] = {0};
    switch (resp->status)
    {
        case NOT_FOUND:
            strncpy(status_verbose, "NOT_FOUND", 15);
            break;

        case FORBIDDEN:
            strncpy(status_verbose, "FORBIDDEN", 15);
            break;

        default:
            strncpy(status_verbose, "UNAVAILABLE", 15);
            break;
    }
    int headers_len = snprintf(NULL, 0, RESPONSE_ERROR, resp->status, status_verbose, resp->datetime);
    char* headers = malloc(headers_len + 1);
    if (!headers) {
        puts("memory error while generating headers");
        return;
    }
    snprintf(headers, headers_len + 1, RESPONSE_ERROR, resp->status, status_verbose, resp->datetime);
    send(fd, headers, headers_len, MSG_DONTWAIT);
    free(headers);
    return;
}

int parse_http(char* buffer, session* ses) {
    gchar** headers_split;
    gchar** title_split;
    headers_split = g_strsplit((const gchar *)buffer, "\r\n", -1);
    char* title = (char*)headers_split[0];
    title_split = g_strsplit((const gchar *)title, " ", -1);
    if ((char*)title_split[0] && (char*)title_split[1] && (char*)title_split[2]) {
        ses->query = g_strdup(&title_split[1][1]);
        // strncpy(ses->query, &title_split[1][1], MAX_QUERY_LEN); 
    }
    else {
        puts("Invalid method or query or protocol in http request");
        g_strfreev(headers_split);
        g_strfreev(title_split);
        return 1;
    }
    g_strfreev(headers_split);
    g_strfreev(title_split);
    return 0;
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

int do_read(session* ses)
{
    int rc = recv(ses->fd, buffer, sizeof(buffer), 0);
    if (rc < 0 || rc > MAX_REQUEST_LEN) {
        perror("read");
        return 1;
    }
    if(parse_http(buffer, ses)) {
        perror("read");
        return 1;
    }
    return 0;
}

int do_write(session* ses) {
    response res;
    int headers_len;
    char* file_data;
    char buf[100];
    char* filename;
    FILE *fp;

    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    res.datetime = malloc(sizeof(buf) * (sizeof(char) + 1));
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
    snprintf(res.datetime, sizeof(buf), "%s", buf);

    int filename_len = snprintf(NULL, 0, "%s%s", directory, ses->query) + 1;
    filename = malloc(filename_len);
    if(!filename) {
        res.status = UNAVAILABLE;
        puts("memory error while generating response");
        send_error(ses->fd, &res);
        free(res.datetime);
        return(EXIT_FAILURE);
    }
    snprintf(filename, filename_len, "%s%s", directory, ses->query);

    struct stat finfo;
    size_t fsize = 0;
    if (!stat(filename, &finfo) && ((finfo.st_mode & S_IFMT) == S_IFREG)) {
        fsize = finfo.st_size;
    }

    if (!fsize) {
        printf("File %s not found\n", filename);
        res.status = NOT_FOUND;
        send_error(ses->fd, &res);
        free(filename);
        free(res.datetime);
        return(EXIT_FAILURE);
    }
    if ((fp=fopen(filename, "rb") ) == NULL) {
        printf("Error opening file %s\n", filename);
        res.status = FORBIDDEN;
        send_error(ses->fd, &res);
        free(filename);
        free(res.datetime);
        return(EXIT_FAILURE);
    }


    res.content_type = g_strdup(CONTENT_TYPE);
    res.content_length = fsize;
    res.status = OK;
    headers_len = snprintf(NULL, 0, RESPONSE_TEMPLATE, res.status, "OK", res.datetime, res.content_type, res.content_length);
    file_data = malloc(fsize * sizeof(char) + headers_len + 1);
    if (!file_data) {
        puts("memory error while reading file");
        free(filename);
        free(res.content_type);
        free(res.datetime);
        res.status = UNAVAILABLE;
        send_error(ses->fd, &res);
        return(EXIT_FAILURE);
    }
    
    snprintf(file_data, headers_len + 1, RESPONSE_TEMPLATE, res.status, "OK", res.datetime, res.content_type, res.content_length);
    size_t nbytes = fread(file_data + headers_len, sizeof(char), fsize, fp) + headers_len;
    fclose(fp);
    int offset = 0;
    int sent = 0;
    while (nbytes > 0) {
        sent = send(ses->fd, file_data + offset, nbytes, MSG_DONTWAIT);
        if (sent >= 0) {
            offset += sent;
            nbytes -= sent;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
        }
        else if (sent == -1) {
            printf("errno %d while sending file\nreseting..\n", errno);
            free(filename);
            free(file_data);
            free(res.content_type);
            free(res.datetime);
            return(EXIT_FAILURE);
        }
    }
    free(filename);
    free(file_data);
    free(res.content_type);
    free(res.datetime);
    return(EXIT_SUCCESS);
}

void process_error(int fd)
{
    printf("fd %d error!\n", fd);
}

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

    strncpy(directory, argv[1], PATH_MAX);

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

    if (listen(listenfd, MAX_EPOLL_EVENTS) < 0) {
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
                session* ses = malloc(sizeof(session));
                ses->status = READ;
                ses->fd = connfd;
                connev.data.ptr = ses;
                connev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
                if (epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &connev) < 0) {
                    perror("epoll_ctl");
                    free(ses);
                    close(connfd);
                    continue;
                }
                events_count++;
            }
            else {
                session* sess = malloc(sizeof(session));
                sess = events[i].data.ptr;
                int fd = sess->fd;
                if ((events[i].events & EPOLLIN) && sess->status == READ) {
                    if (do_read(sess)) {
                        epoll_ctl(efd, EPOLL_CTL_DEL, fd, &connev);
                        free(sess);
                        close(fd);
                    }
                    else {
                        sess->status = WRITE;
                    }

                }
                if ((events[i].events & EPOLLOUT) && sess->status == WRITE) {
                    if (do_write(sess)) {
                        printf("error while sending respone, closing socket %d\n", fd);
                    }
                    epoll_ctl(efd, EPOLL_CTL_DEL, fd, &connev);
                    free(sess);
                    close(fd);
                }
                if (events[i].events & EPOLLRDHUP) {
                    process_error(fd);
                    epoll_ctl(efd, EPOLL_CTL_DEL, fd, &connev);
                    free(sess);
                    close(fd);
                }
                events_count--;
            }
        }
    }
    return 0;
}