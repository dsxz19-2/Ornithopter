#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

// Wi-Fi Access Point credentials
const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";

// Web server on port 80
WebServer server(80);

// GPIO pins
const int motorPin = 26;         // Motor control pin
const int pitchServoPin = 22;    // Pitch servo pin
const int rollServoPin = 21;      // Roll servo pin

// Servo objects
Servo pitchServo;
Servo rollServo;

// HTML Page (Shortened for readability)
String HTML = R"=====( 
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Ornithopter Remote Control</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin-top: 50px;
        }
        .controller {
            display: flex;
            justify-content: space-between;
            width: 90%;
            margin: 0 auto;
        }
        .joystick-container {
            position: relative;
            width: 200px;
            height: 200px;
            background-color: #ccc;
            border-radius: 50%;
            touch-action: none;
            display: inline-block;
        }
        .stick {
            width: 60px;
            height: 60px;
            background-color: red;
            border-radius: 50%;
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
        }
        .output {
            font-size: 18px;
            margin-top: 20px;
        }
        #speedDisplay {
            font-size: 24px;
        }
        #anglesDisplay {
            font-size: 24px;
            margin-top: 30px;
        }
    </style>
</head>
<body>
    <h1>Ornithopter Remote Control</h1>
    <div class="controller">
        <!-- Speed Joystick on the Left -->
        <div class="joystick-container" id="speedJoystick">
            <div class="stick" id="speedStick"></div>
        </div>
        <!-- Direction Joystick on the Right -->
        <div class="joystick-container" id="directionJoystick">
            <div class="stick" id="directionStick"></div>
        </div>
    </div>
    <!-- Display Outputs -->
    <div id="speedDisplay" class="output">Speed: 0</div>
    <div id="anglesDisplay" class="output">
        Pitch: 0° (Neutral), Roll: 0° (Neutral)
    </div>
    <script>
        const speedJoystick = document.getElementById('speedJoystick');
        const speedStick = document.getElementById('speedStick');
        const directionJoystick = document.getElementById('directionJoystick');
        const directionStick = document.getElementById('directionStick');

        const speedDisplay = document.getElementById('speedDisplay');
        const anglesDisplay = document.getElementById('anglesDisplay');

        let speed = 0;
        let pitchAngle = 0;
        let rollAngle = 0; 

        // Servo angle limits
        const MAX_SERVO_ANGLE = 90;

        // ESP32 IP address (replace with ESP32's IP)
        const ESP32_IP = 'http://192.168.4.1';

        // Handle speed joystick movement (left side)
        speedJoystick.addEventListener('touchstart', handleSpeedJoystickMove, false);
        speedJoystick.addEventListener('touchmove', handleSpeedJoystickMove, false);
        speedJoystick.addEventListener('touchend', resetSpeedJoystick, false);

        function handleSpeedJoystickMove(event) {
            event.preventDefault();
            const touch = event.targetTouches[0];
            const rect = speedJoystick.getBoundingClientRect();
            const centerY = rect.height / 2;
            const offsetY = touch.clientY - rect.top;

            // Calculate joystick position relative to the center for speed control
            const y = offsetY - centerY;
            const newSpeed = Math.max(0, Math.min(100, Math.round((y / centerY) * -100))); // Inverse Y axis for upward movement
            speed = newSpeed;

            speedStick.style.transform = translate(-50%, ${y}px);
            speedDisplay.innerText = Speed: ${speed}%;

            // Send updated data (speed, pitch, roll) to the RC plane (ESP32)
            sendDataToPlane(speed, pitchAngle, rollAngle);
        }

        function resetSpeedJoystick() {
            speedStick.style.transform = 'translate(-50%, -50%)';
            speedDisplay.innerText = Speed: ${speed}%;
        }

        // Handle direction joystick movement (right side)
        directionJoystick.addEventListener('touchstart', handleDirectionJoystickMove, false);
        directionJoystick.addEventListener('touchmove', handleDirectionJoystickMove, false);
        directionJoystick.addEventListener('touchend', resetDirectionJoystick, false);

        function handleDirectionJoystickMove(event) {
            event.preventDefault();
            const touch = event.targetTouches[0];
            const rect = directionJoystick.getBoundingClientRect();
            const centerX = rect.width / 2;
            const centerY = rect.height / 2;
            const offsetX = touch.clientX - rect.left;
            const offsetY = touch.clientY - rect.top;

            // Calculate joystick position relative to the center
            const x = offsetX - centerX;
            const y = offsetY - centerY;

            // Calculate roll and pitch angles based on joystick movement
            rollAngle = Math.round((x / centerX) * MAX_SERVO_ANGLE);    // Left/Right movement (roll)
            pitchAngle = Math.round((y / centerY) * -MAX_SERVO_ANGLE); // Up/Down movement (inverted for pitch)

            // Constrain the angles within ±90 degrees for servos
            rollAngle = Math.max(-MAX_SERVO_ANGLE, Math.min(MAX_SERVO_ANGLE, rollAngle));
            pitchAngle = Math.max(-MAX_SERVO_ANGLE, Math.min(MAX_SERVO_ANGLE, pitchAngle));

            // Move the joystick stick
            const moveX = Math.min(Math.max(x, -centerX), centerX);
            const moveY = Math.min(Math.max(y, -centerY), centerY);
            directionStick.style.transform = translate(${moveX}px, ${moveY}px);

            // Update the display with the current servo angles in degrees
            anglesDisplay.innerText = Pitch: ${pitchAngle}°, Roll: ${rollAngle}°;

            // Send updated data (speed, pitch, roll) to the RC plane (ESP32)
            sendDataToPlane(speed, pitchAngle, rollAngle);
        }

        function resetDirectionJoystick() {
            directionStick.style.transform = 'translate(-50%, -50%)';
            pitchAngle = 0;
            rollAngle = 0;
            anglesDisplay.innerText = Pitch: ${pitchAngle}° (Neutral), Roll: ${rollAngle}° (Neutral);

            // Send updated data (speed, pitch, roll) to the RC plane (ESP32)
            sendDataToPlane(speed, pitchAngle, rollAngle);
        }

        // Send speed, pitch, and roll data to the ESP32 via HTTP in one request
        function sendDataToPlane(speed, pitch, roll) {
            fetch(${ESP32_IP}/control, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    speed: speed,
                    pitch: pitch,
                    roll: roll
                })
            })
            .then(response => response.json())
            .then(data => console.log('Sent data:', data))
            .catch(error => console.error('Error sending data:', error));
        }
    </script>
</body>
</html>

)=====";

void setup() {
    // Initialize serial communication
    Serial.begin(115200);

    // Configure motor pin as output
    pinMode(motorPin, OUTPUT);

    // Attach servos to their respective GPIO pins
    pitchServo.attach(pitchServoPin);
    rollServo.attach(rollServoPin);

    // Set up the ESP32 as an Access Point
    WiFi.softAP(ssid, password);
    Serial.println("Access Point started.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Web server routes
    server.on("/", []() {
        server.send(200, "text/html", HTML);
    });

    server.on("/control", HTTP_POST, []() {
        if (server.hasArg("plain")) {
            String body = server.arg("plain");
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, body);

            // Extract data from the JSON object
            int speed = doc["speed"];
            int pitch = doc["pitch"];
            int roll = doc["roll"];

            // Control motor and servos
            handleControl(speed, pitch, roll);

            // Send success response
            server.send(200, "application/json", "{\"status\":\"success\"}");
            //Serial.printf("Speed: %d%%, Pitch: %d°, Roll: %d°\n", speed, pitch, roll);
        }
    });

    // Start the web server
    server.begin();
    Serial.println("HTTP server started.");
}

// Function to control motor and servos
void handleControl(int speed, int pitch, int roll) {
    // Map pitch and roll from -90° to 90° range to 0° to 180° for servos
    int pitchAngle = pitch + 90;
    int rollAngle = roll + 90;

    // Write angles to servos
    pitchServo.write(pitchAngle);
    rollServo.write(rollAngle);

    // Map speed (0-100%) to PWM values (0-255) and write to motor
    int motorPWM = map(speed, 0, 100, 0, 255);
    analogWrite(motorPin, motorPWM);
}

void loop() {
    // Handle client requests
    server.handleClient();

    // Monitor connected clients
    static int lastClientCount = 0;
    int currentClientCount = WiFi.softAPgetStationNum();

    if (currentClientCount != lastClientCount) {
    //    Serial.printf("Connected clients: %d\n", currentClientCount);
        lastClientCount = currentClientCount;
    }
}




////                                           