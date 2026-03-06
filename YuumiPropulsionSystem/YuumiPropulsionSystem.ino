/*
  Dual Joystick to L293D Motor Control Test (Tank Drive) - FIXED
  - Added 'constrain()' to prevent motor stall at full stick.
*/

// --- Joystick Pins ---
#define LEFT_JOY_Y_PIN A1   
#define RIGHT_JOY_Y_PIN A2  

// Default joystick center values (0-255 range)
#define JOY_CORRECTION 128

// --- LEFT Motor Pins ---
#define LEFT_ENABLE_PIN 3   // PWM 
#define LEFT_IN1_PIN 2
#define LEFT_IN2_PIN 4

// --- RIGHT Motor Pins ---
#define RIGHT_ENABLE_PIN 5  // PWM 
#define RIGHT_IN3_PIN 7      
#define RIGHT_IN4_PIN 6

void setup() {
  Serial.begin(115200);
  Serial.println("--- ROVER READY ---");

  // Set Left Motor pins
  pinMode(LEFT_ENABLE_PIN, OUTPUT);
  pinMode(LEFT_IN1_PIN, OUTPUT);
  pinMode(LEFT_IN2_PIN, OUTPUT);

  // Set Right Motor pins
  pinMode(RIGHT_ENABLE_PIN, OUTPUT);
  pinMode(RIGHT_IN3_PIN, OUTPUT);
  pinMode(RIGHT_IN4_PIN, OUTPUT);
}

void loop() {
  // ==================================================
  // 1. READ JOYSTICKS (With Anti-Ghosting)
  // ==================================================
  int rawLeft = analogRead(LEFT_JOY_Y_PIN);
  delay(2); // Tiny delay to prevent crosstalk
  int rawRight = analogRead(RIGHT_JOY_Y_PIN);

  // ==================================================
  // 2. CONTROL LEFT MOTOR
  // ==================================================
  // Map 0-1023 input to 0-255 range
  int leftY = map(rawLeft, 0, 1023, 0, 255) - JOY_CORRECTION;
  leftY = leftY * -1; // Flip Y-axis so UP is positive

  int leftSpeed = 0;
  
  // Calculate Speed
  if (abs(leftY) > 20) { // Deadzone check
    if (leftY > 0) {
      leftSpeed = map(leftY, 20, 127, 0, 255);
    } else {
      leftSpeed = map(leftY, -20, -127, 0, -255);
    }
  }

  // --- THE FIX: CONSTRAIN VALUES ---
  // Ensure speed never goes above 255 or below -255
  leftSpeed = constrain(leftSpeed, -255, 255);

  // ==================================================
  // 3. CONTROL RIGHT MOTOR
  // ==================================================
  int rightY = map(rawRight, 0, 1023, 0, 255) - JOY_CORRECTION;
  rightY = rightY * -1; // Flip Y-axis so UP is positive

  int rightSpeed = 0;

  // Calculate Speed
  if (abs(rightY) > 20) { // Deadzone check
    if (rightY > 0) {
      rightSpeed = map(rightY, 20, 127, 0, 255);
    } else {
      rightSpeed = map(rightY, -20, -127, 0, -255);
    }
  }

  // --- THE FIX: CONSTRAIN VALUES ---
  rightSpeed = constrain(rightSpeed, -255, 255);

  // ==================================================
  // 4. SEND COMMANDS
  // ==================================================
  // Serial.print("L: "); Serial.print(leftSpeed);
  // Serial.print(" | R: "); Serial.println(rightSpeed);

  controlLeftMotor(leftSpeed);
  controlRightMotor(rightSpeed);

  delay(50);
}

// --- Helper Functions ---

void controlLeftMotor(int speed) {
  if (speed > 0) {
    digitalWrite(LEFT_IN1_PIN, HIGH);
    digitalWrite(LEFT_IN2_PIN, LOW);
    analogWrite(LEFT_ENABLE_PIN, speed);
  } else if (speed < 0) {
    digitalWrite(LEFT_IN1_PIN, LOW);
    digitalWrite(LEFT_IN2_PIN, HIGH);
    analogWrite(LEFT_ENABLE_PIN, abs(speed));
  } else {
    digitalWrite(LEFT_IN1_PIN, LOW);
    digitalWrite(LEFT_IN2_PIN, LOW);
    analogWrite(LEFT_ENABLE_PIN, 0);
  }
}

void controlRightMotor(int speed) {
  if (speed > 0) {
    digitalWrite(RIGHT_IN3_PIN, HIGH);
    digitalWrite(RIGHT_IN4_PIN, LOW);
    analogWrite(RIGHT_ENABLE_PIN, speed);
  } else if (speed < 0) {
    digitalWrite(RIGHT_IN3_PIN, LOW);
    digitalWrite(RIGHT_IN4_PIN, HIGH);
    analogWrite(RIGHT_ENABLE_PIN, abs(speed));
  } else {
    digitalWrite(RIGHT_IN3_PIN, LOW);
    digitalWrite(RIGHT_IN4_PIN, LOW);
    analogWrite(RIGHT_ENABLE_PIN, 0);
  }
}