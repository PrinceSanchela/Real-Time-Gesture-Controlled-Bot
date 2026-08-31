import time
import threading
import queue
import torch
import cv2
import websocket
from ultralytics import YOLO
from ultralytics.utils.plotting import Annotator, colors

# ESP32 WebSocket Settings

ESP32_IP = "10.97.227.48" #ESP32 Current Time Wifi IP
WEBSOCKET_PORT = 81
WEBSOCKET_URL = f"ws://{ESP32_IP}:{WEBSOCKET_PORT}"

# GESTURE -> ROBOT COMMAND MAPPING

GESTURE_COMMANDS = {
    "like":      "RIGHT",
    "dislike":   "LEFT",
    "okay":      "FORWARD",
    "spiderman": "BACKWARD",
    "rock":      "STOP",
}

# GPU / CUDA / DEVICE SETUP

DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

print("================================")
print(f"RUNNING ON DEVICE: {DEVICE.upper()}")
if DEVICE == "cuda":
    print(f"GPU Model: {torch.cuda.get_device_name(0)}")
print("================================")

IMG_SIZE = 320

#Load YOLO Model

print("Loading YOLO model...")
model = YOLO("best.pt")
model.to(DEVICE)
print(f"YOLO model loaded successfully on {DEVICE.upper()}")


# ASYNCHRONOUS WEBSOCKET MANAGER

ws = None
ws_lock = threading.Lock()
command_queue = queue.Queue(maxsize=1)
is_connecting = False
connecting_lock = threading.Lock()

def _connect_worker():
    global ws, is_connecting
    try:
        print(f"Connecting to ESP32: {WEBSOCKET_URL}")
        new_ws = websocket.create_connection(WEBSOCKET_URL, timeout=2)
        with ws_lock:
            ws = new_ws
        print("================================")
        print("CONNECTED TO ESP32 SUCCESSFULLY!")
        print("================================")
    except Exception as e:
        print("================================")
        print("ESP32 CONNECTION FAILED:", e)
        print("================================")
        with ws_lock:
            ws = None
    finally:
        with connecting_lock:
            is_connecting = False

def trigger_connect():
    global is_connecting
    with connecting_lock:
        if is_connecting: 
            return
        is_connecting = True
    thread = threading.Thread(target=_connect_worker, daemon=True)
    thread.start()

# Dedicated thread for non-blocking network socket transmissions
def ws_sender_worker():
    global ws
    while True:
        cmd = command_queue.get()
        if cmd is None:  # Shutdown signal
            break
            
        with ws_lock:
            active_ws = ws

        if active_ws is not None:
            try:
                active_ws.send(cmd)
            except Exception as e:
                print("WebSocket send error:", e)
                with ws_lock:
                    try: 
                        ws.close()
                    except: 
                        pass
                    ws = None
        command_queue.task_done()

sender_thread = threading.Thread(target=ws_sender_worker, daemon=True)
sender_thread.start()

trigger_connect()

#Start Camera

cap = cv2.VideoCapture(0)
if not cap.isOpened():
    print("ERROR: Could not open camera.")
    exit()

print("Starting gesture detection...")
print("Press 'q' to quit.")

last_gesture = None
last_reconnect_attempt = 0
prev_time = time.time()
fps_history = []
FPS_AVG_WINDOW = 20

# Command heartbeat interval (seconds) to ensure ESP32 knows state without overloading network
HEARTBEAT_INTERVAL = 0.2  # 200 ms
last_cmd_send_time = 0

# Main Loop
try:
    while True:
        success, frame = cap.read()
        if not success:
            print("Failed to grab frame.")
            break
        frame = cv2.flip(frame,1)

        # Non-blocking reconnect trigger
        with ws_lock:
            is_connected = (ws is not None)
            
        if not is_connected:
            current_time = time.time()
            if current_time - last_reconnect_attempt > 3.0:
                last_reconnect_attempt = current_time
                print("Trying to reconnect to ESP32...")
                trigger_connect()

        # YOLO Inference
        results = model(frame, conf=0.5, verbose=False, device=DEVICE, imgsz=IMG_SIZE)
        
        annotator = Annotator(frame)
        boxes = results[0].boxes
        current_command = "STOP"  # Default fallback

        # 1. GESTURE DETECTED
        if boxes is not None and len(boxes) > 0:
            confidences = boxes.conf.cpu().numpy()
            class_ids = boxes.cls.cpu().numpy()
            
            best_index = confidences.argmax()
            class_id = int(class_ids[best_index])
            
            gesture = model.names[class_id]
            gesture_key = str(gesture).strip().lower()
            current_command = GESTURE_COMMANDS.get(gesture_key, "STOP")

            # Annotate Bounding Boxes safely
            for box in boxes:
                b_cls = int(box.cls.item() if hasattr(box.cls, 'item') else box.cls[0])
                b_gesture = str(model.names[b_cls]).strip().lower()
                cmd_label = GESTURE_COMMANDS.get(b_gesture, b_gesture.upper())
                conf = float(box.conf.item() if hasattr(box.conf, 'item') else box.conf[0])
                
                label_text = f"{cmd_label} {conf:.2f}"
                annotator.box_label(box.xyxy[0], label_text, color=colors(b_cls, True))

        annotated_frame = annotator.result()

        # FPS Calculation
        current_time = time.time()
        instant_fps = 1 / (current_time - prev_time) if current_time != prev_time else 0
        prev_time = current_time
        fps_history.append(instant_fps)
        if len(fps_history) > FPS_AVG_WINDOW: 
            fps_history.pop(0)
        avg_fps = sum(fps_history) / len(fps_history)

        # Draw Overlay Text
        cv2.putText(annotated_frame, f"FPS: {avg_fps:.1f}", (10, 30), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

        cmd_color = (0, 0, 255) if current_command == "STOP" else (0, 255, 255)
        cv2.putText(annotated_frame, f"COMMAND: {current_command}", (10, 70), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.9, cmd_color, 2)

        # 2. QUEUE COMMAND TO ESP32 (State Change OR Periodic Heartbeat)
        if (current_command != last_gesture) or (current_time - last_cmd_send_time > HEARTBEAT_INTERVAL):
            if current_command != last_gesture:
                print(f"Action Changed -> Queueing: {current_command}")
                last_gesture = current_command
            
            last_cmd_send_time = current_time
            
            # Non-blocking enqueue (replaces oldest if queue full)
            if command_queue.full():
                try: 
                    command_queue.get_nowait()
                except queue.Empty: 
                    pass
            command_queue.put(current_command)

        # Display Frame
        cv2.imshow("Real-Time Gesture Controlled Robot", annotated_frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

finally:
    # Cleanup
    print("Closing program...")
    cap.release()
    cv2.destroyAllWindows()

    if DEVICE == "cuda":
        torch.cuda.empty_cache()

    with ws_lock:
        if ws is not None:
            try:
                ws.send("STOP")
                ws.close()
            except: 
                pass
            
    command_queue.put(None)  # Terminate worker thread
    print("Program stopped.")