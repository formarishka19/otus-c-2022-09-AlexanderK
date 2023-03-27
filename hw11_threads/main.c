#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>
#include <gmodule.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

#define MAX_URL_LEN 10000
#define MAX_USERAGENT_LEN 1000
#define MAX_LOGENTRY_LEN 10000
#define START_LOG_ENTRIES 100000
#define MAX_PATH_LEN 1000
#define MAX_LOG_FILES 100

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_BOLD    "\033[1m"
#define ANSI_COLOR_RESET   "\x1b[0m"

struct timespec start, finish;
double elapsed;
unsigned long total_bytes = 0;

typedef struct { 
    char url[MAX_URL_LEN];
    size_t count;
    unsigned long bytes;
} record;

typedef struct {
    char url[MAX_URL_LEN];
    char stats[50];
    char ref[MAX_URL_LEN];
    char ua[MAX_USERAGENT_LEN];
    unsigned long bytes;
} row;

typedef struct {
    size_t fsize;
    char filepath[MAX_PATH_LEN];
} logfile;

typedef struct {
    char dir_path[MAX_PATH_LEN];
    GSList* list;
    int count;
} logdir;

typedef struct {
    pthread_t id;
    logfile file;
    int status;
    GHashTable* hash_table_url;
    GHashTable* hash_table_ref;
    char temp_buffer[MAX_LOGENTRY_LEN];
} worker;

int compare_url(const void *a, const void *b) {
    record* recordA = (record*)a;
    record* recordB = (record*)b;
    return (recordB->bytes - recordA->bytes);
}

int compare_ref(const void *a, const void *b) {
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

void get_files(logdir* file_list, logfile* files) {
    DIR *dp;
    FILE *fp;
    struct dirent *ep;
    char filepath[MAX_PATH_LEN];
    size_t fsize;
    dp = opendir(file_list->dir_path);
    int counter = 0;
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
            strncpy(files[counter].filepath, filepath, MAX_PATH_LEN);
            files[counter].fsize = fsize;
            file_list->list = g_slist_append(file_list->list, (logfile*)&files[counter]);
            counter++;
        }
        file_list->count = counter;
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

void parse_log_record(char* data, GHashTable* ht_url, GHashTable* ht_ref) {
    char* part_string;
    char* saveptr;
    int x = 0;
    row row_parsed;
    char* temp;
    gchar** url_split;
    gchar** stats_split;
    part_string = strtok_r(data, "\"", &saveptr);
    while (part_string != NULL)
    {
        switch (x)
        {
        case 1:
            url_split = g_strsplit((const gchar *)part_string, " ", -1);
            if ((char*)url_split[1] != NULL) {
                strncpy(row_parsed.url, (char*)url_split[1], MAX_URL_LEN);
            }
            else {
                strncpy(row_parsed.url, (char*)url_split[0], MAX_URL_LEN);
            }
            g_strfreev(url_split);
            break;
        case 2:
            if (strcmp("-", part_string) == 0) {
                row_parsed.bytes = 0;
            }
            else {
                stats_split = g_strsplit((const gchar *)part_string, " ", -1);
                if (stats_split[2]) {
                    row_parsed.bytes = strtol((char*)stats_split[2], &temp, 10);
                }
                g_strfreev(stats_split);
            }  
            break;
        case 3:
            strncpy(row_parsed.ref, part_string, MAX_URL_LEN);
            break;
        case 5:
            strncpy(row_parsed.ua, part_string, MAX_USERAGENT_LEN);
            break;
        default:
            break;
        }
        x++;
        part_string = strtok_r(NULL,"\"", &saveptr);
    }
    unsigned int url_bytes = (uint32_t)(uintptr_t)g_hash_table_lookup(ht_url, row_parsed.url);
    if (!url_bytes) {
        g_hash_table_insert(ht_url, (gpointer)g_strdup(row_parsed.url), (gpointer)(uintptr_t)row_parsed.bytes);
    }
    else {
        g_hash_table_insert(ht_url, (gpointer)g_strdup(row_parsed.url), (gpointer)(uintptr_t)(url_bytes + row_parsed.bytes));
    }
    unsigned int ref_count = (uint32_t)(uintptr_t)g_hash_table_lookup(ht_ref, row_parsed.ref);
    if (!ref_count) {
        g_hash_table_insert(ht_ref, (gpointer)g_strdup(row_parsed.ref), (gpointer)(uintptr_t)1);
    }
    else {
        g_hash_table_insert(ht_ref, (gpointer)g_strdup(row_parsed.ref), (gpointer)(uintptr_t)(ref_count + 1));
    }    
}

void clear_key(gpointer data) {
    free(data);
}

void fetch_url_ht(gpointer key, gpointer value, gpointer array) {
    record rec;
    strncpy(rec.url, (char*)key, MAX_URL_LEN);
    rec.bytes = (uint32_t)(uintptr_t)value;
    total_bytes += rec.bytes;
    g_array_append_val(array, rec);
}

void fetch_ref_ht(gpointer key, gpointer value, gpointer array) {
    record rec;
    strncpy(rec.url, (char*)key, MAX_URL_LEN);
    rec.count = (uint32_t)(uintptr_t)value;
    g_array_append_val(array, rec);
}

void fetch_url_results(gpointer key, gpointer value, gpointer url_data) {

    unsigned int url_bytes = (uint32_t)(uintptr_t)g_hash_table_lookup(url_data, key);
    if (!url_bytes) {
        g_hash_table_insert(url_data, (gpointer)g_strdup((char*)key), (gpointer)(uintptr_t)value);
    }
    else {
        g_hash_table_insert(url_data, (gpointer)g_strdup((char*)key), (gpointer)(url_bytes + (uintptr_t)value));
    }
}

void fetch_ref_results(gpointer key, gpointer value, gpointer ref_data) {
    unsigned int ref_count = (uint32_t)(uintptr_t)g_hash_table_lookup(ref_data, key);
    if (!ref_count) {
        g_hash_table_insert(ref_data, (gpointer)g_strdup((char*)key), (gpointer)(uintptr_t)value);
    }
    else {
        g_hash_table_insert(ref_data, (gpointer)g_strdup((char*)key), (gpointer)(ref_count + (uintptr_t)value));
    }
}

int worker_fetch(worker* w, GHashTable* ht_url, GHashTable* ht_ref) {
    g_hash_table_foreach(w->hash_table_url, fetch_url_results, ht_url);
    g_hash_table_foreach(w->hash_table_ref, fetch_ref_results, ht_ref);

    return(EXIT_SUCCESS);
}

int worker_load(worker* w, logfile* file) {
    w->status = 1;
    w->hash_table_url = g_hash_table_new_full(g_str_hash, g_str_equal, clear_key, NULL);
    w->hash_table_ref = g_hash_table_new_full(g_str_hash, g_str_equal, clear_key, NULL);
    w->file = *file;
    memset(w->temp_buffer, 0, MAX_LOGENTRY_LEN);
    return(EXIT_SUCCESS);
}

int worker_clear(worker* w) {
    w->status = 0;
    g_hash_table_destroy(w->hash_table_url);
    g_hash_table_destroy(w->hash_table_ref);
    return(EXIT_SUCCESS);
}

void* thread_func(void* arg) {
    worker* a = arg;
    size_t i = 0;
    size_t k = 0;
    FILE *fp = fopen(a->file.filepath, "rb");
    uint8_t* start_address = mmap(NULL, a->file.fsize, PROT_READ, MAP_PRIVATE, fileno(fp), 0);
    fclose(fp);
    while (i < a->file.fsize) {
        if (*(start_address + i*sizeof(uint8_t)) != '\n' && i != a->file.fsize - 1) {
            a->temp_buffer[k] =  *(start_address + i*sizeof(uint8_t));
            k++;
        } 
        else {
            a->temp_buffer[k + 1] = '\0';
            k = 0;
            parse_log_record(a->temp_buffer, a->hash_table_url, a->hash_table_ref);
            memset(a->temp_buffer, 0, MAX_LOGENTRY_LEN);
        }
        i++;
    }
    munmap(start_address, a->file.fsize);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {

    checkArgs(&argc, argv);
    int t_count;
    sscanf(argv[2], "%d", &t_count);
    (void)argc;

    logfile files[MAX_LOG_FILES];
    logdir log_files;
    log_files.count = 0;
    log_files.list = NULL;
    strncpy(log_files.dir_path, argv[1], MAX_PATH_LEN);

    get_files(&log_files, files);

    worker workers[t_count];
    for (int w = 0; w < t_count; w++) {
        workers[w].status = 0;
    }
    GSList* iterator = log_files.list;

    GHashTable* hash_table_url_res = g_hash_table_new_full(g_str_hash, g_str_equal, clear_key, NULL);
    GHashTable* hash_table_ref_res = g_hash_table_new_full(g_str_hash, g_str_equal, clear_key, NULL);
    GArray* url_arr_cobmined;
    GArray* ref_arr_cobmined;
    url_arr_cobmined = g_array_sized_new(FALSE, FALSE, sizeof (record), START_LOG_ENTRIES);
    ref_arr_cobmined = g_array_sized_new(FALSE, FALSE, sizeof (record), START_LOG_ENTRIES);

    clock_gettime(CLOCK_MONOTONIC, &start);

    int threads_count = 0;
    while (iterator) {
        for (int counter = 0; counter < t_count; counter++) {
            if (workers[counter].status == 0 && iterator) {
                threads_count++;
                printf("active threads %d\n", threads_count);
                worker_load(&workers[counter], (logfile*)(iterator->data));
                pthread_create(&workers[counter].id, NULL, thread_func, &workers[counter]);
                printf("started file\t %s\n", workers[counter].file.filepath);
                iterator = iterator->next;
            }
        }
        do {
            threads_count--;
            pthread_join(workers[threads_count].id, NULL);
            puts(workers[threads_count].file.filepath);
            worker_fetch(&workers[threads_count], hash_table_url_res, hash_table_ref_res);
            worker_clear(&workers[threads_count]);
            printf("active threads %d\n", threads_count);
        }
        while (threads_count);
    }

    
    g_hash_table_foreach(hash_table_url_res, fetch_url_ht, url_arr_cobmined);
    g_hash_table_foreach(hash_table_ref_res, fetch_ref_ht, ref_arr_cobmined);

    g_array_sort(url_arr_cobmined, (GCompareFunc)compare_url);
    g_array_sort(ref_arr_cobmined, (GCompareFunc)compare_ref);

    puts("\n");
    printf(ANSI_COLOR_BOLD "Total bytes " ANSI_COLOR_RED "%lu\n" ANSI_COLOR_RESET, total_bytes);
    for (int t = 0; t < 10; t++) {
        record* rec = &g_array_index(url_arr_cobmined, record, t);
        printf("%d URL: " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\tbytes " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET "\n", t+1, rec->url, rec->bytes);
    }
    puts("\n");
    puts("TOP REFERERS");
    for (int t = 0; t < 10; t++) {
        record* rec = &g_array_index(ref_arr_cobmined, record, t);
        printf("%d REF: " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET ", count " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET "\n", t+1, rec->url, rec->count);
    }

    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

    puts("\n");
    printf("workers: %d, execute time: %f seconds\n", t_count, elapsed);
    g_slist_free(iterator);
    g_hash_table_destroy(hash_table_url_res);
    g_hash_table_destroy(hash_table_ref_res);
    g_array_free(url_arr_cobmined, 1);
    g_array_free(ref_arr_cobmined, 1);
    g_slist_free(log_files.list);
	return EXIT_SUCCESS;
}
