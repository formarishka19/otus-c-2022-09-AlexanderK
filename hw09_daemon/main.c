#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "libconfig.h"

#define APP_NAME "FILE_DAEMON"
#define U_SOCKET_NAME "check_fsize.socket"
#define DEFAULT_CFG_FILE "/home/otus/hw09_daemon/cfg/daemon.cfg"
#define GREETING "Здравствуйте, введите команды check для получения размера файла или stop для завершения работы сервера\n"
#define FILE_SIZE_MSG "Размер отслеживаемого файла (байт): "
#define REPEATE_MSG "Получена неизвестная команда. Повторите ввод (ckeck или stop).\n"
#define NOFILE_MSG "Не могу найти файл. Останавливаю сервер\n"

void checkArgs(int* argc, char* argv[]) {
    if (*argc < 2 || *argc > 3) {
        printf("\n\n --- ! ОШИБКА ЗАПУСКА ----\n\n"
        "Программа принимает на вход один аргумент - путь до файла конфигурации\nи опционально ключ -d для режима демонизации\n"
        "------ Синтаксис ------\n"
        "%s <cfg filepath>\n"
        "sudo %s <cfg filepath> -d\n"
        "-----------------------\n\n"
        "Пример запуска\n"
        "sudo %s /home/otus/hw09_daemon/cfg/daemon.cfg -d \n\n"
        "--- Повторите запуск правильно ---\n\n", argv[0], argv[0], argv[0]);
        exit (EXIT_FAILURE);
    }
}

size_t getFileSize(char* filename) {

    struct stat finfo;
    int status;
    status = stat(filename, &finfo);
    if(status == 0) {
        return finfo.st_size;
    }
    else {
        printf("Cannot get file info.\n");
        syslog(LOG_ERR, "Cannot get file info %s", filename);
        return(0);
    }

}

char* getFileName(char* config_file) {
    config_t cfg;
    const char* filename;
    char* filename_res = (char*) malloc(100 * sizeof(char));

    if(! config_read_file(&cfg, config_file))
    {
        syslog(LOG_ERR, "Ошибка чтения конфигурации %s:%d - %s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
        syslog(LOG_ERR, "Пробуем запустить дефолтную конфигурацию %s", DEFAULT_CFG_FILE);
        if(! config_read_file(&cfg, DEFAULT_CFG_FILE)) {
            syslog(LOG_ERR, "Ошибка чтения дефолтной конфигурации %s:%d - %s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
            config_destroy(&cfg);
            free(filename_res);
            exit(EXIT_FAILURE);
        }
    }
    
    if(config_lookup_string(&cfg, "file", &filename)) {
        syslog(LOG_INFO, "Имя отслеживаемого файла: %s", filename);
        strcpy(filename_res, filename);
        config_destroy(&cfg);
    }
    else {
        syslog(LOG_ERR, "Параметр 'file' не найден в конфигурации %s", config_file);
        config_destroy(&cfg);
        free(filename_res);
        exit(EXIT_FAILURE);
    }
    return filename_res;
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
    /*
     * Получить максимально возможный номер дескриптора файла.
     */
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        perror("невозможно получить максимальный номер дескриптора");
    }
    /*
     * Стать лидером нового сеанса, чтобы утратить управляющий терминал.
     */
    if ((pid = fork()) < 0) {
        perror("ошибка вызова функции fork");
    }
    else if (pid != 0) {
        exit(0);
    }
    setsid();
    /*
     * Обеспечить невозможность обретения управляющего терминала в будущем.
     */
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
    /*
     * Назначить корневой каталог текущим рабочим каталогом,
     * чтобы впоследствии можно было отмонтировать файловую систему.
     */
    if (chdir("/") < 0) {
        syslog(LOG_CRIT, "невозможно сделать текущим рабочим каталогом /");
    }
    /*
     * Закрыть все открытые файловые дескрипторы.
     */
    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }
    for (rlim_t i = 0; i < rl.rlim_max; i++) {
        close(i);
    }
    /*
     * Присоединить файловые дескрипторы 0, 1 и 2 к /dev/null.
     */
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_CRIT, "ошибочные файловые дескрипторы %d %d %d", fd0, fd1, fd2);
    }
    syslog(LOG_INFO, "Программа успешно демонизирована =)");
}

void socket_listen(char* filename) {
    int sock, msgsock, rval; 
    struct sockaddr_un server; 
    char buf[100];
    const char* greeting = GREETING;
    const char* filesize = FILE_SIZE_MSG;
    const char* repeate = REPEATE_MSG;
    const char* nofile = NOFILE_MSG;
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        syslog(LOG_ERR, "Невозможно открыть unix socket, выход.");
        exit(EXIT_FAILURE);
    }
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, U_SOCKET_NAME);
    if (bind(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un))) {
        syslog(LOG_ERR, "Невозможно присвоить адрес unix socket-у, выход.");
        exit(1); 
    }
    syslog(LOG_INFO, "Адрес созданного сокета %s", server.sun_path);
    listen(sock, 5);
    for (;;) {
        msgsock = accept(sock, 0, 0); 
        if (msgsock == -1) {
            syslog(LOG_ERR, "Невозможно открыть на сокете входящее соединение. Серверный сокет закрываем. Выход");
            break; 
        }
        else {
            send(msgsock, greeting, strlen(greeting), 0);
            do {
                // bzero(buf, sizeof(buf));
                memset(buf, 0 , sizeof(buf));
                if ((rval = read(msgsock, buf, 100)) < 0) {
                    syslog(LOG_WARNING, "Невозможно прочитать входящее сообщение");
                }
                
                else if (rval == 0) {
                    syslog(LOG_ERR, "Ошибка соединения");
                }
                else {

                    if (!strncmp(buf, "stop", 4)) {
                        syslog(LOG_INFO, "Получена команда остановки программы. Закрытие сокета %s", U_SOCKET_NAME);
                        close(msgsock);
                        close(sock);
                        unlink(U_SOCKET_NAME);
                        return;
                    } else if (!strncmp(buf, "check", 5)) {
                        syslog(LOG_INFO, "Получена запрос на проверку размера файла");
                        size_t fsize = getFileSize(filename);
                        if (fsize) {
                            char str[10];
                            sprintf(str, "%lu", fsize);
                            send(msgsock, filesize, strlen(filesize), 0);
                            send(msgsock, str, strlen(str), 0);
                            send(msgsock, "\n", 1, 0);
                            close(msgsock);
                        }
                        else {
                            syslog(LOG_ERR, "Файл %s невозможно открыть/прочитать", filename);
                            send(msgsock, nofile, strlen(nofile), 0);
                            close(msgsock);
                            close(sock);
                            unlink(U_SOCKET_NAME);
                            return;
                        }
                        
                    } else {
                        send(msgsock, repeate, strlen(repeate), 0); 
                    }
                }
            } while (rval > 0);
        }
        close(msgsock);
    }
    close(sock);
    unlink(U_SOCKET_NAME);
}

int main(int argc, char* argv[]) {

    checkArgs(&argc, argv);
    int daemon = 0;
    if ((argc == 3) && (strcmp(argv[2], "-d\0") == 0)) {
        daemon = 1;
    }
    char* CFG_FILE_NAME = argv[1];
    (void)argc;
 
    if (daemon) {
        daemonize(APP_NAME);
    }
    else {
        printf("Программа %s запущена в обычном режиме\n", APP_NAME);
        printf("Имя сокета %s\n", U_SOCKET_NAME);
        printf("Подключиться: nc -U %s\n", U_SOCKET_NAME);  
    }
    char* filename = getFileName(CFG_FILE_NAME);
    syslog(LOG_INFO, "Открываем сокет для входящих запросов.");
    socket_listen(filename);
    syslog(LOG_INFO, "Завершение работы программы");
	return 0;
}
