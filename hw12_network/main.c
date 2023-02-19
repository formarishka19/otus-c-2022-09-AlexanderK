#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define PORT    23
#define CHUNK_SIZE 2048
#define BUF_SIZE 20480
#define MAX_MSG_SIZE 100
#define RCV_RETRY_TIMEOUT 100000 //0.1 sec
#define RCV_TIMEOUT 5 //sec
#define HOST "64.13.139.230"
#define CMD "figlet "
#define CMD_SIZE 7

#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

enum MODE {
    INVITE,
    MESSAGE
} mode;


int recv_data(int s , int timeout, enum MODE m, char* buffer, int* buffer_len)
{
	int size_recv;
	struct timeval begin , now;
	char chunk[CHUNK_SIZE];
	double timediff;
    int res = 0;
    
	gettimeofday(&begin , NULL);
	while(1)
	{
		gettimeofday(&now , NULL);
		timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);
        if (res) {
            break;
        }
        else if (timediff > timeout*2) {
            printf("timeouted %d mode\n", m);
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
                memcpy(&buffer[*buffer_len], chunk, size_recv);
                *buffer_len = *buffer_len + size_recv;
            }
            if (chunk[size_recv-1] == 0x2e && chunk[size_recv-2] == 0x0a) {
                res = 1;
            }
			gettimeofday(&begin , NULL); //reset beginning time
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

    int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    if (sock_fd < 0) {
        perror("socket");
        return EXIT_FAILURE; 
    }

    static char host[20] = HOST;
    char message[MAX_MSG_SIZE + 10] = CMD;
    char font[50] = "/";
    char buffer[BUF_SIZE];
    int buffer_len = 0;
    
    if (strlen(argv[2]) > MAX_MSG_SIZE || strlen(argv[1]) > MAX_MSG_SIZE) {
        printf("Слишком длинное сообщение или название шрифта. Повторите запуск с длиной аргументов не более %d символов", MAX_MSG_SIZE);
        return EXIT_FAILURE;
    }
    strcat(font, argv[1]);
    strcat(message, font);
    strcat(message, " ");
    strcat(message, argv[2]);
    strcat(message, "\r\n");

    struct sockaddr_in sock_addr = {0};
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(PORT);
    
    int r = inet_pton(PF_INET, host, &sock_addr.sin_addr); 
    if(r <= 0) {
        perror("inet_pton");
        close(sock_fd);
        return EXIT_FAILURE;
    }
    if(connect(sock_fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return EXIT_FAILURE;
    }
    puts("connected");
    if(send(sock_fd, "\n", 1, 0) < 0) {
        perror("send");
        shutdown(sock_fd, SHUT_RDWR);
        close(sock_fd);
        return EXIT_FAILURE;
    }

	int data = recv_data(sock_fd, RCV_TIMEOUT, INVITE, NULL, NULL);
    if (data) {
        data = 0;
        printf("Got invite from server, sending message %s \n", message);
        if(send(sock_fd, message, sizeof(message), MSG_DONTWAIT) < 0) {
            perror("send");
            shutdown(sock_fd, SHUT_RDWR);
            close(sock_fd);
            return EXIT_FAILURE;
        }
        data = recv_data(sock_fd, RCV_TIMEOUT, MESSAGE, buffer, &buffer_len);
        if (data) {
            for (int y = (strlen(message)); y < buffer_len - 1; y++) {
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
            return EXIT_SUCCESS;
        }
    }
    puts("cann't get reply from server, exiting");
    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);

    return 0; 
}
