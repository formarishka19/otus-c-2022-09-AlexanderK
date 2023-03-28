add_library(ws STATIC base64.c base64.h handshake.c sha1.c sha1.h utf8.c utf8.h ws.c ws.h)
target_include_directories(ws INTERFACE .)