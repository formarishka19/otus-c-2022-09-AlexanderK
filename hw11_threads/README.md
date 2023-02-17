# Программа разбора combined логов веб-сервера
формат логов https://httpd.apache.org/docs/2.4/logs.html#combined

## Компиляция и запуск (программа принимает два аргумента - абсолютный путь до директории с логами и количество потоков)
```
mkdir build
cd build
cmake ..
make
cd bin
./parse </logdir/> 10
```

