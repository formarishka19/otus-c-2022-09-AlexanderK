# Программа демон

## Подключаемые библиотеки
- libconfig (static)

## Компиляция и запуск (программа принимает единственный аргумент - в режиме демона)
```sh
sh build.sh
sudo ./daemon -d
```

sudo nc -U /check_fsize.socket

команды:
check - проверка размера
stop - стоп программы

sudo tail -100 /var/log/syslog