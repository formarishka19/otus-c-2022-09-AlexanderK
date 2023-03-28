#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "ws.c"
#include "ws.h"
#include <sys/types.h>
#include <linux/limits.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/resource.h>
#include <poll.h>
#include <sys/inotify.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>


#define LAST_LOG_OFFSET 1024 //bytes from EOF
#define APP_NAME "LOGSERVER"
#define START_ARGS "\n\n --- ! ОШИБКА ЗАПУСКА ----\n\n" 			\
        "Программа принимает на вход следующие аргументы:\n" 		\
		"\t-f путь до файла с логами \n" 							\
		"\t-p порт, на котором будет работать сервер \n" 	\
		"\t-d опциональный ключ для режима демонизации\n\n" 		\
        "Пример запуска\n" 											\
        "%s -f ./test.log -p 33223 -d\n\n" 							\
        "--- Повторите запуск правильно ---\n\n" 					\

enum CLIENT_STATUS {
    CONNECTED,
    TAILING
};

char filepath[PATH_MAX] = {0};
int port;
int daemon_mode = 0;

void checkArgs(int* argc, char* argv[]) {
    if (*argc < 5 || *argc > 6) {
        printf(START_ARGS, argv[0]);
        exit (EXIT_FAILURE);
    }
	int opt;
	while ((opt = getopt(*argc, argv, "f:p:d")) != -1) {
		switch (opt) {
			case 'f':
				strncpy(filepath, optarg, PATH_MAX);
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'd':
				daemon_mode = 1;
				break;
			default: /* '?' */
				printf(START_ARGS, argv[0]);
        		exit (EXIT_FAILURE);
		}
	}
}

void daemonize(const char* cmd)
{
    int                 fd0, fd1, fd2;
    pid_t               pid;
    struct rlimit       rl;
    struct sigaction    sa;

    openlog(cmd, LOG_CONS | LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Начинаю демонизацию");
    umask(0);

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        perror("невозможно получить максимальный номер дескриптора");
    }

    if ((pid = fork()) < 0) {
        perror("ошибка вызова функции fork");
    }
    else if (pid != 0) {
        exit(0);
    }
    setsid();

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        syslog(LOG_CRIT, "невозможно игнорировать сигнал SIGHUP");
    }
    if ((pid = fork()) < 0) {
        syslog(LOG_CRIT, "ошибка вызова функции fork");
    }
    else if (pid != 0) {
        exit(0);
    }
    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }
    for (rlim_t i = 0; i < rl.rlim_max; i++) {
        close(i);
    }
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_CRIT, "ошибочные файловые дескрипторы %d %d %d", fd0, fd1, fd2);
    }
    syslog(LOG_INFO, "Программа успешно демонизирована =)");
}

static int handle_events(int fd) {

	char buf[4096]
		__attribute__ ((aligned(__alignof__(struct inotify_event))));
	const struct inotify_event *event;
	ssize_t len;


	/* Loop while events can be read from inotify file descriptor. */

	// for (;;) {
		/* Read some events. */

	len = read(fd, buf, sizeof(buf));
	if (len == -1 && errno != EAGAIN) {
		syslog(LOG_CRIT, "Cannot read while handle inotify events. Exiting process.");
		exit(EXIT_FAILURE);
	}

	if (len <= 0)
		return 0;

	for (char *ptr = buf; ptr < buf + len;
			ptr += sizeof(struct inotify_event) + event->len) {

		event = (const struct inotify_event *) ptr;
		if (event->mask & IN_MODIFY) {
			return 1;
		}
	}
	// }
	return 0;
}

static size_t get_file_size(char* filename) {
	size_t fsize;
	struct stat finfo;
	if (!stat(filename, &finfo)){
		fsize = finfo.st_size;
	}
	else {
		syslog(LOG_CRIT, "Cannot get file size '%s' Exiting process.", filename);
		exit(EXIT_FAILURE);
	}
	return fsize;
}

static void get_last_logs(size_t size, size_t offset, char* buffer) {
	int fd = open(filepath, O_RDONLY);
	if (fd < 0) {
		syslog(LOG_CRIT, "Cannot open file '%s'. Exiting process.", filepath);
		exit(EXIT_FAILURE);
	}
	pread(fd, buffer, size, offset);
	close(fd);
}

void* tailing_logs(void* arg) {

	ws_cli_conn_t* client = arg;
	char* cli = ws_getaddress(client);
	char* buf;

	size_t fsize = get_file_size(filepath);

	if (fsize < LAST_LOG_OFFSET) {
		buf = malloc(fsize + 1);
		if (!buf) {
			syslog(LOG_CRIT, "Memory error at line %d. Exiting process.", __LINE__);
			exit(EXIT_FAILURE);
		}
		get_last_logs(fsize, 0, buf);
		buf[fsize] = '\0';
	}
	else {
		buf = malloc(LAST_LOG_OFFSET + 1);
		if (!buf) {
			syslog(LOG_CRIT, "Memory error at line %d. Exiting process.", __LINE__);
			exit(EXIT_FAILURE);
		}
		get_last_logs(LAST_LOG_OFFSET, fsize - LAST_LOG_OFFSET, buf);
		buf[LAST_LOG_OFFSET] = '\0';
	}

	char* part_string;
	char* saveptr;
	int count = 0;

	part_string = strtok_r(buf, "\n", &saveptr);
	while (part_string != NULL)
	{	
		if (count != 0) {
			if (client->query == NULL) {
				ws_sendframe_txt(client, part_string);
			}
			else {
				char* haystack = strdup(part_string);
				if (strstr(haystack, client->query) != NULL) {
					ws_sendframe_txt(client, part_string);
				}
				free(haystack);
			}
		}
		count++;
		part_string = strtok_r(NULL,"\n", &saveptr);
	}
	free(part_string);
	free(buf);

	int fd = inotify_init1(IN_NONBLOCK);
	if (fd == -1) {
		syslog(LOG_CRIT, "inotify_init1 error. Exiting process.");
		exit(EXIT_FAILURE);
	}

	int wd = inotify_add_watch(fd, filepath, IN_MODIFY);
	if (wd == -1) {
		syslog(LOG_CRIT, "Cannot watch '%s': %s. Exiting process.", filepath, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Prepare for polling. */
	nfds_t nfds = 1;
	struct pollfd fds[1];
	fds[0].fd = fd;                 /* Inotify input */
	fds[0].events = POLLIN;

	/* Wait for events. */

	syslog(LOG_INFO, "Client: %s Tailing logs", cli);

	while (client->status == TAILING && get_client_state(client) > -1) {
		int poll_num = poll(fds, nfds, 3000);
		if (poll_num == -1) {
			if (errno == EINTR)
				continue;
			syslog(LOG_CRIT, "poll error. Exiting process.");
			exit(EXIT_FAILURE);
		}
		if (poll_num > 0) {
			if (fds[0].revents & POLLIN) {
				int changed = handle_events(fd);
				if (changed) {
					size_t fsize_current = get_file_size(filepath);
					if (fsize_current > fsize) {
						buf = malloc(fsize_current - fsize + 1);
						if (!buf) {
							syslog(LOG_CRIT, "Memory error at line %d. Exiting process.", __LINE__);
							exit(EXIT_FAILURE);
						}
						get_last_logs(fsize_current - fsize, fsize, buf);
						buf[fsize_current - fsize] = '\0';
						if (client->query == NULL) {
							ws_sendframe_txt(client, buf);
						}
						else {
							char* haystack = strndup(buf, fsize_current - fsize + 1);
							if (strstr(haystack, client->query) != NULL) {
								ws_sendframe_txt(client, buf);
							}
							free(haystack);
						}
						free(buf);
						fsize = fsize_current;
					}
					else if (fsize_current < fsize && fsize_current != 0) {
						buf = malloc(fsize_current + 1);
						if (!buf) {
							syslog(LOG_CRIT, "Memory error at line %d. Exiting process.", __LINE__);
							exit(EXIT_FAILURE);
						}
						get_last_logs(fsize_current, 0, buf);
						buf[fsize_current] = '\0';
						if (client->query == NULL) {
							ws_sendframe_txt(client, buf);
						}
						else {
							char* haystack = strndup(buf, fsize_current + 1);
							if (strstr(haystack, client->query) != NULL) {
								ws_sendframe_txt(client, buf);
							}
							free(haystack);
						}
						free(buf);
						fsize = fsize_current;
					}
					changed = 0;
				}
			}
		}
	}
	syslog(LOG_INFO, "Client: %s Tailing logs stopped.", cli);
	close(fd);
    pthread_exit(NULL);
}

void onopen(ws_cli_conn_t *client)
{
	char* cli = ws_getaddress(client);
	client->status = CONNECTED;
	client->tailing_t = 0;
	syslog(LOG_INFO, "Connection opened, addr: %s", cli);
	syslog(LOG_INFO, "Client %s status changed to CONNECTED", cli);
	client->query = NULL;
	ws_sendframe_txt(client, "~~Вы подключились к сервису просмотра логов");
}

void onclose(ws_cli_conn_t *client)
{
	char* cli = ws_getaddress(client);
	if (client->query) {
		free(client->query);
	}
	if (client->tailing_t) {
		pthread_cancel(client->tailing_t);
	}
	syslog(LOG_INFO, "Connection closed, addr: %s\n", cli);
}

void onmessage(ws_cli_conn_t *client, const unsigned char *msg, uint64_t size, int type) {
	char* cli = ws_getaddress(client);
	syslog(LOG_INFO, "Received message: %s (size: %" PRId64 ", type: %d), from: %s\n", msg, size, type, cli);
	
	char* message = malloc(size + 1);
	if (!message) {
		syslog(LOG_CRIT, "Memory error at line %d. Exiting process.", __LINE__);
		exit(EXIT_FAILURE);
	}
	strncpy(message, (const char *)msg, size);
	message[size] = '\0';

	if (strchr(message, ':') != NULL) {
		if (client->status == CONNECTED) {
			char* saveptr;
			char* msg_temp = strndup((const char *)msg, size);
			char* msg_cmd = strtok_r(msg_temp, ":", &saveptr);
			if (msg_cmd != NULL && (strncmp(msg_cmd, "tail", 4) == 0)) {
				client->status = TAILING;
				syslog(LOG_INFO, "Client %s status changed to TAILING", cli);
				msg_cmd = strtok_r(NULL, ":", &saveptr);
				client->query = strdup(msg_cmd);
				syslog(LOG_INFO, "Client %s put query '%s'", cli, client->query);
				ws_sendframe_txt(client, "~~tailing started");
				pthread_create(&client->tailing_t, NULL, tailing_logs, client);
			}
			else {
				syslog(LOG_WARNING, "Client %s send bad command %s", cli, msg);
				ws_sendframe_txt(client, "~~bad command");
			}
			free(msg_temp);
		}
		else {
			ws_sendframe_txt(client, "~~tailing in process, press STOP button");
		}
	}
	else if (size == 4 && strncmp((const char *)msg, "stop", 4) == 0) {
		if (client->tailing_t) {
			pthread_cancel(client->tailing_t);
		}
		client->status = CONNECTED;
		client->query = NULL;
		syslog(LOG_INFO, "Client %s status changed to CONNECTED", cli);
		ws_sendframe_txt(client, "~~tailing stopped. waiting for command");
	}
	else if (size == 4 && strncmp((const char *)msg, "tail", 4) == 0) {
		if (client->status == CONNECTED) {
			client->status = TAILING;
			ws_sendframe_txt(client, "~~tailing started");
			syslog(LOG_INFO, "Client %s status changed to TAILING", cli);
			client->query = NULL;
			pthread_create(&client->tailing_t, NULL, tailing_logs, client);
		} else {
			ws_sendframe_txt(client, "~~tailing in process, press STOP button");
		}
	}
	else {
		syslog(LOG_WARNING, "Client %s send bad command %s", cli, msg);
		ws_sendframe_txt(client, "~~bad command");
	}
	free(message);
}

int main(int argc, char** argv) {
    checkArgs(&argc, argv);
	openlog(APP_NAME, LOG_CONS | LOG_PID, LOG_DAEMON);
    (void)argc;
    
    if (daemon_mode) {
        daemonize(APP_NAME);
		syslog(LOG_INFO, "WebSocket сервер запущен в режиме демона на порту %d", port);
    }
	else {
		syslog(LOG_INFO, "WebSocket сервер запущен в режиме терминала на порту %d", port);
	}
	
	struct ws_events evs;
	evs.onopen    = &onopen;
	evs.onclose   = &onclose;
	evs.onmessage = &onmessage;
	ws_socket(&evs, port, 0, 1000); /* Never returns. */

	return (EXIT_SUCCESS);
}
