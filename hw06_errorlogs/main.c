#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "alog.h"

FILE* log_file_ptr = NULL;

void checkArgs(int* argc, char* argv[]) {
    if (*argc != 2) {
        printf("\n\n --- ! ОШИБКА ЗАПУСКА ----\n\n"
        "Программа принимает на вход только один аргумент - путь до файла логирования\n\n"
        "------ Синтаксис ------\n"
        "%s <filepath>\n"
        "-----------------------\n\n"
        "Пример запуска\n"
        "%s ./log.txt \n\n"
        "--- Повторите запуск правильно ---\n\n", argv[0], argv[0]);
        exit (1);
    }
}

void test_error(void) {
    alog_error("test test error");
}

int main(int argc, char *argv[])
{
    checkArgs(&argc, argv);
    (void)argc;

    alog_init(argv[1]);
    test_error();
    alog_debug("program started");
    sleep(1);
    alog_error("fake error occured");
    alog_fin();
    
    return 0;
}
