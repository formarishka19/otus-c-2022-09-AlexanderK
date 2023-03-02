/* modern glibc will complain about the above if it doesn't see this. */
#define _DEFAULT_SOURCE

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>


#define CHUNK_SIZE 2048
#define BUF_SIZE 20480
#define RCV_RETRY_TIMEOUT 100000 //0.1 sec
#define RCV_TIMEOUT 5 //sec
#define SERVICE "telnet"
#define HOSTNAME "telehack.com"
#define CMD "figlet "
#define CMD_SIZE 7

#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

enum MODE {
    INVITE,
    MESSAGE
} mode;

int recv_data(int s, enum MODE m, char* buffer, int* buffer_len)
{
	int size_recv;
	char chunk[CHUNK_SIZE];
    int res = 0;
	while(1)
	{
        if (res) {
            break;
        }
		memset(chunk, 0, CHUNK_SIZE);	//clear the chunk
		if((size_recv =  recv(s, chunk, CHUNK_SIZE , 0) ) < 0)
		{
            printf("mode %d sleeping\n", m);
			usleep(RCV_RETRY_TIMEOUT); //if nothing was received then we want to wait a little before trying again
		}
		else
		{
            if (m == MESSAGE) {
                if (*buffer_len + size_recv > BUF_SIZE) {
                    memcpy(&buffer[*buffer_len], chunk, BUF_SIZE - *buffer_len);
                    *buffer_len = BUF_SIZE;
                    puts("too long reply from server, cutting message and stop recieving. Increase BUF_SIZE");
                    res = 1;
                } 
                else {
                    memcpy(&buffer[*buffer_len], chunk, size_recv);
                    *buffer_len = *buffer_len + size_recv;
                }
                
            }
            if (chunk[size_recv-1] == 0x2e && chunk[size_recv-2] == 0x0a) {
                res = 1;
            }
		}
	}
    return res;
}

int main(int argc, char** argv)
{
    if(argc != 3) {
        printf("Неверные параметры запуска, правильный формат:\n%s /font \"message\"\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct addrinfo hints;
    struct addrinfo* res;
    char buffer[BUF_SIZE];
    int buffer_len = 0;
    int msglen = snprintf(NULL, 0, "figlet /%s %s\r\n", argv[1], argv[2]);
    char* message = malloc(msglen+1);
    if(!message) {
        puts("memory error");
        return EXIT_FAILURE;
    }
    snprintf(message, msglen, "figlet /%s %s\r\n", argv[1], argv[2]);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int error = getaddrinfo(HOSTNAME, SERVICE, &hints, &res);
    if (error) {
        printf("Не смогли получить ip-адрес сервера %s\n", HOSTNAME);
        free(message);
        return EXIT_FAILURE;
    }
    struct timeval timeout;      
    timeout.tv_sec = RCV_TIMEOUT;
    timeout.tv_usec = 0;
    int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    if (sock_fd < 0) {
        perror("socket");
        free(message);
        return EXIT_FAILURE; 
    }
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
        puts("setsockopt failed");
    }
        
    if(connect(sock_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sock_fd);
        free(message);
        return EXIT_FAILURE;
    }
    puts("connected");
    if(send(sock_fd, "\n", 1, 0) < 0) {
        perror("send");
        shutdown(sock_fd, SHUT_RDWR);
        close(sock_fd);
        free(message);
        return EXIT_FAILURE;
    }

	int data = recv_data(sock_fd, INVITE, NULL, NULL);
    if (data) {
        data = 0;
        if(send(sock_fd, message, msglen, MSG_DONTWAIT) < 0) {
            perror("send");
            shutdown(sock_fd, SHUT_RDWR);
            close(sock_fd);
            free(message);
            return EXIT_FAILURE;
        }
        free(message);
        data = recv_data(sock_fd, MESSAGE, buffer, &buffer_len);
        if (data) {
            for (int y = msglen; y < buffer_len - 1; y++) {
                printf(ANSI_COLOR_GREEN "%c" ANSI_COLOR_RESET, buffer[y]);
            }
            if(send(sock_fd, "exit\r\n", 6, MSG_DONTWAIT) < 0) {
                perror("send");
                shutdown(sock_fd, SHUT_RDWR);
                close(sock_fd);
                return EXIT_FAILURE;
            }
            shutdown(sock_fd, SHUT_RDWR);
            close(sock_fd);
            freeaddrinfo(res);
            return EXIT_SUCCESS;
        }
    }
    puts("can't get reply from server, exiting");
    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);
    freeaddrinfo(res);

    return 0; 
}
