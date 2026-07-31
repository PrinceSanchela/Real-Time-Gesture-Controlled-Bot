#include <WiFi.h>
#include <WebSocketsServer.h>

// Wifi Settings

const char* ssid = "Your-SSID";
const char* password = "Your-PassWord";

// BTS Motor Driver Pins

#define LEFT_RPWM_PIN  16  
#define LEFT_LPWM_PIN  17  
#define RIGHT_RPWM_PIN 14 
#define RIGHT_LPWM_PIN 27

#define PWM_FREQ       5000  
#define PWM_RESOLUTION 8     // 0-255

// Calibrated Baseline Speed

int leftMotorSpeed  = 190;
int rightMotorSpeed = 190;

// Left turn individual motor speeds
int turnLeft_LeftMotor   = 0;    // Left wheel during LEFT turn (0 = pivot)
int turnLeft_RightMotor  = 150;  // Right wheel during LEFT turn

// Right turn individual motor speeds
int turnRight_LeftMotor  = 150;  // Left wheel during RIGHT turn
int turnRight_RightMotor = 0;    // Right wheel during RIGHT turn (0 = pivot)

// Tracks active real-time output PWM values to print accordingly
int activeLeftFwdPWM  = 0;
int activeLeftRevPWM  = 0;
int activeRightFwdPWM = 0;
int activeRightRevPWM = 0;

// Websocket & Timers

WebSocketsServer webSocket = WebSocketsServer(81);

unsigned long lastSignalTime = 0;
const unsigned long SIGNAL_TIMEOUT = 1000; 

unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 1000; // Serial monitor update rate

bool isMoving = false; 
String currentAction = "STOP";

// Forward Declarations
void stopRobot();
void moveForward();
void moveBackward();
void turnLeft();
void turnRight();
void refreshCurrentMovement();
void handleSerialCalibration();
void printContinuousStatus();


// WebSocket Event Handler 

void webSocketEvent(uint8_t clientNum, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("\n[WEBSOCKET] Controller Connected!");
      webSocket.sendTXT(clientNum, "ESP32 Connected");
      break;

    case WStype_DISCONNECTED:
      Serial.println("\n[WEBSOCKET] Controller Disconnected! Stopping.");
      stopRobot();
      break;

    case WStype_TEXT: {
      String gesture = String((char*)payload).substring(0, length);
      gesture.trim();

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


// Motor Contoller Functions

void driveMotors(int leftFwd, int leftRev, int rightFwd, int rightRev) {
  activeLeftFwdPWM  = leftFwd;
  activeLeftRevPWM  = leftRev;
  activeRightFwdPWM = rightFwd;
  activeRightRevPWM = rightRev;

  ledcWrite(LEFT_RPWM_PIN, leftFwd);
  ledcWrite(LEFT_LPWM_PIN, leftRev);
  ledcWrite(RIGHT_RPWM_PIN, rightFwd);
  ledcWrite(RIGHT_LPWM_PIN, rightRev);
}

void stopRobot() {
  if (currentAction == "STOP") return;
  currentAction = "STOP";
  isMoving = false;
  driveMotors(0, 0, 0, 0);
}

void moveForward() {
  currentAction = "FORWARD";
  isMoving = true;
  driveMotors(leftMotorSpeed, 0, rightMotorSpeed, 0);
}

void moveBackward() {
  currentAction = "BACKWARD";
  isMoving = true;
  driveMotors(0, leftMotorSpeed, 0, rightMotorSpeed);
}

void turnLeft() {
  currentAction = "LEFT";
  isMoving = true;
  // Left motor reverses/holds, Right motor drives forward
  driveMotors(0, turnLeft_LeftMotor, turnLeft_RightMotor, 0);
}

void turnRight() {
  currentAction = "RIGHT";
  isMoving = true;
  // Left motor drives forward, Right motor reverses/holds
  driveMotors(turnRight_LeftMotor, 0, 0, turnRight_RightMotor);
}

void refreshCurrentMovement() {
  if (currentAction == "FORWARD") {
    moveForward();
  } else if (currentAction == "BACKWARD") {
    moveBackward();
  } else if (currentAction == "LEFT") {
    turnLeft();
  } else if (currentAction == "RIGHT") {
    turnRight();
  }
}


// Serial Status Monitor

void printContinuousStatus() {
  if (millis() - lastPrintTime >= PRINT_INTERVAL) {
    lastPrintTime = millis();

    Serial.println("----------------------------------------------------------------------------------");
    Serial.print("[ACTION: ");
    Serial.print(currentAction);
    Serial.println("]");

    // 1. Prints actual real-time PWM values feeding the motors right now
    Serial.print("  ACTIVE OUTPUT -> ");
    Serial.print("LEFT MOTOR [FWD: ");
    Serial.print(activeLeftFwdPWM);
    Serial.print(" | REV: ");
    Serial.print(activeLeftRevPWM);
    Serial.print("]  ||  RIGHT MOTOR [FWD: ");
    Serial.print(activeRightFwdPWM);
    Serial.print(" | REV: ");
    Serial.print(activeRightRevPWM);
    Serial.println("]");

    // 2. Prints current calibrated reference table
    Serial.print("  CALIBRATION   -> ");
    Serial.print("Fwd/Bwd Base (L: ");
    Serial.print(leftMotorSpeed);
    Serial.print(", R: ");
    Serial.print(rightMotorSpeed);
    Serial.print(") | Turn-Left Speeds (L: ");
    Serial.print(turnLeft_LeftMotor);
    Serial.print(", R: ");
    Serial.print(turnLeft_RightMotor);
    Serial.print(") | Turn-Right Speeds (L: ");
    Serial.print(turnRight_LeftMotor);
    Serial.print(", R: ");
    Serial.print(turnRight_RightMotor);
    Serial.println(")");
    Serial.println("----------------------------------------------------------------------------------");
  }
}


// Calibration Parse

void applySpeedChange(int &targetSpeed, String valStr, const char* label) {
  int deltaOrAbsolute = valStr.toInt();

  if (valStr.startsWith("+") || valStr.startsWith("-")) {
    targetSpeed += deltaOrAbsolute;
  } else {
    targetSpeed = deltaOrAbsolute;
  }

  targetSpeed = constrain(targetSpeed, 0, 255);

  Serial.println("\n>>> [UPDATED] <<<");
  Serial.print(label);
  Serial.print(" set to: ");
  Serial.println(targetSpeed);

  refreshCurrentMovement();
}

void handleSerialCalibration() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();

    if (input.length() == 0) return;

    // Check Turn Left individual motor adjustments
    if (input.startsWith("TLL")) {
      applySpeedChange(turnLeft_LeftMotor, input.substring(3), "Turn-Left [Left Wheel]");
    } else if (input.startsWith("TLR")) {
      applySpeedChange(turnLeft_RightMotor, input.substring(3), "Turn-Left [Right Wheel]");
    } 
    // Check Turn Right individual motor adjustments
    else if (input.startsWith("TRL")) {
      applySpeedChange(turnRight_LeftMotor, input.substring(3), "Turn-Right [Left Wheel]");
    } else if (input.startsWith("TRR")) {
      applySpeedChange(turnRight_RightMotor, input.substring(3), "Turn-Right [Right Wheel]");
    } 
    // Check Forward / Backward base speeds
    else if (input.startsWith("L")) {
      applySpeedChange(leftMotorSpeed, input.substring(1), "Drive Base [Left Motor]");
    } else if (input.startsWith("R")) {
      applySpeedChange(rightMotorSpeed, input.substring(1), "Drive Base [Right Motor]");
    } else {
      Serial.println("\n[ERROR] Unknown command. Examples: TLL+2, TLR-5, TRL160, TRR0, L+5, R-3");
    }
  }
}



void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================================");
  Serial.println(" ESP32 REAL-TIME DUAL-MOTOR CALIBRATION SYSTEM");
  Serial.println("========================================================");
  Serial.println("Input format: [Target][+/-Value or DirectNumber] + Enter");
  Serial.println("  TLL -> Turn-Left  : Left motor  (e.g., TLL+2, TLL-5, TLL0)");
  Serial.println("  TLR -> Turn-Left  : Right motor (e.g., TLR+4, TLR160)");
  Serial.println("  TRL -> Turn-Right : Left motor  (e.g., TRL+5, TRL155)");
  Serial.println("  TRR -> Turn-Right : Right motor (e.g., TRR0, TRR-4)");
  Serial.println("  L   -> Straight   : Left motor  (e.g., L+2, L-5, L180)");
  Serial.println("  R   -> Straight   : Right motor (e.g., R+3, R-2, R185)");
  Serial.println("========================================================\n");

  ledcAttach(LEFT_RPWM_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(LEFT_LPWM_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(RIGHT_RPWM_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(RIGHT_LPWM_PIN, PWM_FREQ, PWM_RESOLUTION);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
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
  handleSerialCalibration();
  printContinuousStatus();

  // Watchdog Timer
  if (isMoving && (millis() - lastSignalTime > SIGNAL_TIMEOUT)) {
    Serial.println("\n[WATCHDOG] Signal timed out. Auto-stopping.");
    stopRobot();
  }
}