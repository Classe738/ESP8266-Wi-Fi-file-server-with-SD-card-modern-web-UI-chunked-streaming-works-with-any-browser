
      
            D R O P . C L O U D
     
     ESP8266 SD Card File Server v2.0
    
 * ============================================================================
 * 
 * TECHNICAL DOCUMENTATION AND FIRMWARE ARCHITECTURE
 * 
 * Author: VS_Drop.cloud Team
 * Documentation Version: 2.0
 * Date: 2024
 * 
 * ============================================================================
 * 
 * TABLE OF CONTENTS:
 * 
 * 1. SYSTEM ARCHITECTURE
 *    1.1. General Workflow
 *    1.2. System Components
 *    1.3. Data Flows
 * 
 * 2. ESP8266 HARDWARE
 *    2.1. Chip Specifications
 *    2.2. Memory Limitations
 *    2.3. Peripherals (SPI, WiFi)
 *    2.4. Prototyping and Assembly (with photos)
 * 
 * 3. SOFTWARE ARCHITECTURE
 *    3.1. Code Structure
 *    3.2. Memory Optimizations
 *    3.3. Web Server Mechanism
 * 
 * 4. DATA TRANSFER TECHNOLOGIES
 *    4.1. Chunked Transfer Encoding
 *    4.2. Streaming File Transfer
 *    4.3. MIME Types
 * 
 * 5. SECURITY
 *    5.1. Authentication
 *    5.2. File System Protection
 * 
 * 6. EXTENDING FUNCTIONALITY
 *    6.1. Adding New Routes
 *    6.2. Adding New File Types
 *    6.3. Integration with External Services
 * 
 * 7. DEBUG AND MONITORING
 *    7.1. Serial Debug
 *    7.2. Memory Monitoring
 * 
 * ============================================================================
 */




// ============================================================================
// SECTION 1: SYSTEM ARCHITECTURE
// ============================================================================

/*
 * 1.1. GENERAL WORKFLOW
 * -----------------------
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                         VS_Drop.cloud System                           │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │   ┌─────────────┐      ┌─────────────┐      ┌─────────────┐            │
 * │   │   CLIENT    │      │   ESP8266   │      │  SD CARD    │            │
 * │   │ (Smartphone)│◄────►│   Server    │◄────►│  (Files)    │            │
 * │   └─────────────┘ WiFi └─────────────┘ SPI  └─────────────┘            │
 * │         │                     │                                        │
 * │         │                     │                                        │
 * │         ▼                     ▼                                        │
 * │   ┌─────────────┐      ┌─────────────┐                                 │
 * │   │  Browser    │      │   Flash     │                                 │
 * │   │   HTTP      │      │  (HTML/CSS) │                                 │
 * │   └─────────────┘      └─────────────┘                                 │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 * REQUEST SEQUENCE:
 * 
 * 1. Client connects to the WiFi access point "VS_Drop_Cloud"
 * 2. Client opens browser and enters http://192.168.4.1
 * 3. ESP8266 accepts HTTP request via ESP8266WebServer
 * 4. WebServer parses the URL and calls the appropriate handler
 * 5. Handler reads/writes files on the SD card via SPI
 * 6. Handler generates HTML response (from Flash memory)
 * 7. WebServer sends response to client
 * 
 */


/*
 * 1.2. SYSTEM COMPONENTS
 * -----------------------
 * 
 * ┌────────────────────────────────────────────────────────────────┐
 * │                    ESP8266 COMPONENTS                          │
 * ├────────────────────────────────────────────────────────────────┤
 * │                                                                │
 * │  ┌──────────────────┐                                         │
 * │  │   WiFi Module    │  - SoftAP (access point)                │
 * │  │                  │  - TCP/IP stack                         │
 * │  │  lwIP Stack      │  - HTTP server                          │
 * │  └──────────────────┘                                         │
 * │           │                                                    │
 * │           ▼                                                    │
 * │  ┌──────────────────┐                                         │
 * │  │ ESP8266WebServer │  - HTTP request handling                │
 * │  │                  │  - URL routing                          │
 * │  │                  │  - Argument parsing                     │
 * │  │                  │  - File streaming                       │
 * │  └──────────────────┘                                         │
 * │           │                                                    │
 * │           ▼                                                    │
 * │  ┌──────────────────┐                                         │
 * │  │   SPI Module     │  - Communication with SD card           │
 * │  │                  │  - MOSI/MISO/SCK/CS pins                │
 * │  │                  │  - Half-speed for stability             │
 * │  └──────────────────┘                                         │
 * │           │                                                    │
 * │           ▼                                                    │
 * │  ┌──────────────────┐                                         │
 * │  │    SD Library    │  - FAT32 file system                    │
 * │  │                  │  - Read/Write/Delete/Mkdir              │
 * │  │                  │  - File streaming                       │
 * │  └──────────────────┘                                         │
 * │                                                                │
 * └────────────────────────────────────────────────────────────────┘
 * 
 * ARDUINO IDE LIBRARIES:
 * 
 * 1. ESP8266WiFi.h
 *    - WiFi management (STA/AP modes)
 *    - WiFiServer class for TCP connections
 *    - WiFiClient class for client handling
 * 
 * 2. ESP8266WebServer.h
 *    - HTTP server with routing
 *    - GET/POST request handling
 *    - Multipart file upload
 *    - Large file streaming
 * 
 * 3. SPI.h
 *    - Serial Peripheral Interface
 *    - Master mode for SD card
 *    - Configurable speed
 * 
 * 4. SD.h
 *    - FAT32 file system operations
 *    - Based on SdFat library
 *    - File = wrapper over sdcard_file_t
 * 
 */


/*
 * 1.3. DATA FLOWS
 * ------------------
 * 
 * FILE UPLOAD:
 * 
 * ┌──────────┐    HTTP POST     ┌──────────┐    chunks     ┌──────────┐
 * │  Client  │ ───────────────► │  ESP8266 │ ────────────► │ SD card  │
 * │          │   multipart/     │  8KB     │   write()     │          │
 * │  File    │   form-data      │  buffer  │               │  File    │
 * └──────────┘                  └──────────┘               └──────────┘
 *      │                              │                          ▲
 *      │                              │                          │
 *      │  1. Select file              │  2. Receive chunks       │
 *      │  2. Submit form              │  3. Write to SD          │
 *      │  3. Progress bar             │  4. Confirmation         │
 *      ▼                              ▼                          │
 *                                                                   │
 *                                                                 3. Streaming write
 * 
 * FILE DOWNLOAD:
 * 
 * ┌──────────┐                   ┌──────────┐    read()    ┌──────────┐
 * │  Client  │ ◄─────────────── │  ESP8266 │ ◄─────────── │ SD card  │
 * │          │   HTTP Response   │ streamFile│   chunks    │          │
 * │  File    │   MIME type       │           │             │  File    │
 * └──────────┘                   └──────────┘              └──────────┘
 *      │                               │                         │
 *      │  1. GET /download?path=file   │                         │
 *      │  2. Receive file              │  3. Read chunks         │
 *      │  3. Save                      │  4. Send to client      │
 *      ▼                               ▼                         ▼
 * 
 * 
 * IMPORTANT: ESP8266 does NOT load the entire file into memory!
 *            Streaming is used - reading/writing in 8KB chunks.
 *            This allows working with files of ANY size.
 * 
 */




// ============================================================================
// SECTION 2: ESP8266 HARDWARE
// ============================================================================

/*
 * 2.1. CHIP SPECIFICATIONS
 * -------------------------
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    ESP8266EX Specifications                     │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  CPU:         Tensilica L106 32-bit RISC                        │
 * │               80 MHz (default)                                  │
 * │               160 MHz (overclock, unstable)                     │
 * │                                                                 │
 * │  RAM:         160 KB total                                      │
 * │               ├── 32 KB for instructions (IRAM)                 │
 * │               ├── 32 KB for data (DRAM)                         │
 * │               └── 96 KB for WiFi stack (not available!)         │
 * │                                                                 │
 * │               ACTUALLY AVAILABLE: ~40-50 KB for your program!   │
 * │                                                                 │
 * │  Flash:       External, 1-16 MB                                 │
 * │               Code executed via cache (XIP)                     │
 * │               Data read directly                                │
 * │                                                                 │
 * │  WiFi:        802.11 b/g/n                                      │
 * │               2.4 GHz only                                      │
 * │               Up to 72 Mbps PHY rate                            │
 * │               TCP/IP stack: lwIP                                │
 * │                                                                 │
 * │  SPI:         Maximum 4 slaves                                  │
 * │               Up to 40 MHz clock                                │
 * │               Used for Flash and SD card                        │
 * │                                                                 │
 * │  GPIO:        17 pins                                           │
 * │               6 occupied by Flash!                              │
 * │               11 available (with limitations)                   │
 * │                                                                 │
 * │  ADC:         10-bit, 1 channel (0-1.0V!)                       │
 * │                                                                 │
 * │  UART:        2 ports (UART0 for Serial, UART1 for debug)       │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * 
 * ESP8266 MEMORY MAP:
 * 
 * ┌────────────────────┬──────────────┬─────────────────────────────┐
 * │      Region        │    Size      │         Description         │
 * ├────────────────────┼──────────────┼─────────────────────────────┤
 * │ 0x40100000-...     │ 32 KB        │ IRAM (Instructions RAM)     │
 * │ 0x40200000-...     │ 1 MB mapped  │ Flash (XIP - execute in place)│
 * │ 0x3FFE8000-...     │ 32 KB        │ DRAM (Data RAM)             │
 * │ 0x40110000-...     │ 96 KB        │ WiFi Buffer (used by SDK)   │
 * └────────────────────┴──────────────┴─────────────────────────────┘
 * 
 */


/*
 * 2.2. MEMORY LIMITATIONS - CRITICALLY IMPORTANT!
 * -------------------------------------------
 * 
 * WHY THIS MATTERS:
 * 
 * ESP8266 has VERY little RAM. If programming carelessly,
 * the program will crash with "Exception 0" or simply reboot.
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │              TYPICAL RAM USAGE                                  │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  WiFi Stack:              ~24 KB (always occupied)              │
 * │  lwIP TCP/IP:             ~8 KB                                │
 * │  System:                  ~4 KB                                │
 * │  HTTP Server buffers:     ~4 KB                                │
 * │  ─────────────────────────────────                             │
 * │  TOTAL system used:       ~40 KB                               │
 * │                                                                 │
 * │  FREE FOR YOUR CODE:       ~40 KB                               │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * 
 * MAIN RAM ENEMIES:
 * 
 * 1. String class - dynamic memory allocation
 *    BAD:
 *    String bigString = "Very long string...";
 *    
 *    GOOD:
 *    Serial.println(F("Very long string...")); // F() stores in Flash
 * 
 * 2. Global variables - permanently occupy RAM
 *    BAD:
 *    char buffer[8192];  // 8 KB occupied forever!
 *    
 *    GOOD:
 *    Use stack or avoid large buffers altogether
 * 
 * 3. String concatenation - memory fragmentation
 *    BAD:
 *    String html = "<html>";
 *    html += "<head>";
 *    html += "<title>";
 *    // Each += allocates new memory!
 *    
 *    GOOD:
 *    Store HTML in Flash (PROGMEM)
 * 
 * 
 * SOLUTION: PROGMEM AND F() MACRO
 * 
 * PROGMEM - a special attribute that tells the compiler to place
 * data in Flash memory, not in RAM.
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                   Flash vs RAM                                  │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  RAM:    50 KB    - Fast, but scarce                           │
 * │           ▲         For variables and stack                     │
 * │           │                                                     │
 * │  Flash:  4 MB     - Slower, but abundant                       │
 * │           ▲         For code and constants                      │
 * │           │                                                     │
 * │  PROGMEM stores data in Flash, freeing RAM                     │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * USAGE EXAMPLES:
 * 
 * // 1. F() macro - for strings in Serial.print
 * Serial.println(F("This string is in Flash")); // 0 bytes RAM
 * Serial.println("This string is in RAM");       // ~30 bytes RAM
 * 
 * // 2. PROGMEM - for large arrays and strings
 * const char CSS_STYLES[] PROGMEM = R"=====(
 *   body { background: #000; }
 *   ... thousands of CSS characters ...
 * )=====";
 * 
 * // 3. FPSTR() - for reading PROGMEM strings
 * html += FPSTR(CSS_STYLES);  // Reads from Flash
 * 
 * // 4. PSTR() - for inline strings
 * Serial.println(PSTR("Inline Flash string"));
 * 
 */


/*
 * 2.3. PERIPHERALS: SPI AND WIFI
 * ---------------------------
 * 
 * SPI (Serial Peripheral Interface):
 * ──────────────────────────────────
 * 
 * Used to communicate with the SD card.
 * 
 * ┌───────────────────────────────────────────────────────────────┐
 * │                  SPI Pins on ESP8266                          │
 * ├───────────────────────────────────────────────────────────────┤
 * │                                                               │
 * │  Signal   │  GPIO  │  Pin   │  SD Card   │  Direction        │
 * │───────────┼────────┼────────┼────────────┼───────────────────│
 * │  MOSI     │  13    │  D7    │  MOSI      │  ESP → SD         │
 * │  MISO     │  12    │  D6    │  MISO      │  SD → ESP         │
 * │  SCK      │  14    │  D5    │  SCK       │  ESP → SD         │
 * │  CS       │  4     │  D2    │  CS        │  ESP → SD         │
 * │                                                               │
 * │  MOSI = Master Out Slave In (data from master to slave)      │
 * │  MISO = Master In Slave Out (data from slave to master)      │
 * │  SCK  = Serial Clock                                          │
 * │  CS   = Chip Select (activates the device)                   │
 * │                                                               │
 * └───────────────────────────────────────────────────────────────┘
 * 
 * IMPORTANT: SD card operates at 3.3V!
 *            Connecting to 5V will destroy the card!
 * 
 * 
 * SPI SPEED:
 * 
 * SD.begin(CS, SPI_HALF_SPEED);    // ~6 MHz - stable
 * SD.begin(CS, SPI_QUARTER_SPEED); // ~3 MHz - for poor cards
 * SD.begin(CS, SPI_EIGHTH_SPEED);  // ~1.5 MHz - for very poor cards
 * 
 * Why HALF_SPEED? 
 * - At maximum speed, SD cards often produce errors
 * - Cheap Chinese modules have poor layout
 * - HALF_SPEED is the sweet spot for stability and speed
 * 
 * 
 * WiFi SoftAP (Access Point):
 * ────────────────────────────
 * 
 * In AP mode, the ESP8266 creates its own WiFi network.
 * 
 * ┌───────────────────────────────────────────────────────────────┐
 * │                  SoftAP Configuration                         │
 * ├───────────────────────────────────────────────────────────────┤
 * │                                                               │
 * │  WiFi.mode(WIFI_AP);           // Access point mode          │
 * │  WiFi.softAP(ssid, password);  // Create network             │
 * │                                                               │
 * │  IP address ALWAYS: 192.168.4.1                              │
 * │  DHCP range:         192.168.4.2 - 192.168.4.x               │
 * │  Maximum clients:    4-8 (depends on SDK)                    │
 * │                                                               │
 * └───────────────────────────────────────────────────────────────┘
 * 
 * Advantages of AP mode:
 * 
 * 1. Autonomy - no router needed
 * 2. Predictability - always one IP (192.168.4.1)
 * 3. Simplicity - client simply connects to the network
 * 4. Security - network is isolated from the internet
 * 
 * Disadvantages:
 * 
 * 1. No internet for clients
 * 2. Limited range
 * 3. Only 2.4 GHz
 * 
 */


/*
 * 2.4. PROTOTYPING AND ASSEMBLY
 * ------------------------------
 * 
 * This section shows the step-by-step process of building the hardware,
 * from breadboard prototype to the final enclosed device.
 * 
 * 2.4.1. Breadboard Prototype
 * 
 * The first step is to test the circuit on a breadboard. This allows
 * quick changes and ensures everything works before soldering.
 * 
 * ![Breadboard prototype](docs/images/photo_2026-03-18_15-01-38.jpg)
 * *Photo: ESP8266 connected to SD card module on a breadboard.*
 * 
 * 2.4.2. Soldering the Components
 * 
 * Once the prototype is stable, the components are soldered onto a
 * perforated board (protoboard) or a custom PCB.
 * 
 * ![Soldering process](docs/images/photo_2026-03-20_04-06-28.jpg)
 * *Photo: Soldering wires and pin headers.*
 * 
 * 2.4.3. Assembled Board
 * 
 * After soldering, the board is cleaned and inspected.
 * 
 * ![Assembled board](docs/images/photo_2026-03-20_04-06-39.jpg)
 * *Photo: The completed board with ESP8266 and SD card module.*
 * 
 * 2.4.4. Final Enclosed Device
 * 
 * For a polished look, the board is placed inside a 3D-printed or
 * off-the-shelf enclosure.
 * 
 * ![Final device in enclosure](docs/images/photo_2026-03-20_04-06-39.jpg)
 * *Photo: The finished file server in a compact case.*
 * 
 * 2.4.5. User Interface Screenshots
 * 
 * Here are three screenshots of the web interface:
 * 
 * ![Login screen](docs/images/photo_2026-03-18_15-01-24.png)
 * *Login page with dark theme.*
 * 
 * ![File browser](docs/images/565.png)
 * *Main file manager showing folders and files.*
 * 
 * ![Upload progress](docs/images/5646546.png)
 * *Upload page with real‑time progress bar.*
 * 
 */




// ============================================================================
// SECTION 3: SOFTWARE ARCHITECTURE
// ============================================================================

/*
 * 3.1. CODE STRUCTURE
 * --------------------
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                  Firmware file structure                        │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  VS_Drop_Cloud_ESP8266.ino                                      │
 * │  │                                                              │
 * │  ├── 1. HEADER                                                  │
 * │  │   ├── Comments and documentation                            │
 * │  │   ├── Library includes (#include)                           │
 * │  │   └── Configuration (#define)                               │
 * │  │                                                              │
 * │  ├── 2. GLOBAL VARIABLES                                       │
 * │  │   ├── ESP8266WebServer server                               │
 * │  │   ├── bool sdInitialized                                    │
 * │  │   └── String sessionToken                                   │
 * │  │                                                              │
 * │  ├── 3. HTML TEMPLATES (PROGMEM)                               │
 * │  │   ├── CSS_STYLES[]                                          │
 * │  │   └── SVG icons[]                                           │
 * │  │                                                              │
 * │  ├── 4. UTILITY FUNCTIONS                                      │
 * │  │   ├── formatSize()                                          │
 * │  │   ├── urlDecode()                                           │
 * │  │   ├── getMimeType()                                         │
 * │  │   └── getUsedSpace()                                        │
 * │  │                                                              │
 * │  ├── 5. AUTHENTICATION                                         │
 * │  │   ├── checkAuth()                                           │
 * │  │   ├── sendAuthRequired()                                    │
 * │  │   └── generateToken()                                       │
 * │  │                                                              │
 * │  ├── 6. HTML HANDLERS (Web pages)                              │
 * │  │   ├── handleLogin()                                         │
 * │  │   ├── handleRoot()                                          │
 * │  │   ├── handleUpload()                                        │
 * │  │   ├── handleDownload()                                      │
 * │  │   ├── handleView()                                          │
 * │  │   ├── handleMkdir()                                         │
 * │  │   ├── handleDelete()                                        │
 * │  │   └── handleInfo()                                          │
 * │  │                                                              │
 * │  ├── 7. SD INITIALIZATION                                      │
 * │  │   └── initSDCard()                                          │
 * │  │                                                              │
 * │  ├── 8. SETUP()                                                │
 * │  │   ├── Serial.begin()                                        │
 * │  │   ├── SPI.begin()                                           │
 * │  │   ├── initSDCard()                                          │
 * │  │   ├── WiFi.softAP()                                         │
 * │  │   ├── server.on() routing                                   │
 * │  │   └── server.begin()                                        │
 * │  │                                                              │
 * │  └── 9. LOOP()                                                 │
 * │      ├── server.handleClient()                                 │
 * │      ├── Memory monitoring                                     │
 * │      └── yield()                                               │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * 
 * EXECUTION FLOW:
 * 
 * ┌──────────────┐
 * │    setup()   │ ─────────────────────────────────────────────┐
 * └──────────────┘                                              │
 *        │                                                      │
 *        ├── Serial.begin(115200)                               │
 *        │   Initialize UART for debugging                      │
 *        │                                                      │
 *        ├── SPI.begin()                                        │
 *        │   Initialize SPI for SD card                         │
 *        │                                                      │
 *        ├── SD.begin(CS_PIN)                                   │
 *        │   Mount file system                                  │
 *        │                                                      │
 *        ├── WiFi.softAP(ssid, password)                        │
 *        │   Start access point                                 │
 *        │                                                      │
 *        ├── server.on("/", handleRoot)                         │
 *        │   Register URL handlers                              │
 *        │                                                      │
 *        └── server.begin()                                     │
 *            Start HTTP server                                  │
 *                                                               │
 *                                                               ▼
 * ┌──────────────┐     ┌─────────────────────────────────────────┐
 * │    loop()    │ ──► │  while(true) {                          │
 * └──────────────┘     │    server.handleClient(); // HTTP reqs  │
 *        │             │    yield();              // Watchdog    │
 *        └─────────────│  }                                      │
 *                      └─────────────────────────────────────────┘
 * 
 */


/*
 * 3.2. MEMORY OPTIMIZATIONS
 * ------------------------
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │              Optimization techniques in the firmware            │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  1. PROGMEM FOR HTML/CSS                                        │
 * │     ─────────────────────────────────────────────────────────  │
 * │                                                                 │
 * │     // HTML stored in Flash, not RAM                           │
 * │     const char CSS_STYLES[] PROGMEM = R"=====                  │
 * │       body { background: #0a0a0a; }                            │
 * │       ... thousands of characters ...                          │
 * │     )=====";                                                   │
 * │                                                                 │
 * │     // Reading via FPSTR():                                    │
 * │     html += FPSTR(CSS_STYLES);  // Does not copy to RAM!       │
 * │                                                                 │
 * │     // Savings: ~5 KB CSS does not occupy RAM                  │
 * │                                                                 │
 * │                                                                 │
 * │  2. F() MACRO FOR STRINGS                                       │
 * │     ─────────────────────────────────────────────────────────  │
 * │                                                                 │
 * │     // Instead of:                                             │
 * │     Serial.println("Error: SD card not found");  // In RAM    │
 * │                                                                 │
 * │     // Use:                                                    │
 * │     Serial.println(F("Error: SD card not found")); // Flash   │
 * │                                                                 │
 * │     // Savings: ~30 bytes per string                           │
 * │                                                                 │
 * │                                                                 │
 * │  3. STRING RESERVE()                                            │
 * │     ─────────────────────────────────────────────────────────  │
 * │                                                                 │
 * │     // When String is unavoidable:                             │
 * │     String html;                                                │
 * │     html.reserve(4096);  // Allocate memory once               │
 * │                                                                 │
 * │     // Instead of multiple reallocations                       │
 * │                                                                 │
 * │                                                                 │
 * │  4. AVOID GLOBAL BUFFERS                                        │
 * │     ─────────────────────────────────────────────────────────  │
 * │                                                                 │
 * │     // BAD:                                                    │
 * │     char globalBuffer[8192];  // 8 KB occupied forever         │
 * │                                                                 │
 * │     // GOOD:                                                   │
 * │     void processFile() {                                       │
 * │       char localBuffer[512];  // On stack, will be freed       │
 * │     }                                                          │
 * │                                                                 │
 * │                                                                 │
 * │  5. STREAMING INSTEAD OF BUFFERING                              │
 * │     ─────────────────────────────────────────────────────────  │
 * │                                                                 │
 * │     // BAD: Load entire file into memory                       │
 * │     String content = file.readString();                        │
 * │     server.send(200, "text/html", content);                    │
 * │                                                                 │
 * │     // GOOD: Streamed transfer                                 │
 * │     server.streamFile(file, "text/html");                      │
 * │     // Reads and sends in ~1460 byte chunks                    │
 * │                                                                 │
 * │                                                                 │
 * │  6. SERIALPRINTF_P FOR FORMATTING                               │
 * │     ─────────────────────────────────────────────────────────  │
 * │                                                                 │
 * │     // Format strings in Flash                                 │
 * │     Serial.printf_P(PSTR("[UPLOAD] %s: %u bytes\n"),          │
 * │                      filename, size);                          │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * 
 * MEMORY CONSUMPTION COMPARISON:
 * 
 * ┌──────────────────────────┬────────────────┬────────────────┐
 * │        Approach          │     RAM        │     Flash      │
 * ├──────────────────────────┼────────────────┼────────────────┤
 * │  HTML in String          │  ~15 KB        │  -             │
 * │  HTML in PROGMEM         │  ~100 B        │  ~15 KB        │
 * ├──────────────────────────┼────────────────┼────────────────┤
 * │  Serial.print("text")    │  len(text)     │  -             │
 * │  Serial.print(F("text")) │  ~0            │  len(text)     │
 * ├──────────────────────────┼────────────────┼────────────────┤
 * │  file.readString()       │  file.size     │  -             │
 * │  streamFile()            │  ~1.5 KB       │  -             │
 * └──────────────────────────┴────────────────┴────────────────┘
 * 
 */


/*
 * 3.3. WEB SERVER MECHANISM
 * -------------------------
 * 
 * ESP8266WebServer works based on callback functions.
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                  HTTP Request Processing                        │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  1. Client sends HTTP request                                  │
 * │     GET /?path=/documents HTTP/1.1                             │
 * │                                                                 │
 * │  2. ESP8266WebServer accepts connection                        │
 * │     server.handleClient() in loop()                            │
 * │                                                                 │
 * │  3. Request parsing                                            │
 * │     - Method: GET, POST, etc.                                  │
 * │     - URL: /?path=/documents                                   │
 * │     - Arguments: path=/documents                               │
 * │     - Headers: Cookie, Authorization                           │
 * │                                                                 │
 * │  4. Handler lookup (Routing)                                   │
 * │     server.on("/", handleRoot);  // Registered route           │
 * │                                                                 │
 * │  5. Callback function invocation                               │
 * │     handleRoot() generates response                            │
 * │                                                                 │
 * │  6. Response sending                                           │
 * │     server.send(200, "text/html", html);                       │
 * │     or server.streamFile(file, mimeType);                      │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * 
 * ROUTING:
 * 
 * // Simple routes
 * server.on("/", handleRoot);                    // GET /
 * server.on("/login", HTTP_GET, handleLogin);    // GET /login
 * server.on("/login", HTTP_POST, handleLoginPost); // POST /login
 * 
 * // Route with parameters
 * // URL: /download?path=/file.txt
 * server.on("/download", handleDownload);
 * // In handler: server.arg("path") → "/file.txt"
 * 
 * // File upload
 * server.on("/upload", HTTP_POST, 
 *           handleUploadDone,    // Called at the end
 *           handleUploadPost);   // Called for each chunk
 * 
 * // 404 handler
 * server.onNotFound(handleNotFound);
 * 
 * 
 * MULTIPART FILE UPLOAD:
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                Multipart Upload Flow                            │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Client sends:                                                 │
 * │  POST /upload HTTP/1.1                                         │
 * │  Content-Type: multipart/form-data; boundary=----WebKitForm    │
 * │                                                                 │
 * │  ------WebKitForm                                              │
 * │  Content-Disposition: form-data; name="file"; filename="a.jpg" │
 * │  Content-Type: image/jpeg                                      │
 * │                                                                 │
 * │  <binary file data>                                            │
 * │  ------WebKitForm--                                            │
 * │                                                                 │
 * │  ESP8266WebServer parses and calls:                            │
 * │                                                                 │
 * │  1. UPLOAD_FILE_START                                          │
 * │     upload.filename = "a.jpg"                                  │
 * │     upload.totalSize = 0                                       │
 * │     → Open file on SD                                          │
 * │                                                                 │
 * │  2. UPLOAD_FILE_WRITE (many times)                             │
 * │     upload.buf[] = data (up to 1460 bytes)                     │
 * │     upload.currentSize = size of current chunk                 │
 * │     → Write to SD                                              │
 * │                                                                 │
 * │  3. UPLOAD_FILE_END                                            │
 * │     upload.totalSize = total size                              │
 * │     → Close file, show success                                 │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * 
 * EXAMPLE UPLOAD HANDLER:
 * 
 * File uploadFile;
 * 
 * void handleUploadPost() {
 *   HTTPUpload& upload = server.upload();
 *   
 *   if (upload.status == UPLOAD_FILE_START) {
 *     String filename = upload.filename;
 *     uploadFile = SD.open("/" + filename, FILE_WRITE);
 *     
 *   } else if (upload.status == UPLOAD_FILE_WRITE) {
 *     // Write chunk to SD
 *     if (uploadFile) {
 *       uploadFile.write(upload.buf, upload.currentSize);
 *     }
 *     
 *   } else if (upload.status == UPLOAD_FILE_END) {
 *     if (uploadFile) {
 *       uploadFile.close();
 *     }
 *   }
 * }
 * 
 */




// ============================================================================
// SECTION 4: DATA TRANSFER TECHNOLOGIES
// ============================================================================

/*
 * 4.1. CHUNKED TRANSFER ENCODING
 * -------------------------------
 * 
 * HTTP/1.1 supports transferring data in parts without knowing the
 * total size in advance.
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                 Chunked Transfer Example                        │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  HTTP/1.1 200 OK                                                │
 * │  Transfer-Encoding: chunked                                    │
 * │  Content-Type: text/html                                       │
 * │                                                                 │
 * │  1a\r\n           (26 bytes in hex)                            │
 * │  <html><body>Hello\r\n                                         │
 * │  10\r\n           (16 bytes)                                   │
 * │  World!</body></html>\r\n                                      │
 * │  0\r\n            (end)                                        │
 * │  \r\n                                                           │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * In ESP8266WebServer this is done automatically via streamFile():
 * 
 * File file = SD.open("/large_file.zip");
 * server.streamFile(file, "application/zip");
 * // Automatically uses chunked encoding
 * 
 * 
 * WHY THIS IS IMPORTANT FOR ESP8266:
 * 
 * 1. No need to know file size in advance
 * 2. No need to load the entire file into RAM
 * 3. Send as we read from SD
 * 4. Works with files of ANY size
 * 
 * 
 * FLOW DIAGRAM:
 * 
 * ┌───────────┐     read()      ┌───────────┐     send()     ┌───────────┐
 * │ SD card   │ ──────────────► │  ESP8266  │ ─────────────► │  Client   │
 * │           │   512-4096B     │  Buffer   │    1460B       │  Browser  │
 * │  File     │                 │  ~1.5KB   │    TCP packet  │           │
 * └───────────┘                 └───────────┘                └───────────┘
 *      ▲                             │
 *      │                             │
 *      └─────────────────────────────┘
 *         Loop: read → send → repeat
 * 
 */


/*
 * 4.2. STREAMING FILE TRANSFER
 * -----------------------------
 * 
 * Key optimization for working with large files.
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │              Buffered vs Streaming                              │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  BUFFERED (BAD for large files):                               │
 * │                                                                 │
 * │  ┌─────────────────────────────────────────────────────┐       │
 * │  │  RAM                                                 │       │
 * │  │  ┌─────────────────────────────────────────────┐    │       │
 * │  │  │  Entire file loaded into memory              │    │       │
 * │  │  │  10 MB file = 10 MB RAM                      │    │       │
 * │  │  │  → OUT OF MEMORY!                            │    │       │
 * │  │  └─────────────────────────────────────────────┘    │       │
 * │  └─────────────────────────────────────────────────────┘       │
 * │                                                                 │
 * │  STREAMING (GOOD):                                             │
 * │                                                                 │
 * │  ┌─────────────────────────────────────────────────────┐       │
 * │  │  RAM                                                 │       │
 * │  │  ┌──────────────────────────────┐                   │       │
 * │  │  │  Small buffer ~1.5 KB        │                   │       │
 * │  │  │  Read → Send → Read          │                   │       │
 * │  │  │  → Works with files of any size                  │       │
 * │  │  └──────────────────────────────┘                   │       │
 * │  └─────────────────────────────────────────────────────┘       │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * 
 * IMPLEMENTATION:
 * 
 * void handleDownload() {
 *   File file = SD.open(path, FILE_READ);
 *   
 *   // Option 1: Automatic streaming
 *   server.streamFile(file, getMimeType(path));
 *   
 *   // Option 2: Manual streaming (more control)
 *   server.setContentLength(file.size());
 *   server.sendHeader("Content-Disposition", 
 *                     "attachment; filename=" + filename);
 *   server.send(200, getMimeType(path), "");
 *   
 *   uint8_t buffer[1460];
 *   while (file.available()) {
 *     size_t len = file.read(buffer, sizeof(buffer));
 *     server.client().write(buffer, len);
 *     yield(); // Important: let the WiFi stack work
 *   }
 *   
 *   file.close();
 * }
 * 
 * 
 * IMPORTANT: yield() IN LOOPS!
 * 
 * The ESP8266 WiFi stack requires regular attention.
 * If code is busy in a loop for more than 50 ms without yield(),
 * the WiFi connection will drop.
 * 
 * while (file.available()) {
 *   // ... read/write ...
 *   yield();  // Let WiFi stack process events
 * }
 * 
 */


/*
 * 4.3. MIME TYPES
 * ---------------
 * 
 * MIME (Multipurpose Internet Mail Extensions) - standard for
 * specifying content type.
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    Common MIME Types                            │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Text:                                                         │
 * │  ├── text/html           .html, .htm                           │
 * │  ├── text/css            .css                                  │
 * │  ├── text/javascript     .js                                   │
 * │  ├── text/plain          .txt, .log, .md                       │
 * │  ├── application/json    .json                                 │
 * │  └── application/xml     .xml                                  │
 * │                                                                 │
 * │  Images:                                                       │
 * │  ├── image/jpeg          .jpg, .jpeg                           │
 * │  ├── image/png           .png                                  │
 * │  ├── image/gif           .gif                                  │
 * │  ├── image/svg+xml       .svg                                  │
 * │  └── image/webp          .webp                                 │
 * │                                                                 │
 * │  Audio/Video:                                                  │
 * │  ├── audio/mpeg          .mp3                                  │
 * │  ├── audio/wav           .wav                                  │
 * │  ├── video/mp4           .mp4                                  │
 * │  └── video/webm          .webm                                 │
 * │                                                                 │
 * │  Documents:                                                    │
 * │  ├── application/pdf     .pdf                                  │
 * │  └── application/zip     .zip                                  │
 * │                                                                 │
 * │  Binary:                                                       │
 * │  └── application/octet-stream   (default)                      │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * 
 * getMimeType() FUNCTION:
 * 
 * String getMimeType(String path) {
 *   path.toLowerCase();
 *   
 *   if (path.endsWith(".html")) return F("text/html");
 *   if (path.endsWith(".css"))  return F("text/css");
 *   if (path.endsWith(".jpg"))  return F("image/jpeg");
 *   // ... and so on
 *   
 *   return F("application/octet-stream");  // Binary default
 * }
 * 
 * 
 * WHY THIS MATTERS:
 * 
 * 1. Browser knows how to display the file
 *    text/html → renders the page
 *    image/jpeg → shows the picture
 *    application/octet-stream → prompts to download
 * 
 * 2. Correct headers = correct behavior
 *    Content-Type: text/plain → display as text
 *    Content-Type: application/json → can be parsed as JSON
 * 
 */




// ============================================================================
// SECTION 5: SECURITY
// ============================================================================

/*
 * 5.1. AUTHENTICATION
 * -----------------
 * 
 * Simple HTTP Basic Auth + Session Cookie.
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    Auth Flow                                    │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  1. Unauthorized request                                       │
 * │     GET / HTTP/1.1                                             │
 * │     Cookie: (none)                                             │
 * │                                                                 │
 * │  2. Server redirects to /login                                 │
 * │     HTTP/1.1 302 Found                                         │
 * │     Location: /login                                           │
 * │                                                                 │
 * │  3. User enters login/password                                 │
 * │     POST /login HTTP/1.1                                       │
 * │     user=admin&pass=admin                                      │
 * │                                                                 │
 * │  4. Server validates and issues cookie                         │
 * │     HTTP/1.1 302 Found                                         │
 * │     Set-Cookie: session=a1b2c3d4e5f6...                        │
 * │     Location: /                                                │
 * │                                                                 │
 * │  5. Subsequent requests with cookie                            │
 * │     GET / HTTP/1.1                                             │
 * │     Cookie: session=a1b2c3d4e5f6...                            │
 * │                                                                 │
 * │  6. Server checks cookie and grants access                     │
 * │     HTTP/1.1 200 OK                                            │
 * │     ... content ...                                            │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * 
 * IMPLEMENTATION:
 * 
 * // Authentication check
 * bool checkAuth() {
 *   if (server.hasHeader("Cookie")) {
 *     String cookie = server.header("Cookie");
 *     if (cookie.indexOf("session=" + sessionToken) != -1) {
 *       return true;
 *     }
 *   }
 *   return false;
 * }
 * 
 * // Generate token
 * String generateToken() {
 *   String token = "";
 *   for (int i = 0; i < 16; i++) {
 *     token += String(random(16), HEX);
 *   }
 *   return token;
 * }
 * 
 * // Logout
 * void handleLogout() {
 *   sessionToken = "";
 *   server.sendHeader("Set-Cookie", "session=; Path=/; Max-Age=0");
 *   server.sendHeader("Location", "/login");
 *   server.send(302, "text/plain", "");
 * }
 * 
 * 
 * LIMITATIONS:
 * 
 * 1. Token stored in RAM - lost on reboot
 * 2. No HTTPS - password sent in plain text
 * 3. Single token for all users - only one active user
 * 
 * For production you would need:
 * - HTTPS (requires more memory)
 * - Token storage in EEPROM
 * - Token expiration
 * 
 */


/*
 * 5.2. FILE SYSTEM PROTECTION
 * -----------------------------
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                  Security Measures                              │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  1. PATH TRAVERSAL PROTECTION                                  │
 * │                                                                 │
 * │     Attack: GET /download?path=../../etc/passwd                │
 * │                                                                 │
 * │     Protection:                                                │
 * │     String path = server.arg("path");                          │
 * │     if (path.indexOf("..") >= 0) {                             │
 * │       path = "/";  // Safe default path                        │
 * │     }                                                          │
 * │                                                                 │
 * │                                                                 │
 * │  2. FORBID DELETION OF ROOT                                    │
 * │                                                                 │
 * │     if (path == "/") {                                          │
 * │       server.send(400, "text/plain", "Cannot delete root");    │
 * │       return;                                                   │
 * │     }                                                           │
 * │                                                                 │
 * │                                                                 │
 * │  3. CHECK EXISTENCE BEFORE ACTION                              │
 * │                                                                 │
 * │     if (!SD.exists(path)) {                                     │
 * │       server.send(404, "text/plain", "Not found");             │
 * │       return;                                                   │
 * │     }                                                           │
 * │                                                                 │
 * │                                                                 │
 * │  4. DELETE ONLY EMPTY FOLDERS                                  │
 * │                                                                 │
 * │     SD.rmdir(path);  // Deletes only empty folders             │
 * │                                                                 │
 * │                                                                 │
 * │  5. UPLOAD SIZE LIMIT                                          │
 * │                                                                 │
 * │     // In HTTPUpload:                                          │
 * │     if (upload.totalSize > MAX_FILE_SIZE) {                    │
 * │       // Abort upload                                          │
 * │     }                                                           │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 */




// ============================================================================
// SECTION 6: EXTENDING FUNCTIONALITY
// ============================================================================

/*
 * 6.1. ADDING NEW ROUTES
 * -----------------------------
 * 
 * TEMPLATE FOR A NEW HANDLER:
 * 
 * // 1. Function declaration
 * void handleMyFeature() {
 *   
 *   // 2. Get parameters
 *   String param = server.hasArg("param") ? server.arg("param") : "default";
 *   
 *   // 3. Check authentication (if needed)
 *   if (!checkAuth()) {
 *     sendAuthRequired();
 *     return;
 *   }
 *   
 *   // 4. Validation
 *   if (param.indexOf("..") >= 0) {
 *     server.send(400, "text/plain", "Invalid path");
 *     return;
 *   }
 *   
 *   // 5. Business logic
 *   String result = doSomething(param);
 *   
 *   // 6. Build response
 *   String html = F("<!DOCTYPE html>...");
 *   html += FPSTR(CSS_STYLES);
 *   // ...
 *   
 *   // 7. Send
 *   server.send(200, "text/html", html);
 * }
 * 
 * // 8. Register route in setup()
 * server.on("/myfeature", handleMyFeature);
 * 
 * 
 * EXAMPLE: Adding a JSON API for file listing
 * 
 * void handleApiList() {
 *   String path = server.hasArg("path") ? server.arg("path") : "/";
 *   
 *   // JSON response
 *   String json = F("[");
 *   
 *   File root = SD.open(path);
 *   File file = root.openNextFile();
 *   bool first = true;
 *   
 *   while (file) {
 *     if (!first) json += F(",");
 *     first = false;
 *     
 *     json += F("{\"name\":\"");
 *     json += file.name();
 *     json += F("\",\"type\":\"");
 *     json += file.isDirectory() ? "folder" : "file";
 *     json += F("\",\"size\":");
 *     json += String(file.size());
 *     json += F("}");
 *     
 *     file.close();
 *     file = root.openNextFile();
 *   }
 *   
 *   json += F("]");
 *   
 *   server.send(200, "application/json", json);
 * }
 * 
 * // In setup():
 * server.on("/api/list", handleApiList);
 * 
 */


/*
 * 6.2. ADDING NEW FILE TYPES
 * ----------------------------------
 * 
 * 1. ADD MIME TYPE:
 * 
 * String getMimeType(String path) {
 *   path.toLowerCase();
 *   
 *   // Add new types:
 *   if (path.endsWith(".woff2")) return F("font/woff2");
 *   if (path.endsWith(".webp")) return F("image/webp");
 *   if (path.endsWith(".mp4"))  return F("video/mp4");
 *   if (path.endsWith(".pdf"))  return F("application/pdf");
 *   // ...
 * }
 * 
 * 
 * 2. ADD ICON/HANDLING:
 * 
 * String getFileIcon(String name) {
 *   name.toLowerCase();
 *   
 *   if (name.endsWith(".pdf")) return F("📄");
 *   if (name.endsWith(".mp3")) return F("🎵");
 *   if (name.endsWith(".mp4")) return F("🎬");
 *   // ...
 * }
 * 
 * 
 * 3. ADD PREVIEW SUPPORT:
 * 
 * bool canPreview(String path) {
 *   path.toLowerCase();
 *   
 *   // Already have: .txt, .jpg, .png, .gif
 *   // Add:
 *   if (path.endsWith(".pdf"))  return true;  // Embedded PDF viewer
 *   if (path.endsWith(".mp4"))  return true;  // HTML5 video
 *   if (path.endsWith(".mp3"))  return true;  // HTML5 audio
 *   
 *   return false;
 * }
 * 
 * 
 * 4. HTML FOR PREVIEW:
 * 
 * // In handleView():
 * if (path.endsWith(".mp4")) {
 *   html += F("<video controls style='max-width:100%'>");
 *   html += F("<source src='/download?path=");
 *   html += path;
 *   html += F("' type='video/mp4'></video>");
 * }
 * 
 * if (path.endsWith(".mp3")) {
 *   html += F("<audio controls style='width:100%'>");
 *   html += F("<source src='/download?path=");
 *   html += path;
 *   html += F("' type='audio/mpeg'></audio>");
 * }
 * 
 */


/*
 * 6.3. INTEGRATION WITH EXTERNAL SERVICES
 * -------------------------------------
 * 
 * ESP8266 can work in STA (client) and AP (access point) mode SIMULTANEOUSLY.
 * 
 * WiFi.mode(WIFI_AP_STA);  // Combined mode
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │              WiFi AP_STA Mode                                   │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │   ┌─────────────┐         ┌─────────────┐                      │
 * │   │  Smartphone │◄───────►│             │                      │
 * │   │  (AP mode)  │  192.   │             │                      │
 * │   └─────────────┘  168.4.x│   ESP8266   │                      │
 * │                            │             │◄──────► Internet     │
 * │   ┌─────────────┐         │             │        (STA mode)    │
 * │   │   Router    │◄───────►│             │                      │
 * │   │  (STA mode) │  DHCP   │             │                      │
 * │   └─────────────┘         └─────────────┘                      │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * 
 * EXAMPLE: Syncing with the cloud
 * 
 * #include <ESP8266HTTPClient.h>
 * 
 * void syncToCloud() {
 *   // Switch to STA mode
 *   WiFi.mode(WIFI_STA);
 *   WiFi.begin("YourWiFi", "password");
 *   
 *   while (WiFi.status() != WL_CONNECTED) {
 *     delay(500);
 *   }
 *   
 *   // Send file
 *   HTTPClient http;
 *   http.begin("https://api.example.com/upload");
 *   http.addHeader("Content-Type", "application/octet-stream");
 *   
 *   File file = SD.open("/data.txt", "r");
 *   
 *   // Streaming upload
 *   int httpCode = http.sendRequest("POST", &file, file.size());
 *   
 *   if (httpCode > 0) {
 *     Serial.printf("[SYNC] Response: %d\n", httpCode);
 *   }
 *   
 *   file.close();
 *   http.end();
 *   
 *   // Return to AP mode
 *   WiFi.mode(WIFI_AP);
 * }
 * 
 */




// ============================================================================
// SECTION 7: DEBUG AND MONITORING
// ============================================================================

/*
 * 7.1. SERIAL DEBUG
 * ------------------
 * 
 * The serial port is the main debugging tool.
 * 
 * // Initialization
 * Serial.begin(115200);
 * 
 * // Simple messages
 * Serial.println(F("System started"));
 * 
 * // Formatted output
 * Serial.printf_P(PSTR("[UPLOAD] %s: %u bytes\n"), filename, size);
 * 
 * // Memory debugging
 * Serial.printf_P(PSTR("[MEM] Free: %d bytes\n"), ESP.getFreeHeap());
 * 
 * 
 * LOGGING LEVELS:
 * 
 * #define DEBUG_LEVEL 2  // 0=none, 1=error, 2=info, 3=debug
 * 
 * #if DEBUG_LEVEL >= 1
 *   #define LOG_ERROR(msg) Serial.println(F("[ERR] " msg))
 * #else
 *   #define LOG_ERROR(msg)
 * #endif
 * 
 * #if DEBUG_LEVEL >= 2
 *   #define LOG_INFO(msg) Serial.println(F("[INF] " msg))
 * #else
 *   #define LOG_INFO(msg)
 * #endif
 * 
 * #if DEBUG_LEVEL >= 3
 *   #define LOG_DEBUG(msg) Serial.println(F("[DBG] " msg))
 * #else
 *   #define LOG_DEBUG(msg)
 * #endif
 * 
 * 
 * USAGE:
 * 
 * LOG_ERROR("SD card not found");
 * LOG_INFO("Client connected");
 * LOG_DEBUG("Processing request");
 * 
 */


/*
 * 7.2. MEMORY MONITORING
 * -----------------------
 * 
 * Critically important for ESP8266!
 * 
 * // Get free heap
 * uint32_t freeHeap = ESP.getFreeHeap();
 * 
 * // Monitor in loop()
 * void loop() {
 *   server.handleClient();
 *   
 *   static unsigned long lastCheck = 0;
 *   if (millis() - lastCheck > 60000) {  // Every minute
 *     lastCheck = millis();
 *     
 *     uint32_t free = ESP.getFreeHeap();
 *     Serial.printf_P(PSTR("[MEM] Free: %u bytes\n"), free);
 *     
 *     // Low memory warning
 *     if (free < 10000) {
 *       Serial.println(F("[MEM] WARNING: Low memory!"));
 *     }
 *   }
 * }
 * 
 * 
 * LEAK DETECTION:
 * 
 * uint32_t lastHeap = 0;
 * 
 * void checkMemoryLeak() {
 *   uint32_t current = ESP.getFreeHeap();
 *   
 *   if (lastHeap > 0 && current < lastHeap - 100) {
 *     Serial.printf_P(PSTR("[MEM] Leak detected! Was: %u, Now: %u\n"), 
 *                     lastHeap, current);
 *   }
 *   
 *   lastHeap = current;
 * }
 * 
 * 
 * CAUSES OF LEAKS:
 * 
 * 1. String concatenation
 *    String s = "a";
 *    s += "b";  // Allocates new memory, old not freed immediately
 * 
 * 2. Unclosed File
 *    File f = SD.open("file");
 *    // Forgot f.close() → leak!
 * 
 * 3. Unreleased WiFiClient
 *    WiFiClient client = server.client();
 *    // Forgot client.stop()
 * 
 */




// ============================================================================
// APPENDIX A: CONFIGURATION
// ============================================================================

/*
 * FIRMWARE SETTINGS
 * 
 * Change these values to suit your needs:
 */

// WiFi AP Settings
#define AP_SSID "VS_Drop_Cloud"      // WiFi network name
#define AP_PASSWORD "admin123"        // Password (minimum 8 characters)

// Authentication
#define AUTH_USER "admin"             // Login
#define AUTH_PASS "admin"             // Password

// SD Card
#define SD_CS_PIN 4                   // GPIO4 (D2) - Chip Select

// Transfer buffer size
#define CHUNK_SIZE 8192               // 8 KB - optimal for ESP8266

// Debug level (0=none, 1=error, 2=info, 3=debug)
#define DEBUG_LEVEL 2

// Maximum upload file size (bytes)
#define MAX_UPLOAD_SIZE (4 * 1024 * 1024)  // 4 MB

// Memory check interval (ms)
#define MEMORY_CHECK_INTERVAL 60000   // 1 minute




// ============================================================================
// APPENDIX B: VERSION HISTORY
// ============================================================================

/*
 * CHANGELOG:
 * 
 * v2.0 (2024)
 * ─────────────────────────────────────────────────
 * [+] Completely redesigned UI VS_Drop.cloud
 * [+] Black/white theme with orange accents
 * [+] Memory optimization (PROGMEM for HTML/CSS)
 * [+] Chunked transfer encoding
 * [+] Streaming file download
 * [+] Upload progress bar
 * [+] SVG icons (stored in Flash)
 * [+] Session-based authentication
 * [+] Path traversal protection
 * [+] Info page
 * [+] Memory monitoring
 * [+] Detailed documentation
 * 
 * [!] Fixed: collectHeaders API for new ESP8266 versions
 * [!] Fixed: memory leaks during file upload
 * [!] Fixed: freezing with large files
 * 
 * v1.0 (Initial)
 * ─────────────────────────────────────────────────
 * [+] Basic web server
 * [+] File browsing
 * [+] Upload/download
 * [+] Basic Auth
 */




// ============================================================================
// APPENDIX C: KNOWN LIMITATIONS
// ============================================================================

/*
 * KNOWN ESP8266 LIMITATIONS:
 * 
 * 1. MEMORY
 *    - Maximum ~40-50 KB free RAM
 *    - HTML pages must be stored in Flash
 *    - Cannot load entire file into memory
 * 
 * 2. WIFI
 *    - Only 2.4 GHz
 *    - Maximum 4-8 simultaneous clients
 *    - No 5 GHz
 * 
 * 3. FILE SYSTEM
 *    - Only FAT32
 *    - Long filenames may be truncated
 *    - No Linux-style permissions
 * 
 * 4. PERFORMANCE
 *    - Transfer speed: 200-400 KB/s
 *    - Single-threaded request processing
 *    - No parallel connections
 * 
 * 5. SECURITY
 *    - No hardware HTTPS
 *    - Passwords sent in plain text
 *    - No data encryption
 * 
 * 
 * RECOMMENDATIONS TO OVERCOME:
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ Problem           │ Solution                                   │
 * ├─────────────────────────────────────────────────────────────────┤
 * │ Low memory        │ PROGMEM, streaming, F() macro              │
 * │ Slow transfer     │ Chunked encoding, parallel processing      │
 * │ No HTTPS          │ Use VPN or tunnel                          │
 * │ Few clients       │ ESP32 (up to 10 clients)                   │
 * │ FAT32 only        │ Use LittleFS for internal Flash            │
 * └─────────────────────────────────────────────────────────────────┘
 */




// ============================================================================
// END OF DOCUMENTATION
// ============================================================================

/*
 * 
 * ╔═══════════════════════════════════════════════════════════════════╗
 * ║                                                                   ║
 * ║   VS_Drop.cloud - ESP8266 SD Card File Server                    ║
 * ║                                                                   ║
 * ║   Documentation: v2.0                                             ║
 * ║   Firmware:   VS_Drop_Cloud_ESP8266.ino                          ║
 * ║                                                                   ║
 * ║   For questions and suggestions:                                 ║
 * ║   - Create an Issue on GitHub                                    ║
 * ║   - Refer to ESP8266 documentation                               ║
 * ║                                                                   ║
 * ║   Good luck with your project!                                   ║
 * ║                                                                   ║
 * ╚═══════════════════════════════════════════════════════════════════╝
 * 
 */
