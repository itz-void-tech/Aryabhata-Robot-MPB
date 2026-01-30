#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ==========================================
//             PIN DEFINITIONS
// ==========================================

#define AP_SSID "ESP32_S3_ROBOT"
#define AP_PASS "12345678"

#define SDA_PIN 1  
#define SCL_PIN 2

#define TFT1_SCLK 8
#define TFT1_MOSI 7
#define TFT1_RST  5
#define TFT1_DC   6
#define TFT1_CS   4

#define TFT2_SCLK 13
#define TFT2_MOSI 12
#define TFT2_RST  11
#define TFT2_DC   9
#define TFT2_CS   10

#define SERVO_A 0
#define SERVO_B 1
#define SERVO_MIN 150
#define SERVO_MAX 600

#define PIXEL_PIN 48
#define PIXEL_COUNT 1

#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 128

// Colors
#define BLACK     0x0000
#define WHITE     0xFFFF
#define RED       0xF800
#define GREEN     0x07E0
#define BLUE      0x001F
#define CYAN      0x07FF 
#define SKIN      0xFDA0 
#define Z_COLOR   0x04FF 

// ==========================================
//           OBJECTS & GLOBALS
// ==========================================

WebServer server(80);
WebSocketsServer webSocket(81);
Adafruit_PWMServoDriver pwm(0x40);
Adafruit_NeoPixel pixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

SPIClass spi1(FSPI);
SPIClass spi2(HSPI);
Adafruit_ST7735 tft1 = Adafruit_ST7735(&spi1, TFT1_CS, TFT1_DC, TFT1_RST);
Adafruit_ST7735 tft2 = Adafruit_ST7735(&spi2, TFT2_CS, TFT2_DC, TFT2_RST);
GFXcanvas16 canvas1(160, 128); 
GFXcanvas16 canvas2(160, 128);

// --- EMOTION STATE ---
// Added WRONG and CORRECT here
enum Emotion { NEUTRAL, ANGRY, SAD, LOVING, SLEEPING, WRONG, CORRECT };
Emotion currentEmotion = NEUTRAL;

// --- ANIMATION VARS ---
unsigned long lastFrameTime = 0;
unsigned long lastBlinkTime = 0;
int blinkState = 0; 
int eyeHeight = 60; 
int maxEyeHeight = 60; 
int tearY_1 = -10; int tearY_2 = -10; 
bool isCrying_1 = false; bool isCrying_2 = false;
float pulseSpeed = 0.005; 
float breathAngle = 0.0;
struct ZParticle { float x, y, speedY; int size; bool active; };
ZParticle zParticles[3]; 

// NEW VARS FOR WRONG/CORRECT
int crossSize = 10;
bool crossGrowing = true;

// --- ROBOT MOVEMENT STATE ---
float currentAngleA = 0;
float currentAngleB = 108; 
int limitMinA = 0, limitMaxA = 180;
int limitMinB = 0, limitMaxB = 180;

int handshakePhase = 0; 
float targetHandshakeAngle = 0;
unsigned long lastHandshakeMove = 0;
unsigned long handshakeHoldStartTime = 0;

bool sweepMode = false;
bool sweepDir = true; 
unsigned long lastSweepMove = 0;

uint8_t fadeVal = 0; int fadeDir = 1; unsigned long lastPixel = 0;

// ==========================================
//           WEB PAGE HTML
// ==========================================

const char webpage[] PROGMEM = R"HTML(
<!DOCTYPE html><html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Robot Control Center</title>
<style>
body{background:#111;color:#fff;font-family:sans-serif;text-align:center;padding:10px;}
h2{border-bottom:2px solid #333;padding-bottom:10px;}
.section{background:#222;border-radius:10px;padding:15px;margin-bottom:20px;}
input[type=range]{width:100%; margin: 10px 0;}
input[type=number]{width:60px; padding:5px; border-radius:4px; border:none; text-align:center; font-weight:bold;}
.config-row{display:flex; justify-content:space-between; align-items:center; margin-bottom:5px; font-size: 14px; color: #aaa;}
.control-group{display:flex; gap:5px; margin-top:10px; align-items:center;}
button{padding:15px;border:none;border-radius:6px;font-size:16px;color:#fff;cursor:pointer;width:100%;}
button.active{background:#00aa44 !important;} 
.btn-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;}
.b-blue{background:#0066ff;}
.b-neut{background:#4CAF50;}
.b-ang{background:#f44336;}
.b-sad{background:#2196F3;}
.b-lov{background:#E91E63;}
.b-sleep{background:#9C27B0;}
/* New Button Styles */
.b-wrong{background:#d32f2f;}
.b-corr{background:#388e3c;}
</style>
</head>
<body>

<div class="section">
  <h2>Arm Control</h2>
  <p>Servo A (Current: <span id="av">0</span>°)</p>
  <div class="config-row">
     <span>Min: <input type="number" id="minA" value="0" onchange="updateLims()"></span>
     <span>Max: <input type="number" id="maxA" value="180" onchange="updateLims()"></span>
  </div>
  <input type="range" min="0" max="180" id="a">

  <p style="margin-top:20px;">Servo B (Current: <span id="bv">0</span>°)</p>
  <div class="config-row">
     <span>Min: <input type="number" id="minB" value="0" onchange="updateLims()"></span>
     <span>Max: <input type="number" id="maxB" value="180" onchange="updateLims()"></span>
  </div>
  <input type="range" min="0" max="180" id="b">

  <div style="margin-top:20px;">
    <div class="control-group">
      <input type="number" id="hsAngle" placeholder="Angle" value="90">
      <button class="b-blue" id="handshake" onclick="triggerHandshake()">Slow Handshake</button>
    </div>
    <div class="control-group">
      <button class="b-blue" id="sweep" onclick="send('SWEEP')">Start 10s Sweep</button>
    </div>
  </div>
</div>

<div class="section">
  <h2>Face Expressions</h2>
  <div class="btn-grid">
    <button class="b-neut" onclick="setFace('neutral')">Neutral</button>
    <button class="b-ang" onclick="setFace('angry')">Angry</button>
    <button class="b-sad" onclick="setFace('sad')">Sad</button>
    <button class="b-lov" onclick="setFace('loving')">Loving</button>
    <button class="b-wrong" onclick="setFace('wrong')">Wrong</button>
    <button class="b-corr" onclick="setFace('correct')">Correct</button>
  </div>
  <button class="b-sleep" style="width:100%;margin-top:10px;" onclick="setFace('sleeping')">Sleeping</button>
</div>

<script>
let ws = new WebSocket(`ws://${location.hostname}:81`);
function send(m){ ws.send(m); }

function updateLims() {
  let minA = document.getElementById('minA').value;
  let maxA = document.getElementById('maxA').value;
  let minB = document.getElementById('minB').value;
  let maxB = document.getElementById('maxB').value;
  document.getElementById('a').min = minA; document.getElementById('a').max = maxA;
  document.getElementById('b').min = minB; document.getElementById('b').max = maxB;
  send(`CFG:${minA}:${maxA}:${minB}:${maxB}`);
}

function triggerHandshake() {
  document.getElementById("handshake").classList.add("active");
  document.getElementById("handshake").innerText = "Shaking (5s)...";
  let angle = document.getElementById('hsAngle').value;
  send("HANDSHAKE:" + angle);
}

let a = document.getElementById('a');
let b = document.getElementById('b');
a.oninput = e => { document.getElementById('av').innerText = e.target.value; send("A:"+e.target.value); };
b.oninput = e => { document.getElementById('bv').innerText = e.target.value; send("B:"+e.target.value); };

function setFace(mode){ fetch('/'+mode); }

ws.onmessage = e => {
  if(e.data.startsWith("MODE:")){
     let m = e.data.split(":")[1];
     if(m==="SWEEP") {
       document.getElementById("sweep").classList.add("active");
     } else if(m==="HANDSHAKE") {
       document.getElementById("handshake").classList.add("active");
     } else if(m==="IDLE") {
       document.getElementById("sweep").classList.remove("active");
       let btnHS = document.getElementById("handshake");
       btnHS.classList.remove("active");
       btnHS.innerText = "Slow Handshake";
     }
  }
};
</script></body></html>
)HTML";

// ==========================================
//           LOGIC HELPERS
// ==========================================

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void resetAnimation() {
  blinkState = 0; eyeHeight = maxEyeHeight; lastBlinkTime = millis();
  tearY_1 = -10; tearY_2 = -10; isCrying_1 = false; isCrying_2 = false;
  
  // Reset Wrong/Correct vars
  crossSize = 10; crossGrowing = true;
  
  for(int i=0; i<3; i++) zParticles[i].active = false;
  canvas1.fillScreen(BLACK); canvas2.fillScreen(BLACK); tft1.fillScreen(BLACK); tft2.fillScreen(BLACK);
}

void setServoRaw(uint8_t ch, int angle){
  angle = constrain(angle, 0, 180);
  if (ch == SERVO_B) {
    pwm.setPWM(ch, 0, map(angle, 0, 180, SERVO_MAX, SERVO_MIN));
  } else {
    pwm.setPWM(ch, 0, map(angle, 0, 180, SERVO_MIN, SERVO_MAX));
  }
}

void broadcastMode(const char* mode){
  webSocket.broadcastTXT(String("MODE:") + mode);
}

// ==========================================
//         ROBOT MOTION LOGIC
// ==========================================

void handleHandshake() {
  if (handshakePhase == 0) return;
  unsigned long now = millis();
  if (handshakePhase == 1) {
    if (now - lastHandshakeMove > 30) {
      lastHandshakeMove = now;
      if (abs(currentAngleA - targetHandshakeAngle) < 1.0) {
        currentAngleA = targetHandshakeAngle;
        setServoRaw(SERVO_A, (int)currentAngleA);
        handshakePhase = 2; handshakeHoldStartTime = now;
      } else {
        if (currentAngleA < targetHandshakeAngle) currentAngleA += 1.0;
        else currentAngleA -= 1.0;
        setServoRaw(SERVO_A, (int)currentAngleA);
      }
    }
  } else if (handshakePhase == 2) {
    if (now - handshakeHoldStartTime > 5000) handshakePhase = 3;
  } else if (handshakePhase == 3) {
    float returnAngle = (float)limitMinA;
    if (now - lastHandshakeMove > 30) {
      lastHandshakeMove = now;
      if (abs(currentAngleA - returnAngle) < 1.0) {
        currentAngleA = returnAngle;
        setServoRaw(SERVO_A, (int)currentAngleA);
        handshakePhase = 0; 
        currentEmotion = NEUTRAL;
        resetAnimation();
        broadcastMode("IDLE");
      } else {
        if (currentAngleA < returnAngle) currentAngleA += 1.0;
        else currentAngleA -= 1.0;
        setServoRaw(SERVO_A, (int)currentAngleA);
      }
    }
  }
}

void handleSweep() {
  if (!sweepMode) return;
  int rangeA = abs(limitMaxA - limitMinA);
  int stepDelay = 10000 / (rangeA > 0 ? rangeA : 180); 
  
  if (millis() - lastSweepMove > stepDelay) {
    lastSweepMove = millis();
    if (sweepDir) { 
      currentAngleA += 1; currentAngleB -= 1; 
      if (currentAngleA >= limitMaxA || currentAngleB <= limitMinB) sweepDir = false; 
    } else { 
      currentAngleA -= 1; currentAngleB += 1;
      if (currentAngleA <= limitMinA || currentAngleB >= limitMaxB) sweepDir = true; 
    }
    currentAngleA = constrain(currentAngleA, limitMinA, limitMaxA);
    currentAngleB = constrain(currentAngleB, limitMinB, limitMaxB); 
    setServoRaw(SERVO_A, (int)currentAngleA);
    setServoRaw(SERVO_B, (int)currentAngleB);
  }
}

// ==========================================
//         DRAWING FUNCTIONS
// ==========================================

void fillHeart(GFXcanvas16 &cvs, int cx, int cy, float s, uint16_t color) {
  int r = 20 * s; int offset_x = 18 * s; int offset_y = 10 * s; 
  int triangle_h = 55 * s; int tri_w = 38 * s; 
  cvs.fillCircle(cx - offset_x, cy - offset_y, r, color);
  cvs.fillCircle(cx + offset_x, cy - offset_y, r, color);
  cvs.fillTriangle(cx - tri_w, cy - 5 * s, cx + tri_w, cy - 5 * s, cx, cy + triangle_h, color);
}

void drawNeutralEye(GFXcanvas16 &canvas, int currentRadY) {
  canvas.fillScreen(BLACK);
  int cx = 80; int cy = 64; int radX = 55;
  if (currentRadY < 2) currentRadY = 2;
  canvas.fillEllipse(cx, cy, radX, currentRadY, WHITE);
  if (currentRadY > 15) {
    canvas.fillCircle(cx, cy, 24, GREEN); 
    canvas.fillCircle(cx, cy, 12, BLACK);
    canvas.fillCircle(cx - 8, cy - 8, 5, WHITE);
  }
  if (maxEyeHeight - currentRadY > 0) {
     canvas.fillRect(0, cy + currentRadY, 160, 64, BLACK);
     canvas.fillRect(0, 0, 160, cy - currentRadY, BLACK);
  }
}

void drawAngryEye(GFXcanvas16 &canvas, bool isLeftEye, int currentRadY) {
  canvas.fillScreen(BLACK);
  int cx = 80; int cy = 64; int radX = 55;
  if (currentRadY < 2) currentRadY = 2;
  canvas.fillEllipse(cx, cy, radX, currentRadY, WHITE);
  if (currentRadY > 15) {
    canvas.fillCircle(cx, cy, 24, RED);
    canvas.fillCircle(cx, cy, 12, BLACK);
    canvas.fillCircle(cx - 8, cy - 8, 5, WHITE);
  }
  int browY_Outer = 20; int browY_Inner = 65; 
  if (isLeftEye) { 
    canvas.fillTriangle(0, 0, 160, 0, 0, browY_Outer, BLACK);
    canvas.fillTriangle(160, 0, 160, browY_Inner, 0, browY_Outer, BLACK);
    canvas.drawLine(0, browY_Outer, 160, browY_Inner, SKIN);
    canvas.drawLine(0, browY_Outer+1, 160, browY_Inner+1, SKIN);
  } else { 
    canvas.fillTriangle(0, 0, 160, 0, 160, browY_Outer, BLACK);
    canvas.fillTriangle(0, 0, 0, browY_Inner, 160, browY_Outer, BLACK);
    canvas.drawLine(160, browY_Outer, 0, browY_Inner, SKIN);
    canvas.drawLine(160, browY_Outer+1, 0, browY_Inner+1, SKIN);
  }
  if (maxEyeHeight - currentRadY > 0) canvas.fillRect(0, cy + currentRadY, 160, 64, BLACK);
}

void drawSadEye(GFXcanvas16 &canvas, bool isLeftEye, int currentRadY, int tearY) {
  canvas.fillScreen(BLACK); 
  int cx = 80; int cy = 64; int radX = 55;
  if (currentRadY < 2) currentRadY = 2;
  canvas.fillEllipse(cx, cy, radX, currentRadY, WHITE);
  if (currentRadY > 15) {
    int pupilOffset = 5; 
    canvas.fillCircle(cx, cy + pupilOffset, 24, BLUE);
    canvas.fillCircle(cx, cy + pupilOffset, 12, BLACK);
    canvas.fillCircle(cx - 8, cy + pupilOffset - 8, 5, WHITE);
  }
  if (tearY > 0) { canvas.fillCircle(cx, tearY, 6, CYAN); canvas.fillTriangle(cx-6, tearY, cx+6, tearY, cx, tearY-10, CYAN); }
  int browY_Outer = 65; int browY_Inner = 15; 
  if (isLeftEye) { 
    canvas.fillTriangle(0, 0, 160, 0, 0, browY_Outer, BLACK);
    canvas.fillTriangle(160, 0, 160, browY_Inner, 0, browY_Outer, BLACK);
    canvas.drawLine(0, browY_Outer, 160, browY_Inner, SKIN);
    canvas.drawLine(0, browY_Outer+1, 160, browY_Inner+1, SKIN);
  } else { 
    canvas.fillTriangle(0, 0, 160, 0, 160, browY_Outer, BLACK);
    canvas.fillTriangle(0, 0, 0, browY_Inner, 160, browY_Outer, BLACK);
    canvas.drawLine(0, browY_Inner, 160, browY_Outer, SKIN);
    canvas.drawLine(0, browY_Inner+1, 160, browY_Outer+1, SKIN);
  }
  if (maxEyeHeight - currentRadY > 0) canvas.fillRect(0, cy + currentRadY, 160, 64, BLACK);
}

void drawLovingEye(GFXcanvas16 &canvas, float scale) {
  canvas.fillScreen(BLACK);
  int cx = 80; int cy = 64; int radX = 55; int radY = 60;
  canvas.fillEllipse(cx, cy, radX, radY, WHITE);
  float heartSize = scale * 0.6; 
  fillHeart(canvas, cx, cy + 5, heartSize, RED); 
  canvas.fillCircle(cx - 10, cy - 8, 4, WHITE);
}

void drawSleepingEye(GFXcanvas16 &canvas, bool isRightEye, float breathOffset) {
  canvas.fillScreen(BLACK); 
  int cx = 80; int cy = 64; int lidRadius = 50; 
  int cutOffset = 15 - (breathOffset * 3); 
  canvas.fillCircle(cx, cy + 10, lidRadius, SKIN);
  canvas.fillCircle(cx, cy + 10 - cutOffset, lidRadius, BLACK);
  if (isRightEye) {
    for (int i=0; i<3; i++) {
      if (zParticles[i].active) {
        canvas.setCursor((int)zParticles[i].x, (int)zParticles[i].y);
        canvas.setTextColor(Z_COLOR);
        canvas.setTextSize(zParticles[i].size);
        canvas.print("Z");
      }
    }
  }
}

// --- NEW DRAWING FUNCTIONS ---

void drawWrongEye(GFXcanvas16 &canvas, int size) {
  canvas.fillScreen(BLACK);
  int cx = 80; int cy = 64; 
  int thick = 6; 
  // Draw Thick Cross (Red)
  for (int offset = -thick/2; offset <= thick/2; offset++) {
    // Top-Left to Bottom-Right
    canvas.drawLine(cx - size, cy - size + offset, cx + size, cy + size + offset, RED);
    // Top-Right to Bottom-Left
    canvas.drawLine(cx + size, cy - size + offset, cx - size, cy + size + offset, RED);
  }
}

void drawCorrectEye(GFXcanvas16 &canvas) {
  canvas.fillScreen(BLACK);
  int cx = 80; int cy = 64;
  int r = 35;
  
  // Draw Green Circle
  canvas.drawCircle(cx, cy, r, GREEN);
  canvas.drawCircle(cx, cy, r-1, GREEN); // Double thickness
  
  // Draw Checkmark
  // Short stroke
  for (int i = 0; i <= 3; i++) {
      canvas.drawLine(cx - 18 + i, cy + i, cx - 3, cy + 15, GREEN);
  }
  // Long stroke
  for (int i = 0; i <= 3; i++) {
      canvas.drawLine(cx - 3, cy + 15, cx + 27 - i, cy - 15 + i, GREEN);
  }
}


// ==========================================
//           NETWORK HANDLERS
// ==========================================

void setMode(Emotion e) {
  if (currentEmotion != e) {
    currentEmotion = e;
    resetAnimation();
  }
  server.send(200, "text/plain", "OK");
}

void processCommand(String msg) {
  msg.trim(); // Remove whitespace
  
  if(msg.startsWith("A:")) {
    sweepMode = false; handshakePhase = 0; 
    currentAngleA = msg.substring(2).toFloat();
    setServoRaw(SERVO_A, (int)currentAngleA);
  }
  else if(msg.startsWith("B:")) {
    sweepMode = false; handshakePhase = 0; 
    currentAngleB = msg.substring(2).toFloat();
    setServoRaw(SERVO_B, (int)currentAngleB);
  }
  else if(msg.startsWith("CFG:")) {
    int first = msg.indexOf(':');
    int second = msg.indexOf(':', first+1);
    int third = msg.indexOf(':', second+1);
    int fourth = msg.indexOf(':', third+1);
    
    limitMinA = msg.substring(first+1, second).toInt();
    limitMaxA = msg.substring(second+1, third).toInt();
    limitMinB = msg.substring(third+1, fourth).toInt();
    limitMaxB = msg.substring(fourth+1).toInt();
  }
  else if(msg.startsWith("HANDSHAKE:")) {
    sweepMode = false;
    handshakePhase = 1; 
    targetHandshakeAngle = msg.substring(10).toFloat();
    broadcastMode("HANDSHAKE");
    currentEmotion = LOVING;
    resetAnimation();
  }
  else if(msg=="SWEEP"){ 
    sweepMode = true; handshakePhase = 0;
    broadcastMode("SWEEP"); 
  }
  else if(msg.startsWith("FACE:")) {
    String face = msg.substring(5);
    if(face == "NEUTRAL") setMode(NEUTRAL);
    else if(face == "ANGRY") setMode(ANGRY);
    else if(face == "SAD") setMode(SAD);
    else if(face == "HAPPY" || face == "LOVING") setMode(LOVING);
    else if(face == "SLEEPING") setMode(SLEEPING);
    else if(face == "WRONG") setMode(WRONG);
    else if(face == "CORRECT") setMode(CORRECT);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  if(type != WStype_TEXT) return;
  String msg = (char*)payload;
  processCommand(msg);
}

// ==========================================
//           SETUP & LOOP
// ==========================================

void setup(){
  Serial.begin(115200);

  // Init Hardware
  pixel.begin(); pixel.setBrightness(40);
  spi1.begin(TFT1_SCLK, -1, TFT1_MOSI, TFT1_CS);
  spi2.begin(TFT2_SCLK, -1, TFT2_MOSI, TFT2_CS);
  tft1.initR(INITR_BLACKTAB); tft2.initR(INITR_BLACKTAB);
  tft1.setRotation(1); tft2.setRotation(1);
  tft1.fillScreen(BLACK); tft2.fillScreen(BLACK);

  // Init Network
  WiFi.softAP(AP_SSID, AP_PASS);
  server.on("/", [](){ server.send(200,"text/html",webpage); });
  // Emotion Endpoints
  server.on("/neutral", [](){ setMode(NEUTRAL); });
  server.on("/angry", [](){ setMode(ANGRY); });
  server.on("/sad", [](){ setMode(SAD); });
  server.on("/loving", [](){ setMode(LOVING); });
  server.on("/sleeping", [](){ setMode(SLEEPING); });
  // New Endpoints
  server.on("/wrong", [](){ setMode(WRONG); });
  server.on("/correct", [](){ setMode(CORRECT); });
  
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Init Servos
  Wire.begin(SDA_PIN, SCL_PIN); 
  pwm.begin(); pwm.setPWMFreq(50);
  
  setServoRaw(SERVO_A, 0);
  setServoRaw(SERVO_B, 180);

  resetAnimation();
}

void loop(){
  server.handleClient();
  webSocket.loop();
  
  // Serial Command Handling
  if (Serial.available() > 0) {
    String msg = Serial.readStringUntil('\n');
    processCommand(msg);
  }
  
  // Logic
  handleHandshake();
  handleSweep();

  // Pixel Status
  if(sweepMode || handshakePhase > 0){ pixel.setPixelColor(0, pixel.Color(0,255,0)); pixel.show(); }
  else { 
    if(millis() - lastPixel > 20){
      fadeVal += fadeDir * 4;
      if(fadeVal >= 250 || fadeVal <= 5) fadeDir *= -1;
      pixel.setPixelColor(0, pixel.Color(0,0,fadeVal)); pixel.show(); lastPixel = millis();
    }
  }

  // Eye Animation
  unsigned long currentMillis = millis();
  bool useCommonBlink = (currentEmotion == NEUTRAL || currentEmotion == ANGRY || currentEmotion == SAD);
  
  if (useCommonBlink) {
      if (blinkState == 0) { 
        eyeHeight = maxEyeHeight;
        int interval = (currentEmotion == SAD) ? 4000 : 3000; 
        if (currentMillis - lastBlinkTime > random(2000, interval)) blinkState = 1; 
      } else if (blinkState == 1) { 
        eyeHeight -= (currentEmotion == SAD) ? 6 : 10; 
        if (eyeHeight <= 0) { eyeHeight = 0; blinkState = 2; }
      } else if (blinkState == 2) { 
        eyeHeight += (currentEmotion == SAD) ? 6 : 10; 
        if (eyeHeight >= maxEyeHeight) { eyeHeight = maxEyeHeight; blinkState = 0; lastBlinkTime = currentMillis; }
      }
  }

  switch (currentEmotion) {
    case NEUTRAL:
      drawNeutralEye(canvas2, eyeHeight); tft2.drawRGBBitmap(0, 0, canvas2.getBuffer(), 160, 128);
      drawNeutralEye(canvas1, eyeHeight); tft1.drawRGBBitmap(0, 0, canvas1.getBuffer(), 160, 128);
      break;
    case ANGRY:
      drawAngryEye(canvas2, true, eyeHeight); tft2.drawRGBBitmap(0, 0, canvas2.getBuffer(), 160, 128);
      drawAngryEye(canvas1, false, eyeHeight); tft1.drawRGBBitmap(0, 0, canvas1.getBuffer(), 160, 128);
      break;
    case SAD:
      if (!isCrying_1 && random(0, 100) < 2) { isCrying_1 = true; tearY_1 = 80; }
      if (isCrying_1) { tearY_1 += 4; if (tearY_1 > 140) { isCrying_1 = false; tearY_1 = -10; } }
      if (!isCrying_2 && random(0, 100) < 2) { isCrying_2 = true; tearY_2 = 80; }
      if (isCrying_2) { tearY_2 += 4; if (tearY_2 > 140) { isCrying_2 = false; tearY_2 = -10; } }
      drawSadEye(canvas2, true, eyeHeight, tearY_2); tft2.drawRGBBitmap(0, 0, canvas2.getBuffer(), 160, 128);
      drawSadEye(canvas1, false, eyeHeight, tearY_1); tft1.drawRGBBitmap(0, 0, canvas1.getBuffer(), 160, 128);
      break;
    case LOVING:
      {
        float wave = sin(currentMillis * pulseSpeed); 
        float currentScale = mapFloat(wave, -1.0, 1.0, 0.8, 1.1);
        drawLovingEye(canvas1, currentScale);
        tft1.drawRGBBitmap(0, 0, canvas1.getBuffer(), 160, 128);
        tft2.drawRGBBitmap(0, 0, canvas1.getBuffer(), 160, 128);
      }
      break;
    case SLEEPING:
      if (currentMillis - lastFrameTime > 20) { 
        lastFrameTime = currentMillis;
        breathAngle += 0.05; float breathVal = sin(breathAngle);
        if (random(0, 100) < 3) { 
           for (int i=0; i<3; i++) { if (!zParticles[i].active) {
               zParticles[i].active = true; zParticles[i].x = 100; zParticles[i].y = 80; 
               zParticles[i].size = random(2, 4); zParticles[i].speedY = random(10, 20) / 10.0; break; 
           }}
        }
        for (int i=0; i<3; i++) { if (zParticles[i].active) {
             zParticles[i].y -= zParticles[i].speedY; zParticles[i].x += 0.5;
             if (zParticles[i].y < 0) zParticles[i].active = false;
        }}
        drawSleepingEye(canvas2, false, breathVal); tft2.drawRGBBitmap(0, 0, canvas2.getBuffer(), 160, 128);
        drawSleepingEye(canvas1, true, breathVal); tft1.drawRGBBitmap(0, 0, canvas1.getBuffer(), 160, 128);
      }
      break;
      
    // --- NEW EMOTION CASES ---
    case WRONG:
      // Pulse animation logic
      if (currentMillis - lastFrameTime > 5) {
        lastFrameTime = currentMillis;
        if (crossGrowing) {
           crossSize++;
           if (crossSize >= 45) crossGrowing = false;
        } else {
           crossSize--;
           if (crossSize <= 10) crossGrowing = true;
        }
        drawWrongEye(canvas1, crossSize);
        tft1.drawRGBBitmap(0, 0, canvas1.getBuffer(), 160, 128);
        tft2.drawRGBBitmap(0, 0, canvas1.getBuffer(), 160, 128);
      }
      break;

    case CORRECT:
      // Draws a static green checkmark
      // (Redrawing every frame to prevent clearing)
      drawCorrectEye(canvas1);
      tft1.drawRGBBitmap(0, 0, canvas1.getBuffer(), 160, 128);
      tft2.drawRGBBitmap(0, 0, canvas1.getBuffer(), 160, 128);
      break;
  }
}