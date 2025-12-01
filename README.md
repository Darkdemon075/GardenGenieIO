# GardenGenieIO

Capstone: automatic garden watering + monitoring project.

## Files
- `ARD_source.ino` — Arduino sketch (ultrasonic + TDS + motor control)
- `ESP_source.ino` — ESP32 sketch (Wi-Fi / web UI integration)

## How to run
### Arduino
1. Open `ARD_source.ino` in Arduino IDE.
2. Select correct Arduino board and COM port.
3. Upload.

### ESP32
1. Open `ESP_source.ino` in Arduino IDE (select ESP32 board).
2. Set Wi-Fi credentials inside the sketch if required.
3. Upload.

## Notes
- Remove secrets (Wi-Fi passwords) before sharing publicly.
- For large binaries (>100 MB) use Git LFS or remove them.

## License
MIT (see `LICENSE`).
