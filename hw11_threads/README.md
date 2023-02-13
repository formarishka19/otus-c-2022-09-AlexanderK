# Программа разбора combined логов веб-сервера
формат https://httpd.apache.org/docs/2.4/logs.html#combined

## Компиляция и запуск (программа принимает единственный аргумент - путь директории с логами)
```
mkdir bin
cd bin
cmake ..
make
cd ..
./parse </.../logdir/>
```

