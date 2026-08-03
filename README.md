# Anti-Theft Tracking

An industrial project developed in collaboration with **STMicroelectronics** to design and implement a highly accurate, multi-anchor asset tracking and anti-theft system utilizing Ultra-Wideband (UWB) technology that offers centimeter-level precision. 

This repository contains the complete firmware, hardware PCB designs, and comprehensive documentation for the entire project lifecycle.

---

## 🏗️ System Architecture

The tracking ecosystem relies on a 4-subsystem architecture designed for precise trilateration and efficient power management. The system consists of three stationary anchors and one mobile tag:

### 1. STM32 Anchor (Hub)
*   **Hardware:** NUCLEO-U575ZI-Q + BU03 (UWB Module) + ST67W611M (Wi-Fi Module)
*   **Role:** Acts as the central hub of the tracking system. It interfaces with the tag and the secondary ESP32 anchors to compute the final location of the asset and manage the network. It publishes this data to a dashboard through the ST67W611M Wi-Fi module.
*   **Location:** `firmware/stm32-anchor-hub/nucleo-u575zi-q_bu03/`

### 2. ESP32 Anchors (x2)
*   **Hardware:** ESP32 + UWB Modules
*   **Role:** Two secondary stationary anchors strategically placed to provide the additional reference points required for accurate 2D/3D trilateration. 
*   **Location:** `firmware/esp32-anchors/`

### 3. STM32 Tag
*   **Hardware:** NUCLEO-WBA55CG + BU03 (UWB Module) / Custom Compact PCB
*   **Role:** The mobile asset being tracked. Designed for a compact footprint, it broadcasts its signals to the anchor network.
*   **Location:** `firmware/stm32-tag/Nucleo-WBA55CG_BU03/`

---

## 📂 Repository Structure

The repository is modularized to separate different microcontrollers, hardware designs, and documentation, preventing workspace conflicts.

*   **`/docs`**: Contains all reference materials, component datasheets, project presentations, and research (including AirTag teardowns).
*   **`/firmware`**: Separated source code for the ESP-IDF and STM32CubeIDE workspaces. Each subsystem contains its own build instructions and configurations.
*   **`/hardware`**: Contains the custom compact tag PCB designs (KiCad/Altium). Includes standard EDA source files alongside a `/production` folder for manufacturing Gerbers and BOMs.

---

## 👥 Contributors

*   Krishang Singh
*   Krishna Kumar Mahto
*   Harshul Tripathi
