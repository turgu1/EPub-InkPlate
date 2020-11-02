# EPub-InkPlate An EPub Reader for the InkPlate-6 device

Work in progress...

Translation of basic InkPlate-6 device driver classes is complete. Look at the EPub-Linux project for the current application being ported to this project.

The driver classes are:

- EInk: The e-ink display panel class. (Arduino Inkplate equivalent)
- MCP - MCP23017 16 bits IO expander class. (Arduino Mcp)
- ESP - Some methods similar to Arduino supplied methods, but in an ESP-IDF context
- Wire - Similar to Arduino Wire, but built using i2c functions
- Inkplate6Ctrl - Will be a low-level class to control various modes of usage
  (like deep sleep, light sleep, sd card mounting, etc.)

There has been copied from the Arduino implementation and have been modified extensively to be in line with the rest of the EPub-Linux code. There is still some refactoring to do. It's now time to migrate the EPub-Linux to this project.

This is now working in a PlatformIO/Espressif32 development framework. PlatformIO offers the ESP-IDF but with some easier control of configuration (Sort of as there is still some configuration that must be done using the ESP-IDF menuconfig). This will allow for using the same source code for both InkPlate6 and Linux versions of the application.

