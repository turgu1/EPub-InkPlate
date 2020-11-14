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

A [Video](https://www.youtube.com/watch?v=VnTLMhEgsqA) is available on YouTube that show the first working version of the EPub-InkPlate application.

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

[Visual Studio Code](https://code.visualstudio.com/) is the code editor I'm using. The [PlatformIO](https://platformio.org/) extension is used to manage application configuration for both Linux and the ESP32.

All source code are located in various folders:

- Source code used by both Linux and InkPlate are located in the `include` and `src` folders
- Source code in support of Linux only are located in the `lib_linux` folder
- Source code in support of the InkPlate-6 (ESP32) only are locate in the `lib_esp32` folder
- The freetype library for ESP32 is in folder `lib_freetype`

The file `platformio.ini` contains the configuration options required to compile both Linux and InkPlate applications.

Note that source code located in folders `old` and `test` are not used. They will be deleted from the project when the application developement will be completed.

### Dependencies

The following are the libraries currently in use by the application:

- [GTK+3](https://www.gtk.org/) (Only for the Linux version) The development headers must be installed. This can be done with the following command (on Linux Mint):
  
  ``` bash
  $ sudo apt-get install build-essential libgtk-3-dev
  ```

The following are imported C header and source files, that implement some algorithms:

- [FreeType](https://www.freetype.org) (Parse, decode, and rasterize characters from TrueType fonts) A version of the library has been loaded in folder `freetype-2.10.4/` and compiled with specific options for the ESP32. See sub-section **FreeType library compilation for ESP32** below for further explanations.
- [PubiXML](https://pugixml.org/) (For XML parsing)
- [STB](https://github.com/nothings/stb) (For image loading and resizing, and zip deflating, etc.) :

  - `stb_image.h` image loading/decoding from file/memory: JPG, PNG, BMP, GIF; deflating (unzip)
  - `stb_image_resize.h` resize images larger/smaller 

The following libraries were used at first but replaced with counterparts:

- [ZLib](https://zlib.net/) deflating (unzip). A file deflater is already supplied with `stb_image.h`.
- [RapidXML](http://rapidxml.sourceforge.net/index.htm) (For XML parsing) Too much stack space required. Replaced with PubiXML.
- [SQLite3](https://www.sqlite.org/index.html) (The amalgamation version. For books simple database) Too many issues to get it runs on an ESP32. I built my own simple DB tool (look at `src/simple_db.cpp` and `include/simble_db.hpp`)

### FreeType library compilation for ESP32

The FreeType library is using a complex makefile structure to simplify (!) the compilation process. Here are the steps taken to get a library suitable for integration in the EPub-InkPlate ESP32 application. As this process is already done, there is no need to run it again, unless a new version of the library is required or some changes to the modules selection is done.

1. The folder named `lib_freetype` is created to receive the library and its dependancies at install time:

    ``` bash
    $ mkdir lib_freetype
    ```

2. The ESP-IDF SDK must be installed in the main user folder. Usually it is in folder ~/esp. The following location documents the installation procedure: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html . Look at Steps 1 to 4 (Setting Up Development Environment). This is important as the configuration setup below will access ESP32 related compilation tools.

3. The files `freetype-2.10.4/modules.cfg` and `freetype-2.10.4/include/freetype/config/ftoption.h` are modified to only keep the capabilities required to support TrueType and OpenType fonts. The original files have been saved in `*.orig` files.

4. A file named `freetype-2.10.4/myconf.sh` is created to simplify the configuration of the makefile structure. The `--prefix` option may require some modification to take into account the location where the EPub-InkPlate source code has been put. The `--prefix` must point to the `lib_freetype` folder.

5. The following commands are executed:

   ``` bash
   $ cd freetype-2.10.4
   $ bash myconf.sh
   $ make
   $ make install
   ```

   This will have created several files in folder `lib_freetype`.

6. Edit file named `lib_freetype/lib/pkgconfig/freetype2.pc` and remove the entire line that contains `harfbuzz` reference.
7. Voilà...

### ESP-IDF configuration specifics

The EPub-InkPlate application requires some functionalities to be properly setup within the ESP-IDF. The following elements have been done (No need to do it again):

- **Flash memory partitioning**: the file `partitions.csv` contains the table of partitions required to support the application in the 4MB flash memory. The partitions OTA_0 and OTA_1 have been set to be 1.5MB in size. In the `platformio.ini` file, the line `board_build.partitions=...` is directing the use of these partitions configuration. The current size of the application is a bit larger than 1MB, that is the reason for 1.5MB OTA partitions.
  
- **PSRAM memory management**: The PSRAM is an extension to the ESP32 memory that offer 4MB+4MB of additional RAM. The first 4MB is readily available to integrate to the dynamic memory allocation of the ESP-IDF SDK. To do so, some parameters located in the `sdkconfig` file must be set accordingly. This must be done using the menuconfig application that is part of the ESP-IDF. The following command will launch the application (the current folder must be the main folder of EPub-InkPlate):

  ```
  $ idf.py menuconfig
  ```

  The application will show a list of configuration aspects. To configure PSRAM:

  - Select `Component Config` > `ESP32-Specific` > `Support for external, SPI-Connected RAM`
  - Select `SPI RAM config` > `Initialize SPI RAM during startup`
  - Select `Run memory test on SPI RAM Initialization`
  - Select `Enable workaround for bug in SPI RAM cache for Rev 1 ESP32s`
  - Select `SPI RAM access method` > `Make RAM allocatable using malloc() as well`

  Leave the other options as they are. 

- **ESP32 processor speed**: The processor must be run at 240MHz. The following line in `platformio.ini` request this speed:

    ```
    board_build.f_cpu = 240000000L
    ```
  You can also select the speed in the sdkconfig file:

  - Select `Component config` > `ESP32-Specific` > `CPU frequency` > `240 Mhz`

- **FAT Filesystem Support**: The application requires usage of the micro SD card. This card must be formatted on a computer (Linux or Windows) with a FAT 32 partition. The following parameters must be adjusted in `sdkconfig`:

  - Select `Component config` > `FAT Filesystem support` > `Max Long filename length` > `255`
  - Select `Number of simultaneous open files protected  by lock function` > `5`
  - Select `Prefer external RAM when allocating FATFS buffer`

## In Memoriam

When I started this effort, I was aiming at supplying a tailored ebook reader for a friend of mine that has been impaired by a spinal cord injury for the last 13 years and a half. Reading books and looking at TV were the only activities she was able to do as she lost control of her body, from the neck down to the feet. After several years of physiotherapy, she was able to do some movement with her arms, without any control of her fingers. She was then able to push on buttons of an ebook reader with a lot of difficulties. I wanted to build a joystick-based interface to help her with any standard ebook reader but none of the commercially available readers allowed for this kind of integration.

On September 27th, 2020, we learned that she was diagnosed with the Covid-19 virus. She passed away during the night of October 1st.

I dedicate this effort to her. Claudette, my wife and I will always remember you!
