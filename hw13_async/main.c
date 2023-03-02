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
#include <pthread.h>


#define RESPONSE_TEMPLATE "HTTP/1.1 %d %s\r\nDate: %s\r\nContent-Type: %s; charset=UTF-8\r\nServer: sasha/1.0\r\nContent-Length: %u\r\nConnection: close\r\n\r\n"
#define RESPONSE_ERROR "HTTP/1.1 %d %s\r\nDate: %s\r\nConnection: close\r\n\r\n"

#define IPV4_ADDR_LEN 16
#define MAX_EPOLL_EVENTS 128
// #define DIR_REINDEXING_RATE 60 //sec

#define MAX_PATH_LEN 1000
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

typedef struct HTTP_HEADER {
    char method[10];
    char query[MAX_QUERY_LEN];
    char version[10];
} header;

typedef struct HTTP_RESPONSE {
    unsigned int status;
    char protocol[10];
    char datetime[100];
    char content_type[50];
    char server[20];
    unsigned int content_length;
} response;

typedef struct SESSION {
    int fd;
    int status; //read/write
    header http_h;
} session;

typedef struct LOGDIR {
    char dir_path[MAX_PATH_LEN];
    int indexed;
    GHashTable* filelist;
} logdir;


static char buffer[MAX_REQUEST_LEN];
static struct epoll_event events[MAX_EPOLL_EVENTS];
static session sessions[MAX_EPOLL_EVENTS];
static logdir log_files;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


size_t getFileSize(char* filename) {
    struct stat finfo;
    int status;
    status = stat(filename, &finfo);
    if(status == 0) {
        return finfo.st_size;
    }
    else {
        printf("Нельзя найти файл %s или получить о нем информацию\n", filename);
        return(0);
    }
}

void send_error(int* fd, response* resp) {
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
    send(*fd, headers, headers_len, MSG_DONTWAIT);
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
        strncpy(ses->http_h.method, (char*)title_split[0], 10);
        strncpy(ses->http_h.query, &title_split[1][1], MAX_QUERY_LEN);
        strncpy(ses->http_h.version, (char*)title_split[2], 10);
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

int do_read(int fd, session* ses)
{
    int rc = recv(fd, buffer, sizeof(buffer), 0);
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

int do_write(int fd, session* ses) // написать функцию формирующую респонз (передаем туда статус и структуру res)
{
    response res;
    int headers_len;
    char* file_data;
    char buf[100];
    char* filename;
    FILE *fp;
    time_t now = time(0);
    struct tm tm = *gmtime(&now);

    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
    snprintf(res.datetime, sizeof(buf), "%s", buf);

    int filename_len = snprintf(NULL, 0, "%s%s", log_files.dir_path, ses->http_h.query) + 1;
    filename = malloc(filename_len);
    if(!filename) {
        res.status = UNAVAILABLE;
        puts("memory error while generating response");
        send_error(&fd, &res);
        return(EXIT_FAILURE);
    }
    snprintf(filename, filename_len, "%s%s", log_files.dir_path, ses->http_h.query);
    unsigned int fsize = (uint32_t)(uintptr_t)g_hash_table_lookup(log_files.filelist, filename);
    if (!fsize) {
        printf("File %s not found\n", filename);
        res.status = NOT_FOUND;
        send_error(&fd, &res);
        free(filename);
        return(EXIT_FAILURE);
    }
    if ((fp=fopen(filename, "rb") ) == NULL) {
        printf("Error opening file %s\n", filename);
        res.status = FORBIDDEN;
        send_error(&fd, &res);
        free(filename);
        return(EXIT_FAILURE);
    }

    strncpy(res.content_type, "application/octet-stream", 50);
    res.content_length = fsize;
    headers_len = snprintf(NULL, 0, RESPONSE_TEMPLATE, res.status, "OK", res.datetime, res.content_type, res.content_length);
    file_data = malloc(fsize * sizeof(char) + headers_len + 1);
    if (!file_data) {
        puts("memory error while reading file");
        free(filename);
        res.status = UNAVAILABLE;
        send_error(&fd, &res);
        return(EXIT_FAILURE);
    }
    snprintf(file_data, headers_len + 1, RESPONSE_TEMPLATE, res.status, "OK", res.datetime, res.content_type, res.content_length);
    size_t nbytes = fread(file_data + headers_len, sizeof(char), fsize, fp) + headers_len;
    fclose(fp);

    res.status = OK;
    int offset = 0;
    int sent = 0;
    while (nbytes > 0) {
        sent = send(fd, file_data + offset, nbytes, MSG_DONTWAIT);
        if (sent >= 0) {
            offset += sent;
            nbytes -= sent;
            // printf("sent bytes %d\n", sent);
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
        }
        else if (sent == -1) {
            printf("errno %d while sending file\nreseting..\n", errno);
            free(filename);
            free(file_data);
            return(EXIT_FAILURE);
        }
    }
    free(filename);
    free(file_data);
    return(EXIT_SUCCESS);
}

void process_error(int fd)
{
    printf("fd %d error!\n", fd);
}

void* get_files() {

    DIR *dp;
    struct dirent *ep;
    char* dirfile;
    unsigned int fsize;


    while (1) {
        pthread_mutex_lock(&mutex);
        dp = opendir(log_files.dir_path);
        if (!dp)
        {
            printf("Couldn't open the directory %s\n", log_files.dir_path);
            exit(EXIT_FAILURE);
        }
        while ((ep = readdir(dp)) != NULL) {
            if (strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0) {
                int dirfile_len = snprintf(NULL, 0, "%s%s", log_files.dir_path, ep->d_name) + 1;
                dirfile = malloc(dirfile_len);
                if(!dirfile) {
                    puts("memory error while indexing folder");
                    exit(EXIT_FAILURE);
                }
                snprintf(dirfile, dirfile_len, "%s%s", log_files.dir_path, ep->d_name);
                fsize = (unsigned int) getFileSize(dirfile);
                if (fsize == 0) {
                    // printf("Размер проверяемого файла %s 0. Пропускаем его.\n", dirfile);
                    free(dirfile);
                    continue;
                }
                else {
                    g_hash_table_insert(log_files.filelist, (gpointer)g_strdup(dirfile), (gpointer)(uintptr_t)fsize);
                }
                free(dirfile);
            }
        }
        (void)closedir(dp);
        log_files.indexed = 1;
        pthread_mutex_unlock(&mutex);
        // printf("directory indexed, next indexing in %d sec\n", DIR_REINDEXING_RATE);
        // sleep(DIR_REINDEXING_RATE);
    }

    pthread_exit(NULL);
}

void clear_key(gpointer data) {
    free(data);
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


    log_files.indexed = 0;
    log_files.filelist = g_hash_table_new_full(g_str_hash, g_str_equal, clear_key, NULL);
    strncpy(log_files.dir_path, argv[1], MAX_PATH_LEN);

    pthread_t index_pid;
    pthread_create(&index_pid, NULL, get_files, NULL);
    pthread_detach(index_pid);

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
    while (!log_files.indexed) {
        sleep(1);
    }

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
                    sessions[connfd].status = READ;
                    close(connfd);
                    continue;
                }
                events_count++;
            }
            else {
                int fd = events[i].data.fd;

                if ((events[i].events & EPOLLIN) && sessions[fd].status == READ) {
                    if (do_read(fd, &sessions[fd])) {
                        sessions[fd].status = READ;
                        epoll_ctl(efd, EPOLL_CTL_DEL, fd, &connev);
                        close(fd);
                    }
                    else {
                        sessions[fd].status = WRITE;
                    }

                }

                if ((events[i].events & EPOLLOUT) && sessions[fd].status == WRITE) {
                    if (do_write(fd, &sessions[fd])) {
                        printf("error while sending respone, closing socket %d\n", fd);
                    }
                    sessions[fd].status = READ;
                    epoll_ctl(efd, EPOLL_CTL_DEL, fd, &connev);
                    close(fd);
                }

                if (events[i].events & EPOLLRDHUP) {
                    process_error(fd);
                    sessions[fd].status = READ;
                    epoll_ctl(efd, EPOLL_CTL_DEL, fd, &connev);
                    close(fd);
                }
                events_count--;

            }
        }
    }
    g_hash_table_destroy(log_files.filelist);
    pthread_mutex_destroy(&mutex);
    return 0;
}