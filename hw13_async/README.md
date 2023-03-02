# Асинхронный http сервер по раздаче файлов


## Компиляция и запуск (программа принимает два аргумента - путь до директории с файлами и адрес:порт интерфейса, на котором будет работать сервер)

```
mkdir build
cd build
cmake ..
make
cd bin
sudo ./server <file directory> <interface ip address>:<port>
```
sudo ./server /home/otus/hw13_async/build/bin/temp/ 10.129.0.25:80