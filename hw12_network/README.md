# Программа ASCII искусства
Программа преобразует сообщение в красивый вывод на консоли  
Используется ресурс telehack.com  
для запуска из РФ использовать утилиту proxychains4  

## Компиляция
```
gcc main.c -Wall -Wextra -Wpedantic -std=c11
```
## Запуск (программа принимает два аргумента - шрифт и строчку для преобразования (оба аргумента до 50 символов))
```
./a.out verdana "hello world"
./a.out arial hello-world
```

для запуска через прокси
```
proxychains4 ./a.out arial abirvalg
```

## Пример
![output example](./_img.png)