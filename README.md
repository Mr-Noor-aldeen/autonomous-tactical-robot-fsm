# Autonomous Tactical Edge System & Embedded FSM

A production-grade embedded architecture engineered on the ESP32 framework, showcasing an advanced event-driven Finite State Machine (FSM) that shifts a mobile robotic asset from standard reactive mechanics into a highly sophisticated, state-memory autonomous system.

## Architectural Highlights & Core Engine

* **Deterministic Priority-Based FSM:** Engineered a strict, non-linear execution loop that prioritizes safety and tactical functions. The loop isolates critical protocols (**Priority A: Uninterruptible Avoidance**, **Priority B: Fail-Safe Emergency Stop**) from normal telemetry, mimicking aviation and automotive control architectures.
* **Algorithmic Path Backtracking Memory:** Upgraded the system runtime by implementing a high-velocity state logger utilizing dynamic data structures (`std::vector<Step>`). The system records multi-directional movements in real-time and executes an automated inverse-path asset recovery routine (`executeReturnPath`) with zero runtime latency.
* **Asynchronous Time-Slicing Multitasking:** Completely eliminated blocking routines (`delay()`) in favor of asynchronous-like delta time computations (`millis()`). This ensures the localized `WebServer` remains highly responsive to inbound telemetry packages while managing heavy hardware actuator loads simultaneously.
* **Signal Noise Filtering & Edge Telemetry:** Integrated algorithmic deadbands and continuous time-window validation to filter ultrasonic sensor data, effectively suppressing false-positive feedback loops during high-velocity sweep states.
* **Embedded UI Template Engine:** Features an on-board control interface built with raw HTML5 and an asynchronous JavaScript Canvas. It utilizes a custom micro-template search-and-replace mechanism to inject real-time radar data points directly into memory slices.

## Tech Stack & Protocols

* **Language/Environment:** C++ (Embedded), Arduino Core, JavaScript (ES6), HTML5/CSS3.
* **Hardware Interfacing:** ESP32 Microcontroller, PWM Servo Control (`ESP32Servo`), Ultrasonic Telemetry Hub, H-Bridge DC Motor Driver Inversion.
* **Networking & Web Layer:** SoftAP Wi-Fi Infrastructure, Localized Asynchronous HTTP Server (`WebServer`), AJAX Polling Protocols.

---
*Designed and developed by Nour Aldin Qwaider (Mr-Noor-aldeen).*
