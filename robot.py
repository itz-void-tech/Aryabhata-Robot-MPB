from flask import Flask, render_template, Response, jsonify

import cv2
import mediapipe as mp
import numpy as np
import time
import random
import math
import threading
from threading import Lock, Thread
import queue

# ======================================================
# APP INIT
# ======================================================
app = Flask(__name__)

# ======================================================
# ROBOT CONTROLLER (SERIAL COMM)
# ======================================================
import serial
import serial.tools.list_ports

# SET YOUR COM PORT HERE (Set to None to auto-detect)
# Example: ROBOT_PORT = "COM3" 
ROBOT_PORT = "COM18"

class RobotController:
    def __init__(self, port=ROBOT_PORT, baud_rate=115200):
        self.ser = None
        self.port = port
        self.baud_rate = baud_rate
        self.lock = Lock()
        self.connect()

    def connect(self):
        """Attempt to connect to the configured port or auto-detect"""
        target_port = self.port

        if target_port is None:
            # Auto-detect logic
            ports = list(serial.tools.list_ports.comports())
            print(f"Found ports: {[p.device for p in ports]}")
            
            # Try to find a likely candidate (ESP32/CP210x/CH340)
            for p in ports:
                if "USB" in p.description or "Serial" in p.description or "CH340" in p.description:
                    target_port = p.device
                    break
            
            if not target_port and ports:
                target_port = ports[0].device # Fallback to first port
            
        if target_port:
            try:
                self.ser = serial.Serial(target_port, self.baud_rate, timeout=1)
                time.sleep(2) # Wait for Arduino reset
                print(f"[ROBOT] Connected on {target_port}")
            except Exception as e:
                print(f"[ROBOT] Connection failed: {e}")
        else:
             print("[ROBOT] No suitable COM port found.")

    def send_command(self, cmd):
        if not self.ser: return
        try:
            with self.lock:
                full_cmd = f"{cmd}\n"
                self.ser.write(full_cmd.encode('utf-8'))
        except Exception as e:
            print(f"[ROBOT] Send error: {e}")
            self.ser = None

    def send_face(self, face_name):
        self.send_command(f"FACE:{face_name}")

# Initialize Global Robot Controller
robot = RobotController()

# ======================================================
# GLOBAL STATE FOR ALL GAMES
# ======================================================
game_locks = {
    'air': Lock(),
    'face': Lock(),
    'rps': Lock()
}

# ======================================================
# AIR DRAWING GAME STATE
# ======================================================
FRAME_W, FRAME_H = 640, 480
PROCESS_W, PROCESS_H = 320, 240

air_points = []
air_drawing_active = False
air_hand_present = False
air_last_hand_present = False
air_final_result = None
air_final_conf = 0.0

EPIC_MAP = {
    "LINE": ("Nandaka", "Vishnu", "The divine sword of preservation"),
    "ARC": ("Gandiva", "Arjuna", "The celestial bow of flawless aim"),
    "CIRCLE": ("Sudarshan Chakra", "Krishna", "The eternal spinning discus"),
    "ZIGZAG": ("Trishul", "Shiva", "Weapon of cosmic balance")
}

# ======================================================
# FACE MATCHER GAME STATE
# ======================================================
face_present = False
face_fps = 0

DIVINE_CHARACTERS = [
    "Shiva", "Vishnu", "Krishna", "Rama", "Ganesha", "Hanuman",
    "Durga", "Kali", "Lakshmi", "Saraswati", "Parvati",
    "Arjuna", "Karna", "Bhishma", "Ravana", "Sita",
    "Narada", "Surya", "Yama", "Indra"
]

DIVINE_TEXT = {
    "Shiva": "Transformation through stillness.",
    "Vishnu": "Balance sustains the cosmos.",
    "Krishna": "Wisdom hides behind playfulness.",
    "Rama": "Dharma is your backbone.",
    "Ganesha": "Obstacles yield to intelligence.",
    "Hanuman": "Strength through devotion.",
    "Durga": "Fearless protector of truth.",
    "Kali": "Liberation through destruction.",
    "Lakshmi": "Abundance flows where gratitude lives.",
    "Saraswati": "Knowledge is the highest power.",
    "Parvati": "Gentleness with inner fire.",
    "Arjuna": "Focus is your greatest weapon.",
    "Karna": "Loyalty beyond circumstance.",
    "Bhishma": "Sacrifice defines destiny.",
    "Ravana": "Power without restraint destroys itself.",
    "Sita": "Unshaken purity and resilience.",
    "Narada": "Truth travels faster than silence.",
    "Surya": "Radiance fuels all action.",
    "Yama": "Discipline defines balance.",
    "Indra": "Leadership is tested by chaos."
}

# ======================================================
# ROCK PAPER SCISSORS GAME STATE
# ======================================================
class GameState:
    IDLE = 0
    COUNTDOWN = 1
    DETECTING = 2
    RESULT = 3

rps_state = GameState.IDLE
rps_countdown_start = 0
rps_player_move = None
rps_computer_move = None
rps_winner = None

# ======================================================
# FRAME BUFFER SYSTEM (Thread-safe camera frame sharing)
# ======================================================
class FrameBuffer:
    def __init__(self):
        self.frame = None
        self.lock = Lock()
    
    def update(self, frame):
        with self.lock:
            self.frame = frame
    
    def get(self):
        with self.lock:
            if self.frame is not None:
                return self.frame.copy()
            return None

# Global frame buffers for each game
air_frame_buffer = FrameBuffer()
face_frame_buffer = FrameBuffer()
rps_frame_buffer = FrameBuffer()

# Background reader flags
stop_air_reader = threading.Event()
stop_face_reader = threading.Event()
stop_rps_reader = threading.Event()

# ======================================================
# MEDIAPIPE SETUP
# ======================================================
MEDIAPIPE_AVAILABLE = False
try:
    # Try importing solutions explicitly for some versions
    import mediapipe.python.solutions as mp_solutions
    mp_hands = mp_solutions.hands
    mp_face = mp_solutions.face_mesh
    mp_draw = mp_solutions.drawing_utils
    MEDIAPIPE_AVAILABLE = True
except (ImportError, AttributeError):
    try:
        # Standard import fallback
        mp_hands = mp.solutions.hands
        mp_face = mp.solutions.face_mesh
        mp_draw = mp.solutions.drawing_utils
        MEDIAPIPE_AVAILABLE = True
    except (ImportError, AttributeError):
        print("!!! WARNING: MediaPipe could not be loaded. AI features will be disabled. !!!")
        MEDIAPIPE_AVAILABLE = False

if MEDIAPIPE_AVAILABLE:
    hand_detector = mp_hands.Hands(
        max_num_hands=1,
        model_complexity=0,
        min_detection_confidence=0.6,
        min_tracking_confidence=0.6
    )

    face_mesh_detector = mp_face.FaceMesh(
        static_image_mode=False,
        max_num_faces=2,
        refine_landmarks=True,
        min_detection_confidence=0.6,
        min_tracking_confidence=0.6
    )
else:
    hand_detector = None
    face_mesh_detector = None

# ======================================================
# CAMERA SETUP - Single shared camera for all games
# ======================================================
shared_camera = None
camera_lock = Lock()

def get_shared_camera():
    """Initialize and return the shared camera instance with robust fallback"""
    global shared_camera
    if shared_camera is None or not shared_camera.isOpened():
        # Try multiple backends and indices
        backends = [
            (cv2.CAP_DSHOW, "DirectShow"),
            (cv2.CAP_MSMF, "Media Foundation"),
            (cv2.CAP_ANY, "Auto")
        ]
        
        for backend, name in backends:
            print(f"Trying camera index 0 with {name}...")
            temp_cam = cv2.VideoCapture(0, backend)
            if temp_cam.isOpened():
                ret, _ = temp_cam.read()
                if ret:
                    shared_camera = temp_cam
                    shared_camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
                    shared_camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
                    shared_camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)
                    shared_camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)
                    shared_camera.set(cv2.CAP_PROP_FPS, 30)
                    print(f"[OK] Shared camera initialized using {name}")
                    return shared_camera
                else:
                    print(f"  Camera opened with {name} but failed to read frame.")
                    temp_cam.release()
            else:
                print(f"  Failed to open camera with {name}.")

        print("!!! ALL CAMERA BACKENDS FAILED !!!")
    return shared_camera

# ======================================================
# BACKGROUND FRAME READERS
# ======================================================
def unified_frame_reader():
    """Single background thread that reads from one camera and updates all buffers"""
    global shared_camera
    shared_camera = get_shared_camera()
    try:
        while not stop_air_reader.is_set():  # Use air_reader as master stop flag
            shared_camera = get_shared_camera() # Ensure camera is available
            
            if shared_camera is None:
                print("Waiting for camera...")
                time.sleep(2)
                continue

            with camera_lock:
                ret, frame = shared_camera.read()
            
            if ret:
                # Update all game buffers with the same frame
                air_frame_buffer.update(frame)
                face_frame_buffer.update(frame)
                rps_frame_buffer.update(frame)
            else:
                print("Failed to read frame from camera. Re-initializing...")
                shared_camera.release()
                shared_camera = None
                time.sleep(1)
    except Exception as e:
        print(f"Error in unified_frame_reader: {e}")
    finally:
        if shared_camera:
            shared_camera.release()
            print("[OK] Camera released")

# Start unified background reader thread
unified_reader_thread = Thread(target=unified_frame_reader, daemon=True)
unified_reader_thread.start()
print("[OK] Unified frame reader thread started")

# ======================================================
# HELPER FUNCTIONS
# ======================================================
def classify_shape(points):
    """
    Improved shape classification using geometric heuristics.
    - LINE: High linearity (start-to-end dist ~ total path len)
    - CIRCLE: Low linearity (closed loop) + High aspect ratio (round)
    - ZIGZAG: Multiple sharp turns
    - ARC: Low linearity (curved) but open (not closed) + Smooth turns
    """
    if len(points) < 30:
        return None, 0.0

    pts = np.array(points, dtype=np.float32)
    
    # 1. Linearity Check
    # Calculate total path length (sum of distances between consecutive points)
    segment_dists = np.linalg.norm(np.diff(pts, axis=0), axis=1)
    path_len = np.sum(segment_dists)
    
    # Distance between start and end point
    start_end_dist = np.linalg.norm(pts[0] - pts[-1])
    
    if path_len < 10: return None, 0.0 # Too small movement
    
    linearity = start_end_dist / path_len  # 1.0 = Straight Line, ~0.0 = Closed Loop
    
    # 2. Aspect Ratio (Bounding Box)
    min_xy = np.min(pts, axis=0)
    max_xy = np.max(pts, axis=0)
    dims = max_xy - min_xy
    width, height = dims[0], dims[1]
    aspect_ratio = min(width, height) / (max(width, height) + 1e-5)
    
    # 3. Angle Analysis (for ZigZag vs Arc)
    # Subsample points to reduce jitter noise for angle calc
    step = max(1, len(points) // 15)
    sampled_pts = pts[::step]
    
    sharp_turns = 0
    total_angle_change = 0
    
    if len(sampled_pts) >= 3:
        vecs = np.diff(sampled_pts, axis=0)
        # Normalize vectors
        norms = np.linalg.norm(vecs, axis=1, keepdims=True)
        vecs = vecs / (norms + 1e-6)
        
        for i in range(len(vecs) - 1):
            # Dot product to find angle
            dot = np.clip(np.dot(vecs[i], vecs[i+1]), -1.0, 1.0)
            angle = math.acos(dot) # Result in radians (0 to 3.14)
            total_angle_change += angle
            
            # If angle > 60 degrees (approx 1.0 rad), it's a sharp turn
            if angle > 1.0: 
                sharp_turns += 1

    # --- DECISION TREE ---
    
    # A. CIRCLE CHECK
    # Closed loop (low linearity) and relatively square bounding box
    if linearity < 0.25:
        if aspect_ratio > 0.5:
            return "CIRCLE", 0.95
        # If it's a closed loop but very flat, could be a flat ellipsis or failed circle
        # defaulting to circle for tolerance
        return "CIRCLE", 0.85

    # B. LINE CHECK
    # Very straight path
    if linearity > 0.90:
        return "LINE", 0.92

    # C. ZIGZAG vs ARC CHECK
    # Both are "Open" curves (Linearity between 0.25 and 0.90)
    
    # Zigzag has multiple sharp directional changes or high total rotation
    # (e.g. Up-Down-Up = ~180 + ~180 degrees change)
    if sharp_turns >= 1 or total_angle_change > 2.5: 
        return "ZIGZAG", 0.88
        
    # Arc is a smooth curve (low sharp turns, moderate total angle change)
    return "ARC", 0.85

def index_only_up(lm):
    tips = [8, 12, 16, 20]
    pips = [6, 10, 14, 18]
    states = [lm[t].y < lm[p].y for t, p in zip(tips, pips)]
    return states[0] and not any(states[1:])

def calculate_distance(p1, p2):
    return math.hypot(p1.x - p2.x, p1.y - p2.y)

def analyze_rps_gesture(landmarks):
    wrist = landmarks[0]
    tips = [8, 12, 16, 20]
    knuckles = [6, 10, 14, 18]
    open_fingers = []
    for i in range(4):
        dist_tip = calculate_distance(landmarks[tips[i]], wrist)
        dist_knuckle = calculate_distance(landmarks[knuckles[i]], wrist)
        open_fingers.append(dist_tip > dist_knuckle)

    thumb_dist = calculate_distance(landmarks[4], landmarks[17])
    thumb_joint_dist = calculate_distance(landmarks[3], landmarks[17])
    thumb_open = thumb_dist > thumb_joint_dist

    if all(open_fingers) and thumb_open:
        return "Paper"
    if open_fingers[0] and open_fingers[1] and not open_fingers[2] and not open_fingers[3]:
        return "Scissors"
    if not any(open_fingers):
        return "Rock"
    return "Unknown"

def resolve_rps_winner(p, c):
    if p == c: return "Tie"
    beats = {"Rock": "Scissors", "Paper": "Rock", "Scissors": "Paper"}
    return "Player" if beats[p] == c else "Computer"

# ======================================================
# MAIN INDEX
# ======================================================
@app.route("/")
def index():
    # Force robot to neutral state when returning to home/menu
    robot.send_face("NEUTRAL") 
    return render_template('index.html')

# ======================================================
# AIR DRAWING ROUTES
# ======================================================
@app.route("/air")
def air_index():
    return render_template('air.html')

def air_gen_frames():
    global air_points, air_drawing_active, air_hand_present, air_last_hand_present, air_final_result, air_final_conf

    while True:
        frame = air_frame_buffer.get()
        if frame is None:
            time.sleep(0.01)
            continue

        frame = cv2.flip(frame, 1)
        small = cv2.resize(frame, (PROCESS_W, PROCESS_H))
        
        if MEDIAPIPE_AVAILABLE:
            rgb = cv2.cvtColor(small, cv2.COLOR_BGR2RGB)
            res = hand_detector.process(rgb)
        else:
            res = type('obj', (object,), {'multi_hand_landmarks': None})
            cv2.putText(frame, "MediaPipe Missing", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        air_last_hand_present = air_hand_present
        air_hand_present = False

        if res.multi_hand_landmarks:
            air_hand_present = True
            lm = res.multi_hand_landmarks[0].landmark

            if index_only_up(lm):
                air_drawing_active = True
                x = int(lm[8].x * FRAME_W)
                y = int(lm[8].y * FRAME_H)
                air_points.append((x, y))
            else:
                air_drawing_active = False

        if air_last_hand_present and not air_hand_present:
            shape, conf = classify_shape(air_points)
            if shape:
                with game_locks['air']:
                    air_final_result = EPIC_MAP[shape]
                    air_final_conf = conf
                
                # TRIGGER ROBOT EMOTION
                robot.send_face("CORRECT") 
            
            else:
                 # TRIGGER WRONG IF DRAWING WAS ATTEMPTED BUT FAILED?
                 # For now, maybe just "WRONG" if points were > 10 but no shape?
                 if len(air_points) > 20: 
                     robot.send_face("WRONG")

            air_points.clear()
            air_drawing_active = False

        for i in range(1, len(air_points)):
            cv2.line(frame, air_points[i - 1], air_points[i], (0, 215, 255), 2)

        _, jpg = cv2.imencode('.jpg', frame)
        yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + jpg.tobytes() + b'\r\n')

@app.route("/air/video")
def air_video():
    return Response(air_gen_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route("/air/result")
def air_result():
    global air_final_result, air_final_conf
    with game_locks['air']:
        if not air_final_result:
            return jsonify({})
        return jsonify({
            "name": air_final_result[0],
            "deity": air_final_result[1],
            "lore": air_final_result[2],
            "conf": round(air_final_conf, 2)
        })

# ======================================================
# FACE MATCHER ROUTES
# ======================================================
@app.route("/face")
def face_index():
    return render_template('face.html')

def face_gen_frames():
    global face_present, face_fps
    prev = time.time()

    while True:
        frame = face_frame_buffer.get()
        if frame is None:
            time.sleep(0.01)
            continue

        h, w, _ = frame.shape
        if MEDIAPIPE_AVAILABLE:
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            res = face_mesh_detector.process(rgb)
        else:
             res = type('obj', (object,), {'multi_face_landmarks': None})
             cv2.putText(frame, "MediaPipe Missing", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        face_present = False

        if res.multi_face_landmarks:
            face_present = True
            face = res.multi_face_landmarks[0]

            for lm in face.landmark:
                cv2.circle(
                    frame,
                    (int(lm.x * w), int(lm.y * h)),
                    1,
                    (0, 215, 255),
                    -1
                )
        
        # ROBOT FACE LOGIC
        # We need a static variable to track state changes to avoid spamming serial
        if not hasattr(face_gen_frames, "last_state"):
             face_gen_frames.last_state = None
        
        current_state = "PRESENT" if face_present else "ABSENT"
        if current_state != face_gen_frames.last_state:
             if face_present:
                 robot.send_face("NEUTRAL") # Waking up
             else:
                 robot.send_face("SLEEPING") # Going to sleep
             face_gen_frames.last_state = current_state

        now = time.time()
        face_fps = int(1 / (now - prev)) if now != prev else face_fps
        prev = now

        cv2.putText(
            frame, f"FPS: {face_fps}",
            (20, 40),
            cv2.FONT_HERSHEY_SIMPLEX,
            1, (255, 215, 0), 2
        )

        _, buffer = cv2.imencode(".jpg", frame)
        yield (
            b"--frame\r\n"
            b"Content-Type: image/jpeg\r\n\r\n" +
            buffer.tobytes() +
            b"\r\n"
        )

@app.route("/face/video")
def face_video():
    return Response(face_gen_frames(), mimetype="multipart/x-mixed-replace; boundary=frame")

@app.route("/face/find")
def face_find():
    if not face_present:
        return jsonify({
            "name": "No Face Detected",
            "confidence": "—",
            "text": "Look into the divine mirror to reveal your form."
        })

    god = random.choice(DIVINE_CHARACTERS)
    return jsonify({
        "name": god,
        "confidence": f"{random.randint(75, 99)}%",
        "text": DIVINE_TEXT.get(god, "")
    })

# ======================================================
# ROCK PAPER SCISSORS ROUTES
# ======================================================
@app.route("/rps")
def rps_index():
    return render_template('rps.html')


def rps_gen_frames():
    global rps_state, rps_countdown_start, rps_player_move, rps_computer_move, rps_winner

    while True:
        frame = rps_frame_buffer.get()
        if frame is None:
            time.sleep(0.01)
            continue

        frame = cv2.flip(frame, 1)
        h, w, _ = frame.shape
        
        if MEDIAPIPE_AVAILABLE:
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            results = hand_detector.process(rgb)
        else:
            results = type('obj', (object,), {'multi_hand_landmarks': None})
            cv2.putText(frame, "MediaPipe Missing", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        detected_label = "Scanning..."

        if results.multi_hand_landmarks:
            for hand_lms in results.multi_hand_landmarks:
                detected_label = analyze_rps_gesture(hand_lms.landmark)
                coords_x = [int(lm.x * w) for lm in hand_lms.landmark]
                coords_y = [int(lm.y * h) for lm in hand_lms.landmark]
                x_min, x_max = max(0, min(coords_x) - 30), min(w, max(coords_x) + 30)
                y_min, y_max = max(0, min(coords_y) - 30), min(h, max(coords_y) + 30)

                accent_color = (248, 189, 56)
                cv2.rectangle(frame, (x_min, y_min), (x_max, y_max), accent_color, 2)
                cv2.rectangle(frame, (x_min, y_min - 35), (x_min + 110, y_min), accent_color, -1)
                cv2.putText(frame, detected_label, (x_min + 5, y_min - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 2)

                mp_draw.draw_landmarks(frame, hand_lms, mp_hands.HAND_CONNECTIONS,
                                            mp_draw.DrawingSpec(color=(255, 255, 255), thickness=1),
                                            mp_draw.DrawingSpec(color=accent_color, thickness=2))

                with game_locks['rps']:
                    if rps_state == GameState.DETECTING:
                        if detected_label in ["Rock", "Paper", "Scissors"]:
                            rps_player_move = detected_label
                            rps_computer_move = random.choice(["Rock", "Paper", "Scissors"])
                            rps_winner = resolve_rps_winner(rps_player_move, rps_computer_move)
                            rps_state = GameState.RESULT
                            
                            # ROBOT EMOTION
                            if rps_winner == "Computer":
                                robot.send_face("LOVING") # Gloat
                            elif rps_winner == "Player":
                                robot.send_face("SAD") # Sore loser
                            else:
                                robot.send_face("NEUTRAL")

        with game_locks['rps']:
            if rps_state == GameState.COUNTDOWN:
                elapsed = time.time() - rps_countdown_start
                cd_val = 3 - int(elapsed)
                if cd_val <= 0:
                    rps_state = GameState.DETECTING
                else:
                    cv2.putText(frame, str(cd_val), (int(w / 2) - 40, int(h / 2) + 40),
                                cv2.FONT_HERSHEY_DUPLEX, 4, (56, 189, 248), 8)

        _, buffer = cv2.imencode('.jpg', frame)
        yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')

@app.route('/rps/video_feed')
def rps_video_feed():
    return Response(rps_gen_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/rps/start', methods=['POST'])
def rps_start_game():
    global rps_state, rps_countdown_start
    with game_locks['rps']:
        rps_state = GameState.COUNTDOWN
        rps_countdown_start = time.time()
    return jsonify(success=True)

@app.route('/rps/reset', methods=['POST'])
def rps_reset_game():
    global rps_state, rps_player_move, rps_computer_move, rps_winner
    with game_locks['rps']:
        rps_state = GameState.IDLE
        rps_player_move = rps_computer_move = rps_winner = None
        
        # Reset robot face
        robot.send_face("NEUTRAL")

    return jsonify(success=True)

@app.route('/rps/status')
def rps_status():
    with game_locks['rps']:
        return jsonify({
            "state": rps_state,
            "player": rps_player_move or "—",
            "computer": rps_computer_move or "—",
            "winner": rps_winner
        })

# ======================================================
# RUN
# ======================================================
if __name__ == "__main__":
    # The '0.0.0.0' tells the computer to listen to your tablet
    # The '0.0.0.0' tells the computer to listen to your tablet
    # use_reloader=False is CRITICAL for camera apps on Windows to prevent double-execution/locking
    app.run(host='0.0.0.0', port=5000, debug=True, use_reloader=False)
