# Программа демон

## Подключаемые библиотеки
- libconfig (static)

## Компиляция
```sh
sh build.sh
```
## Состав файла конфигурации (одна строка)
file  = "/home/otus/hw09_daemon/example.txt";

## Запуск 
# Первый обязательный параметр - абсолютный пусть до файла с конфигурацией.
# Второй опциональный ключ -d для запуска в режиме демона

```sh
./daemon /home/otus/hw09_daemon/cfg/daemon.cfg
```
или
```sh
sudo ./daemon /home/otus/hw09_daemon/cfg/daemon.cfg -d 
```

# доступ к серверу
в режиме демона
```sh
cd /
sudo nc -U /check_fsize.socket
```
в отладочном запуске из папки с бинарником daemon
```sh
nc -U /check_fsize.socket
```

# Команды
check - возвращает текущий размер отслеживаемого файла по конфигурации в байтах
stop - останавливает сервер, закрывает сокет и процесс

# Логирование с помощью syslog
```sh
sudo tail -100 /var/log/syslog
```

# Проверка процесса демона
```sh
ps -axj | grep daemon
```