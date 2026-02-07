# Cardputer Tracker

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A real-time music tracker (synthesizer/drum machine) for the M5Stack Cardputer-ADV device. This portable acid house production tool features wavetable synthesis, state variable filters, and pattern-based sequencing.

## Hardware Requirements

- **M5Stack Cardputer-ADV** (ESP32-S3 based)
- **Note**: This project has only been tested on the Cardputer-ADV. Compatibility with other M5Stack devices is unknown.

## Installation

### Using Pre-built Releases

1. Download the latest firmware from the [Releases](../../releases) page
2. Flash the `.bin` file to your Cardputer-ADV using your preferred flashing tool
3. Power on and start making music!

### Building from Source

You can build this project using either Arduino IDE or Arduino CLI.

#### Option 1: Arduino IDE

1. **Install Arduino IDE** (2.0 or later recommended)
   - Download from [arduino.cc](https://www.arduino.cc/en/software)

2. **Add M5Stack Board Support**
   - Open Arduino IDE
   - Go to `File` → `Preferences`
   - Add this URL to "Additional Boards Manager URLs":
     ```
     https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json
     ```
   - Go to `Tools` → `Board` → `Boards Manager`
   - Search for "M5Stack" and install "M5Stack by M5Stack"

3. **Install Required Libraries**
   - Go to `Tools` → `Manage Libraries`
   - Search for and install "M5Cardputer"

4. **Open and Configure**
   - Clone or download this repository
   - Open `CardputerTracker/CardputerTracker.ino` in Arduino IDE
   - Select board: `Tools` → `Board` → `M5Stack Arduino` → `M5Stack-Cardputer`
   - Select port: `Tools` → `Port` → (your device's COM port)

5. **Upload**
   - Click the Upload button or press `Ctrl+U`

#### Option 2: Arduino CLI

**Prerequisites:**
- [Arduino CLI](https://arduino.github.io/arduino-cli/latest/installation/)

**Build Steps:**

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/Cardputer-Adv-Tracker.git
   cd Cardputer-Adv-Tracker
   ```

2. Install M5Stack board support:
   ```bash
   arduino-cli core install m5stack:esp32
   ```

3. Install required libraries:
   ```bash
   arduino-cli lib install M5Cardputer
   ```

4. Compile the project:
   ```bash
   arduino-cli compile --fqbn m5stack:esp32:m5stack_cardputer CardputerTracker
   ```

5. Upload to your device:
   ```bash
   arduino-cli upload -p <PORT> --fqbn m5stack:esp32:m5stack_cardputer CardputerTracker
   ```

   Replace `<PORT>` with your device's serial port (e.g., `COM3` on Windows, `/dev/ttyUSB0` on Linux)

## Basic Usage

- **Power on** the device to start the tracker
- **BtnA**: Navigate between pages/modes
- Explore the wavetable synthesis, filters, and pattern sequencing features

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on:
- Reporting bugs
- Suggesting features
- Submitting pull requests
- Setting up your development environment

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.