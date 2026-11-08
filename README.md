# opencv_opi4_tracker

YOLO-детекция объектов на RTSP-потоке: **считает Orange Pi 4 Pro (headless, серверная ОС), картинку смотрим на ПК**.

```
камера (IP Webcam)  ──RTSP──>  Orange Pi 4 Pro (YOLO + трекинг)  ──MJPEG/HTTP──>  браузер/VLC на ПК
```

## Установка (чистая Ubuntu/Debian на плате)

```bash
git clone https://github.com/integr0k/opencv_opi4_tracker.git
cd opencv_opi4_tracker
./setup.sh --build
```

`setup.sh` ставит системные пакеты (`build-essential`, `cmake`, `ffmpeg`, `libopencv-dev` — без GUI-зависимостей), клонирует и собирает библиотеку трекинга [motrack](https://github.com/clip5/motrack), затем собирает проект. Повторный запуск безопасен.

## Запуск

```bash
# на плате
./main 'rtsp://192.168.1.82:8080/h264_opus.sdp' 8000 yolo11n.onnx

# на ПК — просто открыть в браузере или VLC:
#   http://<ip-платы>:8000
```

Аргументы все необязательные: `./main [rtsp_url] [порт] [модель.onnx]`.
IP-адреса для браузера программа печатает при старте.

Или через make:

```bash
make run URL=rtsp://192.168.1.82:8080/h264_opus.sdp
```

## Как устроен вывод

Без `imshow` и GUI: кадры после детекции кодируются в JPEG и отдаются как
`multipart/x-mixed-replace` MJPEG поверх обычного HTTP (сокеты без
дополнительных зависимостей). Браузер/VLC сами показывают поток. Если
клиентов нет — кадры не кодируются, CPU не тратится. Качество JPEG —
`kJpegQuality` в `main.cpp`.

## Модели

`*.onnx` не хранятся в репозитории (большие). Положите `yolo11n.onnx`
рядом с бинарником. Например, экспорт из ultralytics:

```bash
pip install ultralytics
yolo export model=yolo11n.pt format=onnx
```

## Сборка руками

```bash
make          # бинарник ./main (нужен собранный motrack/build/install)
make clean
```
