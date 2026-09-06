# Contributing to Real-Time Gesture Controlled Robot

Thank you for your interest in contributing to the **Real-Time Gesture Controlled Robot** project! This is an open-source collaborative robotics platform combining deep learning, embedded systems, and electrical hardware engineering.

---

## 👥 Core Project Team

This project was engineered and maintained by our multidisciplinary robotics team:

| Name | GitHub Username | Core Responsibilities & Contributions |
| :--- | :--- | :--- |
| **Prince Sanchela** | [@PrinceSanchela](https://github.com/PrinceSanchela) | **Core Robotics & CPU Vision Pipeline**<br>• End-to-end system design and hardware-software integration<br>• KiCad circuit schematic and power rail isolation<br>• ESP32 firmware (WebSocket server, PWM control, safety watchdog)<br>• CPU-optimized real-time vision client (`main.cpu.py`) with thread tuning & async queue |
| **Vedant Chauhan** | [@VedantChauhan14](https://github.com/VedantChauhan14) | **Machine Learning & CUDA GPU Pipeline**<br>• Google Colab training pipeline (`train_gesture_yolo.ipynb`) & YOLOv8n checkpoint export (`best.pt`)<br>• Dataset consolidation and automated label remapping<br>• CUDA GPU-accelerated vision client (`main.cuda.py`) with VRAM cache management |
| **Aastha** | [@Aastha150](https://github.com/Aastha150) | **Computer Vision & Dataset Annotation**<br>• Hand gesture dataset acquisition and curation<br>• Multi-class bounding box annotation and verification<br>• Dataset quality control across variable lighting conditions |
| **Aahyam Rehman** | [@rehmanaahyam](https://github.com/rehmanaahyam) | **Mechanical Fabrication & Locomotion Testing**<br>• Chassis mechanical assembly and motor bracket mounting<br>• Drive train alignment and track friction reduction<br>• Field testing with differential motor calibration tool |
| **Aakarsh** | [@akarsh-616](https://github.com/akarsh-616) | **Power Systems & Electrical Assembly**<br>• High-power wiring harness and terminal block routing<br>• LM2596 buck converter voltage regulation (12V $\to$ 5V)<br>• Battery discharge monitoring and inductive noise suppression |
| **Het Zaveri** | [@Het-Zaveri](https://github.com/Het-Zaveri) | **Telemetry, Network Benchmarking & QA**<br>• WebSocket network latency and throughput profiling<br>• Hardware watchdog failsafe trigger verification<br>• End-to-end integration and system stress testing |

---

## 🛠️ How to Contribute

We welcome community contributions, bug reports, feature enhancements, and documentation improvements.

### 1. Development Workflow

1. **Fork the Repository**:
   Click the **Fork** button at the top-right of the repository page.

2. **Clone your Fork**:
   ```bash
   git clone https://github.com/<your-username>/Real-Time-Gesture-Controlled-Robot.git
   cd Real-Time-Gesture-Controlled-Robot
   ```

3. **Create a Feature Branch**:
   ```bash
   git checkout -b feature/your-feature-name
   # or for bug fixes:
   git checkout -b fix/issue-description
   ```

4. **Make Your Changes**:
   Follow the coding guidelines detailed below.

5. **Commit Your Changes**:
   Use structured [Conventional Commits](https://www.conventionalcommits.org/):
   ```bash
   git commit -m "feat(vision): add adaptive confidence thresholding"
   ```

6. **Push to Your Fork & Open a Pull Request**:
   ```bash
   git push origin feature/your-feature-name
   ```
   Open a Pull Request against the `main` branch with a clear description of your changes and test results.

---

## 📋 Code Standards & Engineering Guidelines

### Python (Inference & Network Client)
- **Formatting**: Adhere to PEP 8 conventions (4 spaces indentation, 88–100 character line limits).
- **Concurrency**: Network operations must **never** block the main OpenCV video loop. Always utilize worker threads and thread-safe queues (`queue.Queue`).
- **Device Portability**: Any changes to model inference must work cleanly across both CPU (`main.cpu.py`) and CUDA GPU (`main.cuda.py`) pipelines.

### Embedded C++ / Arduino (ESP32 Firmware)
- **Non-blocking Execution**: Do **not** insert long blocking `delay()` calls inside the primary `loop()`. Keep loop iterations non-blocking so the WebSocket server and safety watchdog can process events promptly.
- **Fail-Safe Integrity**: The hardware watchdog timer (`SIGNAL_TIMEOUT = 1000 ms`) is a critical safety feature. Any new movement routines must automatically zero motor PWMs when signals cease.
- **Pin Definitions**: Clearly define all GPIO pin assignments at the top of the file using preprocessor `#define` directives.

### Hardware Schematics (KiCad)
- Schematics must be maintained in **KiCad 10** format.
- Maintain isolated power planes: high-discharge motor power rails (12V) must remain isolated from digital microcontroller logic (5V) with a shared reference ground (`GND`).
- Label all interconnect headers and net names explicitly.

### Datasets & Machine Learning
- Dataset annotations must strictly adhere to the standard YOLO format: `<class_id> <x_center> <y_center> <width> <height>` (normalized between 0 and 1).
- Class IDs must match the 5 configured postures:
  - `0`: `okay`
  - `1`: `spiderman`
  - `2`: `like`
  - `3`: `dislike`
  - `4`: `rock`

---

## 🏷️ Commit Message Conventions

We follow the **Conventional Commits** specification:

- `feat(...)`: A new feature (e.g., `feat(firmware): add closed-loop encoder PID control`)
- `fix(...)`: A bug fix (e.g., `fix(vision): handle dropped camera frames gracefully`)
- `docs(...)`: Documentation changes (e.g., `docs(readme): add wiring schematic explanation`)
- `perf(...)`: Performance optimization (e.g., `perf(inference): reduce input tensor pre-process latency`)
- `build(...)`: Build system or dependency updates (e.g., `build: update ultralytics version`)
- `chore(...)`: Routine repository maintenance or configuration

---

## 🧪 Pre-Submission Checklist

Before opening a Pull Request, please ensure:

- [ ] Python scripts run without unhandled exceptions or crashes.
- [ ] ESP32 firmware compiles cleanly in Arduino IDE without warnings.
- [ ] Motors brake immediately when video stream is closed (watchdog validation).
- [ ] Dependencies are properly cataloged in `requirements-cpu.txt` and `requirements-cuda.txt`.
- [ ] Documentation has been updated to reflect any pinout or logic changes.

---

## 💬 Community & Questions

If you encounter bugs, have questions, or want to propose an architectural change:
- Open an **Issue** on GitHub detailing the environment, steps to reproduce, and relevant logs.
- Reach out to **Prince Sanchela** or any core team member via GitHub.
