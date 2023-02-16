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
    // logfile files[MAX_LOG_FILES];
    int count;
} logdir;

typedef struct {
    pthread_t id;
    logfile file;
    GHashTable* hash_table_url;
    GHashTable* hash_table_ref;
    char temp_buffer[MAX_LOGENTRY_LEN];
    record* url_array;
    record* ref_array;
    int url_count;
    int ref_count;
    int status;
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

void parse_log_record(char* data, GHashTable* ht_url, int* total_urls, GHashTable* ht_ref, int* total_refs, record* url_arr, record* ref_arr, unsigned long* total_bytes) {
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
            strncpy(row_parsed.url, part_string, strlen(part_string)+1); //почему-то после удаления этой строки все валится(((
            gchar** url_split = g_strsplit((const gchar *)part_string, " ", -1);
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
                char temp[60];
                strncpy(temp, part_string + 5, strlen(part_string) - 5);
                strncpy(row_parsed.stats, temp, strlen(temp) + 1);
                sscanf(row_parsed.stats, "%lu", &row_parsed.bytes); 
            }  
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
    if (g_hash_table_contains(ht_url, row_parsed.url)) {
        cur_record = (record*)g_hash_table_lookup(ht_url, row_parsed.url);
        cur_record->count++;
        cur_record->bytes = row_parsed.bytes + cur_record->bytes;
        *total_bytes = *total_bytes + row_parsed.bytes;
    }
    else {
        int cur_counter = *total_urls;
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
        int cur_counter_ref = *total_refs;
        *total_refs = *total_refs + 1;
        g_hash_table_insert(ht_ref, (gpointer)(row_parsed.ref), (gpointer)&ref_arr[cur_counter_ref]);
        strncpy(ref_arr[cur_counter_ref].url, row_parsed.ref, MAX_URL_LEN);
        ref_arr[cur_counter_ref].count = 1;
    }
    
}

void* thread_func(void* arg) {
    worker* a = arg;
    size_t i = 0;
    size_t k = 0;
    FILE *fp = fopen(a->file.filepath, "rb");
    uint8_t* start_address = mmap(NULL, a->file.fsize, PROT_READ, MAP_PRIVATE, fp->_file, 0);
    fclose(fp);
    printf("%s file parsing started\n", a->file.filepath);
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
    // printf(ANSI_COLOR_BOLD "Total bytes " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET ANSI_COLOR_BOLD ", total requests " ANSI_COLOR_RED "%d" ANSI_COLOR_RESET "\n" ANSI_COLOR_RESET, a->bytes, a->url_count);
    // qsort(a->url_array, a->url_count, sizeof(record), compare_url);
    // for (int t=0; t<10; t++) {
    //     printf("%d URL: " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\tbytes " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET ", requests " ANSI_COLOR_BLUE "%ld" ANSI_COLOR_RESET "\n", t+1, a->url_array[t].url, a->url_array[t].bytes, a->url_array[t].count);
    // }
    // puts("\n");
    // qsort(a->ref_array, a->ref_count, sizeof(record), compare_ref);
    // puts("TOP REFERERS");
    // for (int t=0; t<10; t++) {
    //     printf("%d REF: " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET ", count " ANSI_COLOR_RED "%ld" ANSI_COLOR_RESET "\n", t+1, a->ref_array[t].url, a->ref_array[t].count);
    // }
    // puts("\n");
    // g_hash_table_destroy(a->hash_table_url);
    // g_hash_table_destroy(a->hash_table_ref);
    // free(a->url_array);
    // free(a->ref_array);
    a->status = 0;
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {

    checkArgs(&argc, argv);
    // char CUR_DIR[200] = "/Users/aleksandrk/dev/c/otus-c-2022-09-AlexanderK/hw11_threads/logs/logs/";
    int t_count;
    // int f_count;
    sscanf(argv[2], "%d", &t_count);
    (void)argc;

    // logfile* files = malloc(MAX_LOG_FILES * sizeof(logfile));
    logfile files[MAX_LOG_FILES];
    logdir log_files;
    log_files.count = 0;
    log_files.list = NULL;
    strncpy(log_files.dir_path, argv[1], MAX_PATH_LEN);


    get_files(&log_files, files);

    // f_count = log_files.count;
    // int count = 0;
    worker workers[t_count];
    for (int w = 0; w < t_count; w++) {
        workers[w].status = 0;
    }
    GSList* iterator = log_files.list;

    GHashTable* hash_table_res = g_hash_table_new(g_str_hash, NULL);
    GHashTable* hash_table_ref_res = g_hash_table_new(g_str_hash, NULL);
    record* res_url_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
    record* res_ref_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
    record* cur_record;
    unsigned long total_urls = 0;
    unsigned long total_refs = 0;
    unsigned long total_bytes = 0;
    unsigned long res_url_count = 0;
    unsigned long res_ref_count = 0;

    clock_t t;
    t = clock();
    int shifts = log_files.count / t_count;
    int w_count = t_count;
    int w_count_rest = 0;
    printf("Количество смен %d\n", shifts);
    if (shifts == 0) {
        w_count = log_files.count;
    }
    else {
        w_count_rest = log_files.count - (t_count * shifts);
    }
    printf("Количество воркеров %d\n", w_count);
    printf("Количество воркеров на остаток %d\n", w_count_rest);

    if (shifts == 0) {
        int w = 0;
        while (iterator) {
            // if (workers[w].status == 0) {
            workers[w].status = 1;
            workers[w].hash_table_url = g_hash_table_new(g_str_hash, NULL);
            workers[w].hash_table_ref = g_hash_table_new(g_str_hash, NULL);
            workers[w].file = *(logfile*)(iterator->data);
            workers[w].url_count = 0;
            workers[w].ref_count = 0;
            workers[w].bytes = 0;
            memset(workers[w].temp_buffer, 0, MAX_LOGENTRY_LEN);
            workers[w].url_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
            workers[w].ref_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
            if (!workers[w].url_array || !workers[w].ref_array ) {
                printf("Memory error while worker %d initialazing, set less threads\n", w);
                return EXIT_FAILURE;
            }
            pthread_create(&workers[w].id, NULL, thread_func, &workers[w]);
            iterator = iterator->next;
            w++;
            // }
            // else {
            //     pthread_join(workers[w].id, NULL);
            // }
        }
        for (int x = 0; x < w_count; x++) {
            if (workers[x].status == 0) {
                pthread_detach(workers[w].id);
            }
            else {
                pthread_join(workers[x].id, NULL);
            }
        }
    }
    else {
        for(int s = 0; s < shifts; s++) {
            for (int w = 0; w < w_count; w++) {
                if (workers[w].status == 0) {
                    workers[w].status = 1;
                    workers[w].hash_table_url = g_hash_table_new(g_str_hash, NULL);
                    workers[w].hash_table_ref = g_hash_table_new(g_str_hash, NULL);
                    workers[w].file = *(logfile*)(iterator->data);
                    workers[w].url_count = 0;
                    workers[w].ref_count = 0;
                    workers[w].bytes = 0;
                    memset(workers[w].temp_buffer, 0, MAX_LOGENTRY_LEN);
                    workers[w].url_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
                    workers[w].ref_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
                    if (!workers[w].url_array || !workers[w].ref_array ) {
                        printf("Memory error while worker %d initialazing, set less threads\n", w);
                        return EXIT_FAILURE;
                    }
                    pthread_create(&workers[w].id, NULL, thread_func, &workers[w]);
                    iterator = iterator->next;
                }
                else {
                    pthread_join(workers[w].id, NULL);
                }
            }
            for (int x = 0; x < w_count; x++) {
                if (workers[x].status == 0) {
                    pthread_detach(workers[x].id);
                }
                else {
                    pthread_join(workers[x].id, NULL);
                }
            }
        }
        if (w_count_rest) {
            for(int d = 0; d < w_count_rest; d++) {
                if (workers[d].status == 0) {
                    workers[d].status = 1;
                    workers[d].hash_table_url = g_hash_table_new(g_str_hash, NULL);
                    workers[d].hash_table_ref = g_hash_table_new(g_str_hash, NULL);
                    workers[d].file = *(logfile*)(iterator->data);
                    workers[d].url_count = 0;
                    workers[d].ref_count = 0;
                    workers[d].bytes = 0;
                    memset(workers[d].temp_buffer, 0, MAX_LOGENTRY_LEN);
                    workers[d].url_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
                    workers[d].ref_array = malloc(MAX_LOG_ENTRIES * sizeof(record));
                    if (!workers[d].url_array || !workers[d].ref_array ) {
                        printf("Memory error while worker %d initialazing, set less threads\n", d);
                        return EXIT_FAILURE;
                    }
                    pthread_create(&workers[d].id, NULL, thread_func, &workers[d]);
                    iterator = iterator->next;
                }
                else {
                    pthread_join(workers[d].id, NULL);
                }
            }
            for (int x = 0; x < w_count_rest; x++) {
                if (workers[x].status == 0) {
                    pthread_detach(workers[x].id);
                }
                else {
                    pthread_join(workers[x].id, NULL);
                }
            }
        }
        
    }

    for (int w = 0; w < w_count; w++) {
        for (int u_counter = 0; u_counter < workers[w].url_count; u_counter++) {
            if (g_hash_table_contains(hash_table_res, workers[w].url_array[u_counter].url)) {
                cur_record = (record*)g_hash_table_lookup(hash_table_res, workers[w].url_array[u_counter].url);
                cur_record->count += workers[w].url_array[u_counter].count;
                cur_record->bytes += workers[w].url_array[u_counter].bytes;
            }
            else {
                int cur_counter = total_urls;
                g_hash_table_insert(hash_table_res, (gpointer)(workers[w].url_array[u_counter].url), (gpointer)&res_url_array[cur_counter]);
                strncpy(res_url_array[cur_counter].url, workers[w].url_array[u_counter].url, MAX_URL_LEN);
                res_url_array[cur_counter].count = workers[w].url_array[u_counter].count;
                res_url_array[cur_counter].bytes = workers[w].url_array[u_counter].bytes;
                total_bytes += workers[w].url_array[u_counter].bytes;
                total_urls += workers[w].url_array[u_counter].count;
                res_url_count++;
            }
        }
        cur_record = NULL;
        for (int r_counter = 0; r_counter < workers[w].ref_count; r_counter++) {
            
            if (g_hash_table_contains(hash_table_ref_res, workers[w].ref_array[r_counter].url)) {
                cur_record = (record*)g_hash_table_lookup(hash_table_ref_res, workers[w].ref_array[r_counter].url);
                cur_record->count += workers[w].ref_array[r_counter].count;
            }
            else {
                // puts(workers[w].ref_array[r_counter].url);
                int cur_counter_ref = total_refs;
                g_hash_table_insert(hash_table_ref_res, (gpointer)(workers[w].ref_array[r_counter].url), (gpointer)&res_ref_array[cur_counter_ref]);
                strncpy(res_ref_array[cur_counter_ref].url, workers[w].ref_array[r_counter].url, MAX_URL_LEN);
                res_ref_array[cur_counter_ref].count = workers[w].ref_array[r_counter].count;
                total_refs += workers[w].ref_array[r_counter].count;
                res_ref_count++;
            }
        }
        g_hash_table_destroy(workers[w].hash_table_url);
        free(workers[w].url_array);
        g_hash_table_destroy(workers[w].hash_table_ref);
        free(workers[w].ref_array);
    }

    printf(ANSI_COLOR_BOLD "Total bytes " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET ANSI_COLOR_BOLD ", total requests " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET "\n" ANSI_COLOR_RESET, total_bytes, total_urls);
    qsort(res_url_array, res_url_count, sizeof(record), compare_url);
    for (int t=0; t<10; t++) {
        printf("%d URL: " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\tbytes " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET ", requests " ANSI_COLOR_BLUE "%ld" ANSI_COLOR_RESET "\n", t+1, res_url_array[t].url, res_url_array[t].bytes, res_url_array[t].count);
    }

    puts("\n");
    puts("TOP REFERERS");
    // for (int y=0; y<20; y++) {
    //     printf("%s \t %lu\n", res_ref_array[y].url, res_ref_array[y].count);
    // }
    qsort(res_ref_array, res_ref_count, sizeof(record), compare_ref);
    for (int t=0; t<10; t++) {
        printf("%d REF: " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET ", count " ANSI_COLOR_RED "%lu" ANSI_COLOR_RESET "\n", t+1, res_ref_array[t].url, res_ref_array[t].count);
    }

    t = clock() - t;
    
    double time_taken = ((double)t)/CLOCKS_PER_SEC;
    printf("workers: %d, execute time: %f seconds\n", t_count, time_taken);
    g_slist_free(iterator);
    g_hash_table_destroy(hash_table_res);
    free(res_url_array);
    free(res_ref_array);
    free(cur_record);
    g_slist_free(log_files.list);
	return EXIT_SUCCESS;
}
