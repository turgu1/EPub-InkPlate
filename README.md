# EPub-InkPlate An EPub Reader for the InkPlate-6 device

Work in progress...

Translation of basic InkPlate-6 device driver classes ongoing. Look at the EPub-Linux project for the current application being ported to the this project.

The driver classes are:

- InkPlate6: The overall InkPlate-6 device class. (Arduino System class equivalent)
- EInk: The e-ink display panel class. (Arduino Inkplate equivalent)
- MCP - MCP23017 16 bits IO expander class. (Arduino Mcp)

These has been copied from the Arduino implementation and are modified extensively to be inline with the rest of the
EPub-Linux code. A lot remains to be done.

