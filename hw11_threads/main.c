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
#define MAX_LOG_ENTRIES 100000000
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


clock_t start, end;
double cpu_time_used;

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
    record* url_array;
    record* ref_array;
    unsigned long url_count;
    unsigned long ref_count;
    unsigned long bytes;
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

void parse_log_record(char* data, GHashTable* ht_url, unsigned long* total_urls, GHashTable* ht_ref, unsigned long* total_refs, record* url_arr, record* ref_arr, unsigned long* total_bytes) {
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
        cur_record = (record*)g_hash_table_lookup(ht_url, row_parsed.url);
        cur_record->count++;
        cur_record->bytes = row_parsed.bytes + cur_record->bytes;
        *total_bytes = *total_bytes + row_parsed.bytes;
    }
    else {
        unsigned long cur_counter = *total_urls;
        *total_urls = *total_urls + 1;
        g_hash_table_insert(ht_url, (gpointer)(row_parsed.url), (gpointer)&url_arr[cur_counter]);
        strncpy(url_arr[cur_counter].url, row_parsed.url, MAX_URL_LEN);
        url_arr[cur_counter].count = 1;
        url_arr[cur_counter].bytes = row_parsed.bytes;
        *total_bytes = *total_bytes + row_parsed.bytes;
    }
    if (g_hash_table_contains(ht_ref, row_parsed.ref)) {
        cur_record = (record*)g_hash_table_lookup(ht_ref, row_parsed.ref);
        cur_record->count++;
    }
    else {
        unsigned long cur_counter_ref = *total_refs;
        *total_refs = *total_refs + 1;
        g_hash_table_insert(ht_ref, (gpointer)(row_parsed.ref), (gpointer)&ref_arr[cur_counter_ref]);
        strncpy(ref_arr[cur_counter_ref].url, row_parsed.ref, MAX_URL_LEN);
        ref_arr[cur_counter_ref].count = 1;
    }
    
}

int worker_fetch(worker* w, unsigned long *total_req, unsigned long *total_refs, unsigned long *total_bytes, unsigned long *res_url_count, GHashTable* ht_url, GHashTable* ht_ref, record* res_url_array, record* res_ref_array) {

    record* cur_record;
    record* cur_ref_record;
    record ref_element;
    record url_element;
    unsigned long u_counter;
    unsigned long r_counter;
    unsigned long cur_counter_url;
    unsigned long cur_counter_ref;
    char ref[MAX_URL_LEN];
    char url[MAX_URL_LEN];

    for (u_counter = 0; u_counter < w->url_count; u_counter++) {
        url_element = w->url_array[u_counter];
        strncpy(url, url_element.url, MAX_URL_LEN);
        if (g_hash_table_contains(ht_url, url)) {
            cur_record = (record*)g_hash_table_lookup(ht_url, url);
            cur_record->count = cur_record->count + url_element.count;
            cur_record->bytes = cur_record->bytes + url_element.bytes;
        }
        else {
            cur_counter_url = *res_url_count;
            g_hash_table_insert(ht_url, (gpointer)(url), (gpointer)&res_url_array[cur_counter_url]);
            strncpy(res_url_array[cur_counter_url].url, url, MAX_URL_LEN);
            res_url_array[cur_counter_url].count = url_element.count;
            res_url_array[cur_counter_url].bytes = url_element.bytes;
            
            *res_url_count = *res_url_count + 1;
        }
        *total_req = *total_req + url_element.count;
        *total_bytes = *total_bytes + url_element.bytes;
    }
    for (r_counter = 0; r_counter < w->ref_count; r_counter++) {
        ref_element = w->ref_array[r_counter];
        strncpy(ref, ref_element.url, MAX_URL_LEN);
        if (g_hash_table_contains(ht_ref, ref)) {
            cur_ref_record = (record*)g_hash_table_lookup(ht_ref, ref);
            cur_ref_record->count = cur_ref_record->count + ref_element.count;
        }
        else {
            cur_counter_ref = *total_refs;
            g_hash_table_insert(ht_ref, (gpointer)(ref), (gpointer)&res_ref_array[cur_counter_ref]);
            strncpy(res_ref_array[cur_counter_ref].url, ref, MAX_URL_LEN);
            res_ref_array[cur_counter_ref].count = ref_element.count;
            *total_refs = *total_refs + 1;
        }
    }
    return 0;
}

int worker_load(worker* w, logfile* file) {
    w->status = 1;
    w->hash_table_url = g_hash_table_new(g_str_hash, NULL);
    w->hash_table_ref = g_hash_table_new(g_str_hash, NULL);
    w->file = *file;
    w->url_count = 0;
    w->ref_count = 0;
    w->bytes = 0;
    memset(w->temp_buffer, 0, MAX_LOGENTRY_LEN);
    w->url_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
    w->ref_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
    if (!w->url_array || !w->ref_array) {
        puts("Memory error while worker initialazing, set less threads");
        exit(EXIT_FAILURE);
    }
    return(EXIT_SUCCESS);
}

int worker_clear(worker* w) {
    w->status = 0;
    g_hash_table_destroy(w->hash_table_url);
    g_hash_table_destroy(w->hash_table_ref);
    free(w->url_array);
    free(w->ref_array);
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
            parse_log_record(a->temp_buffer, a->hash_table_url, &(a->url_count), a->hash_table_ref, &(a->ref_count), a->url_array, a->ref_array, &(a->bytes));
            memset(a->temp_buffer, 0, MAX_LOGENTRY_LEN);
        }
        i++;
    }
    munmap(start_address, a->file.fsize);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {

    checkArgs(&argc, argv);
    // char CUR_DIR[200] = "/Users/aleksandrk/dev/c/otus-c-2022-09-AlexanderK/hw11_threads/logs/logs/";
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
    record* res_url_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
    record* res_ref_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
    unsigned long total_requests = 0;
    unsigned long total_refs = 0;
    unsigned long total_bytes = 0;
    unsigned long res_url_count = 0;

    clock_t t;
    t = clock();

    int threads_count = 0;
    while (iterator) {
        // int count = 0;
        for (int counter = 0; counter < t_count; counter++) {
            if (workers[counter].status == 0 && iterator) {
                threads_count++;
                // printf("active threads %d\n", threads_count);
                worker_load(&workers[counter], (logfile*)(iterator->data));
                pthread_create(&workers[counter].id, NULL, thread_func, &workers[counter]);
                // printf("started file\t %s\n", workers[counter].file.filepath);
                iterator = iterator->next;
            }
        }
        do {
            threads_count--;
            pthread_join(workers[threads_count].id, NULL);
            puts(workers[threads_count].file.filepath);
            printf("worker bytes %lu\n", workers[threads_count].bytes);
            worker_fetch(&workers[threads_count], &total_requests, &total_refs, &total_bytes, &res_url_count, hash_table_url_res, hash_table_ref_res, res_url_array, res_ref_array);
            worker_clear(&workers[threads_count]);
            // threads_count--;
            // printf("active threads %d\n", threads_count);
            printf("total bytes %lu\n", total_bytes);
        }
        while (threads_count);
    }

    qsort(res_url_array, res_url_count, sizeof(record), compare_url);
    qsort(res_ref_array, total_refs, sizeof(record), compare_ref);

    puts("\n");
    printf(ANSI_COLOR_BOLD "Total bytes " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET ANSI_COLOR_BOLD ", total requests " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET "\n" ANSI_COLOR_RESET, total_bytes, total_requests);
    for (int t = 0; t < 10; t++) {
        printf("%d URL: " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\tbytes " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET ", requests " ANSI_COLOR_BLUE "%ld" ANSI_COLOR_RESET "\n", t+1, res_url_array[t].url, res_url_array[t].bytes, res_url_array[t].count);
    }
    puts("\n");
    puts("TOP REFERERS");
    for (int t = 0; t < 10; t++) {
        printf("%d REF: " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET ", count " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET "\n", t+1, res_ref_array[t].url, res_ref_array[t].count);
    }

    t = clock() - t;
    double time_taken = ((double)t)/CLOCKS_PER_SEC;
    puts("\n");
    printf("workers: %d, execute time: %f seconds\n", t_count, time_taken);
    g_slist_free(iterator);
    g_hash_table_destroy(hash_table_url_res);
    g_hash_table_destroy(hash_table_ref_res);
    free(res_url_array);
    free(res_ref_array);
    g_slist_free(log_files.list);
	return EXIT_SUCCESS;
}
