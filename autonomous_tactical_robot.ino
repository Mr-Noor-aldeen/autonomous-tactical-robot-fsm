#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

// --- Network Settings ---
const char* ssid = "NOURALDIN71604";
const char* password = "12345678";

// --- Pins ---
#define TRIG_PIN 12
#define ECHO_PIN 27
#define BUZZER_PIN 25
#define SERVO_PIN 32
#define IN1 5
#define IN2 18
#define IN3 19
#define IN4 21 

Servo myServo;

// --- Servo Control ---
int angleMin = 0;
int angleMax = 180;
int currentAngle = angleMin;
int stepDirection = 1;
int sweepStartAngle = 30; 
int sweepEndAngle = 180; 
int servoStepDelay = 20; 
unsigned long lastServoTime = 0;

// --- Radar and Threshold ---
long duration;
long distance = 0; 
int distanceThreshold = 15; 

// --- Buzzer Control ---
unsigned long lastBuzzerTime = 0;
int buzzerIntervalNormal = 1000;
int buzzerIntervalAlert = 200;
unsigned long buzzerOffTime = 0;
const int buzzerFlashDuration = 50; 

// --- State Flags ---
bool isBuzzerNormalEnabled = true; 
bool isBuzzerAlertEnabled = true; 
bool isServoSweeping = true; 
bool isEmergencyStopEnabled = true; // Disabled only by E-Stop button
bool isTargetTrackingEnabled = false; 
bool isAutonomousDrivingEnabled = false; 

// --- Tracking / Avoidance ---
int lastDetectedAngle = -1; 
bool isTrackingTarget = false; 
const int trackingThreshold = 5; 
bool isAvoiding = false; 
unsigned long avoidanceStartTime = 0; 
const long avoidanceStep1Duration = 500; // Backup duration
const long avoidanceStep2Duration = 700; // Turn duration

// --- Web Server Setup ---
WebServer server(80);

// ===================================
// == 1. MOTOR & CONTROL FUNCTIONS ==
// ===================================

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void forwardMotors() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backwardMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeftMotors() {
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRightMotors() {
  digitalWrite(IN1, HIGH); 
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); 
  digitalWrite(IN4, LOW);
}

// --- Toggle State Functions ---
void toggleBuzzerNormal() { isBuzzerNormalEnabled = !isBuzzerNormalEnabled; if (!isBuzzerNormalEnabled) { digitalWrite(BUZZER_PIN, HIGH); }}
void toggleBuzzerAlert() { isBuzzerAlertEnabled = !isBuzzerAlertEnabled; if (!isBuzzerAlertEnabled) { digitalWrite(BUZZER_PIN, HIGH); }}
void toggleServoSweep() { isServoSweeping = !isServoSweeping; }

void toggleEmergencyStop() { 
  // ONLY function that changes isEmergencyStopEnabled
  isEmergencyStopEnabled = !isEmergencyStopEnabled; 
  if (!isEmergencyStopEnabled) { 
      // Stop all autonomous systems when E-Stop is OFF
      stopMotors();
      isAutonomousDrivingEnabled = false;
      isTargetTrackingEnabled = false;
      isAvoiding = false;
      isServoSweeping = false;
  } else {
      // Re-enable sweep when E-Stop is ON
      isServoSweeping = true;
  }
}

void toggleTargetTracking() {
  isTargetTrackingEnabled = !isTargetTrackingEnabled;
  if (isTargetTrackingEnabled) {
      isAutonomousDrivingEnabled = false; 
      isServoSweeping = true;
      isAvoiding = false;
      isEmergencyStopEnabled = true; 
  } else {
      isTrackingTarget = false;
      stopMotors();
      lastDetectedAngle = -1;
      isAvoiding = false;
      isServoSweeping = true; 
  }
}

void toggleAutonomousDriving() {
  isAutonomousDrivingEnabled = !isAutonomousDrivingEnabled;
  if (isAutonomousDrivingEnabled) {
    isTargetTrackingEnabled = false; 
    isTrackingTarget = false;
    isServoSweeping = true; 
    isAvoiding = false;
    isEmergencyStopEnabled = true; // Ensure E-Stop is ON
    forwardMotors(); // Start moving
  } else {
    stopMotors();
  }
}

void servoManualRight() {
  if (!isServoSweeping) { 
    currentAngle -= 1; 
    if (currentAngle < angleMin) currentAngle = angleMin;
    myServo.write(currentAngle);
  }
}

void servoManualLeft() {
  if (!isServoSweeping) { 
    currentAngle += 1; 
    if (currentAngle > angleMax) currentAngle = angleMax;
    myServo.write(currentAngle);
  }
}

void setSweepSpeed(int speedOption) {
  if (speedOption == 1) { servoStepDelay = 40; } 
  else if (speedOption == 2) { servoStepDelay = 20; } 
  else if (speedOption == 3) { servoStepDelay = 5; }
}

void handleSetSweepRange() {
  if (server.hasArg("start") && server.hasArg("end")) {
    int newStart = server.arg("start").toInt();
    int newEnd = server.arg("end").toInt();
    if (newStart >= 0 && newStart <= 180 && newEnd >= 0 && newEnd <= 180 && newStart != newEnd) {
      sweepStartAngle = newStart;
      sweepEndAngle = newEnd;
      currentAngle = sweepStartAngle; 
      myServo.write(currentAngle);
      server.send(200, "text/plain", "Sweep range set to: " + String(sweepStartAngle) + " to " + String(sweepEndAngle));
      return;
    }
  }
  server.send(400, "text/plain", "Invalid sweep range values.");
}

void handleSetThreshold() {
  if (server.hasArg("value")) {
    String valueStr = server.arg("value");
    int newThreshold = valueStr.toInt();
    if (newThreshold > 0 && newThreshold < 200) { 
      distanceThreshold = newThreshold;
      server.send(200, "text/plain", "Threshold set to: " + String(distanceThreshold) + " cm");
      return;
    }
  }
  server.send(400, "text/plain", "Invalid threshold value.");
}

void handleGetRadarData() {
  String data = String(currentAngle) + "," + String(distance); 
  server.send(200, "text/plain", data);
}

// ===================================
// == 2. HTML CONTENT (WEB PAGE) ==
// ===================================

void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>NOUR ALDIN Radar Robot</title>
    <style>
      body { text-align:center; font-family:Arial; background:#111; color: #0f0; padding-top: 20px;}
      h2, h3 { color:#0f0; } 
      .control-group { max-width: 650px; margin: 15px auto; padding: 15px; background: #222; border: 1px solid #0f0; border-radius: 15px; box-shadow: 0 4px 8px rgba(0,0,0,0.5); }
      .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }
      .grid-3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
      .grid-2 { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }
      .grid-auto { display: grid; grid-template-columns: repeat(auto-fit, minmax(100px, 1fr)); gap: 10px; }
      
      button { font-size:18px; padding:15px 10px; margin:5px 0; border:none; border-radius:10px; background:#007bff; color:white; width: 100%; transition: background 0.3s; user-select: none; -webkit-user-select: none; -webkit-tap-highlight-color: transparent; }
      .motor-controls button { font-size: 22px; padding: 20px; background: #007bff; }
      button.danger { background: #f44336; }
      button.warning { background: #ff9800; }
      button.special { background: #9b59b6; }
      button.success { background: #2ecc71; }
      button:hover { filter: brightness(1.1); cursor:pointer; }
      .threshold-control input, .sweep-range-inputs input { padding: 10px; font-size: 18px; border: 2px solid #ccc; border-radius: 8px; margin-right: 5px; background: #333; color: #fff; display: inline-block; }
      .threshold-control input { width: 60%; }
      .threshold-control button { width: 30%; padding: 10px; font-size: 18px; background: #3498db;}
      .sweep-range-inputs input { width: 45%; margin: 5px 0; }
      #radarScreen { background-color: #000; border: 2px solid #0f0; margin: 20px auto; display: block; width: 500px; height: 250px; }
      .data-display { margin: 10px auto; font-size: 1.2em; color: #0f0; }
      .data-display span { font-weight: bold; color: #ff0; }
    </style>
  </head>
  <body>
    <h2>NOUR ALDIN Radar Robot</h2>
    
    <div class="control-group">
        <h3>Primary Modes & Emergency Stop</h3>
        <div class="grid-3"> 
            <button class="special" onclick="sendCommand('toggleTargetTracking')">fear and avoidance</button> 
            <button class="success" onclick="sendCommand('toggleAutonomousDriving')">Toggle Autonomous Drive</button>
            <button class="danger" onclick="sendCommand('toggleEmergencyStop')">Toggle E-Stop (Radar)</button>
        </div>
        <hr style="border-color:#444;">
        <div class="grid-auto"> 
            <button class="warning" onclick="sendCommand('toggleBuzzerNormal')">Normal Buzzer</button>
            <button class="warning" onclick="sendCommand('toggleBuzzerAlert')">Alert Buzzer</button>
            <button onclick="sendCommand('toggleServoSweep')">Automatic radar movement</button>
        </div>
    </div>
    <div class="control-group">
        <h3>Live Sonar/Radar View</h3>
        <canvas id="radarScreen" width="500" height="250"></canvas>
        <div class="data-display">
            Angle: <span id="currentAngleDisplay">--</span>° | Distance: <span id="currentDistanceDisplay">--</span> cm
        </div>
      <div class="data-display">
    Last Target:&nbsp;<span id="lastTargetAngle">--</span>°&nbsp;@&nbsp;<span id="lastTargetDistance">--</span>&nbsp;cm
</div>

    </div>
    
    <div class="control-group motor-controls">
        <h3>Motor Control</h3>
        <button onmousedown="sendCommand('forward_start')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('forward_start')" ontouchend="sendCommand('stop')">Forward</button>
        <div class="grid">
          <button onmousedown="sendCommand('left_start')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('left_start')" ontouchend="sendCommand('stop')">Turn Left</button>
          <button onmousedown="sendCommand('right_start')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('right_start')" ontouchend="sendCommand('stop')">Turn Right</button>
        </div>
        <button onmousedown="sendCommand('backward_start')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('backward_start')" ontouchend="sendCommand('stop')">Backward</button>
        <button class="danger" onclick="sendCommand('stop')">STOP Motors</button>
    </div>

    <div class="control-group">
        <h3>Radar movement range (Auto-Scan)</h3>
        <p>Current Range: <span id="currentSweepStart">%SWEEP_START%</span>° to <span id="currentSweepEnd">%SWEEP_END%</span>°</p>
        
        <div class="sweep-range-inputs">
            <input type="number" id="startAngleInput" value="%SWEEP_START%" min="0" max="180" placeholder="Start Angle (0-180)">
            <input type="number" id="endAngleInput" value="%SWEEP_END%" min="0" max="180" placeholder="End Angle (0-180)">
        </div>
        <button onclick="setSweepRange()">Set Sweep Range</button>
        <hr style="border-color:#444;">

        <h4>Sweep Speed (Current: <span id="currentSpeed">Medium</span>)</h4>
        <div class="grid">
            <button class="warning" onclick="setSpeed(1)">Slow</button>
            <button onclick="setSpeed(2)">Medium</button>
            <button class="danger" onclick="setSpeed(3)">Fast</button>
        </div>
    </div>

    <div class="control-group">
        <h3>Scanning and detection distance range (cm)</h3>
        <p>Current Threshold: <span id="currentThreshold">%CURRENT_THRESHOLD%</span> cm</p>
        <div class="threshold-control">
            <input type="number" id="thresholdInput" value="%CURRENT_THRESHOLD%" min="1" max="200" placeholder="New Distance (cm)">
            <button onclick="setThreshold()">Set</button>
        </div>
    </div>
    
    <div class="control-group">
        <h3>Manual radar control (automatic scanning must be turned off)</h3>
        <p>Press and hold for smooth movement.</p>
        <div class="grid">
            <button 
                onmousedown="startServoCommand('servo_left')" 
                onmouseup="stopServoCommand()" 
                ontouchstart="startServoCommand('servo_left')" 
                ontouchend="stopServoCommand()">
                Servo LEFT
            </button>
            <button 
                onmousedown="startServoCommand('servo_right')" 
                onmouseup="stopServoCommand()" 
                ontouchstart="startServoCommand('servo_right')" 
                ontouchend="stopServoCommand()">
                Servo RIGHT
            </button>
        </div>
    </div>

    <script>
      let servoInterval = null; 
      let lastDetectedTarget = { angle: '--', distance: '--' }; 

      function sendCommand(cmd) {
        fetch('/' + cmd)
          .then(response => {
            if (response.ok) {
              console.log(cmd + ' sent successfully.');
            }
          });
      }

      function startServoCommand(cmd) {
        if (servoInterval) {
          clearInterval(servoInterval);
        }
        sendCommand(cmd); 
        servoInterval = setInterval(() => { sendCommand(cmd); }, 100); 
      }

      function stopServoCommand() {
        if (servoInterval) {
          clearInterval(servoInterval);
          servoInterval = null;
        }
      }

      function setThreshold() {
        const input = document.getElementById('thresholdInput');
        const value = input.value;
        const thresholdDisplay = document.getElementById('currentThreshold');
        if (value && parseInt(value) > 0 && parseInt(value) < 200) {
          fetch('/setThreshold?value=' + value)
            .then(response => response.text())
            .then(data => {
              console.log(data);
              thresholdDisplay.textContent = value;
              input.value = value;
              alert('Radar Threshold Updated to ' + value + ' cm');
            })
            .catch(error => { console.error('Error setting threshold:', error); alert('Failed to update threshold.'); });
        } else { alert('Please enter a valid value between 1 and 200.'); }
      }

      function setSweepRange() {
        const startInput = document.getElementById('startAngleInput');
        const endInput = document.getElementById('endAngleInput');
        const startValue = parseInt(startInput.value);
        const endValue = parseInt(endInput.value);
        if (startValue >= 0 && startValue <= 180 && endValue >= 0 && endValue <= 180 && startValue !== endValue) {
          fetch('/setSweepRange?start=' + startValue + '&end=' + endValue)
            .then(response => response.text())
            .then(data => {
              console.log(data);
              document.getElementById('currentSweepStart').textContent = startValue;
              document.getElementById('currentSweepEnd').textContent = endValue;
              alert('Sweep Range Updated!');
            })
            .catch(error => { console.error('Error setting sweep range:', error); alert('Failed to update sweep range.'); });
        } else { alert('Invalid range. Values must be between 0 and 180, and Start cannot equal End.'); }
      }

      function setSpeed(speedOption) {
          let endpoint;
          let speedName;
          if (speedOption === 1) { endpoint = 'setSpeedSlow'; speedName = 'Slow'; } 
          else if (speedOption === 2) { endpoint = 'setSpeedMedium'; speedName = 'Medium'; } 
          else if (speedOption === 3) { endpoint = 'setSpeedFast'; speedName = 'Fast'; } 
          else { return; }
          fetch('/' + endpoint).then(response => {
            if (response.ok) { document.getElementById('currentSpeed').textContent = speedName; console.log('Speed set to ' + speedName); }
          });
      }
      
      // --- RADAR JAVASCRIPT LOGIC ---
      const canvas = document.getElementById('radarScreen');
      const ctx = canvas.getContext('2d');
      const centerX = canvas.width / 2;
      const centerY = canvas.height;
      const maxRange = canvas.height * 1.25; 
      const maxPhysicalRange = 250; 
      let lastDistance = 0;
      let lastAngle = 90; 
      let obstaclePoints = []; 

      function drawRadar() {
        ctx.fillStyle = '#000'; ctx.fillRect(0, 0, canvas.width, canvas.height);
        ctx.strokeStyle = '#0f0'; ctx.fillStyle = '#0f0'; ctx.lineWidth = 1;
        const numRings = 5; const distanceStep = maxPhysicalRange / numRings; 
        
        for (let i = 1; i <= numRings; i++) {
          let radius = maxRange * (i / numRings);
          ctx.beginPath(); ctx.arc(centerX, centerY, radius, Math.PI, 0); ctx.stroke();
          ctx.fillText((i * distanceStep) + 'cm', centerX + radius - 30, centerY - 5);
        }
        
        for (let angle = 0; angle <= 180; angle += 30) {
          let rad = angle * (Math.PI / 180);
          ctx.beginPath(); ctx.moveTo(centerX, centerY);
          let x = centerX + maxRange * Math.cos(rad);
          let y = centerY - maxRange * Math.sin(rad);
          ctx.lineTo(x, y); ctx.stroke();
        }

        obstaclePoints = obstaclePoints.filter(p => (Date.now() - p.time) < 5000); 

        obstaclePoints.forEach(p => {
          let distanceRatio = p.distance / maxPhysicalRange; 
          let radius = maxRange * distanceRatio;
          let rad = p.angle * (Math.PI / 180);
          let x = centerX + radius * Math.cos(rad);
          let y = centerY - radius * Math.sin(rad);

          if(p.distance > 0 && distanceRatio <= 1) { 
              ctx.beginPath(); ctx.arc(x, y, 5, 0, 2 * Math.PI); 
              ctx.fillStyle = '#f00'; ctx.fill();
              if (p.angle === lastDetectedTarget.angle && p.distance === lastDetectedTarget.distance) {
                  ctx.fillStyle = '#fff'; ctx.font = '10px Arial'; ctx.fillText(p.distance + 'cm', x + 8, y + 4);
              }
          }
        });

        ctx.strokeStyle = '#fff'; ctx.lineWidth = 2;
        ctx.beginPath(); ctx.moveTo(centerX, centerY);
        let currentRad = lastAngle * (Math.PI / 180);
        let sweepX = centerX + maxRange * Math.cos(currentRad);
        let sweepY = centerY - maxRange * Math.sin(currentRad);
        ctx.lineTo(sweepX, sweepY); ctx.stroke();
      }

      function fetchRadarData() {
        fetch('/getRadarData').then(response => response.text()).then(data => {
            const parts = data.split(',');
            if (parts.length === 2) {
              lastAngle = parseInt(parts[0]);
              lastDistance = parseInt(parts[1]);
              document.getElementById('currentAngleDisplay').textContent = lastAngle;
              document.getElementById('currentDistanceDisplay').textContent = lastDistance;
              
              const currentThreshold = parseInt(document.getElementById('currentThreshold').textContent);
              if (lastDistance > 0 && lastDistance <= maxPhysicalRange) { 
                  if (lastDistance <= currentThreshold) {
                      lastDetectedTarget.angle = lastAngle;
                      lastDetectedTarget.distance = lastDistance;
                      document.getElementById('lastTargetAngle').textContent = lastDetectedTarget.angle;
                      document.getElementById('lastTargetDistance').textContent = lastDetectedTarget.distance;
                  }
                  
                  let exists = obstaclePoints.some(p => p.angle === lastAngle && (Date.now() - p.time) < 100);
                  if (!exists) {
                    obstaclePoints.push({ angle: lastAngle, distance: lastDistance, time: Date.now() });
                  }
              }
              drawRadar(); 
            }
          }).catch(error => { console.error('Error fetching radar data:', error); });
      }
      
      setInterval(fetchRadarData, 50); 

      document.addEventListener('DOMContentLoaded', () => {
          document.getElementById('thresholdInput').value = document.getElementById('currentThreshold').textContent;
          document.getElementById('startAngleInput').value = document.getElementById('currentSweepStart').textContent;
          document.getElementById('endAngleInput').value = document.getElementById('currentSweepEnd').textContent;
          drawRadar(); 
      });
    </script>
  </body>
  </html>
  )rawliteral";
  
  html.replace("%CURRENT_THRESHOLD%", String(distanceThreshold));
  html.replace("%SWEEP_START%", String(sweepStartAngle));
  html.replace("%SWEEP_END%", String(sweepEndAngle));

  server.send(200, "text/html", html);
}


// ===================================
// == 3. SETUP FUNCTION ==
// ===================================

void setup() {
  // Pin Setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); 

  myServo.attach(SERVO_PIN);
  myServo.write(currentAngle);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopMotors();

  // WiFi Setup (Access Point Mode)
  WiFi.softAP(ssid, password);

  // Server Routing
  server.on("/", handleRoot);
  server.on("/stop", [](){ stopMotors(); server.send(200,"text/plain","OK"); });
  server.on("/forward_start", [](){ forwardMotors(); server.send(200,"text/plain","OK"); });
  server.on("/backward_start", [](){ backwardMotors(); server.send(200,"text/plain","OK"); });
  server.on("/left_start", [](){ turnLeftMotors(); server.send(200,"text/plain","OK"); });
  server.on("/right_start", [](){ turnRightMotors(); server.send(200,"text/plain","OK"); });
  server.on("/toggleBuzzerNormal", [](){ toggleBuzzerNormal(); server.send(200,"text/plain","OK"); });
  server.on("/toggleBuzzerAlert", [](){ toggleBuzzerAlert(); server.send(200,"text/plain","OK"); });
  server.on("/toggleServoSweep", [](){ toggleServoSweep(); server.send(200,"text/plain","OK"); });
  server.on("/toggleEmergencyStop", [](){ toggleEmergencyStop(); server.send(200,"text/plain","OK"); }); 
  server.on("/toggleTargetTracking", [](){ toggleTargetTracking(); server.send(200,"text/plain","OK"); }); 
  server.on("/toggleAutonomousDriving", [](){ toggleAutonomousDriving(); server.send(200,"text/plain","OK"); }); 
  server.on("/servo_left", [](){ servoManualLeft(); server.send(200,"text/plain","OK"); }); 
  server.on("/servo_right", [](){ servoManualRight(); server.send(200,"text/plain","OK"); }); 
  server.on("/setSweepRange", handleSetSweepRange);
  server.on("/setSpeedSlow", [](){ setSweepSpeed(1); server.send(200,"text/plain","OK"); });
  server.on("/setSpeedMedium", [](){ setSweepSpeed(2); server.send(200,"text/plain","OK"); });
  server.on("/setSpeedFast", [](){ setSweepSpeed(3); server.send(200,"text/plain","OK"); });
  server.on("/setThreshold", handleSetThreshold);
  server.on("/getRadarData", handleGetRadarData);

  server.begin();
}

// ===================================
// == 4. MAIN LOOP (THE CORE LOGIC) ==
// ===================================

void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();

  // --- 1. Distance Measurement ---
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 30000); 
  distance = (duration > 0 && duration * 0.034 / 2 < 250) ? (duration * 0.034 / 2) : 0;

  // Check for danger
  bool isDangerouslyClose = (distance > 0 && distance <= distanceThreshold);

  // ------------------------------------------------------------------------------------------------
  // *** Priority A: Avoidance Protocol (Uninterruptible) ***
  // ------------------------------------------------------------------------------------------------
  if (isAvoiding) {
      long elapsedTime = currentMillis - avoidanceStartTime;
      
      // Step 1: Backup
      if (elapsedTime < avoidanceStep1Duration) {
          backwardMotors(); 
      } 
      // Step 2: Turn Right
      else if (elapsedTime < (avoidanceStep1Duration + avoidanceStep2Duration)) {
          turnRightMotors(); 
      } 
      // Avoidance finished
      else {
          stopMotors();
          isAvoiding = false;      
          
          isServoSweeping = true; // Resume sweep
          isTrackingTarget = false; 
          lastDetectedAngle = -1; 
          
          // Resume Autonomous Drive if enabled
          if (isAutonomousDrivingEnabled) {
              forwardMotors();
          }
      }

      // Buzzer Alert during avoidance
      if (isBuzzerAlertEnabled) {
          if (currentMillis - lastBuzzerTime >= buzzerIntervalAlert) {
              digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
              lastBuzzerTime = currentMillis;
          }
      } else {
          digitalWrite(BUZZER_PIN, HIGH); 
      }
      myServo.write(currentAngle); 

      return; // Stop other logic until avoidance is done
  }

  // ------------------------------------------------------------------------------------------------
  // *** Priority B: Obstacle Detection and E-Stop (Universal) ***
  // ------------------------------------------------------------------------------------------------
  
  // E-Stop condition: E-Stop is ON AND danger detected
  if (isEmergencyStopEnabled && isDangerouslyClose) { 
      
      // If in Auto Mode, start avoidance protocol.
      if (isAutonomousDrivingEnabled || isTargetTrackingEnabled) {
          stopMotors();
          isServoSweeping = false;
          
          if (!isAvoiding) {
              lastDetectedAngle = currentAngle; 
              isTrackingTarget = false; 
              isAvoiding = true;               
              avoidanceStartTime = currentMillis; 
          }
      } 
      // If in Manual Mode, just stop motors (no avoidance).
      else {
          stopMotors();
      }

      // Alert Buzzer Logic
      if (isBuzzerAlertEnabled) {
          if (currentMillis - lastBuzzerTime >= buzzerIntervalAlert) {
              digitalWrite(BUZZER_PIN, LOW);
              buzzerOffTime = currentMillis + buzzerFlashDuration;
              lastBuzzerTime = currentMillis; 
          }
          if (buzzerOffTime > 0 && currentMillis >= buzzerOffTime) {
              digitalWrite(BUZZER_PIN, HIGH); 
              buzzerOffTime = 0; 
          }
      } else {
          digitalWrite(BUZZER_PIN, HIGH); 
      }
      myServo.write(currentAngle); 
      
      // Exit loop if not in auto-mode (to maintain stop)
      if (!isAutonomousDrivingEnabled && !isTargetTrackingEnabled) {
          return;
      }
  } 
  
  // ------------------------------------------------------------------------------------------------
  // *** Priority C: Normal Operation (Driving/Tracking/Sweeping) ***
  // ------------------------------------------------------------------------------------------------
  else {
      // 1. Autonomous Driving Logic 
      if (isAutonomousDrivingEnabled) {
          forwardMotors();
          isServoSweeping = true;
      }
      // 2. Target Tracking Logic
      else if (isTargetTrackingEnabled && isTrackingTarget) {
          
          int angleDiff = lastDetectedAngle - currentAngle;
        
          if (abs(angleDiff) > trackingThreshold) {
            if (angleDiff > 0) { 
              turnRightMotors();
            } else { 
              turnLeftMotors();
            }
          } else {
            // Target angle reached!
            stopMotors();
            isTrackingTarget = false;      
            isServoSweeping = true;        
            lastDetectedAngle = -1;        
          }
            
      } 
      // 3. Manual Mode (Motor control handled by server.on)
      
      // 4. Normal Buzzer Logic (slow flash)
      if (isBuzzerNormalEnabled) {
        if (currentMillis - lastBuzzerTime >= buzzerIntervalNormal) {
          digitalWrite(BUZZER_PIN, LOW); 
          buzzerOffTime = currentMillis + buzzerFlashDuration;
          lastBuzzerTime = currentMillis; 
        }
        
        if (buzzerOffTime > 0 && currentMillis >= buzzerOffTime) {
            digitalWrite(BUZZER_PIN, HIGH); 
            buzzerOffTime = 0; 
        }
        
      } else {
        digitalWrite(BUZZER_PIN, HIGH); 
      }
      
      // 5. Servo Motion Logic
      if (isServoSweeping) { 
        if (currentMillis - lastServoTime >= servoStepDelay) {
          currentAngle += stepDirection; 
          
          int minLimit = min(sweepStartAngle, sweepEndAngle);
          int maxLimit = max(sweepStartAngle, sweepEndAngle);

          if (currentAngle >= maxLimit) stepDirection = -1;
          if (currentAngle <= minLimit) stepDirection = 1;
          
          myServo.write(currentAngle);
          lastServoTime = currentMillis; 
        }
      } else {
        myServo.write(currentAngle);
      }
  }
}
