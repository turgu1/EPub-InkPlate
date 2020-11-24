# EPub-InkPlate - An EPub Reader for the InkPlate-6 device

## Installation Guide

(This is a draft version. It will be adjusted to add information for installation from an MS Windows platform)

Here is the installation procedure for the EPub-InkPlate application. This procedure can be adapted depending on your requirements.  

The installation consists of

- Preparing an SD-Card with the appropriate information.
- Uploading the application to the InkPlate-6 device.
  
The last version of the binaries for the Inkplate-6 are located in release bundles that you will find with the application GitHub [project](https://github.com/turgu1/EPub-InkPlate). This procedure shows how to install it using the *esptool* upload tool. This is the simplest way to install EPub-InkPlate as it does not require to have a full development environment (VSCode + PlatformIO + ESP_IDF) to install the binary version.

(You can also compile and upload the result within a VSCode/PlatformIO development environment. The supplied `platformio.ini` file is already set up such that once the project is loaded into the IDE, you can launch the builder and the uploader.)

### Prerequesite

The *esptool* is a Python program that is used to upload an application to an ESP32 (or ESP8266) device. It must be installed on your computer. It is compatible with both *Python* versions 2 and 3. Verify that you have *Python* and *pip* installed on your computer (The following link may be useful: https://wiki.python.org/moin/BeginnersGuide/Download). Then, to install esptool, the following command must be executed (in a shell window):

```sh
$ pip install esptool
```

You then must retrieve the release from the Github repository. Look at this [location](https://github.com/turgu1/EPub-InkPlate/releases) on GitHub. The file to download is **release.Vxxxx.zip**. It is located in the `assets`, down under the description text. Extract its content. You will get two folders: `bin` and `SDCard`, the installation and user's guide documents in markdown format.


### Preparing the SD-Card

The SD-Card must be formatted with a FAT32 (or MS-DOS or VFAT) partition. This is usually the case with brand new cards. The release's `SDCard` folder contains everything required to initialize the card's content. Simply copy the content of the folder (including the sub-folders) to the card as is.

The file `config.txt` located in the card's root folder may be edited to identify your wifi parameters (`wifi_ssid`, `wifi_pwd`) (as these parameters contain text information, they are not editable through the EPub-InkPlate application). This file is loaded at startup. This will allow for accessing the InkPlate-6 from a Web browser to manage the list of books present on the card. This is optional as it's always possible to update the SD-Card content by inserting it into your computer.

Once done, insert the card into the device.

### Uploading the application program

The release's `bin` folder contains the application, the bootloader, and the partitions binaries that must be downloaded to the device. To do so connects the device to a USB port, turn it on, change your current directory to that folder, and execute the following command (in a shell window):

```sh
$ sh upload.sh
```

Here is an example output of the execution:

```sh
$ sh upload.sh 
esptool.py v3.0
Serial port /dev/ttyUSB0
Connecting......
Chip is ESP32-D0WDQ6 (revision 1)
Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, Coding Scheme None
Crystal is 40MHz
MAC: fc:f5:c4:1b:4e:cc
Uploading stub...
Running stub...
Stub running...
Changing baud rate to 230400
Changed.
Configuring flash size...
Auto-detected Flash size: 4MB
Compressed 25136 bytes to 15148...
Wrote 25136 bytes (15148 compressed) at 0x00001000 in 0.7 seconds (effective 297.9 kbit/s)...
Hash of data verified.
Compressed 3072 bytes to 143...
Wrote 3072 bytes (143 compressed) at 0x00008000 in 0.0 seconds (effective 2244.8 kbit/s)...
Hash of data verified.
Compressed 1086128 bytes to 554716...
Wrote 1086128 bytes (554716 compressed) at 0x00010000 in 24.9 seconds (effective 348.5 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
$ 
```

Once the upload is complete, the device will automatically reboot. The first task done would be the page location computation for all books present in the SD-Card `books` folder. This is a relatively long process that will take between 1 and 3 minutes per book. The duration depends on book size and the SD-Card access speed. Once the computation is completed, the application will show the list of books present on the device, allowing the user to select a book to read.

Some options on the esptool command may have to be modified depending on your computer:

- The USB device connected to the InkPlate-6 is expected to be named `/dev/ttyUSB0` (That is the case on Linux Mint and Ubuntu). If it's not the case, you must find it and modify the `upload.sh` script accordingly. 

- Another issue you may have is the download speed that is too high for your computer. Again, you may change it in the `upload.sh` script. The speed (baud rate) is **230400** in the file. You can change it to **115200** baud or lower.
