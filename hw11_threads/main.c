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

typedef struct {
    char url[10000];
    char stats[50];
    char ref[10000];
    char ua[1000];
} row;


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
        "Программа принимает на вход только один аргумент - путь до директории с логами\n\n"
        "------ Синтаксис ------\n"
        "%s <dirpath>\n"
        "-----------------------\n\n"
        "Пример запуска\n"
        "%s ./logs/\n\n"
        "--- Повторите запуск правильно ---\n\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }
}

void parse_log_record(char* data, row* row_parsed) {
    char* part_string;
    int x = 0;
    part_string = strtok(data, "\"");
    while (part_string != NULL)
    {
        switch (x)
        {
        case 1:
            strncpy(row_parsed->url, part_string, strlen(part_string)+1);
            break;
        case 2:
            strncpy(row_parsed->stats, part_string, strlen(part_string)+1);
            break;
        case 3:
            strncpy(row_parsed->ref, part_string, strlen(part_string)+1);
            break;
        case 5:
            strncpy(row_parsed->ua, part_string, strlen(part_string)+1);
            break;
        default:
            break;
        }
        x++;
        part_string = strtok(NULL,"\"");

    }
}

int main(int argc, char* argv[]) {

    checkArgs(&argc, argv);
    char LOGDIR[200];
    strncpy(LOGDIR, argv[1], strlen(argv[1]));
    char CUR_FILE_NAME[200] = "";
    // char CUR_DIR[200] = "/Users/aleksandrk/dev/c/otus-c-2022-09-AlexanderK/hw11_threads/logs/logs/";
    (void)argc;

    FILE *fp;
    DIR *dp;
    struct dirent *ep;     
    dp = opendir(LOGDIR);
    buffer temp_buffer;
    int r_count = 0;
    record* cur_record = NULL;
    uint8_t* start_address;
    GHashTable* hash_table = g_hash_table_new(g_str_hash, NULL);
    record* r_array = malloc(1000000000 * sizeof(record));
    row row_parsed;
    size_t i = 0;
    size_t k = 0;

    if (!dp)
    {
        printf("Couldn't open the directory %s\n", LOGDIR);
        return EXIT_FAILURE;
    }
    while ((ep = readdir(dp)) != NULL) {
        if (strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0) {
            strncpy(CUR_FILE_NAME, LOGDIR, sizeof(LOGDIR));
            strcat(CUR_FILE_NAME, ep->d_name);
            printf("дир %s 0\n", LOGDIR);
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
            while (i < 100000) {
                if (*(start_address + i*sizeof(uint8_t)) != '\n' && i != fsize - 1) {
                    temp_buffer.data[k] =  *(start_address + i*sizeof(uint8_t));
                    k++;
                } 
                else {
                    temp_buffer.data[k + 1] = '\0';
                    temp_buffer.len = k + 1;
                    k = 0;
                    parse_log_record(temp_buffer.data, &row_parsed);
                    if (g_hash_table_contains(hash_table, row_parsed.url)) {
                        cur_record = (record*)g_hash_table_lookup(hash_table, row_parsed.url);
                        cur_record->count = cur_record->count + 1;
                    }
                    else {
                        strncpy(r_array[r_count].url, row_parsed.url, sizeof(row_parsed.url));
                        r_array[r_count].count = 1;
                        g_hash_table_insert(hash_table, (gpointer)row_parsed.url, (gpointer)&r_array[r_count]);
                        r_count++;
                    }
                    memset(&row_parsed.url[0], 0, sizeof(row_parsed.url));
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