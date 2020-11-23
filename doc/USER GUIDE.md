# EPub-InkPlate - An EPub Reader for the InkPlate-6 device

## User's Guide - Version 0.9
(Draft document)

The EPub-InkPlate is an EPub e-books reader application built specifically for the InkPlate-6 ESP32 based device.

For the installation process, please consult the `INSTALL.md` document.

Here are the main characteristics of the application:

- TTF and OTF embedded fonts support
- Normal, Bold, Italic, Bold+Italic face types
- Bitmap images dithering display (JPEG, PNG)
- EPub (V2, V3) e-book format subset
- UTF-8 characters
- InkPlate-6 tactile keys (single and double click to get six buttons)
- Screen orientation (buttons located to the left, right, down positions from the screen)
- Left, center, right, and justify text alignments
- Indentation
- Some basic parameters and options
- Limited CSS formatting
- WiFi-based documents download
- Battery management (light, deep sleep, battery level display)

## 1 - Application startup

When the device is turned ON, the application executes the following tasks:
- Initializes itself, verify the presence of e-books on the SD-Card, and updates its database if required. The e-books must be located in the `books` folder, be in the EPub V2 or V3 format and have a filename ending with the `.epub` extension.
- Presents the list of e-books to the user, ready to let the user selects an e-book to read.

## 2 - Interacting with the application

The InkPlate-6 device possesses three tactical keys that are used to interact with the application. On the device, they are labeled 1, 2, and 3. The application allows for the selection of the screen orientation in the parameters form. Depending on the screen orientation, the function associated with the keys will change as follow:

- When the keys are on the **Bottom** side of the screen:

  - Key 1 is the **SELECT** function
  - Key 2 is the **PREVIOUS** function
  - Key 3 is the **NEXT** function

- When the keys are on the **Left** side of the screen:

  - Key 3 is the **SELECT** function
  - Key 1 is the **PREVIOUS** function
  - Key 2 is the **NEXT** function

- When the keys are on the **Right** side of the screen:

  - Key 1 is the **SELECT** function
  - Key 3 is the **PREVIOUS** function
  - Key 2 is the **NEXT** function

All keys are associated with a second function when the user double-click on them. They are **DOUBLE-CLICK-SELECT**, **DOUBLE-CLICK-NEXT**, and **DOUBLE-CLICK-PREVIOUS**.

In the following text, the functions are called buttons.

The application is having two main display modes:

- E-Books List - Presents the list of books available on th SD-Card, showing for each e-book a small title page caption, the title and the author of the book.
- E-Book Reader - Presents an e-book content, one page at a time for reading.

Each of the display modes is also supllying a list of specific functions that can be selected by the user. They are presented in the following sub-sections after the description of the display mode.

### 2.1 - The E-Books List

The list presents all e-books available to the user for reading. They are presented in alphabetical order and may require several pages depending on the number of books present on the SD-Card.

Use the **NEXT** and the **PREVIOUS** buttons to select the appriate e-book that you want to read, then use the **SELECT** button to have the e-book loaded, presenting the first page of it.

The **DOUBLE-CLICk-NEXT** and **DOUBLE-CLICK-PREVIOUS** buttons can be used to moved one page at a time in the list.

The **DOUBLE-CLICK-SELECT** will open a list of options. These options are presented at the top of the screen with an icon and label shown below the icons. The list is as follow:

- **Return to the e-books list** - This will simply get out of the options list, back to the list of books.
- **Return to the last book being read** - This will open the last book read by the user, to the last page shown on screen. 
- **EPub-InkPlate parameters** - This will present a parameters' form, allowing the user to modify some elements pertaining to the application behaviour. It's content is described below.
- **WiFi access to the e-books folder** - This will start the WiFi connexion and a Web server, allowing the user to access - through a Web Browser - the list of e-books on the SD-Card, uploading, downloading and removing e-books from there. Once started, pressing one of the keys on the device will stop the server and the WiFi connexion, and the device will be restarted. 
- **Refresh the e-books list** - This will lauhch the e-books database refresher, looking at potential new e-books to be added to the database. This operation is usually done automatically at application startup and is not usually required to be used.
- **About the EPub-InkPlate application** - This will show a simple box showing the application version number and the EPub-InkPlate developer name (me!).
- **Power Off (Deep Sleep)** - This option will put the device in DeepSleep. The device will be restarted by pressing any button.

The **NEXT** and **PREVIOUS** buttons can be used to move the cursor from one option to the other. The **SELECT** button can then be used to select the option and execute it's function. The **DOUBLE-CLICK-SELECT** button will simply get out of the options list, back to the list of books (Same behaviour as if the first entry of the options list is selected).

### 2.2 - The E-Book reader

The reader present the e-book selected by the user on page at a time. Use the **NEXT** and **PREVIOUS** buttons to go to the next or previous page. The **DOUBLE-CLICK-NEXT** and **DOUBLE-CLICK-PREVIOUS** buttons will go 10 pages at a time.

As for the e-books list, the **DOUBLE-CLICK-SELECT** button will open a list of options. These options are presented at the top of the screen with an icon and label shown below the icons. The list is as follow:

- **Return to the e-book reader** - This will simply get out of the options list, back to the page being read in the currently displayed e-book.
- **E-Books List** - This will get you out of the e-book reader, returning to the e-books list.
- **WiFi access to the e-books folder** - This will start the WiFi connexion and a Web server, allowing the user to access - through a Web Browser - the list of e-books on the SD-Card, uploading, downloading and removing e-books from there. Once started, pressing one of the keys on the device will stop the server and the WiFi connexion, and the device will be restarted. 
- **About the EPub-InkPlate application** - This will show a simple message box showing the application version number and the EPub-InkPlate developer name (me!).
- **Power Off (Deep Sleep)** - This option will put the device in DeepSleep. The device will be restarted by pressing any button.

The **NEXT** and **PREVIOUS** buttons can be used to move the cursor from one option to the other. The **SELECT** button can then be used to select the option and execute it's function. **DOUBLE-CLICK-SELECT** will simply get out of the options list, back to the list of books (Same behaviour as if the first entry of the options list is selected).

### 2.3 - The Parameters Form

As indicated in section 2.1, the EPub-InkPlate parameters' form allow for the modification of some items available to the user that will change some application behaviour. Each item is presented with a list of options selectable throuhg the use of the keys.

The following items are displayed:

- **Minutes before sleeping** - Options: 5, 10 or 15 minutes. This is the timeout period for which the application will wait before entering in a Deep Sleep state. Deep Sleep is a mean by which battery power usage is minimal. Once sleeping, the device will be rebooted at the press of a key. 
- **Battery Visualisation** - Options: NONE, PERCENT, VOLTAGE, ICON. This option is showing the battery level at the bottom left of the screen and is updated every time the screen is refreshed in the e-books list and the e-book reader modes (It is *not* refreshed when the options menus or the parameters form is displayed). PERCENT will show the power percentage (2.5 volts and below is 0%, 3.7 volts and higher is 100%). The ICON is shown for all options, but NONE.
- **Default Font Size** - Options: 8, 10, 12, 15 points. This option will select the size of the characters to be presented on the screen, in points (1 point = ~1/72 of an inch). Changing the size of the fonts will trigger refreshing the pages location for all e-books.
- **Buttons Position** - Options: LEFT, RIGHT, BOTTOM. This option select the orientation of the device, such that the keys will be located on the left, the right or the bottom of the screen. Changing the orientation may trigger refreshing the pages location if passing from BOTTOM to LEFT or RIGHT, or from LEFT or RIGHT to BOTTOM. As the screen geometry is changing (between Portrait and Landscape), this impact the amount of texte that will appears on each page of all books.
- **OK and CANCEL** - When entering in the parameters form, the CANCEL option is selected. That means that none of the modification done in the form will ne kept. Before leaving the form, it is necessary to select the OK option to get the selection options been saved.

When the form is presented on screen, the current selected option of each item is surrounded with a small rectangle. A bigger rectangle appears around all options of the first item in the form. It is a thin lines rectangle, called the selecting box below, that can be moved from an item to the other.

To be able to modify an item, you must first move the selecting box from one item to the other using the **NEXT** and **PREVIOUS** buttons. Then, you press the **SELECT** button. The selection box will change as a **bold** rectangle around the options. You can change the current option with the **NEXT** and **PREVIOUS** button. Pressing the **SELECT** button again will then *freeze* the selection. The selection box will change to thin lines and will go to the next item.

To quit the form, use the **DOUBLE-CLICK-SELECT** button. If the OK options as been selected before using that button, the new selected options will then be saved and applied by the application.


## 3 - Additional information

### 3.1 - The e-books database

The application maintains a small database that contains minimal meta-data about the e-books (Title, author, description) and the list of page positions. This list is computed initially when the application sees for the first time the presence of an e-book on the SD-Card. 

Page positions in the e-books depend on the screen orientation (portrait or landscape) and the characters' size. Both are selectable by the user in the application parameter form. Changes to these parameters will trigger the scan of the e-books to recompute the list.

There is a big difference in duration between using slow SD-Cards and fast SD-Cards. The author made some tests with cards in hands. With SanDisk Ultra SD-Cards (both 16GB and 32GB), the scan duration with the two supplied e-books is ~3 minutes. With a slow SD-Card (very old Sandisk 4GB), it took 8 minutes and 20 seconds.

### 3.2 - On the complexity of EPub page formatting

The EPub standard allows for the use of a very large amount of flexible formatting capabilities available with HTML/CSS engines. This is quite a challenge to pack a reasonable amount of interpretation of formatting scripts on a small processor.

I've chosen a *good-enough* approach by which I obtain a reasonable page formatting quality. The aim is to get something that will allow the user to read a book and enjoy it without too much effort. Another aspect is the memory required to prepare a book to be displayed. As performance is also a key factor, fonts are loaded and kept in memory by the application. The InkPlate-6 is limited in memory. Around 4.5 megabytes of memory are available. A part of it is dedicated to the screen buffer and the rest of it is mainly used by the application. If a book is using too many fonts or fonts that are too big (they may contain more glyphs than necessary for the book), it will not be possible to show the document with the original fonts.

Images that are integrated into a book may also be taking a lot of memory. 1600x1200 images require close to 6 megabytes of memory. To get them in line with the screen resolution of the InkPlate-6 (that is 600x800), the convert tool can be tailored to do so. Simply select the 'Generic e-ink' output profile from the 'Page setup' options once the convert tool is launched. Even at this size, a 600x800 image will take close to 1.5 megabytes...

There are cases for which the ebook content is way too complex to get good results...

One way to circumvent the problems is to use the epub converter provided with the [Calibre](https://calibre-ebook.com/) book management application. This is a tool able to manage a large number of books on computers. There are versions for Windows, macOS, and Linux. *Calibre* supplies a conversion tool (called 'Convert books' on the main toolbar) that, when choosing to *convert EPub to EPub*, will simplify the coding of styling that would be more in line with the interpretation capability of *EPub-InkPlate*. The convert tool of *Calibre* can also shrink fonts such that they only contain the glyphs required for the book (When the 'Convert books' tool is launched, the option is located in 'Look & feel' > 'Fonts' > 'Subset all embedded fonts'). I've seen some books having four of five fonts requiring 1.5 megabytes each shrunk to around 1 meg for all fonts by the convert tool (around 200 kilobytes per fonts). 
