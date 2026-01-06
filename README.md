# Desktoy 🎮

An animated desk companion with expressive anime eyes, running on an ESP32-C3 microcontroller.

![Desktoy Demo](docs/demo.gif) <!-- TODO: Add demo gif -->

## Features

- **14 Emotions**: Normal, Happy, Laughing, Angry, Sad, Surprised, Sleepy, Sleeping, Crazy, Love, Wink, Smug, Scared, and Birthday!
- **Smooth 120 FPS Animations**: Buttery smooth eye movements, blinks, and transitions
- **Expressive Eyes**: Large anime-style eyes with gradient irises, dynamic pupils, and reactive highlights
- **Sound Effects**: Piezo buzzer with portamento effects for each emotion
- **Special Modes**:
  - 💕 Love: Shaded 3D beating hearts
  - 🎂 Birthday: Cake eyes with falling spinning stars, plays Happy Birthday song
  - 😱 Scared: Smooth wiggling face with darting nervous eyes
- **Splash Screen**: Custom "KRG" boot screen

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32-C3 SuperMini |
| Display | GME12864-17 OLED (SSD1306, 128x64, I2C) |
| Sound | Piezo buzzer |

### Wiring

| OLED Pin | ESP32-C3 Pin |
|----------|--------------|
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO8 |
| SDA | GPIO9 |

| Buzzer | ESP32-C3 Pin |
|--------|--------------|
| + | GPIO3 |
| - | GND |

## Building

### Prerequisites

- [ESP-IDF v5.5+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/)

### Build & Flash

```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh  # Linux/macOS
# or
%USERPROFILE%\esp\esp-idf\export.bat  # Windows

# Build
idf.py build

# Flash
idf.py -p PORT flash

# Monitor output
idf.py -p PORT monitor
```

## Project Structure

```
desktoy/
├── main/
│   ├── desktoy_main.c       # Main application, emotions, animation logic
│   ├── ssd1306.c/h          # Custom SSD1306 OLED driver
│   ├── render3d.c/h         # 3D rendering engine (for future features)
│   ├── sprites.c/h          # Sprite-based rendering mode
│   ├── buzzer.c/h           # Sound effects and MIDI playback
│   └── obj_loader.c/h       # OBJ file loader
├── content/                  # Video content scripts
├── CMakeLists.txt
└── README.md
```

## Render Modes

The project supports multiple rendering modes (set `RENDER_MODE` in `desktoy_main.c`):

| Mode | Description |
|------|-------------|
| 0 | Procedural (original) |
| 1 | Sprite-based |
| 2 | **3D Rendering (default)** - Full featured with all emotions |

## Customization

### Adding Emotions

1. Add new emotion to the `emotion_t` enum
2. Add visual parameters in `apply_emotion()`
3. Add mouth drawing in `draw_mouth_2d()`
4. Add sound effect in `buzzer.c`

### Adjusting Animation Speed

- Frame rate: Change `vTaskDelay(pdMS_TO_TICKS(8))` in main loop (8ms = ~120 FPS)
- Eye size: Modify `eye_w` and `eye_h` in `draw_anime_eye_2d()`

## License

MIT License - Feel free to use, modify, and share!

## Contributing

Pull requests welcome! Feel free to:
- Add new emotions
- Improve animations
- Add new sound effects
- Port to other hardware

## Acknowledgments

Built with ❤️ for my kid's desk.

---

*Made with ESP-IDF and lots of late-night debugging sessions.*
