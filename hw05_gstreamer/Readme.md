# Сборка и проверка плагина
```sh
mkdir bin
cd ./bin
meson ..
ninja install
gst-launch-1.0 player location=../files/test.wav ! audio/x-raw,format=S16LE,channels=1,rate=48000 ! autoaudiosink
```
