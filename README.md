# EPub-InkPlate An EPub Reader for InkPlate devices

## Last news

(Updated 2026.01.20)

- Now using ESP-IDF framework v5.5.2
- No longer using PlatformIO. Using cmake through idf.py to build applications

- Added support through the ESP-IDF-InkPlace v0.9.8 project:
  - Support for new devices Inkplace-6PLUS-V2, Inkplace-6FLICK
  - Added support for PCAL GPIO chip (Soldered devices)
  - Added support for Cypress touchscreen (6FLICK)
  - Added support for SD Card power control (Soldered devices)

### Building the application image

As the building process no longer uses PlatformIO, here is some explanation on how to get a new
image ready to be uploaded.

You must first install the v5.5.2 of the ESP-IDF framework. There is two way to do it: from an IDE like VSCode or manually. Look at the ESP-IDF framework  installation documentation. Here we assume that the framework has been installed in your `~/esp/v5.5.2` folder.

You must retrieve a copy of this project locally using the `git` application. Pay attention to add the submodules through the --recursive option:

```bash
$ git clone --recursive https://github.com/turgu1/EPub-InkPlate.git
```

1. Go into the main EPub-InkPlate folder. For example, if it is located in your home folder:

```bash
$ cd ~/EPub-InkPlate
```

2. In a terminal, do the following (this must be done every time you use a terminal, or only once before launching your prefered IDE into which you would launch a sub-terminal):

```bash
$  . ~/esp/v5.5.2/export.sh
```

3. To build one image for a specific device, you can use the `idf.py build` command. There is two mandatory parameters to add to the command:

- -DDEVICE=INKPLATE_XXX : For which device the build will be done
- -DAPP_VERSION=2.2-BETA : The version number (Here `2.2-BETA` as an example)

Here is the list of potential device names to use:

- INKPLATE_6
- INKPLATE_6PLUS
- INKPLATE_6PLUS_V2
- INKPLATE_6FLICK
- INKPLATE_10

For example:

```bash
$ idf.py build -DDEVICE=INKPLATE_6PLUS_V2 -DAPP_VERSION=2.2-BETA
```

Once completed without any error, the application image will be in the `build/EPub-InkPlate.bin` file.

### Building application releases

The releases are zip files that contain everything to get a device ready to use the application. That includes, the application binary image, the fonts, user guide and installation guide.

To do so, two scripts in the main folder are used to automate the process. They are:

- `bld_release.sh` : To build a single release.
- `bld_all.sh` : To generate releases for all Inkplate device types, using the `bld_release.sh` script.

#### The bld_all.sh usage

The `bld_all.sh`  requires one parameter: the release version number. For example:

```bash
$ ./bld_all.sh 2.2-BETA
```

You will then get a serie of release files named `release-v<version>-inkplate_<XXX>.zip` located in the current main project folder.

#### The bld_release.sh usage

The `bld_release.sh` requires 3 parameters and one optional parameter:

- the first parameter is the version number (e.g. `2.2-BETA`, or `2.2`, etc)
- the second parameter is the device type from the following list: `6`, `10`, `6plus`, `6plusv2`, `6flick`
- the third parameter is if the device is using the buttons extension. values are `0`: no extension, `1` : extension present. As of today, there is no people known of using that extension, so it must be 0.
- the fourth parameter is used for some optimisation:
  - If not present, the build folder is cleared. The script check if there is a release zip file already done and if so, will abort the process.
  - if present and = `1`, the build folder is cleared and the image is build, NO release file is built.
  - if present and = `2`, the build folder is kept and the image is rebuilt, NO release file is built.

For example:

```bash
$ ./bld_releae 2.2-BETA 6plusv2 0
```

------

(Updated 2022.5.01)

Update to version 2.0.1

- For Inkplate-6PLUS and Inkplate-10: The ESP-IDF-Inkplate library has been updated (v0.9.6) to support some of these devices to be delivered without a second MCP chip onboard. The presence of the second MCP is now dynamically detected by the software.

- For all Inkplates: Now using ESP-IDF framework v4.3.2

## Unresolved issue

[ ] A device reset may happen reading a book, and changing the current font as the background process is computing pages location. 

---

This is an EPub reader for the e-Radionica made Inkplate devices.

Here are the main characterics:

- TTF, and OTF embedded fonts support.
- Normal, Bold, Italic, Bold+Italic face types.
- Bitmap images dithering display (JPEG, PNG).
- EPub (V2, V3) book format subset.
- UTF-8 characters (supplied fonts limited to latin1).
- Inkplate tactile keys (single and double click to get six buttons).
- Screen orientation (portrait / landscape).
- Left, center, right, and justify text alignments.
- Font size.
- Indentation.
- Some basic parameters and options.
- Limited CSS formatting.
- WiFi-based documents download (Web server based).
- Battery state and power management (light, deep sleep, battery level display).
- Table of content.
- Multiple fonts choices selectable by the user.
- Linear and matrix view of book list.
- Real-Time clock.
- Inkplate-6PLUS touch screen and backlit.
- Keeps location of the last 10 books being read.

Some vidos are  available on YouTube:

- The first working version of the EPub-InkPlate application [Here](https://www.youtube.com/watch?v=VnTLMhEgsqA).
- Demostration on the InkPlate-10 [Here](https://www.youtube.com/watch?v=qNAjbnEax8k).
- Demonstration on the Inkplate-6PLUS [Here](https://www.youtube.com/watch?v=z1nvakbxiUQ).

Some pictures from the InkPlate-6 version:

<img src="doc/pictures/IMG_1377.JPG" alt="picture" width="300"/><img src="doc/pictures/IMG_1378.JPG" alt="picture" width="300"/>
<img src="doc/pictures/IMG_1381.JPG" alt="picture" width="300"/>

Some pictures from the Linux version:

<img src="doc/pictures/books_select.png" alt="drawing" width="300"/><img src="doc/pictures/book_page.png" alt="drawing" width="300"/>

A picture of the Web Server in a browser:

<img src="doc/pictures/web_server.png" alt="drawing" width="500"/>

Books Directory List: Linear vs Matrix View:

<img src="doc/pictures/linear_view_6.png" alt="picture" width="300"/><img src="doc/pictures/matrix_view_6.png" alt="picture" width="300"/>


### Runtime environment

The EPub-InkPlate application requires that a micro-SD Card be present in the device. This micro-SD Card must be pre-formatted with a FAT32 partition. Two folders must be present in the partition: `fonts` and `books`. You must put the base fonts in the `fonts` folder and your EPub books in the `books` folder. The books must have the extension `.epub` in lowercase. 

Height font types are supplied with the application. For each type, there are four fonts supplied, to support regular, bold, oblique, and bold-italic glyphs. The application offers the user to select one of those font types as the default font. The fonts have been cleaned-up and contain only Latin-1 glyphs.

Another font is mandatory. It can be found in `SDCard/fonts/drawings.otf` and must also be located in the micro-SD Card `fonts` folder. It contains the icons presented in parameters/options menus.

The `SDCard` folder under GitHub reflects what the micro-SD Card should look like. One file is missing there is the `books_dir.db` that is managed by the application. It contains the meta-data required to display the list of available ebooks on the card and is automatically maintained by the application. It is refreshed at boot time and when the user requires it to do so through the parameters menu. The refresh process takes some time (between 5 and 10 seconds per ebook) but is required to get fast ebook directory list on screen.

### Fonts cleanup

All fonts have been subsetted to Latin-1 plus some usual characters. The `fonts/orig` folder in the GitHub project contains all original fonts that are processed using the script `fonts/subsetter.sh`. This script uses the Python **pyftsubset** tool that is part of the **fontTools** package. To install the tool, you need to execute the following command:

```bash
$ pip install fonttools brotli zopfli
```

The script takes all font from the `orig` folder and generate the new subset fonts in `subset-latin1/otf` folder. The following commands must be executed:

```bash
$ cd fonts
$ ./subsetter.sh
```

After that, all fonts in the `subset-latin1/otf` folder must be copied back in the `SDCard/fonts` folder.

## Development environment

[Visual Studio Code](https://code.visualstudio.com/) is the code editor I'm using. The [PlatformIO](https://platformio.org/) extension is used to manage application configuration for both Linux and the ESP32.

All source code is located in various folders:

- Source code used by both Linux and InkPlate is located in the `include` and `src` folders
- Source code in support of Linux only is located in the `lib_linux` folder
- Source code in support of the InkPlate device (ESP32) only are located in the `lib_esp32` folder
- The FreeType library for ESP32 is in folder `lib_freetype`

The file `platformio.ini` contains the configuration options required to compile both Linux and InkPlate applications.

Note that source code located in folders `old` and `test` is not used. It will be deleted from the project when the application development will be completed.

### Dependencies

The following are the libraries currently in use by the application:

- [GTK+3](https://www.gtk.org/) (Only for the Linux version) The development headers must be installed. This can be done with the following command (on Linux Mint):
  
  ``` bash
  $ sudo apt-get install build-essential libgtk-3-dev
  ```

The following are imported C header and source files, that implement some algorithms:

- [FreeType](https://www.freetype.org) (Parse, decode, and rasterize characters from TrueType fonts) A version of the library has been loaded in folder `freetype-2.10.4/` and compiled with specific options for the ESP32. See sub-section **FreeType library compilation for ESP32** below for further explanations.
- [PubiXML](https://pugixml.org/) (For XML parsing)
- [STB](https://github.com/nothings/stb) (For image resizing) :

  - `stb_image_resize.h` resize images larger/smaller 

- [PNGLE](https://github.com/kikuchan/pngle) (For PNG Images) The EPub-Inkplate uses a modified version that is optimized for grayscale output instead of RGBA.
- [MINIZ](https://github.com/kikuchan/pngle) (For Zip/PNG files and epub files decompress)
- [TJPGD](http://elm-chan.org/fsw/tjpgd/00index.html) (For JPeg Images)

The following libraries were used at first but replaced with counterparts:

- [ZLib](https://zlib.net/) deflating (unzip). A file deflation function is already supplied with `PNGLE`.
- [RapidXML](http://rapidxml.sourceforge.net/index.htm) (For XML parsing) Too much stack space required. Replaced with PubiXML.
- [SQLite3](https://www.sqlite.org/index.html) (The amalgamation version. For books simple database) Too many issues to get it runs on an ESP32. I built my own simple DB tool (look at `src/simple_db.cpp` and `include/simble_db.hpp`)
- [STB](https://github.com/nothings/stb) (For image extraction and unzip) Requires a lot of memory space depending on the ePub stored image resolution. Changed to use PNGLE and TJPGD combined with my own image classes to stream the image to the appropriate size without requiring to much memory space:

  - `stb_image.h` PNG and JPeg images extraction 

### FreeType library compilation for ESP32

The FreeType library is using a complex makefile structure to simplify (!) the compilation process. Here are the steps used to get a library suitable for integration in the EPub-InkPlate ESP32 application. As this process is already done, there is no need to run it again, unless a new version of the library is required or some changes to the modules selection are done.

1. The folder named `lib_freetype` is created to receive the library and its dependencies at install time:

    ``` bash
    $ mkdir lib_freetype
    ```

2. The ESP-IDF SDK must be installed in the main user folder. Usually, it is in folder ~/esp. The following location documents the installation procedure: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html . Look at Steps 1 to 4 (Setting Up Development Environment). This is important as the configuration setup below will access ESP32 related compilation tools.

3. The files `freetype-2.10.4/modules.cfg` and `freetype-2.10.4/include/freetype/config/ftoption.h` are modified to only keep the capabilities required to support TrueType and OpenType fonts. The original files have been saved in `*.orig` files.

4. A file named `freetype-2.10.4/myconf.sh` is created to simplify the configuration of the makefile structure. The `--prefix` option may require some modification to take into account the location where the EPub-InkPlate source code has been put. The `--prefix` must point to the `lib_freetype` folder.

5. The following commands are executed:

   ``` bash
   $ cd freetype-2.10.4
   $ bash myconf.sh
   $ make
   $ make install
   ```

   This will have created several files in the folder `lib_freetype`.

6. Edit file named `lib_freetype/lib/pkgconfig/freetype2.pc` and remove the entire line that contains `harfbuzz` reference.
7. Voilà...

### ESP-IDF configuration specifics

The EPub-InkPlate application requires some functionalities to be properly set up within the ESP-IDF. To do so, some parameters located in the `sdkconfig` file must be set accordingly. This must be done using the menuconfig application that is part of the ESP-IDF. 

The following is not required to be done as the file `sdkconfig.defaults` contains the changes that will trigger the generation of the suitable `sdkconfig.<project_name>` file related to the project being compiled.

The current release of PlatformIO allow for editing the `sdkconfig` through the PlatformIO's `Run Menuconfig` command located in the Project Tasks. 

The application will show a list of configuration aspects. 

The following elements have been done (No need to do it again as they are defined in file `sdkconfig.defaults`):
  
- **PSRAM memory management**: The PSRAM is an extension to the ESP32 memory that offers 4MB+4MB of additional RAM. The first 4MB is readily available to integrate into the dynamic memory allocation of the ESP-IDF SDK. To configure PSRAM:

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

- **FAT Filesystem Support**: The application requires the usage of the micro SD card. This card must be formatted on a computer (Linux or Windows) with a FAT32 partition (maybe not required as this is the default format of brand new cards). The following parameters must be adjusted in `sdkconfig`:

  - Select `Component config` > `FAT Filesystem support` > `Max Long filename length` > `255`
  - Select `Number of simultaneously open files protected  by lock function` > `5`
  - Select `Prefer external RAM when allocating FATFS buffer`
  - Depending on the language to be used (My own choice is Latin-1 (CP850)), select the appropriate Code Page for filenames. Select `Component config` > `FAT Filesystem support` > `OEM Code Page...`. DO NOT use Dynamic as it will add ~480KB to the application!!
  - Also select `Component config` > `FAT Filesystem support` > `API character encoding` > `... UTF-8 ...`

- **HTTP Server**: The application is supplying a Web server (through the use of HTTP) to the user to modify the list of books present on the SDCard. The following parameters must be adjusted:
  - Select `Component config` > `HTTP Server` > `Max HTTP Request Header Length` > 1024
  - Select `Component config` > `HTTP Server` > `Max HTTP URI Length` > 1024

- **WiFi memory buffers in PSRAM**: The WiFi implementation use a large portion of memory. There is not enough main memory available for the buffer required, so it must be allocated from the PSRAM:
  - Select `Component config` > `ESP32-specific` > `Support for externa,, SPI-connected RAM` > `SPI RAM config` > `Try to allocate memories of WiFi and LWIP in SPIRAM firstly.`

The following is not configured through *menuconfig:*

- **Flash memory partitioning**: the file `partitions.csv` contains the table of partitions required to support the application in the 4MB flash memory. The factory partition has been set to be ~2.4MB in size (OTA is not possible as the application is too large to accomodate this feature; the OTA related partitions have been commented out...). In the `platformio.ini` file, the line `board_build.partitions=...` is directing the use of these partitions configuration.
    
## In Memoriam

When I started this effort, I was aiming at supplying a tailored ebook reader for a friend of mine that has been impaired by a spinal cord injury for the last 13 years and a half. Reading books and looking at TV were the only activities she was able to do as she lost control of her body, from the neck down to the feet. After several years of physiotherapy, she was able to do some movement with her arms, without any control of her fingers. She was then able to push on buttons of an ebook reader with a lot of difficulties. I wanted to build a joystick-based interface to help her with any standard ebook reader but none of the commercially available readers allowed for this kind of integration.

On September 27th, 2020, we learned that she was diagnosed with the Covid-19 virus. She passed away during the night of October 1st.

I dedicate this effort to her. Claudette, my wife and I will always remember you!
