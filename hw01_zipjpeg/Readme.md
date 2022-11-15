Программа определения наличия архива в файле

Компиляция
gcc main.c -Wall -Wextra -Wpedantic -std=c11 -o ./main.out

Запуск (программа принимает единственный аргумент - путь до проверяемого файла)
Примеры:
./main.out ./files/zipjpeg.jpg
./main.out ./files/non-zipjpeg.jpg