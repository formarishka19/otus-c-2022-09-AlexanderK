#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include "libconfig.h"

#define APP_NAME "FILE_SIZE_DAEMON"


void checkArgs(int *argc, char* argv[]) {
    if (*argc != 2) {
        printf("\n\n --- ! ОШИБКА ЗАПУСКА ----\n\n"
        "Программа принимает на вход только один аргумент - демонизировать или нет\n\n"
        "------ Синтаксис ------\n"
        "%s -d\n"
        "-----------------------\n\n"
        "Пример запуска\n"
        "%s -d\n\n"
        "--- Повторите запуск правильно ---\n\n", argv[0], argv[0]);
        exit (1);
    }
}

void daemonize(const char* cmd)
{
    int                 fd0, fd1, fd2;
    pid_t               pid;
    struct rlimit       rl;
    struct sigaction    sa;

    openlog(cmd, LOG_CONS | LOG_PID, LOG_DAEMON);
    // setlogmask(LOG_UPTO(LOG_DEBUG));
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

int main(int argc, char* argv[]) {

    if (argc != 1 && argc != 2) {
        (void)argc;
        printf("Программа %s запущена с неверными аргументами. Выход.\n", APP_NAME);
        exit(EXIT_FAILURE);
    }

    // checkArgs(&argc, argv);
    // string CUR_FILE_NAME = argv[1];
    // printf("%d", argc);
    config_t cfg;
    const char *str;
    if(! config_read_file(&cfg, "./cfg/daemon.cfg"))
    {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return(EXIT_FAILURE);
    }
    /* Get the file name. */
    openlog(APP_NAME, LOG_CONS | LOG_PID, LOG_DAEMON);
    if(config_lookup_string(&cfg, "file", &str)) {
        syslog(LOG_INFO, "Имя отслеживаемого файла: %s\n", str);
    }
    else {
        syslog(LOG_ERR, "Параметр 'file' не найден в конфигурации ./cfg/daemon.cfg\n");
    }
    config_destroy(&cfg);

    if (argc == 2 && (strcmp(argv[1], "-d") == 0)) {
        (void)argc;
        daemonize(APP_NAME);
    }
    else if (argc == 1) {
        printf("Программа %s была запущена в обычном режиме\n", APP_NAME);
    }
    else {
        printf("Программа %s запущена с неверными аргументами. Выход.\n", APP_NAME);
        exit(EXIT_FAILURE);
    }
    (void)argc;

    

    sleep(60);
    
    
	return 0;
}
