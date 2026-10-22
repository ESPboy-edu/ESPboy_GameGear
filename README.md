# **ESPboy Sega Master System & Game Gear Emulator**

A high-performance, deeply optimized Sega Master System (SMS) and Game Gear (GG) emulator ported to the **ESPboy** handheld platform (ESP8266 @ 160 MHz, \~80 KB RAM, 128×128 color TFT LCD).  
This project pushes the resource-constrained ESP8266 microchip to its absolute limits, delivering smooth gameplay and crisp audio through cycle-accurate assembly-level tweaks, cache prefetching, branchless algorithms, and hardware-accelerated SPI and Sigma-Delta pipelines.

## **Key Features**

* **High Performance**: Stable 30–60 FPS emulation powered by an optimized Z80 core and hardware-timed VDP scanline engine.  
* **Dual Display Modes (On-the-Fly Toggle)**:  
  * **Fullscreen Scaled (160×144 → 128×128)**: Utilizes precomputed hardware lookup tables (LUTs) combined with an ultra-fast, two-cycle pseudo-anti-aliasing horizontal blending filter (0xDEF6 bitmask for Little-Endian SPI) to eliminate scrolling shimmer without dropping frames.  
  * **Pixel-Perfect Crop (128×128 Centered)**: 1:1 original pixel aspect ratio cropped to the screen center for maximum sharpness.  
* **Direct Hardware Sigma-Delta Audio**: Clean SN76489 PSG audio generation at 11,025 Hz using direct 32-bit hardware register writes (GPSD) in IRAM timer interrupts, bypassing framework overhead and audio jitter.  
* **Non-Blocking SPI Screen Refresh**: Fully asynchronous background SPI transfer (nbSPI) using double-buffered ping-pong chunks.  
* **Strict Heap & IRAM Architecture**: Zero dynamic fragmentation, leaving \~45+ KB of free heap at cold boot via unified pointer-based runtime allocations.  
* **Robust Input Handling**: Full mapping for Game Gear controls with state-machine debounce and NMI/Start isolation on side buttons.

## **Controls**

| ESPboy Button | Game Gear Action | Master System Action |
| :---- | :---- | :---- |
| **D-Pad (Up, Down, Left, Right)** | D-Pad Direction | D-Pad Direction |
| **ACT** | Button 1 / Start | Button 1 |
| **ESC** | Button 2 | Button 2 |
| **RGT** | Start | — |
| **LFT** | — (Protected against NMI crashes) | Pause |
| **LFT \+ RGT** | **Toggle Screen Mode** (Scale AA $\leftrightarrow$ 1:1 Crop) | **Toggle Screen Mode** |

## **Technical Highlights & Optimizations**

### **1\. Z80 CPU Core Acceleration**

* **32-Bit Instruction Prefetching**: Eliminates repetitive bus access by caching aligned 32-bit program blocks directly from Flash.  
* **Direct RAM Timing Tables**: Relocated Z80 opcode cycle arrays (cc\[6\]\[256\]) and flag tables (SZ, SZ\_BIT, SZP) into direct-access DRAM, removing slow pgm\_read\_byte macros and saving \~50,000 CPU cycles per frame.

### **2\. High-Speed Video Rendering (VDP)**

* **Branchless Sprite Merging**: Replaced conditional per-pixel transparency and sprite priority branching with an ultra-fast 4 KB RAM lookup table (merge\_lut\[256\]\[16\]).  
* **Hardware-Optimized RGB565 Blending**: Fast 50/50 color blending macro tuned for little-endian SPI byte order:  
  C  
  \#**define** BLEND\_50(c1, c2) (((c1 & c2) \+ (((c1 ^ c2) & 0xDEF6) \>\> 1)))

* **Asynchronous Double-Buffered Chunks**: Background transfer via nbSPI writes 8-line chunks while the CPU concurrently renders the next scanlines into the alternating buffer.

### **3\. Audio Subsystem**

* Hardware timer1 interrupt fetches samples from a ring buffer and writes directly to the Sigma-Delta DAC pin at ultrasonic carrier frequencies (312.5 kHz), preventing audible carrier hum and CPU starvation.

### **4\. Memory Footprint Management**

* **IRAM Conservation**: Non-critical audio mix math and tile decoders are compiled into Flash cache, reserving the scarce 32 KB IRAM strictly for interrupt handlers (audio\_timer\_isr, nbSPI\_ISR).  
* **Dynamic Heap Strategy**: Large buffers (vdp.vram, sms.wram, SPI chunks, merge\_lut) are allocated dynamically in setup(), guaranteeing over 45 KB of contiguous free RAM for device initialization and system tasks.

## **Hardware Requirements**

* **ESPboy Handheld Console** (or compatible ESP8266 development board).  
* **Display**: ST7735 128×128 SPI TFT LCD.  
* **I/O Expander**: MCP23017 (connected via I2C for keypad inputs).  
* **Audio**: Speaker or piezo buzzer connected to GPIO pin D3 (Sigma-Delta channel 0).

## **Building and Flashing**

> 1. Install the latest **Arduino IDE** or **PlatformIO**.  
> 2. Install the **ESP8266 Board Support Package** (v3.0.2 or higher recommended).  
> 3. Board Settings:  
   * **Board**: Generic ESP8266 Module (or ESPboy)  
   * **CPU Frequency**: 160 MHz (Critical for full performance)  
   * **Flash Size**: 4MB (FS:2MB / OTA:\~1019KB)  
   * **Flash Frequency**: 80 MHz  
   * **Upload Speed**: 921600  
> 4. Add your preferred ROM by including it as a C array header file (rom.h) in ESPboy\_GameGear.ino.  
> 5. Compile and upload to your ESPboy.

## **License & Credits**

* Based on the portable SMS Plus / SMS Plus GG engine by Charles Mac Donald and Eke-Eke.  
* Z80 emulation core by Juergen Buchmueller.  
* Ported, optimized, and maintained for **ESPboy** by the ESPboy community.
