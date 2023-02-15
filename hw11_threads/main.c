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
#include <time.h>

#define MAX_URL_LEN 10000
#define MAX_USERAGENT_LEN 1000
#define MAX_LOGENTRY_LEN 10000
#define MAX_LOG_ENTRIES 1000000000
#define MAX_PATH_LEN 1000
#define MAX_LOG_FILES 100

clock_t start, end;
double cpu_time_used;

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
    size_t fsize;
    char filepath[MAX_PATH_LEN];
} logfile;

typedef struct {
    char dir_path[MAX_PATH_LEN];
    logfile files[MAX_LOG_FILES];
    int count;
} logdir;

typedef struct {
    pthread_t id;
    logfile file;
    GHashTable* hash_table;
    char temp_buffer[MAX_LOGENTRY_LEN];
    int r_count;
    record* r_array;
    int status;
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

void get_files(logdir* file_list) {
    DIR *dp;
    FILE *fp;
    struct dirent *ep;
    char filepath[MAX_PATH_LEN];
    size_t fsize;
    dp = opendir(file_list->dir_path);
    if (!dp)
    {
        printf("Couldn't open the directory %s\n", file_list->dir_path);
        exit(EXIT_FAILURE);
    }
    while ((ep = readdir(dp)) != NULL) {
        if (strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0) {
            strncpy(filepath, file_list->dir_path, MAX_PATH_LEN);
            strcat(filepath, ep->d_name);
            fsize = getFileSize(filepath);
            if (fsize == 0) {
                printf("Размер проверяемого файла %s 0\n", filepath);
                exit(EXIT_FAILURE);
            }
            if ((fp = fopen(filepath, "rb")) == NULL) {
                printf("невозможно открыть файл %s\n", filepath);
                exit(EXIT_FAILURE);
            }
            fclose(fp);
            strncpy(file_list->files[file_list->count].filepath, filepath, MAX_PATH_LEN);
            file_list->files[file_list->count].fsize = fsize;
            file_list->count++;
        }
    }
    (void)closedir(dp);
    return;
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

void parse_log_record(char* data, GHashTable* ht, int* total_records, record* r_arr) {
    char* part_string;
    char* saveptr;
    int x = 0;
    record* cur_record = NULL;
    row row_parsed;
    part_string = strtok_r(data, "\"", &saveptr);
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
        part_string = strtok_r(NULL,"\"", &saveptr);
    }
    if (g_hash_table_contains(ht, row_parsed.url)) {
        cur_record = (record*)g_hash_table_lookup(ht, row_parsed.url);
        cur_record->count++;
    }
    else {
        int cur_counter = *total_records;
        *total_records = *total_records + 1;
        g_hash_table_insert(ht, (gpointer)(row_parsed.url), (gpointer)&r_arr[cur_counter]);
        strncpy(r_arr[cur_counter].url, row_parsed.url, MAX_URL_LEN);
        r_arr[cur_counter].count = 1;
    }
    
}

void* thread_func(void* arg) {
    worker* a = arg;
    size_t i = 0;
    size_t k = 0;
    FILE *fp = fopen(a->file.filepath, "rb");
    uint8_t* start_address = mmap(NULL, a->file.fsize, PROT_READ, MAP_PRIVATE, fp->_file, 0);
    fclose(fp);
    printf("%s file mapped\n", a->file.filepath);
    while (i < a->file.fsize) {
        if (*(start_address + i*sizeof(uint8_t)) != '\n' && i != a->file.fsize - 1) {
            a->temp_buffer[k] =  *(start_address + i*sizeof(uint8_t));
            k++;
        } 
        else {
            a->temp_buffer[k + 1] = '\0';
            k = 0;
            parse_log_record(a->temp_buffer, a->hash_table, &(a->r_count), a->r_array);
            memset(a->temp_buffer, 0, MAX_LOGENTRY_LEN);
        }
        i++;
    }
    munmap(start_address, a->file.fsize);
    printf("elements %d\n", a->r_count);
    qsort(a->r_array, a->r_count, sizeof(record), compare);
    for (int t=0; t<10; t++) {
        printf("URL %s - total %ld\n", a->r_array[t].url, a->r_array[t].count);
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {

    checkArgs(&argc, argv);
    // char CUR_DIR[200] = "/Users/aleksandrk/dev/c/otus-c-2022-09-AlexanderK/hw11_threads/logs/logs/";
    int t_count;
    int f_count;
    sscanf(argv[2], "%d", &t_count);
    (void)argc;

    logdir log_files;
    log_files.count = 0;
    strncpy(log_files.dir_path, argv[1], MAX_PATH_LEN);

    get_files(&log_files);
    f_count = log_files.count;
    worker workers[t_count];
    for (int i = 0; i < t_count; i++) {
        if (i < f_count) {
            workers[i].file = log_files.files[i];
            workers[i].status = 1;
            workers[i].hash_table = g_hash_table_new(g_str_hash, NULL);
            workers[i].r_count = 0;
            memset(workers[i].temp_buffer, 0, MAX_LOGENTRY_LEN);
            workers[i].r_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
            if (!workers[i].r_array) {
                printf("Memory error while worker %d initialazing, set less threads\n", i);
                return EXIT_FAILURE;
            }
        }
        else {
            workers[i].status = 0;
        }
    }
    clock_t t;
    t = clock();
    for (int w = 0; w < t_count; w++) {
        if (workers[w].status != 0) {
            pthread_create(&workers[w].id, NULL, thread_func, &workers[w]);
        }
    }
    for (int w = 0; w < t_count; w++) {
        if (workers[w].status != 0) {
            pthread_join(workers[w].id, NULL);
            g_hash_table_destroy(workers[w].hash_table);
            free(workers[w].r_array);
        }
    }
    t = clock() - t;
    double time_taken = ((double)t)/CLOCKS_PER_SEC;
    printf("workers: %d, execute time: %f seconds\n", t_count, time_taken);
	return EXIT_SUCCESS;
}
