# ESP32 Radar Robot

An ESP32 robot project combining a browser control panel, ultrasonic distance sensing, servo scanning, motor control, and timed obstacle-avoidance logic.

The firmware is written in C++ using the Arduino environment. The ESP32 serves its own HTML, CSS, and JavaScript interface over a local Wi-Fi access point.

## Implemented features

- Browser buttons for forward, reverse, left, right, and stop commands.
- Servo scanning with adjustable angle limits and three scanning speeds.
- Ultrasonic distance readings and a browser-based canvas display.
- An adjustable obstacle-distance threshold.
- Automatic forward driving with a timed reverse-and-turn response when an obstacle is detected within the threshold.
- Normal and alert buzzer controls.
- Manual servo adjustment when automatic scanning is disabled.

## Control flow

The main loop handles HTTP requests, reads the ultrasonic sensor, and evaluates control flags.

During automatic driving, a nearby obstacle starts an avoidance sequence: reverse for 500 ms, turn right for 700 ms, then resume forward movement if automatic driving is still enabled. Servo updates, buzzer timing, and the avoidance sequence use `millis()` comparisons.

The browser requests angle and distance data every 50 ms. This is the requested polling interval, not a measured update-rate guarantee.

## Hardware and pin mapping

The sketch uses an ESP32, an ultrasonic sensor, a servo, a buzzer, and four digital outputs for a motor driver controlling the drive motors.

| Connection | ESP32 GPIO |
| --- | --- |
| Ultrasonic trigger | 12 |
| Ultrasonic echo | 27 |
| Buzzer | 25 |
| Servo signal | 32 |
| Motor driver IN1 | 5 |
| Motor driver IN2 | 18 |
| Motor driver IN3 | 19 |
| Motor driver IN4 | 21 |

This table describes the source code, not a complete wiring guide. Exact board, sensor, driver, motor, and power-supply models are not specified in the repository. Confirm voltage compatibility, grounding, current requirements, and board-specific pin constraints before wiring. Do not drive motors directly from ESP32 GPIO pins.

## Build and run

1. Install the Arduino IDE, the ESP32 board package, and the `ESP32Servo` library. `WiFi` and `WebServer` are provided by the ESP32 Arduino environment.
2. Open `autonomous_tactical_robot.ino`. If the IDE asks to place it in a matching sketch folder, accept.
3. Review the pin definitions and change the access-point name and password in the network settings before use.
4. Select the correct ESP32 board and serial port, then compile and upload.
5. Power the hardware safely and connect a computer or phone to the configured ESP32 Wi-Fi access point.
6. Find that connection's gateway/router address in the device's Wi-Fi details and open it in a browser using `http://`.
7. Test the controls with the wheels lifted before allowing movement on the ground.

The source does not pin board-package or library versions. A verified build configuration and wiring diagram are planned additions.

## Project demonstration

[Watch the linked robot demonstration](https://youtu.be/7RLur4daF4w).

The video is a demonstration link, not a reproducible benchmark or a substitute for checking the current source and hardware.

## Current limitations

- **No path recording or return-to-start navigation.** The published sketch does not implement a movement-history vector or an `executeReturnPath` routine.
- **Not fully non-blocking.** Ultrasonic reads use `pulseIn()` with a timeout of up to 30 ms, alongside short trigger pulses using `delayMicroseconds()`. The HTTP server is serviced from the same main loop.
- **Stop commands are not a safety system.** The `/stop` handler stops the motor outputs but does not clear automatic-driving mode. Later loop iterations can command movement again. The timed avoidance sequence also takes priority over ordinary control logic.
- **No disconnect watchdog.** Losing the browser connection does not automatically clear driving commands or operating modes.
- **Prototype obstacle sensing.** Missing or out-of-range echoes become a distance of zero and are not treated as a nearby obstacle. Sensor errors therefore do not guarantee a stop.
- **Tracking is incomplete.** The source contains a target-tracking mode and related flags, but it does not establish a complete working target-tracking behavior. It should not be presented as a verified feature.
- **Local access is not application authentication.** HTTP control routes have no separate user login or authorization checks. The sketch includes a fixed Wi-Fi password that must be changed before use.
- **No proven performance or safety guarantees.** Automated tests, hardware test results, measured latency, and calibration procedures are not included.

## Safety

Use this as a supervised learning project, away from people, animals, traffic, and fragile objects. Start with the drive wheels lifted. Keep a physical way to disconnect motor power immediately. Do not rely on the browser's STOP or E-Stop labels to prevent injury or damage.

## Planned improvements

These are future work, not existing features:

- Add a bill of materials, wiring diagram, and verified build instructions.
- Make stop behavior persistent across automatic modes and avoidance states.
- Add a disconnect watchdog and safer handling of missing sensor readings.
- Replace loosely coupled control flags with explicit, testable state transitions.
- Add hardware tests and measured timing results.
- Implement and test tracking or path recording before documenting either as complete.

## License

MIT. See `LICENSE`.
