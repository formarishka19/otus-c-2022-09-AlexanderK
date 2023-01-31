# Фикс на утечку памяти

## Применение
В папки clib:
```sh
cd ./src/common
patch < fix.diff 
```
Пропатчится файл clib-package.c

Текло 1615 байт, стало течь 1536)

//powered by Valgrind =)
