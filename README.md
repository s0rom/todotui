<div align="center">

# TODOTUI

**A lightweight, customizable Terminal User Interface (TUI) task manager.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)

<!-- Replace with your actual screenshot/GIF path -->
![todotui preview](./assets/preview.png)

</div>

---

## Features

- **Fast & Lightweight**: Built with performance in mind.
- **Custom Themes**: Fully customizable UI colors via ANSI hex/RGB escapes.
- **Auto-Persistence**: Automatically saves and restores your tasks.

---

## Build & Installation

### Prerequisites
- C++17 compatible compiler (`g++`, `clang++`)
- `make` (optional)

### Building from source

```bash
# Clone the repository
git clone [https://github.com/s0rom/todotui.git](https://github.com/s0rom/todotui.git)
cd todotui

# Compile the project
g++ -std=c++17 -O2 main.cpp -o todotui

# Run the app
./todotui
