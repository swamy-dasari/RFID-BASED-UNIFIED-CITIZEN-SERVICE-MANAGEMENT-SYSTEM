# 🏷️ RFID Unified Citizen System

![Microcontroller](https://img.shields.io/badge/Microcontroller-LPC2148-blue.svg)
![Language](https://img.shields.io/badge/Language-Embedded%20C-orange.svg)
![IDE](https://img.shields.io/badge/IDE-Keil%20uVision-green.svg)
![Status](https://img.shields.io/badge/Status-Complete-success.svg)

An advanced embedded system project built on the **LPC2148 (ARM7TDMI-S)** microcontroller. This project implements a comprehensive **Unified Citizen System**, integrating critical utility databases and financial services into a single multi-purpose RFID smart card interface. 

By scanning a unique citizen RFID card, authenticated users can access their Banking (ATM), Voting, Driving License status, and PAN details seamlessly through an interactive LCD and Keypad interface.

---

## 📐 System Architecture & Design

### High-Level Block Diagram
The overall system layout connects the main controller block to various sensors, displays, inputs, and memory storage.

<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/59b8ad00-df9a-43be-97fe-b7319532dd8c" />


### Software & Firmware Architecture
The codebase is structured modularly to separate the low-level peripheral drivers (UART, SPI, Keypad, LCD, RTC) from the high-level application menus.

<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/2b9d6adb-93dc-4f75-889e-baa885f0b13f" />


---

## 🗺️ Main Program Flow Chart
Here is the detailed sequential logic executed by the main program:

<img width="1024" height="1536" alt="Main Program Flow Chart RFID" src="https://github.com/user-attachments/assets/55abb5ab-0275-4d32-8ab3-3e4fddd18786" />

### Firmware Modules & Responsibilities
Each C file is compiled and linked with specific functional responsibilities to form the unified binary:

<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/6505a33d-f474-4e26-ba06-60b75ef8636c" />


---

## ✨ System Features in Detail

### 1. 🪪 Universal RFID Authentication & Card Security
*   **Dual Roles:** Distinguishes between standard citizens (cards registered in parallel arrays) and system administrators (Officer Master Card).
*   **Loss Prevention / Blocking Mechanism:** The Officer Menu allows administrators to select any citizen by index and mark their status as `BLOCKED` (stores `0x01` at their EEPROM index). If a citizen's physical card is stolen/lost and scanned afterwards, access is immediately blocked, trigger outputs (Red LED and continuous Buzzer) are activated, and the incident is logged via serial telemetry.
*   **Inactivity & Redraw Resiliency:** Protects user sessions with a 20-second inactivity timeout for all keypad input operations, returning to the login loop if abandoned.

### 2. 🏦 Secure ATM (Automated Teller Machine) Module
*   **Encrypted Storage:** Balances and individual ATM PINs are stored securely in external non-volatile memory.
*   **Transactional Guards:**
    *   Restricts withdrawals to multiples of `100`, `200`, and `500` rupees.
    *   Enforces a minimum balance limit (`Rs. 500`) and a maximum storage boundary (`Rs. 65,535` based on 16-bit uint representation).
    *   Authenticates with a distinct ATM-only PIN before loading the transaction screen.

### 3. 🗳️ Double-Voting Prevention System
*   **Authentication Check:** Asks for login verification before allowing voting access.
*   **Non-Volatile Registry:** Once a citizen casts their vote for any of the 4 configured parties (**BJP, INC, AAP, BSP**), the vote state is written to EEPROM. If the user tries to access the voting screen again, the system queries the EEPROM and immediately blocks the operation with an "Already Voted!" error message.

### 4. 🚗 Driving License Expiry Engine (RTC & CGRAM)
*   **Real-time Expiry Calculation:** Automatically compares the expiration date parameters (`exp_days`, `exp_months`, `exp_years` stored per user) against the running internal Real-Time Clock (RTC) registers.
*   **Hardware Signaling:** 
    *   **Valid License:** Displays license details, illuminates the Green LED, and silences any buzzer alarms.
    *   **Expired License:** Flashes a custom-built CGRAM "Bold Cross (✖)" icon on the LCD, turns on the Red LED, and sounds a warning Buzzer.
*   **Custom Graphics:** Built-in CGRAM design templates inject custom validation marks directly into the HD44780 LCD module memory:
    *   `Bold Checkmark (✔)` pattern code.
    *   `Bold Cross (✖)` pattern code.

---

## 🗄️ EEPROM Storage & Memory Mapping (AT25LC512)

The system relies on an external **AT25LC512 (512Kbit / 64KB)** EEPROM over SPI0 to maintain persistent user states.

Below is the mapping map designed to keep system parameters persistent across power cycles:

| Hex Memory Address | Data Field description | Default Value / Boot Configuration |
| :--- | :--- | :--- |
| `0x0000` | Magic Validation Byte | Initialized to `0xC6` (triggers default values on first boot) |
| `0x0010 - 0x0011` | User 1 (Bhavesh) ATM Balance (16-bit) | `10000` |
| `0x0012 - 0x0013` | User 2 (Om) ATM Balance (16-bit) | `450` |
| `0x0014 - 0x0015` | User 3 (Mihir) ATM Balance (16-bit) | `5000` |
| `0x0020` | User 1 Vote Registration Code | `0x00` (Not Voted) |
| `0x0021` | User 2 Vote Registration Code | `0x02` (Voted for INC) |
| `0x0022` | User 3 Vote Registration Code | `0x00` (Not Voted) |
| `0x0030 - 0x0033` | User 1 Login PIN (4 characters) | `"1234"` |
| `0x0034 - 0x0037` | User 2 Login PIN (4 characters) | `"1234"` |
| `0x0038 - 0x003B` | User 3 Login PIN (4 characters) | `"1234"` |
| `0x0040 - 0x0043` | User 1 ATM Security PIN (4 chars) | `"1234"` |
| `0x0044 - 0x0047` | User 2 ATM Security PIN (4 chars) | `"1234"` |
| `0x0048 - 0x004B` | User 3 ATM Security PIN (4 chars) | `"1234"` |
| `0x0050` | User 1 Loss/Blocked Status byte | `0x00` (Active) |
| `0x0051` | User 2 Loss/Blocked Status byte | `0x00` (Active) |
| `0x0052` | User 3 Loss/Blocked Status byte | `0x00` (Active) |

---

## 🛠️ Low-Level Drivers & Implementation

### 1. UART0 (RFID Transceiver Driver)
*   **Baud Rate & Frame Configurations:** Runs at a baud rate of `9600` configured for an ARM core peripheral clock (Pclk) of `15MHz` (`U0DLL = 97`, `U0DLM = 0`, `8N1` Mode).
*   **Asynchronous Interrupt Handler:** Leverages the LPC2148 UART0 RX line interrupt (`UART0_ISR` mapped in VIC slot 5) to intercept RFID frames on the fly without blocking execution. 
*   **Framing Delimiters:** Incoming RFID streams are parsed between the standard Serial Start-of-Text (`STX = 0x02`) and End-of-Text (`ETX = 0x03`) bytes to ensure packet transmission integrity.

### 2. Interrupt Management
The Vectored Interrupt Controller (VIC) maps incoming hardware triggers efficiently (e.g., UART0 and EINT3)

### 3. LCD Driver (8-Bit Mode)
*   **Layout:** Operates in 8-bit bus configuration utilizing pins `P0.8 - P0.15` for data and control pins `P0.16` (RS), `P0.17` (R/W), and `P0.18` (EN).
*   **CGRAM Interface:** Contains functions to reprogram the internal CGRAM tables of the LCD display on-the-fly, allowing graphics manipulation of custom display metrics.

### 4. SPI0 Engine & Sakamoto's Calendaring
*   **SPI0 Init:** Standard 8-bit write-only/read SPI routines mapped directly to hardware peripheral registers (`S0SPCR`, `S0SPSR`, `S0SPDR`).
*   **Sakamoto's Algorithm:** Integrated mathematically to keep the RTC day register (`DOW`) calculated dynamically when administrative edits occur:
    ```c
    // DOW calculation using Sakamoto's Algorithm
    static const u8 t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
    ```

---

## 🔌 Hardware Setup & Connections

### Development Board Components
The physical board interfaces a matrix keypad, character LCD, Buzzer, and LEDs:

<img width="1600" height="878" alt="image" src="https://github.com/user-attachments/assets/a20a6680-9e0b-4836-94c5-e9e68dc26a22" />

### Project Hardware Showcase
The following images demonstrate the physical hardware running various modules of the unified system on the ARM7 development board:

**1. System Boot & RFID Scan Screen**  

<img width="1600" height="878" alt="image" src="https://github.com/user-attachments/assets/9a51276b-0d27-4328-acb1-4ac4479bf0ba" />



**2. Officer Master Card Recognition**  
<img width="1280" height="785" alt="image" src="https://github.com/user-attachments/assets/cf835800-0a28-4b71-bae6-c80cdcf42f6d" />


**3. Administrator Officer Menu**  
<img width="1280" height="760" alt="image" src="https://github.com/user-attachments/assets/50a18c65-b42b-45d9-bb47-2560df975292" />

**5. Main Citizen Services Menu**  
<img width="1268" height="772" alt="image" src="https://github.com/user-attachments/assets/366c4201-e8f2-434f-8e81-3fff00a30cd8" />
<img width="1280" height="766" alt="image" src="https://github.com/user-attachments/assets/6adba668-6dbf-403f-b762-295b538ce1f8" />

---

## 🚀 Compilation & Development Environment

### Keil Development Platform Setup
Compile and debug the code using Keil Microcontroller Development Kit (MDK):

![Development Platform](./New%20images/Development%20Platform%20rfid.png)

### Deployment Guidelines:
1. **Prerequisites:**
   *   **Keil uVision IDE v4 or v5** installed with legacy support for the **ARM7 LPC2000** family.
   *   **Flash Magic Utility** (installed for serial programming over UART0).
2. **Build Settings:**
   *   Verify Target Clock frequency is set to **12.0 MHz** (with standard PLL multiplier configuration to yield Cclk = 60MHz).
   *   Ensure the linker script (`RFID_PROJECT.sct`) compiles memory sections correctly to flash.
3. **Flashing Protocol:**
   *   Connect LPC2148 Board via USB-to-UART converter interface.
   *   Place the LPC2148 into **ISP Boot Mode** (usually by asserting ISP/Reset switches).
   *   Point Flash Magic to your generated `.hex` binary and hit **Start**.
