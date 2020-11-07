# EPub-InkPlate An EPub Reader for the InkPlate-6 device

(Updated 2020.11.06)

Work in progress...

Translation of basic InkPlate-6 device driver classes is complete. The integration of the Linux version of the EPub application is complete. Debugging is ongoing.

The driver classes are:

- EInk: The e-ink display panel class. (Arduino Inkplate equivalent)
- MCP - MCP23017 16 bits IO expander class. (Arduino Mcp)
- ESP - Some methods similar to Arduino supplied methods, but in an ESP-IDF context
- Wire - Similar to Arduino Wire, but built using i2c functions
- Inkplate6Ctrl - Will be a low-level class to control various modes of usage
  (like deep sleep, light sleep, sd card mounting, etc.)

The hardware drivers have been copied from the Arduino implementation and have been modified extensively to be in line with the rest of the EPub-Linux code. There is still some refactoring to do. The source code can be found in folder lib_esp32/InkPlate6/src.

All the code has been migrated and debugging is ongoing.

This is now done in a PlatformIO/Espressif32 development framework. PlatformIO offers the ESP-IDF but with some easier control of configuration (Sort of as there is still some configuration that must be done using the ESP-IDF menuconfig). This will allow for using the same source code for both InkPlate6 and Linux versions of the application.

Parameters that must be setup with menuconfig include PSRAM allocation choice and sd_card long filename support. Details to be explained later.

Memory availability is low. Some modifications maybe required to optimize memory usage (memory pools instead of fine grained malloc/new).