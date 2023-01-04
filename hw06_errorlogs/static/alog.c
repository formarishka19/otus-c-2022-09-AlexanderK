#include "alog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <execinfo.h>

extern FILE* log_file_ptr;

static void make_timestamp(char *buffer){
   time_t rawtime;
   struct tm *timeinfo;
   time(&rawtime);
   timeinfo = localtime(&rawtime);
   strftime(buffer,TIMESTAMP_LEN,TIMESTAMP_FORMAT,timeinfo);
}

void alog_init(char* log_filename) {
    if ((log_file_ptr=fopen(log_filename, "w") ) == NULL) {
        printf("Error opening file for writing logs %s\n", log_filename);
        exit(-2);
    }
    char* ts = malloc(TIMESTAMP_LEN * sizeof(char));
    make_timestamp(ts);
    fputs(ts, log_file_ptr);
    free(ts);
    fputs("Logging started\n", log_file_ptr);
    fputs("-------------------------------\n", log_file_ptr);
    
}

void alog(
	const char *file, size_t filelen,
	const char *func, size_t funclen,
	long line, int level, char* msg) {
        int line_len = snprintf(NULL, 0, "%lu", line);
        char* ts = malloc(TIMESTAMP_LEN * sizeof(char));
        char* line_str = malloc(line_len + 1);
        if (!ts || !line_str) {
            fputs("Log record memory error\n", log_file_ptr);
        }
        make_timestamp(ts);
        fputs(ts, log_file_ptr);
        free(ts);

        switch (level)
        {
        case DEBUG:
            fputs("DEBUG ", log_file_ptr);
            break;
        case INFO:
            fputs("INFO ", log_file_ptr);
            break;
        case NOTICE:
            fputs("NOTICE ", log_file_ptr);
            break;
        case WARN:
            fputs("WARN ", log_file_ptr);
            break;
        case ERROR:
            fputs("ERROR ", log_file_ptr);
            break;
        case FATAL:
            fputs("FATAL ", log_file_ptr);
            break;
        default:
            break;
        }
        
        snprintf(line_str, line_len + 1, "%lu", line);

        fputs(file, log_file_ptr);
        fputs("  func: ", log_file_ptr);
        fputs(func, log_file_ptr);
        fputs(" in line ", log_file_ptr);
        fputs(line_str, log_file_ptr);
        fputs(" -- ", log_file_ptr);
        fputs(msg, log_file_ptr);
        fputs(" -- ", log_file_ptr);
        fputs("\n", log_file_ptr);
        if (level == ERROR) {
            int nptrs;
            void *bt_buffer[BT_BUF_SIZE];
            char **bt_strings;
            nptrs = backtrace(bt_buffer, BT_BUF_SIZE);
            bt_strings = backtrace_symbols(bt_buffer, nptrs);
            if (bt_strings == NULL) {
                fputs("No backtrace for this error\n", log_file_ptr);
            }
            else {
                for (int j = 0; j < nptrs; j++) {
                    fputs(bt_strings[j], log_file_ptr);
                    fputs("\n", log_file_ptr);
                }
            }
            free(bt_strings);
        }
        free(line_str);
    }

void alog_fin(void) {
    fputs("-------------------------------\n", log_file_ptr);
    char* ts = malloc(TIMESTAMP_LEN * sizeof(char));
    make_timestamp(ts);
    fputs(ts, log_file_ptr);
    free(ts);
    fputs("Logging finished\n", log_file_ptr);
    fclose(log_file_ptr);
}