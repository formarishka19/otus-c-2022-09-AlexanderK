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
#include <pthread.h>


#define MAX_URL_LEN 10000
#define MAX_USERAGENT_LEN 1000
#define MAX_LOGENTRY_LEN 10000
#define MAX_LOG_ENTRIES 10000000
#define MAX_FILEPATH_LEN 1000

pthread_t tid;

typedef struct { 
    char url[MAX_URL_LEN];
    size_t count;
} record;

typedef struct {
    char url[MAX_URL_LEN];
    char stats[50];
    char ref[MAX_URL_LEN];
    char ua[MAX_USERAGENT_LEN];
} row;


typedef struct {
    char temp_buffer[MAX_LOGENTRY_LEN];
    size_t buffer_size;
    GHashTable* ht;
    int* total_elements;
    record* elements;
} t_args;

typedef struct {
    pthread_t id;
    char temp_buffer[MAX_LOGENTRY_LEN];
    // size_t buffer_size;
    GHashTable* ht;
    int* total_elements;
    record* elements;
    int status;
    // void** rval_ptr;
    // char url[MAX_URL_LEN];
} worker;

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
    if (*argc != 3) {
        printf("\n\n --- ! ОШИБКА ЗАПУСКА ----\n\n"
        "Программа принимает два аргумента - путь до директории с логами и количество потоков\n\n"
        "------ Синтаксис ------\n"
        "%s <dirpath> <n>\n"
        "-----------------------\n\n"
        "Пример запуска\n"
        "%s ./logs/ 2\n\n"
        "--- Повторите запуск правильно ---\n\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }
}


void* thread_func(void* arg) {
    // puts("slave thread");
    // t_args* a = arg;
    worker* a = arg;
    char* part_string;
    int x = 0;
    record* cur_record = NULL;
    row row_parsed;
    part_string = strtok(a->temp_buffer, "\"");
    while (part_string != NULL)
    {
        switch (x)
        {
        case 1:
            strncpy(row_parsed.url, part_string, strlen(part_string)+1);
            break;
        case 2:
            strncpy(row_parsed.stats, part_string, strlen(part_string)+1);
            break;
        case 3:
            strncpy(row_parsed.ref, part_string, strlen(part_string)+1);
            break;
        case 5:
            strncpy(row_parsed.ua, part_string, strlen(part_string)+1);
            break;
        default:
            break;
        }
        x++;
        part_string = strtok(NULL,"\"");
    }
    memset(a->temp_buffer, 0, MAX_LOGENTRY_LEN);
    if (g_hash_table_contains(a->ht, row_parsed.url)) {
        cur_record = (record*)g_hash_table_lookup(a->ht, row_parsed.url);
        cur_record->count = cur_record->count + 1;
        // strncpy(a->url, row_parsed.url, strlen(row_parsed.url));
        a->status = 0;
        pthread_exit(NULL);
    }
    else {
        int cur_counter = *a->total_elements;
        *a->total_elements = *a->total_elements + 1;
        g_hash_table_insert(a->ht, (gpointer)(row_parsed.url), (gpointer)&(a->elements[cur_counter]));
        strncpy(a->elements[cur_counter].url, row_parsed.url, MAX_URL_LEN);
        a->elements[cur_counter].count = 1;
        // strncpy(a->url, row_parsed.url, strlen(row_parsed.url));
        a->status = 0;
        // printf("slave: %s", row_parsed.url);
        pthread_exit(NULL);
    }
}

void parse_log_record(char* data, size_t data_len, GHashTable* ht, int* total_records, record* r_arr) {
    char* part_string;
    int x = 0;
    record* cur_record = NULL;
    row row_parsed;
    part_string = strtok(data, "\"");
    while (part_string != NULL)
    {
        switch (x)
        {
        case 1:
            strncpy(row_parsed.url, part_string, strlen(part_string)+1);
            break;
        case 2:
            strncpy(row_parsed.stats, part_string, strlen(part_string)+1);
            break;
        case 3:
            strncpy(row_parsed.ref, part_string, strlen(part_string)+1);
            break;
        case 5:
            strncpy(row_parsed.ua, part_string, strlen(part_string)+1);
            break;
        default:
            break;
        }
        x++;
        part_string = strtok(NULL,"\"");
    }
    if (g_hash_table_contains(ht, row_parsed.url)) {
        cur_record = (record*)g_hash_table_lookup(ht, row_parsed.url);
        cur_record->count++;
    }
    else {
        int cur_counter = *total_records;
        *total_records = *total_records + 1;
        g_hash_table_insert(ht, (gpointer)(row_parsed.url), (gpointer)&r_arr[cur_counter]);
        strncpy(r_arr[cur_counter].url, row_parsed.url, sizeof(row_parsed.url));
        r_arr[cur_counter].count = 1;
    }
    memset(data, 0, data_len);
}

int main(int argc, char* argv[]) {

    checkArgs(&argc, argv);
    char LOGDIR[MAX_FILEPATH_LEN]="\0";
    strncpy(LOGDIR, argv[1], strlen(argv[1]));
    char CUR_FILE_NAME[MAX_FILEPATH_LEN]="\0";
    int t_count;
    sscanf(argv[2], "%d", &t_count);
    // char CUR_DIR[200] = "/Users/aleksandrk/dev/c/otus-c-2022-09-AlexanderK/hw11_threads/logs/logs/";
    (void)argc;
    worker workers[t_count];
    for (int w = 0; w < t_count; w++) {
        workers[w].status = 0;
    }
    int cur_worker = 0;

    FILE *fp;
    DIR *dp;
    struct dirent *ep;     
    dp = opendir(LOGDIR);
    char temp_buffer[MAX_LOGENTRY_LEN];
    int r_count = 0;
    uint8_t* start_address;
    GHashTable* hash_table = g_hash_table_new(g_str_hash, NULL);
    record* r_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
    if (!r_array) {
        puts("Memory error");
        return EXIT_FAILURE;
    }
    size_t i = 0;
    size_t k = 0;
    int rc;
    // int res = 0;
    // record* cur_record = NULL;
    

    if (!dp)
    {
        printf("Couldn't open the directory %s\n", LOGDIR);
        return EXIT_FAILURE;
    }
    while ((ep = readdir(dp)) != NULL) {
        if (strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0) {
            strncpy(CUR_FILE_NAME, LOGDIR, sizeof(LOGDIR));
            strcat(CUR_FILE_NAME, ep->d_name);
            printf("дир %s\n", LOGDIR);
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
            memset(temp_buffer, 0, sizeof(temp_buffer));
            while (i < fsize) {
                if (*(start_address + i*sizeof(uint8_t)) != '\n' && k < fsize) {
                    if (k == fsize) {
                        k = 0;
                        memset(temp_buffer, 0, MAX_LOGENTRY_LEN);
                        break;
                    }
                    temp_buffer[k] =  *(start_address + i*sizeof(uint8_t));
                    k++;
                }
                else {
                    temp_buffer[k + 1] = '\0';
                    
                    if (cur_worker < t_count) {
                        strncpy(workers[cur_worker].temp_buffer, temp_buffer, MAX_LOGENTRY_LEN);
                        // workers[cur_worker].buffer_size = MAX_LOGENTRY_LEN;
                        workers[cur_worker].ht = hash_table;
                        workers[cur_worker].total_elements = &r_count;
                        workers[cur_worker].elements = r_array;
                        workers[cur_worker].status = 1;
                        cur_worker++;
                    }
                    else {
                        for (int w = 0; w < t_count; ++w) {
                            if ((rc = pthread_create(&workers[w].id, NULL, thread_func, &workers[w]))) {
                                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                                return EXIT_FAILURE;
                            }
                        }
                        for (int w = 0; w < t_count; ++w) {
                            pthread_join(workers[w].id, NULL);
                        }
                        cur_worker = 0;
                        strncpy(workers[cur_worker].temp_buffer, temp_buffer, MAX_LOGENTRY_LEN);
                        // workers[cur_worker].buffer_size = MAX_LOGENTRY_LEN;
                        workers[cur_worker].ht = hash_table;
                        workers[cur_worker].total_elements = &r_count;
                        workers[cur_worker].elements = r_array;
                        workers[cur_worker].status = 1;
                        cur_worker++;
                    }
                    k = 0;
                    memset(temp_buffer, 0, MAX_LOGENTRY_LEN);

                    //single mode
                    // parse_log_record(temp_buffer, sizeof(temp_buffer), hash_table, &r_count, r_array);
                }
                i++;
            }
            for (int w = 0; w < t_count; ++w) {
                if (workers[w].status == 1) {
                    if ((rc = pthread_create(&workers[w].id, NULL, thread_func, &workers[w]))) {
                        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
                        return EXIT_FAILURE;
                    }
                    
                    pthread_join(workers[w].id, NULL);
                }
            }
            cur_worker = 0;
            munmap(start_address, fsize);
        }
    }
    g_hash_table_destroy(hash_table); 
    // r_count--;
    // for (int o=0; o<r_count; ++o) {
    //     printf("count %zu ", r_array[o].count);
    // }
    printf("unique URLs %d\n", r_count);
    qsort(r_array, r_count, sizeof(record), compare);
    for (int t=0; t<10; t++) {
        printf("URL %s - total %ld\n", r_array[t].url, r_array[t].count);
    }
    free(r_array);
    (void) closedir (dp);
	return EXIT_SUCCESS;
}
