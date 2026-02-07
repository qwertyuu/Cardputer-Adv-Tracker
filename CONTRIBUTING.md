# Contributing to Cardputer Tracker

Thank you for your interest in contributing to Cardputer Tracker! This document provides guidelines for contributing to the project.

## How to Report Bugs

Found a bug? Help us fix it by submitting a detailed bug report:

1. Go to the [Issues](../../issues) page
2. Click "New Issue" and select the **Bug Report** template
3. Fill in all required fields:
   - Hardware device (Cardputer-ADV is the only tested device)
   - Firmware version (tag or commit hash)
   - Steps to reproduce the issue
   - Expected vs actual behavior
   - Screenshots if applicable

## How to Suggest Features

Have an idea for a new feature? We'd love to hear it!

1. Go to the [Issues](../../issues) page
2. Click "New Issue" and select the **Feature Request** template
3. Describe:
   - What the feature should do
   - Why it would be useful (use case)
   - Any potential impact on audio performance

## Development Workflow

Ready to contribute code? Follow these steps:

1. **Fork** the repository to your GitHub account
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/yourusername/Cardputer-Adv-Tracker.git
   cd Cardputer-Adv-Tracker
   ```
3. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```
4. **Make your changes** to the code
5. **Test on hardware** - All changes MUST be tested on a physical Cardputer-ADV device
6. **Commit** your changes:
   ```bash
   git commit -m "Add feature: your feature description"
   ```
7. **Push** to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```
8. **Create a Pull Request** from your fork to the main repository

## Setting Up the Development Environment

### Prerequisites

- **Arduino IDE** (2.0+) or **Arduino CLI**
- M5Stack Cardputer-ADV hardware
- USB cable for flashing

You can use either Arduino IDE (recommended for beginners) or Arduino CLI (for command-line workflows).

### Option 1: Arduino IDE Setup

1. **Install Arduino IDE**
   - Download from [arduino.cc](https://www.arduino.cc/en/software)
   - Install version 2.0 or later

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

4. **Open the Project**
   - Open `CardputerTracker/CardputerTracker.ino` in Arduino IDE
   - Select board: `Tools` → `Board` → `M5Stack Arduino` → `M5Stack-Cardputer`
   - Select port: `Tools` → `Port` → (your device's COM port)

5. **Compile and Upload**
   - Click the Upload button or press `Ctrl+U`

### Option 2: Arduino CLI Setup

1. **Install Arduino CLI**
   - Follow instructions at [arduino.github.io/arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/)

2. **Install M5Stack board definitions**
   ```bash
   arduino-cli core install m5stack:esp32
   ```

3. **Install required libraries**
   ```bash
   arduino-cli lib install M5Cardputer
   ```

4. **Compile the project**
   ```bash
   arduino-cli compile --fqbn m5stack:esp32:m5stack_cardputer CardputerTracker
   ```

5. **Upload to your Cardputer-ADV**
   ```bash
   arduino-cli upload -p <PORT> --fqbn m5stack:esp32:m5stack_cardputer CardputerTracker
   ```

   Replace `<PORT>` with your device's serial port (e.g., `COM3` on Windows, `/dev/ttyUSB0` on Linux).

## Testing Requirements

**CRITICAL**: All code changes MUST be tested on physical Cardputer-ADV hardware before submitting a PR.

- Verify the code compiles without warnings
- Flash to the device and test all affected functionality
- Verify audio performance (no glitches, clicks, or pops)
- Test pattern playback and synthesis features
- Check that UI remains responsive

## Pull Request Review Process

What to expect after submitting a PR:

1. **Automated checks**: GitHub Actions will build your code
2. **Code review**: Maintainers will review your changes
3. **Testing**: Changes will be verified on hardware
4. **Feedback**: You may receive requests for modifications
5. **Merge**: Once approved and tested, your PR will be merged

### PR Best Practices

- Keep PRs focused on a single feature or bug fix
- Write clear commit messages
- Include a description of what changed and why
- Reference related issues (e.g., "Fixes #123")
- Respond to review feedback promptly

## Code Style Guidelines

- Follow existing code style and formatting
- Use meaningful variable and function names
- Add comments for complex logic
- Keep functions focused and concise

## Questions?

If you have questions about contributing, feel free to:
- Open a [Discussion](../../discussions)
- Ask in an issue or PR
- Reach out to maintainers

Thank you for contributing to Cardputer Tracker! 🎵
