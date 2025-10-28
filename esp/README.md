# ESP32s3 part of piano
## Build & Run

### Installing ESP-IDF
See https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/linux-macos-setup.html

### Connecting ESP
Connect ESP32 AI-S3 to your computer.
Depending on your system, determine the port corresponding to device.

### Building project and flashing ESP
```bash
cd ~/piano/esp
```

```bash
idf.py add-dependency "espressif/led_strip^3.0.1~1"
```

```bash
idf.py set-target esp32s3
```

Get port of connected ESP32: ls /dev/cu.* on MacOS
```bash
idf.py -p <your com port> build flash monitor
```
