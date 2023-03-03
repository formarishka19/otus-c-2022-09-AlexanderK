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
    GArray* url_arr;
    GArray* ref_arr;
    char temp_buffer[MAX_LOGENTRY_LEN];
    unsigned long url_count;
    unsigned long ref_count;
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

void parse_log_record(char* data, GHashTable* ht_url, unsigned long* total_urls, GHashTable* ht_ref, unsigned long* total_refs, GArray* u_arr, GArray* r_arr) {
    char* part_string;
    char* saveptr;
    int x = 0;
    record* cur_record = NULL;
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
            // strncpy(row_parsed.url, part_string, strlen(part_string)+1); //почему-то после удаления этой строки все валится(((
            // upd: валилось именно при компиляции, не так выразился. решилось отдельным объявлением gchar** url_split вначале функции
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
    if (g_hash_table_contains(ht_url, row_parsed.url)) {
        unsigned long index = (unsigned long)g_hash_table_lookup(ht_url, row_parsed.url);
        cur_record = &g_array_index(u_arr, record, index);
        cur_record->count = cur_record->count + 1;
        cur_record->bytes = cur_record->bytes + row_parsed.bytes;
    }
    else {
        unsigned long cur_counter = *total_urls;
        *total_urls = *total_urls + 1;
        record new_record;
        strncpy(new_record.url, row_parsed.url, MAX_URL_LEN);
        new_record.count = 1;
        new_record.bytes = row_parsed.bytes;
        g_array_append_val(u_arr, new_record);
        g_hash_table_insert(ht_url, (gpointer)(row_parsed.url), (gpointer)cur_counter);
    }
    if (g_hash_table_contains(ht_ref, row_parsed.ref)) {
        unsigned long index = (unsigned long)g_hash_table_lookup(ht_ref, row_parsed.ref);
        cur_record = &g_array_index(r_arr, record, index);
        cur_record->count = cur_record->count + 1;
    }
    else {
        unsigned long cur_counter_ref = *total_refs;
        *total_refs = *total_refs + 1;
        record new_record;
        strncpy(new_record.url, row_parsed.ref, MAX_URL_LEN);
        new_record.count = 1;
        g_array_append_val(r_arr, new_record);
        g_hash_table_insert(ht_ref, (gpointer)(row_parsed.ref), (gpointer)cur_counter_ref);
    }
    
}

int worker_fetch(worker* w, unsigned long *total_req, unsigned long *total_refs, unsigned long *total_bytes, unsigned long *res_url_count, GHashTable* ht_url, GHashTable* ht_ref, GArray* url_arr, GArray* ref_arr) {

    record* cur_record;
    record* u_element;
    record* r_element;
    unsigned long u_counter;
    unsigned long r_counter;
    unsigned long cur_counter_url;
    unsigned long cur_counter_ref;
    char ref[MAX_URL_LEN];
    char url[MAX_URL_LEN];

    for (u_counter = 0; u_counter < w->url_count; u_counter++) {
        u_element = &g_array_index(w->url_arr, record, u_counter);
        strncpy(url, u_element->url, MAX_URL_LEN);
        if (g_hash_table_contains(ht_url, url)) {
            unsigned long index = (unsigned long)g_hash_table_lookup(ht_url, url);
            cur_record = &g_array_index(url_arr, record, index);
            cur_record->count = cur_record->count + u_element->count;
            cur_record->bytes = cur_record->bytes + u_element->bytes;
        }
        else {
            cur_counter_url = *res_url_count;
            g_array_append_val(url_arr, *u_element);
            g_hash_table_insert(ht_url, (gpointer)(url), (gpointer)cur_counter_url);
            *res_url_count = *res_url_count + 1;
        }
        *total_req = *total_req + u_element->count;
        *total_bytes = *total_bytes + u_element->bytes;
    }
    for (r_counter = 0; r_counter < w->ref_count; r_counter++) {
        r_element = &g_array_index(w->ref_arr, record, r_counter);
        strncpy(ref, r_element->url, MAX_URL_LEN);
        if (g_hash_table_contains(ht_ref, ref)) {
            unsigned long index = (unsigned long)g_hash_table_lookup(ht_ref, ref);
            cur_record = &g_array_index(ref_arr, record, index);
            cur_record->count = cur_record->count + r_element->count;
        }
        else {
            cur_counter_ref = *total_refs;
            g_array_append_val(ref_arr, *r_element);
            g_hash_table_insert(ht_ref, (gpointer)(ref), (gpointer)cur_counter_ref);
            *total_refs = *total_refs + 1;
        }
    }
    return 0;
}

int worker_load(worker* w, logfile* file) {
    w->status = 1;
    w->hash_table_url = g_hash_table_new(g_str_hash, NULL);
    w->hash_table_ref = g_hash_table_new(g_str_hash, NULL);
    w->url_arr = g_array_sized_new(FALSE, FALSE, sizeof (record), START_LOG_ENTRIES);
    w->ref_arr = g_array_sized_new(FALSE, FALSE, sizeof (record), START_LOG_ENTRIES);
    w->file = *file;
    w->url_count = 0;
    w->ref_count = 0;
    memset(w->temp_buffer, 0, MAX_LOGENTRY_LEN);
    return(EXIT_SUCCESS);
}

int worker_clear(worker* w) {
    w->status = 0;
    g_hash_table_destroy(w->hash_table_url);
    g_hash_table_destroy(w->hash_table_ref);
    g_array_free(w->url_arr, 1);
    g_array_free(w->ref_arr, 1);
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
            parse_log_record(a->temp_buffer, a->hash_table_url, &(a->url_count), a->hash_table_ref, &(a->ref_count), a->url_arr, a->ref_arr);
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

    GHashTable* hash_table_url_res = g_hash_table_new(g_str_hash, NULL);
    GHashTable* hash_table_ref_res = g_hash_table_new(g_str_hash, NULL);
    GArray* url_arr_cobmined;
    GArray* ref_arr_cobmined;
    url_arr_cobmined = g_array_sized_new(FALSE, FALSE, sizeof (record), START_LOG_ENTRIES);
    ref_arr_cobmined = g_array_sized_new(FALSE, FALSE, sizeof (record), START_LOG_ENTRIES);
    unsigned long total_requests = 0;
    unsigned long total_refs = 0;
    unsigned long total_bytes = 0;
    unsigned long res_url_count = 0;

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
            worker_fetch(&workers[threads_count], &total_requests, &total_refs, &total_bytes, &res_url_count, hash_table_url_res, hash_table_ref_res, url_arr_cobmined, ref_arr_cobmined);
            worker_clear(&workers[threads_count]);
            printf("active threads %d\n", threads_count);
        }
        while (threads_count);
    }

    g_array_sort(url_arr_cobmined, (GCompareFunc)compare_url);
    g_array_sort(ref_arr_cobmined, (GCompareFunc)compare_ref);

    puts("\n");
    printf(ANSI_COLOR_BOLD "Total bytes " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET ANSI_COLOR_BOLD ", total requests " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET "\n" ANSI_COLOR_RESET, total_bytes, total_requests);
    for (int t = 0; t < 10; t++) {
        record* rec = &g_array_index(url_arr_cobmined, record, t);
        printf("%d URL: " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\tbytes " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET ", requests " ANSI_COLOR_BLUE "%ld" ANSI_COLOR_RESET "\n", t+1, rec->url, rec->bytes, rec->count);
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
