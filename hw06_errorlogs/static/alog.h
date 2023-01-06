#ifndef _ALOG_H_
#define _ALOG_H_

#include <stdlib.h>


typedef enum {
	DEBUG,
	INFO,
	NOTICE,
	WARN,
	ERROR,
	FATAL
} alog_level;

// proto

void alog_init(char* log_filename);
void alog(
	const char *file, size_t filelen,
	const char *func, size_t funclen,
	long line, int level, char* msg);
void alog_fin(void);

//-------

#define alog_fatal(...) \
	alog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	FATAL, __VA_ARGS__)
#define alog_error(...) \
	alog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERROR, __VA_ARGS__)
#define alog_warn(...) \
	alog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	WARN, __VA_ARGS__)
#define alog_notice(...) \
	alog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	NOTICE, __VA_ARGS__)
#define alog_info(...) \
	alog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	INFO, __VA_ARGS__)
#define alog_debug(...) \
	alog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	DEBUG, __VA_ARGS__)




#endif /*_ALOG_H_*/
