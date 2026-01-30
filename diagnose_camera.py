import cv2
import time

def test_camera(index, backend_name, backend_id):
    print(f"Testing Camera Index {index} with Backend {backend_name}...")
    cap = cv2.VideoCapture(index, backend_id)
    
    if not cap.isOpened():
        print(f"  FAILED: Could not open camera {index} with {backend_name}")
        return False
    
    # Try to read a few frames
    success = False
    for i in range(5):
        ret, frame = cap.read()
        if ret:
            print(f"  SUCCESS: Frame read successfully ({frame.shape[1]}x{frame.shape[0]})")
            success = True
            break
        else:
            print(f"  WARNING: Camera opened but returned no frame (attempt {i+1})")
            time.sleep(0.5)
            
    cap.release()
    return success

def main():
    print("=== Camera Diagnostic Tool ===")
    
    indices = [0, 1]
    backends = [
        ("cv2.CAP_DSHOW", cv2.CAP_DSHOW),
        ("cv2.CAP_MSMF", cv2.CAP_MSMF),
        ("cv2.CAP_ANY", cv2.CAP_ANY)
    ]
    
    found_any = False
    
    for index in indices:
        print(f"\n--- Checking Index {index} ---")
        for name, bid in backends:
            if test_camera(index, name, bid):
                found_any = True
                print(f"*** RECOMMENDED CONFIG: Index {index}, Backend {name} ***")
                # We found a working one, but let's continue checking others just in case? 
                # Actually, usually if one works, we are good. But let's be thorough.
    
    if not found_any:
        print("\n!!! NO WORKING CAMERA FOUND !!!")
        print("Please check if:")
        print("1. The camera is plugged in.")
        print("2. Another application (Zoom, Teams, etc.) is using the camera.")
        print("3. Privacy settings allow apps to access the camera.")
    else:
        print("\n=== Diagnosis Complete ===")

if __name__ == "__main__":
    main()
