#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct { 
    char data[1000];
    size_t len;
} buffer;

//LogFormat "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-agent}i\"" combined

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

void checkArgs(int *argc, char* argv[]) {
    if (*argc != 2) {
        printf("\n\n --- ! ОШИБКА ЗАПУСКА ----\n\n"
        "Программа принимает на вход только один аргумент - путь до проверяемого файла\n\n"
        "------ Синтаксис ------\n"
        "%s <filepath>\n"
        "-----------------------\n\n"
        "Пример запуска\n"
        "%s ./files/test\n\n"
        "--- Повторите запуск правильно ---\n\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char* argv[]) {

    // checkArgs(&argc, argv);
    // char* CUR_FILE_NAME = argv[1];
    char* CUR_FILE_NAME = "./log.txt";
    (void)argc;

    FILE *fp;

    size_t fsize = getFileSize(CUR_FILE_NAME);
    if (fsize == 0) {
        puts("Размер проверяемого файла 0");
        exit(EXIT_FAILURE);
    }
    if ((fp = fopen(CUR_FILE_NAME, "rb")) == NULL) {
        puts("невозможно открыть файл");
        exit (EXIT_FAILURE);
    }
   
    buffer temp_buffer;
    memset(temp_buffer.data, 0, sizeof(temp_buffer.data));
    temp_buffer.len = 0;
    
    uint8_t* start_address = mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fp->_file, 0);
    fclose(fp);
    size_t i = 0;
    // char sep[1]="\"";
    char *istr;
    while (*(start_address + i*sizeof(uint8_t)) != '\n' && i < fsize) {
        temp_buffer.data[i] =  *(start_address + i*sizeof(uint8_t));
        i++;
    }
    temp_buffer.data[i] = '\0';
    temp_buffer.len = i;
    munmap(start_address, fsize);
    // for (int k = 0; k < temp_buffer.len; k++) {
    //     printf("%c", temp_buffer.data[k]);
    // }
    // puts("");
    int x = 0;
    istr = strtok(temp_buffer.data, "\"");
    while (istr != NULL)
    {
        switch (x)
        {
        case 1:
            printf("URL: \n%s\n\n", istr);
            break;
        case 2:
            printf("Statistics: \n%s\n\n", istr);
            break;
        case 3:
            printf("Referer: \n%s\n\n", istr);
            break;
        case 5:
            printf("User Agent: \n%s\n\n", istr);
            break;
        default:
            break;
        }
        // puts(istr);
        x++;
        istr = strtok(NULL,"\"");
    }

    // free(temp_buffer.data);

	return EXIT_SUCCESS;
}
