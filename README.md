# 🤖 Aryabhata - Interactive Robot with AI Vision & Servo Control

A cutting-edge humanoid robot controlled by **ESP32-S3** microcontroller with dual TFT eye displays, AI-powered hand gesture recognition, and real-time interaction through a Python Flask web interface.

![Robot Demo](images/robot_1.jpeg)

---

## 📹 Video Demonstration

**Watch the robot in action:**  
📺 [Robot Demo Video](images/robot_video.mp4)

---

## ✨ Features

### 🎭 **Expressive Eye System**
- **Dual 160×128 TFT LCD Displays** for real-time eye animations
- **8 Emotion States**: Neutral, Angry, Sad, Loving, Sleeping, Wrong, Correct, and interactive responses
- **Dynamic Effects**: Blinking, tears, pupil tracking, and pulsing animations

### 🦾 **Servo Control**
- **20kg DS3218 Digital Servos** for smooth, powerful movements
- **Dual Servo Control** (Servo A & B) for expressive gestures
- **PCA9685 PWM Driver** for precise servo positioning
- Handshake, wave, and custom motion sequences

### 🎮 **Interactive Games & Recognition**
1. **Air Gesture Game** - Draw shapes in the air (Line, Arc, Circle, Zigzag)
2. **Face Matching Game** - Recognize and match faces with Hindu mythology characters
3. **Rock-Paper-Scissors** - Classic hand gesture game with AI opponent

### 🌐 **Web & WiFi Integration**
- **WiFi AP Mode** - Robot creates its own WiFi hotspot
- **WebSocket Communication** - Real-time interaction between PC and robot
- **Flask Web Server** - Beautiful web UI for control and streaming
- **Live Video Streaming** - Real-time camera feed with gesture analysis

### 🧠 **AI & Computer Vision**
- **MediaPipe Hand Tracking** - Accurate hand detection and landmark tracking
- **Face Detection** - Real-time face recognition and matching
- **Gesture Recognition** - Interpret hand movements as commands

---

## 🔧 Hardware Requirements

| Component | Specifications |
|-----------|-----------------|
| **Microcontroller** | ESP32-S3 (240MHz, 8MB PSRAM) |
| **Servos** | 2× 20kg DS3218 Digital Servos |
| **Eye Displays** | 2× 160×128 ST7735 TFT LCD |
| **Servo Driver** | PCA9685 PWM Servo Driver |
| **LED Indicator** | WS2812B NeoPixel (1× LED) |
| **Camera** | USB Webcam (OpenCV compatible) |
| **Power Supply** | 5V USB Power Bank (for ESP32), 6-8V Battery (for Servos) |

### 📌 Pin Configuration (ESP32-S3)

**I2C Bus:**
- SDA: GPIO 1
- SCL: GPIO 2

**TFT Display 1 (SPI):**
- SCLK: GPIO 8, MOSI: GPIO 7, RST: GPIO 5, DC: GPIO 6, CS: GPIO 4

**TFT Display 2 (SPI):**
- SCLK: GPIO 13, MOSI: GPIO 12, RST: GPIO 11, DC: GPIO 9, CS: GPIO 10

**Other:**
- NeoPixel: GPIO 48
- Servo Driver I2C Address: 0x40

---

## 📦 Software Requirements

### Arduino/ESP32 Libraries
```
- Adafruit_PWMServoDriver
- Adafruit_NeoPixel
- Adafruit_GFX
- Adafruit_ST7735
- WebSocketsServer
- WiFi (ESP32)
```

### Python Requirements
```
Flask
OpenCV (cv2)
MediaPipe
PySerial
NumPy
```

Install Python dependencies:
```bash
pip install -r requirements.txt
```

---

## 🚀 Quick Start

### 1. **Upload Arduino Firmware**
```bash
1. Open eye.ino in Arduino IDE
2. Select Board: ESP32S3 Dev Module
3. Set Upload Speed: 115200
4. Install required Arduino libraries
5. Click Upload
```

### 2. **Run Python Backend**
```bash
# Update COM port in robot.py (line ~22)
ROBOT_PORT = "COM18"  # Change to your ESP32 port

# Run Flask server
python robot.py

# The server will start on http://localhost:5000
```

### 3. **Access Web Interface**
- Open browser: `http://localhost:5000`
- Click "Live Feed" to see camera stream
- Select a game and interact with the robot

### 4. **WiFi Connection (Optional)**
- Robot broadcasts WiFi SSID: **"ESP32_S3_ROBOT"**
- Password: **"12345678"**
- Connect for WebSocket communication

---

## 🎮 How to Play

### **Air Gesture Game**
1. Look at the robot's display
2. Select a gesture type (Line, Arc, Circle, Zigzag)
3. Draw the gesture in front of camera
4. Robot recognizes and responds!

### **Face Matching Game**
1. The robot displays a Hindu character/deity name
2. Your face is captured and analyzed
3. Try to match the emotional expression
4. Get feedback on your match accuracy

### **Rock-Paper-Scissors**
1. Play classic RPS against the robot
2. Show your hand gesture
3. Robot plays and scoreboard updates
4. Best of 5 rounds!

---

## 📁 Project Structure

```
Aryabhata/
├── eye.ino                    # ESP32-S3 firmware (688 lines)
├── robot.py                   # Flask backend & AI logic (734 lines)
├── check_mp.py                # MediaPipe diagnostic tool
├── diagnose_camera.py         # Camera diagnostic utility
├── images/
│   ├── robot_1.jpeg          # Robot photos
│   ├── robot_2.jpeg
│   ├── robot_3.jpeg
│   ├── robot_4.jpeg
│   ├── robot_5.jpeg
│   └── robot_video.mp4       # Demo video
├── templates/                 # Flask HTML templates
│   ├── index.html            # Main web interface
│   ├── air.html              # Air gesture game
│   ├── face.html             # Face matching game
│   └── rps.html              # Rock-Paper-Scissors game
└── README.md                  # This file
```

---

## 🔌 Connection Diagram

```
┌─────────────────────────────────────────────┐
│          USB Webcam                          │
└────────────────┬────────────────────────────┘
                 │ USB
                 ▼
        ┌────────────────┐
        │  PC/Laptop     │
        │  (Python)      │
        │  Flask Server  │
        └────────┬───────┘
                 │ Serial (COM18)
                 ▼
        ┌────────────────────┐
        │   ESP32-S3         │
        │   WiFi/Bluetooth   │
        ├────────────────────┤
        │ 2× TFT LCD Displays│  ◉◯ (Expressive Eyes)
        │ 2× DS3218 Servos   │  (Wave, Handshake)
        │ 1× NeoPixel LED    │  (Status Indicator)
        │ PWM Servo Driver   │
        └────────────────────┘
```

---

## 🐛 Troubleshooting

### Camera Not Detected
```bash
python diagnose_camera.py
# This will test all available camera indices and backends
```

### MediaPipe Issues
```bash
python check_mp.py
# Verifies MediaPipe installation and compatibility
```

### Serial Connection Failed
- Check COM port in `robot.py` (line 22)
- Ensure ESP32 drivers are installed
- Verify baud rate is 115200

### Servos Not Responding
- Check I2C connection (SDA/SCL pins)
- Verify PCA9685 address: `0x40`
- Test with Arduino I2C scanner

---

## 📚 References

- **ESP32-S3 Datasheet**: [Espressif Systems](https://www.espressif.com/)
- **DS3218 Servo**: 20kg torque, digital servo with programmable ID
- **MediaPipe Hand Tracking**: [Google MediaPipe](https://mediapipe.dev/)
- **Adafruit PCA9685**: 16-channel PWM driver for servo control

---

## 🎨 Customization

### Change Robot Emotions
Edit `eye.ino` emotion drawing functions:
```cpp
void drawAngryEyes() { /* Custom drawing code */ }
void drawSadEyes()   { /* Custom drawing code */ }
void drawLoving()    { /* Custom drawing code */ }
```

### Add New Games
1. Create HTML template in `templates/`
2. Add Flask route in `robot.py`
3. Implement game logic with WebSocket communication
4. Update navigation links

### Custom Servo Movements
Add sequences to `robot.py`:
```python
def perform_dance():
    """Custom servo dance routine"""
    # Move servo A and B in sequence
    pass
```

---

## 📜 License

This project is provided as-is for educational and experimental purposes.

---

## 👨‍💻 Author

**Aryabhata Robot Project**  
*"Named after the legendary mathematician, combining ancient wisdom with modern AI"*

---

## 🌟 Show Your Support

If you find this project interesting, please ⭐ star this repository!

---

**Last Updated**: February 2026  
**Status**: Active Development
