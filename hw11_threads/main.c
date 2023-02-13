#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>
#include <sys/types.h>
#include <dirent.h>

typedef struct { 
    char data[10000];
    size_t len;
} buffer;

typedef struct { 
    char url[10000];
    size_t count;
} record;


int compare(const void *a, const void *b) {
  
    record* recordA = (record*)a;
    record* recordB = (record*)b;
  
    return (recordB->count - recordA->count);
}

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
    char CUR_FILE_NAME[200] = "";
    char CUR_DIR[200] = "/Users/aleksandrk/dev/c/otus-c-2022-09-AlexanderK/hw11_threads/logs/logs/";
    (void)argc;

    FILE *fp;
    DIR *dp;
    struct dirent *ep;     
    dp = opendir(CUR_DIR);
    buffer temp_buffer;
    int r_count = 0;
    record* cur_record = NULL;
    uint8_t* start_address;
    GHashTable* hash_table = g_hash_table_new(g_str_hash, NULL);
    record* r_array = malloc(1000000000 * sizeof(record));
    char* istr;
    char url[10000];
    char stats[50];
    char ref[10000];
    char ua[1000];
    int x;
    size_t i = 0;
    size_t k = 0;

    if (!dp)
    {
        puts("Couldn't open the directory");
        printf("Couldn't open the directory %s\n", CUR_DIR);
        return EXIT_FAILURE;
    }
    while ((ep = readdir(dp)) != NULL) {
        if (strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0) {
            strncpy(CUR_FILE_NAME, CUR_DIR, sizeof(CUR_DIR));
            strcat(CUR_FILE_NAME, ep->d_name);
            size_t fsize = getFileSize(CUR_FILE_NAME);
            if (fsize == 0) {
                printf("Размер проверяемого файла %s 0\n", CUR_FILE_NAME);
                exit(EXIT_FAILURE);
            }
            if ((fp = fopen(CUR_FILE_NAME, "rb")) == NULL) {
                printf("невозможно открыть файл %s\n", CUR_FILE_NAME);
                exit(EXIT_FAILURE);
            }
            start_address = mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fp->_file, 0);
            fclose(fp);
            printf("%s file mapped\n", CUR_FILE_NAME);
            i = 0;
            k = 0;
            memset(temp_buffer.data, 0, sizeof(temp_buffer.data));
            temp_buffer.len = 0;
            while (i < fsize) {
                if (*(start_address + i*sizeof(uint8_t)) != '\n' && i != fsize - 1) {
                    temp_buffer.data[k] =  *(start_address + i*sizeof(uint8_t));
                    k++;
                } 
                else {
                    temp_buffer.data[k + 1] = '\0';
                    temp_buffer.len = k + 1;
                    k = 0;
                    x = 0;
                    istr = strtok(temp_buffer.data, "\"");
                    while (istr != NULL)
                    {
                        switch (x)
                        {
                        case 1:
                            strncpy(url, istr, strlen(istr)+1);
                            break;
                        case 2:
                            strncpy(stats, istr, strlen(istr)+1);
                            break;
                        case 3:
                            strncpy(ref, istr, strlen(istr)+1);
                            break;
                        case 5:
                            strncpy(ua, istr, strlen(istr)+1);
                            break;
                        default:
                            break;
                        }
                        x++;
                        istr = strtok(NULL,"\"");

                    }
                    if (g_hash_table_contains(hash_table, url)) {
                        cur_record = (record*)g_hash_table_lookup(hash_table, url);
                        cur_record->count = cur_record->count + 1;
                    }
                    else {
                        strncpy(r_array[r_count].url, url, sizeof(url));
                        r_array[r_count].count = 1;
                        g_hash_table_insert(hash_table, (gpointer)url, (gpointer)&r_array[r_count]);
                        r_count++;
                    }
                    memset(&url[0], 0, sizeof(url));
                    cur_record = NULL;
                    memset(temp_buffer.data, 0, sizeof(temp_buffer.data));
                    temp_buffer.len = 0;
                }
                i++;
            }
            munmap(start_address, fsize);
        }
    }
    g_hash_table_destroy(hash_table);
    r_count--;
    printf("elements %d\n", r_count);
    qsort(r_array, r_count, sizeof(record), compare);
    for (int t=0; t<10; t++) {
        printf("URL %s - total %ld\n", r_array[t].url, r_array[t].count);
    }
    free(r_array);
    (void) closedir (dp);
	return EXIT_SUCCESS;
}
