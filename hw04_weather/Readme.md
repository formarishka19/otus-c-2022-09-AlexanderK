# Программа прогноза погоды на сегодняшний день в заданной локации

## Подключаемые библиотеки
- libcurl (должна быть установлена на рабочей станции)
- cJSON

## Компиляция и запуск (программа принимает единственный аргумент - наименование локации)
```sh
mkdir build
cd ./build
cmake ..
make
./bin/weather Miami
```