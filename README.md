# EPub-InkPlate An EPub Reader for the InkPlate-6 device

(Updated 2020.11.12)

Work in progress... the application is not ready yet. This readme contains information that could be innacurate.

The application is now using 1Bit planes (dithering on images, FreeType library for TrueType rasteriser). Currents results are promising. Still tests ongoing...

Some steps remain to be done:

- Integration of touch buttons
- Options / Parameters menus
- Error dialogs
- Power management (Deep sleep after timeout, Light sleep between page refresh)
- Current book location between restarts
- Performance on new book scans

----

Translation of basic InkPlate-6 device driver classes is complete. The integration of the Linux version of the EPub application is complete. Debugging is ongoing.

The driver classes are:

- EInk: The e-ink display panel class. (Arduino Inkplate equivalent)
- MCP - MCP23017 16 bits IO expander class. (Arduino Mcp)
- ESP - Some methods similar to Arduino supplied methods, but in an ESP-IDF context
- Wire - Similar to Arduino Wire, but built using i2c functions
- Inkplate6Ctrl - Will be a low-level class to control various modes of usage
  (like deep sleep, light sleep, sd card mounting, etc.)

The hardware drivers have been copied from the Arduino implementation and have been modified extensively to be in line with the rest of the EPub-InkPlate code. There is still some refactoring to do. The source code can be found in folder lib_esp32/InkPlate6/src.

All the code has been migrated and debugging is ongoing.

This is now done in a PlatformIO/Espressif32 development framework. PlatformIO offers the ESP-IDF but with some easier control of configuration (Sort of as there is still some configuration that must be done using the ESP-IDF menuconfig). This will allow for using the same source code for both InkPlate6 and Linux versions of the application.

Parameters that must be setup with menuconfig include PSRAM allocation choice and sd_card long filename support. Details to be explained later.

Memory availability is low. Some modifications maybe required to optimize memory usage (memory pools instead of fine grained malloc/new).

Some pictures from the InkPlate-6 version:

<img src="doc/pictures/IMG_1377.JPG" alt="picture" width="300"/><img src="doc/pictures/IMG_1378.JPG" alt="picture" width="300"/>

Some pictures from the Linux version:

<img src="doc/pictures/books_select.png" alt="drawing" width="300"/><img src="doc/pictures/book_page.png" alt="drawing" width="300"/>

## Characteristics

The first release is expected to have basic functionalities (maybe less, depending on memory usage on the ESP32):

- Some pre-defined fonts (TBD) (selectable by the user as a font base to display the document)
- TTF and OTF embedded fonts support
- Normal, Bold, Italic, Bold+Italic face types
- Bitmap images dithering display (JPEG, PNG, GIF, BMP)
- EPUB (V2, V3) ebook format subset
- UTF-8 characters
- Page progression direction: Left then Right
- Portrait display
- 3 or 6 button Keypad (3 buttons for the InkPlate-6 tactile keys, 6 for an EasyC interface keypad)
- left, center, right, justify text alignments
- indentation
- Some basic parameters and options (font size, line space, margins...)
- Books list in title alphabetical order
- Limited CSS formatting

Some elements to consider in the future (no specific order of priority):

- Various views for the Books list
- Table of content
- Hyperlinks (inside a book)
- WiFi-based documents download (Dropbox, Calibre, others)
- Multiple fonts choices selectable by the user
- More CSS formatting
- Footnote management
- Kerning
- TXT, MOBI book formats
- `<table>` formatting
- Page progression direction: Right then left
- Notes
- Bookmarks
- Landscape display selectable
- Other elements proposed by users

And potentially many more...

## Runtime environment

The EPub-InkPlate application requires that a micro-SD Card be present in the device. This micro-SD Card must be pre-formatted with a FAT32 partition. Two folders must be present in the partition: `fonts` and `books`. You must put the base fonts in the `fonts` folder and your EPub books in the `books` folder. The books must have the extension `.epub` in lowercase. 

You can change the base fonts at your desire (TrueType or OpenType only). Some open fonts are available in the `bin/fonts` folder on the GitHub EPub-InkPlate project. Four fonts are mandatory to supply regular, italic, bold, and bold-italic glyphs. Please choose fonts that do not take too much space in memory as they are loaded in memory at start time for performance purposes. At this point, the font filenames must be adjusted in file `src/fonts.cpp` in method `Fonts::setup()` if you want to use other default fonts than the ones currently defined. This will eventually be made easier to configure through a config file...

## On the complexity of EPUB page formatting

The EPUB standard allows for the use of a very large amount of flexible formatting capabilities available with HTML/CSS engines. This is quite a challenge to pack a reasonable amount of interpretation of formatting scripts on a small processor.

I've chosen a *good-enough* approach by which I obtain a reasonable page formatting quality. The aim is to get something that will allow the user to read a book and enjoy it without too much effort.

But there are cases for which the ebook content is too complex to get good results. One way to circumvent the problem is to use the epub converter provided with the [Calibre](https://calibre-ebook.com/) book management application. This is a tool able to manage a large number of books on computers. There are versions for Windows, macOS, and Linux. Calibre supplies a conversion tool (called 'Convert books' on the main toolbar) that, when choosing to convert EPUB to EPUB, will simplify the coding of styling that would be more in line with the interpretation capability of EPUB-InkPlate. 

Another aspect is the memory required to prepare a book to be displayed. As performance is also a key factor, fonts are loaded and kept in memory by the application. The Inkplate-6 is limited in memory. Around 4.5 megabytes of memory are available. A part of it is dedicated to the screen buffer and the rest of it is mainly used by the application. If a book is using too many fonts or fonts that are too big (they may contain more glyphs than necessary for the book), it will not be possible to show the document with the original fonts. The convert tool of Calibre can also shrink fonts such that they only contain the glyphs required for the book (When the 'Convert books' tool is launched, the option is located in 'Look & feel' > 'Fonts' > 'Subset all embedded fonts'). I've seen some books having four of five fonts requiring 1.5 megabytes each shrunk to around 1 meg for all fonts by the convert tool (around 200 kilobytes per fonts). 

Images that are integrated into a book may also be taking a lot of memory. 1600x1200 images require close to 6 megabytes of memory. To get them in line with the screen resolution of the InkPlate-6 (that is 600x800), the convert tool can be tailored to do so. Simply select the 'Generic e-ink' output profile from the 'Page setup' options once the convert tool is launched. Even at this size, a 600x800 image will take close to 1.5 megabytes...

## Development environment

[Visual Studio Code](https://code.visualstudio.com/) is the code editor I'm using. The [PlatformIO](https://platformio.org/) extension used to manage application configuration for both Linux and the ESP32.

### Dependencies

The following are the libraries currently in use by the application:

- [GTK+3](https://www.gtk.org/) (Only for the Linux version)

The following are imported C header and source files, that implement some algorithms:

- [FreeType](https://www.freetype.org) parse, decode, and rasterize characters from TrueType fonts.
- [PubiXML](https://pugixml.org/) (For XML parsing)
- [STB](https://github.com/nothings/stb) (For image loading and resizing, and zip deflating, etc.) :

  - `stb_image.h` image loading/decoding from file/memory: JPG, PNG, BMP, GIF; deflating (unzip)
  - `stb_image_resize.h` resize images larger/smaller 

The following libraries were used at first but replaced with counterparts:

- [ZLib](https://zlib.net/) deflating (unzip). A file deflater is already supplied with `stb_image.h`.
- [RapidXML](http://rapidxml.sourceforge.net/index.htm) (For XML parsing) Too much stack space required. Replaced with PubiXML.
- [SQLite3](https://www.sqlite.org/index.html) (The amalgamation version. For books simple database) Too many issues to get it runs on an ESP32. I built my own simple DB tool (look at `src/simple_db.cpp` and `include/simble_db.hpp`)

## In Memoriam

When I started this effort, I was aiming at supplying a tailored ebook reader for a friend of mine that has been impaired by a spinal cord injury for the last 13 years and a half. Reading books and looking at TV were the only activities she was able to do as she lost control of her body, from the neck down to the feet. After several years of physiotherapy, she was able to do some movement with her arms, without any control of her fingers. She was then able to push on buttons of an ebook reader with a lot of difficulties. I wanted to build a joystick-based interface to help her with any standard ebook reader but none of the commercially available readers allowed for this kind of integration.

On September 27th, 2020, we learned that she was diagnosed with the Covid-19 virus. She passed away during the night of October 1st.

I dedicate this effort to her. Claudette, my wife and I will always remember you!
