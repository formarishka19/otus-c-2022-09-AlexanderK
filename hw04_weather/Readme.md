# Программа прогноза погоды на сегодняшний день в заданной локации

## Подключаемые библиотеки
- libcurl
- cJSON

## Компиляция
```sh
gcc -L./lib/ -lcurl -o weather main.c -Wall -Wextra -Wpedantic -std=c11 ./static/libcjson.a
```

## Запуск (программа принимает единственный аргумент - наименование локации)
Пример:
```sh
./weather Miami
```