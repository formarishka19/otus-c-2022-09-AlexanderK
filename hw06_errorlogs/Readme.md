# Библиотека логирования 

## Компиляция и запуск тестового приложения (программа принимает единственный аргумент - наименование файла для сохранения логов)
```sh
mkdir build
cd ./build
cmake ..
make
./bin/app ./log.txt
cat ./log.txt
```
## Использование
```sh
....
#include "alog.h"
....
alog_init("./log.txt"); // обязательная инициализация логгера, аргумент функции - путь до файла логов
....
alog_debug("program started"); // добавление новой записи в лог типа DEBUG
alog_error("fake error occured");
alog_fatal("");
alog_warn("");
alog_notice("");
alog_info("");
....
alog_fin(); // обязательное завершение логгера
....
```