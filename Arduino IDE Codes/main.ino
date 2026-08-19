#include <WiFi.h>
#include <WebSocketsServer.h>

// Wifi Settings

const char* ssid = "Your-SSID";
const char* password = "Your-Password";

//BTS Motor Driver Pins

#define LEFT_RPWM_PIN 16  
#define LEFT_LPWM_PIN 17  
#define RIGHT_RPWM_PIN 27 
#define RIGHT_LPWM_PIN 14 

#define PWM_FREQ       5000  
#define PWM_RESOLUTION 8     

const int L_SPEED  = 230;  
const int R_SPEED = 190;  
const int TRL_SPEED = 165;
const int TLR_SPEED = 200;


// WebSocket & WatchDog

WebSocketsServer webSocket = WebSocketsServer(81);

unsigned long lastSignalTime = 0;
// INCREASED TIMEOUT: Now waits 1 full second before auto-stopping
const unsigned long SIGNAL_TIMEOUT = 1000; 
bool isMoving = false; 
String currentAction = "STOP"; // Track current action for debugging

// Forward Declarations
void stopRobot();
void moveForward();
void moveBackward();
void turnLeft();
void turnRight();


// WebSocket Event Handler 

void webSocketEvent(uint8_t clientNum, WStype_t type, uint8_t* payload, size_t length) {

  switch (type) {
    case WStype_CONNECTED: {
      Serial.println("\n[WEBSOCKET] Python Connected!");
      webSocket.sendTXT(clientNum, "ESP32 Connected");
      break;
    }
    case WStype_DISCONNECTED:
      Serial.println("\n[WEBSOCKET] Python Disconnected! Stopping.");
      stopRobot();
      break;
    case WStype_TEXT: {
      String gesture = String((char*)payload).substring(0, length);
      gesture.trim();

      // Reset the safety timer every time ANY command arrives
      lastSignalTime = millis(); 

      if (gesture.indexOf("FORWARD") != -1) moveForward();
      else if (gesture.indexOf("BACKWARD") != -1) moveBackward();
      else if (gesture.indexOf("LEFT") != -1) turnLeft();
      else if (gesture.indexOf("RIGHT") != -1) turnRight();
      else if (gesture.indexOf("STOP") != -1) stopRobot();
      
      break;
    }
    default:
      break;
  }
}

// Motor Control Functions (WITH DEBUG PRINTS)

void driveLeftMotor(int forwardSpeed, int reverseSpeed) {
  ledcWrite(LEFT_RPWM_PIN, forwardSpeed);
  ledcWrite(LEFT_LPWM_PIN, reverseSpeed);
  
  // Debug print to verify PWM values are actually triggering
  Serial.print("  -> LEFT Motor PWM  | FWD: "); 
  Serial.print(forwardSpeed); 
  Serial.print(" | REV: "); 
  Serial.println(reverseSpeed);
}

void driveRightMotor(int forwardSpeed, int reverseSpeed) {
  ledcWrite(RIGHT_RPWM_PIN, forwardSpeed);
  ledcWrite(RIGHT_LPWM_PIN, reverseSpeed);
  
  // Debug print to verify PWM values are actually triggering
  Serial.print("  -> RIGHT Motor PWM | FWD: "); 
  Serial.print(forwardSpeed); 
  Serial.print(" | REV: "); 
  Serial.println(reverseSpeed);
}

void stopRobot() {
  if (currentAction == "STOP") return; // Prevent spamming
  currentAction = "STOP";
  isMoving = false;
  
  Serial.println("\n=== EXECUTING: STOP ===");
  driveLeftMotor(0, 0);
  driveRightMotor(0, 0);
}

void moveForward() {
  if (currentAction != "FORWARD") {
    currentAction = "FORWARD";
    Serial.println("\n=== EXECUTING: FORWARD ===");
  }
  isMoving = true;
  driveLeftMotor(L_SPEED, 0);
  driveRightMotor(R_SPEED, 0);
}

void moveBackward() {
  if (currentAction != "BACKWARD") {
    currentAction = "BACKWARD";
    Serial.println("\n=== EXECUTING: BACKWARD ===");
  }
  isMoving = true;
  driveLeftMotor(0, L_SPEED);
  driveRightMotor(0, R_SPEED);
}

void turnLeft() {
  if (currentAction != "LEFT") {
    currentAction = "LEFT";
    Serial.println("\n=== EXECUTING: LEFT ===");
  }
  isMoving = true;
  driveLeftMotor(0, 0);
  driveRightMotor(TLR_SPEED, 0);
}

void turnRight() {
  if (currentAction != "RIGHT") {
    currentAction = "RIGHT";
    Serial.println("\n=== EXECUTING: RIGHT ===");
  }
  isMoving = true;
  driveLeftMotor(TRL_SPEED, 0);
  driveRightMotor(0, 0);
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n================================");
  Serial.println("ESP32 BTS DRIVER GESTURE ROBOT");
  Serial.println("================================");

  // Initialize PWM Pins (ESP32 Core 3.x API)
  if(ledcAttach(LEFT_RPWM_PIN, PWM_FREQ, PWM_RESOLUTION)) Serial.println("LEFT_RPWM Attached.");
  if(ledcAttach(LEFT_LPWM_PIN, PWM_FREQ, PWM_RESOLUTION)) Serial.println("LEFT_LPWM Attached.");
  if(ledcAttach(RIGHT_RPWM_PIN, PWM_FREQ, PWM_RESOLUTION)) Serial.println("RIGHT_RPWM Attached.");
  if(ledcAttach(RIGHT_LPWM_PIN, PWM_FREQ, PWM_RESOLUTION)) Serial.println("RIGHT_LPWM Attached.");

  // Connect WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi Connected! IP: ");
  Serial.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();

  // WATCHDOG TIMER
  
  if (isMoving && (millis() - lastSignalTime > SIGNAL_TIMEOUT)) {
    Serial.println("\n[WATCHDOG] Gesture signal timed out (1 second passed). Auto-stopping.");
    stopRobot();
  }
}