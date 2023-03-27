## WEB-консоль для отображения логов сервера в реальном времени

Проект призван помочь системным администратором в эксплуатации серверов и сервисов ИТ-инфраструктуры.  
Порой не всегда удается подключиться к серверу по ssh, чтобы посмотреть те или иные события в журнале логов, поэтому создан веб-сервис, зайдя
на который с любого устройства, можно в реальном времени посмотреть текущий лог интересующего сервиса, поискать в нем по ключевым словам.

## Сборка и запуск

### CMake

```bash
mkdir build && cd build/
cmake ..
make
./logsend/logsend ./test.log -d
```

### Команды (в виде сообщений к серверу)
* **tail** - выводит последние логи и в режиме реального времени присылает новые записи
* **tail:<search_string>** - присылает только те строчки логов, где найдена <search_string>
* **stop** - останавливает мониторинг логов


### Логи сервиса
```bash
sudo journalctl -f | grep LOGSERVER
```

### Генерация тестовых логов
в директории ./logsend/ файлы:  
* **generator.py** - генератор логов  
* **generator.service** - конфиг для запуска генератора логов в режиме демона systemd, требуется прописать нужные параметры

### Сборка фронта
в директории ./frentend/ readme.md

## TODO
* **SSL/TLS Support**
wsServer does not currently support encryption. However, it is possible to use it
in conjunction with [Stunnel](https://www.stunnel.org/), a proxy that adds TLS
support to existing projects. Just follow [these](doc/TLS.md) four easy steps
to get TLS support on wsServer.

* **Token authorization**
* **Additional logfiles to monitor**

## Технологии
* **C** - backend (ws server)
* **ReactJS** - frontend (node.js required)
* **Python** - log generator (optional)

## Thanks
Thanks for tiny [wsServer](https://github.com/Theldus/wsServer) library.