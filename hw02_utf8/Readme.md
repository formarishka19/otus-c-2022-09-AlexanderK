# Программа кодировки файлов в UTF8

## Компиляция
```
gcc main.c -Wall -Wextra -Wpedantic -std=c11 -o ./main.out
```

## Запуск (программа принимает три аргумента - путь до исходного файла, путь до результирующего файла и кодировку исходного файла)
Программа умеет работать с файлами трех кодировок - CP-1251, KOI8-R, ISO-8859-5

Формат запуска:
```
./main.out <input filepath> <output filepath> <koi8|iso-8859-5|cp1251>
```

Примеры запуска:
```
./main.out ./files/iso-8859-5.txt ./output.txt iso-8859-5
./main.out ./files/koi8.txt ./output.txt koi8
./main.out ./files/cp1251.txt ./output.txt cp1251
```
