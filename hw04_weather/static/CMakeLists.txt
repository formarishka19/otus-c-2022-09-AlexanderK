add_library(cjson STATIC cJSON.c cJSON.h)
target_include_directories(cjson INTERFACE .)