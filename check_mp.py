import mediapipe
import sys

print(f"Python: {sys.executable}")
print(f"MediaPipe Version: {getattr(mediapipe, '__version__', 'unknown')}")
print(f"MediaPipe File: {mediapipe.__file__}")
print("dir(mediapipe):", dir(mediapipe))

try:
    print("mp.solutions:", mediapipe.solutions)
except AttributeError as e:
    print("Error accessing mediapipe.solutions:", e)
