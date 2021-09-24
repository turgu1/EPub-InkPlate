# EPub-InkPlate - User's Guide - Version 1.3.0

The EPub-InkPlate is an EPub books reader application built specifically for the InkPlate ESP32 based devices for which a Mechanical Buttons Extended Case has been added. The extension can be find [here](https://github.com/turgu1/InkPlate-6-Extended-Case).

For the installation process, please consult the `INSTALL.pdf` document.

Here are the main characteristics of the application:

- TTF and OTF embedded fonts support
- Normal, Bold, Italic, Bold+Italic face types
- Bitmap images dithering display (JPEG, PNG)
- EPub (V2, V3) book format subset
- UTF-8 characters
- InkPlate tactile keys (single and double click to get six buttons)
- Screen orientation (buttons located to the left, right, down positions from the screen)
- Linear and Matrix view of books directory
- Up to 200 books is allowed in the directory
- Left, center, right, and justify text alignments
- Indentation
- Some basic parameters and options
- Limited CSS formatting
- WiFi-based documents download
- Battery state and power management (light, deep sleep, battery level display)

## 1. Application startup

When the device is turned ON, the application executes the following tasks:

- Initializes itself, verify the presence of books on the SD-Card, and updates its database if required. The books must be located in the `books` folder, be in the EPub V2 or V3 format and have a filename ending with the `.epub` extension in lowercase.
- Presents the list of books to the user, ready to let the user selects a book to read.

## 2. Interacting with the application

Extended InkPlate devices possess six mechanical buttons that are used to interact with the application. The application allows for the selection of the screen orientation in the parameters form. Depending on the screen orientation, the function associated with the keys will change as follow:

- When the keys are on the **Bottom** side of the screen, from left to right:

  - The **HOME** function
  - The **LEFT** function
  - The **UP** and **DOWN** function
  - The **RIGHT** function
  - The **SELECT** function

- When the keys are on the **Left** side of the screen, from top to bottom:

  - The **SELECT** function
  - The **UP** function
  - The **LEFT** and **RIGHT** function
  - The **DOWN** function
  - The **HOME** function

- When the keys are on the **Right** side of the screen, from top to bottom:

  - The **SELECT** function
  - The **UP** function
  - The **LEFT** and **RIGHT** function
  - The **DOWN** function
  - The **HOME** function

In the following text, the functions are called buttons.

The application is having two main display modes:

- Books List - Presents the list of books available on the SD-Card, showing for each book a small title page caption, the title, and the author of the book.
- Book Reader - Presents a book content, one page at a time for reading.

Each of the display modes is also supplying a list of specific functions that can be selected by the user. They are presented in the following sub-sections after the description of the display mode.

### 2.1 The Books List

The list presents all books available to the user for reading. They are presented in title alphabetical order and may require several pages depending on the number of books present on the SD-Card.

The application keeps track of the reading page location of the last 10 books opened by the user. A book will have its title prefixed with `[Reading]` to show this fact in the displayed list. 

Use the **UP** and the **DOWN** buttons to highlight the appropriate book that you want to read, then use the **SELECT** button to have the book loaded, presenting the first page of it.

The **LEFT** and **RIGHT** buttons can be used to move one page at a time in the list.

The **HOME** will open a list of options. These options are presented at the top of the screen with an icon and label shown below the icons. The list is as follow:

![Books List options](pictures/ebooks-list-options-menu.png){ width=50% }

- **Return to the e-books list** - This will simply get out of the options list, back to the list of books.
- **Return to the last e-book being read** - This will open the last book read by the user, to the last page shown on screen. 
- **Main parameters** - This will present a parameters form, allowing the user to modify some elements of the application behavior. Its content is described below.
- **Default e-book parameters** - This will present a parameters form, allowing the user to modify some elements related to books reader. Those parameters constitute default values for books preparation and presentation. Its content is described below.
- **WiFi access to the e-books folder** - This will start the WiFi connexion and a Web server, allowing the user to access - through a Web Browser - the list of books on the SD-Card, uploading, downloading, and removing books from there. Once started, pressing one of the keys on the device will stop the server and the WiFi connexion, and the device will be restarted. Be aware that as the web server is running, it is *not* lowering the use of the power (deep sleep and light sleep are disabled).
- **Refresh the e-books list** - This will launch the books database refresher. This operation is usually done automatically at application startup and is not usually required to be used. Be aware that this action will refresh *all* books. Depending on the number of books present in the `books` folder, it could be a long process (it takes between five and ten seconds per book to gather metadata). 
- **About the EPub-InkPlate application** - This will show a simple box showing the application version number and the EPub-InkPlate developer name (me!).
- **Power OFF (Deep Sleep)** - This option will put the device in DeepSleep. The device will be restarted by pressing any button.

The **LEFT** and **RIGHT** buttons can be used to move the cursor from one option to the other. The **SELECT** button can then be used to select the option and execute its function. The **HOME** button will simply get out of the options list, back to the list of books (Same behavior as if the first entry of the options list is selected).

### 2.2 The Book Reader

The reader presents the book selected by the user one page at a time. Use the **UP** and **DOWN** buttons to go to the next or previous page. The **LEFT** and **RIGHT** buttons will go 10 pages at a time.

If the user presses the **UP** button when the first page of a book is presented, the reader will display the last page of the book. If the **DOWN** button is pressed when the last page of a book is presented, the reader will display the first page of the book.

As for the books list, the **HOME** button will open a list of options (the **SELECT** button will do the same). These options are presented at the top of the screen with an icon and label shown below the icons. The list is as follow:

![Book Reader options](pictures/ebook-reader-options-menu.png){ width=50% }

- **Return to the e-book reader** - This will simply get out of the options list, back to the page being read in the currently displayed book.
- **Table of Content** - This, if present, will show the book's table of content. The user can move the cursor on one of the entries and press the **SELECT** button. The application will then move in the book to the selected table of content's entry. This option is available only if a table of content structure is available in the EPub file. 
- **E-Books List** - This will get you out of the book reader, returning to the books list.
- **Current e-book parameters** - This will present a parameters form, allowing the user to select specific values related to fonts and pictures usage for the currently displayed book. Those parameters are specific to the currently displayed book and are kept in a `.pars` file on the SD-Card. Its content is similar to the Default parameters form of the books list and is described below.
- **Revert e-book parameters to default values** - This will reset the editable book formatting parameters to default values. 
- **Delete the current e-book** - This will remove the current book from the device. All related files will also be removed. A dialog will be shown asking the user to confirm the deletion of the book. Confirmation must be done using the **SELECT** button. Using another button will cancel the deletion. Once confirmed, the book will be removed and the list of books will then be shown to the user for further selection.
- **WiFi access to the e-books folder** - This will start the WiFi connexion and a Web server, allowing the user to access - through a Web Browser - the list of books on the SD-Card, uploading, downloading, and removing books from there. Once started, pressing one of the keys on the device will stop the server and the WiFi connexion, and the device will be restarted. Be aware that as the web server is running, it is *not* lowering the use of the power (deep sleep and light sleep are disabled). 
- **About the EPub-InkPlate application** - This will show a simple message box showing the application version number and the EPub-InkPlate developer name (me!).
- **Power OFF (Deep Sleep)** - This option will put the device in DeepSleep. The device will be restarted by pressing any button. At start time, if the user was reading a book, it will be presented on screen.

The **UP** and **DOWN**, or **LEFT** and **RIGHT** buttons can be used to move the cursor from one option to the other. The **SELECT** button can then be used to select the option and execute its function. **HOME** will simply get out of the options list, back to the list of books (Same behavior as if the first entry of the options list is selected).

### 2.3 The Main Parameters Form

As indicated in section 2.1, the Main Parameters form allows for the modification of some items available to the user that will change some application behavior. Each item is presented with a list of options selectable through the use of the keys.

![The Main Parameters Form](pictures/parameters-before-selection.png){ width=50% }

The following items are displayed:

- **Minutes Before Sleeping** - Options: 5, 15 or 30 minutes. This is the timeout period for which the application will wait before entering a Deep Sleep state. Deep Sleep is a means by which battery power usage is minimal. Once sleeping, the device will be rebooted at the press of a key.
- **Books Directory View** - Options: Linear or Matrix. This will select how the list of books will be presented to the user. The Linear view will show books as a vertical list, showing the cover page on the left and the title/author on the right. The Matrix view will show covers arranged in a matrix with the title/author of the currently selected book at the top of the screen.
- **Buttons Position** - Options: LEFT, RIGHT, BOTTOM. This item selects the orientation of the device, such that the keys will be located on the left, the right, or the bottom of the screen. Changing the orientation may trigger refreshing the page's location if passing from BOTTOM to LEFT or RIGHT, or from LEFT or RIGHT to BOTTOM. As the screen geometry is changing (between Portrait and Landscape), this impacts the amount of text that will appear on each page of all books.
- **Pixel Resolution** - Select how many bits are used for each pixel on the screen. 3 bits per pixel allow for the use of antialiasing for fonts but will require a complete screen update on every page change. 1 bit per pixel allows for the use of partial screen update, much faster refresh, but no antialiasing possible: the glyphs are displayed with irregularities.
- **Show Battery Level** - Options: NONE, PERCENT, VOLTAGE, ICON. This item is showing the battery level at the bottom left of the screen and is updated every time the screen is refreshed in the books list and the book reader modes (It is *not* refreshed when the options menus or the parameters form is displayed). PERCENT will show the power percentage (2.5 volts and below is 0%, 3.7 volts and higher is 100%). VOLTAGE will show the battery voltage. The ICON is shown for all options, but NONE.
- **Show Title** - When selected, display the book title at the top portion of pages.
- **Show Heap Size** - When selected, the current stack and heap size will be displayed at the bottom of pages. Three numbers are showed (from left to right): the size of the unused stack space, the size of the largest memory chunk available in the heap and the total size of the heap available memory. This is mainly used to debug potential issues with memory allocation. The total stack size is 60 Kbytes and the heap size is ~4.3 Mbytes.
   
When the form is presented on the screen, the currently selected option of each item is surrounded by a small rectangle. A bigger rectangle appears around all options of the first item in the form (see Figure 3). It is a thin line rectangle, called the selecting box, that can be moved from an item to the other.

![Buttons Position is selected](pictures/parameters-after-selection.png){ width=50% }

To be able to modify an item, you must first move the selecting box from one item to the other using the **UP** and **DOWN**, or **LEFT** and **RIGHT** buttons. Then, you press the **SELECT** button. The selection box will change as a **bold** rectangle around the options (see Figure 4). You can change the current option with the **UP** and **DOWN**, or **LEFT** and **RIGHT** buttons. Pressing the **SELECT** button again will then *freeze* the selection. The selection box will change to thin lines and will go to the next item.

To quit the form, use the **HOME** button. The new selected options will then be saved and applied by the application.

### 2.4 The Default Parameters Form

As indicated in section 2.1, the Default Parameters form allows for the modification of default values related to fonts and pictures usage. Each item is presented with a list of options selectable through the use of the keys.

![The Default Parameters Form](pictures/current-parameters.png){ width=50% }

The following items are displayed:

- **Default Font Size** - Options: 8, 10, 12, 15 points. This item will select the size of the characters to be presented on the screen, in points (1 point = ~1/72 of an inch). Please note that this will only be effective with reflowable books. 
- **Use Fonts in E-books** - If a book contains embeded fonts, this item permits to indicate if those fonts are to be used to present the pages.
- **Default Font** - Height fonts are supllied with the application. This item permits the selection of the font to be used by default. Fonts with a **Cond** suffix are *Condensed* fonts. Fonts with a **S** suffix are *Serif* fonts.
- **Show Images in E-books** - This item allow for the display or not of images present in books. This can be used to diminish the amount of memory required and the speed of rendition. 

These are default values. They will be used for parameters that have not be modified for a book.

### 2.5 The Current book parameters form

As indicated in section 2.2, the current book parameters form allows for the selection of specific values related to fonts and pictures usage for the currently displayed book. Those parameters are specific to the currently displayed book and are kept in a `.pars` file on the SD-Card. Its content is similar to the Default parameters form of the books list as described in section 2.4.

![The Current E-book Parameters Form](pictures/default-parameters.png){ width=50% }

The following items are displayed:

- **Font Size** - Options: 8, 10, 12, 15 points. This item will select the size of the characters to be presented on the screen, in points (1 point = ~1/72 of an inch). Please note that this will only be effective with reflowable books. 
- **Use Fonts in E-books** - If a book contains embeded fonts, this item permits to indicate if those fonts are to be used to present the pages.
- **Font** - Height fonts are supllied with the application. This item permits the selection of the font to be used by default. Fonts with a **Cond** suffix are *Condensed* fonts. Fonts with a **S** suffix are *Serif* fonts.
- **Show Images in E-books** - This item allow for the display or not of images present in books. This can be used to diminish the amount of memory required and the speed of rendition. 

When displayed, the form show the current values being used to present the pages of the book. 

Parameters for which no specific value have been choosen by the user will display the default value as specified in the Default Parameters form. If such a default parameter is modified in the `Current E-book Parameters` form, it will then be freezed for this book. If a parameter is not modified from it's default value, it will retain the value present in the Default Parameters form. If the user change the parameter in the Default Parameters form, it will then be applied to the book presentation.
   
## 3. Additional information

### 3.1 The books database

The application maintains a small database that contains minimal meta-data about the books (Title, author, description, small cover picture). This list is computed initially when the application sees for the first time the presence of a book on the SD-Card. This small database is used to present the list of books present on the SD-Card. 

The only limit in terms of the number of books managed by the application is the SD-Card capacity. You can have as many books as possible to put on the SD-Card. Too many books would become difficult for the user to browse in the books list. A few dozen books are manageable. A few hundred books would become difficult to overlook.  

### 3.2 The Pages location computation

A book is presented one page at a time on the screen. The quantity of characters displayed on a page depends on the screen orientation (portrait or landscape), the fonts used, and the characters' size. Parameters in forms described in section 2, selectable by the user, have an impact on the number of pages and their localization in the EPub file. 

When a book is selected for display, the program verifies if it's required to compute the pages' location. This is transparent to the user. If required, a background task is then started to recompute locations and is minimally interfere with the user reading and moving from one page to the other. The page number that is normally shown at the bottom of the screen will only become available at the end of the pages' location computation process. The locations are saved in a file such that the next time the book will be open, the locations will not be required to be computed again if the formatting parameters have not been changed.

There is a big difference in the duration of the location computation between using slow SD-Cards and fast SD-Cards. The author made some tests with cards in hands. With SanDisk Ultra SD-Cards (both 16GB and 32GB), the scan duration with the two supplied books is ~3 minutes each. With a slow SD-Card (very old Sandisk 4GB), it took 8 minutes and 20 seconds.

### 3.3 On the complexity of EPub page formatting

The EPub standard allows for the use of a very large amount of flexible formatting capabilities available with HTML/CSS engines. This is quite a challenge to pack a reasonable amount of interpretation of formatting scripts on a small processor.

I've chosen a *good-enough* approach by which I obtain a reasonable page formatting quality. The aim is to get something that will allow the user to read a book and enjoy it without too much effort. There are cases for which the book content is way too complex to get good results...

One way to circumvent the problems is to use the epub converter provided with the [Calibre](https://calibre-ebook.com/) book management application. This is a tool able to manage a large number of books on computers. There are versions for Windows, macOS, and Linux. *Calibre* supplies a conversion tool (called 'Convert books' on the main toolbar) that, when choosing to *convert EPub to EPub*, will simplify the coding of styling that would be more in line with the interpretation capability of *EPub-InkPlate*. 

The convert tool of *Calibre* can also shrink fonts such that they only contain the glyphs required for the book (When the 'Convert books' tool is launched, the option is located in 'Look & feel' > 'Fonts' > 'Subset all embedded fonts'). I've seen some books having four of five fonts requiring 1.5 megabytes each shrunk to around 1 meg for all fonts by the convert tool (around 200 kilobytes per fonts). 

For images, to get them raisonnably in line with the screen resolution of the InkPlate devices (that is 600x800 for the InkPlate-6), the convert tool can be tailored to do so. Simply select the 'Generic e-ink' output profile from the 'Page setup' options once the convert tool is launched. For example, even at this size, a 600x800 image will take close to 500 kilobytes. 

It appears that the tool may omit to transform some images from the book. Also, the images will remain with RGB pixels instead of grayscale pixels that usually require more time to load. A script named `adjust_size.sh` is supplied with this release that can be used to transform all images in a book to use grayscale and a resolution equal to or lower than 800x600 pixels (if you prefer, you can modify it to use 1200x825 format for InkPlate-10 device). This script is using a tool supplied with the **ImageMagick** package available with Linux or macOS. It can also be loaded under MS Windows with **Cygwin**.  

### 3.4 In case of out of memory situation

The memory required to prepare a book to be displayed may become an issue if there is not enough memory available. The InkPlate devices are limited in memory: around 4.5 megabytes are available. A part of it is dedicated to the screen buffer and the rest of it is mainly used by the application.

As performance is a key factor, fonts are loaded and kept in memory by the application.  If a book is using too many fonts or fonts that are too big (they may contain more glyphs than necessary for the book), it will not be possible to show the document with the original fonts. 

Here are some steps that can be used to minimize the amount of memory that would be required to present the content of books:

- **Convert the book** - As indicated in the previous section, the *Calibre Convert* tool can be used to minimize both fonts and image size.
- **Use 1bit pixels** - The frame buffer used to render pages on screen is using a good chunk of memory: 240 kilobytes for 3bits pixels, 60 kilobytes for 1bit pixels (those numbers are for an Inkplate-6 device). You can select the pixel resolution in the Main Parameters.
- **Desactivate images** - In the Main Parameters, you can request not to show images on the screen.
- **Desactivate book fonts** - In the Font Parameters, you can request not to use fonts supplied with a book.

If an internal problem related to memory allocation is found by the application, a message will appear on the screen and the device will be put in a Deep Sleep state. The message will indicate the reason why the allocation was not successful. This can be used as a hint to use one or more steps indicated above.

### 3.5 - Images rendering

Starting with version 1.3.0, the application is using a new *stream-based* approach to render images that are part of a book. This approach optimizes the use of memory to load pictures by using a minimal amount of memory as a picture is retrieved from the ePub file.

JPeg and PNG image types are supported. Only basic formats of both types are recognized. For some books, the rendering of images may not be possible. A script named `adjust_size.sh` and supplied with the application can be used to transform the resolution of the embedded images. It also may transform the images to a format that will be compatible with the application's capabilities. Look in section 3.3 for further details on how to use the script on your computer.

### 3.6 - Moving the SDCard from an Inkplate model to another

For each book, the application may generate three additional files in the `books/` folder of the SDCard:

- Pages location offsets (files with extension `.locs`). They are tailored to the screen resolution and formatting parameters.
- Table of Content (files with extension `.toc`). They may also be tailored to the screen resolution and formatting parameters.
- The book's formatting parameters (files with extension `.pars`).

These files are automatically generated when they are not present (or when a formatting parameter will impact the page rendering) in the folder at the time the user opens a book to be read.

Inkplate models are using different eInk screens that are using different pixel resolutions. If you ever want to transfer an SDCard from a model to another, it could be beneficial to erase some files in the SDCard's `books` folder.

The best way to do it is to plug the SDCard into your computer or laptop and delete all those `.locs` and `.toc` files. The `.pars` files are the same for all Inkplate models.

### 3.7 - In case of a problem

It is possible that the application behaves in a way that you don't understand. That may happen for a variety of reasons beyond the testing effort made by the author to ensure that the application is working properly.

The first thing to do is to verify if your device is using the last version of the application. This can be done through the **About the EPub-InkPlate application** menu entry. The message will show which version you are using. Time to time, new versions are developped and made available [here](https://github.com/turgu1/EPub-InkPlate/releases). Please consult the Install Manual on how to update your Inkplate device.

If your Inkplate is already using the last available release, one way to try to find what is going wrong is to use a serial port terminal emulator on your computer after the Inkplate device has been connected using a USB cable. When EPub-InkPlate is running, messages are sent to the USB port related to the running firmware. When the application detects something wrong, there is a good chance that a message would be transmitted to the USB serial port indicating what the problem is. 

On both Linux and Mac computers, the author is using **minicom** to access the USB port. The device name is usually `/dev/ttyUSB0` and the baud rate to use is 115200 bps with 8N1 bits/parity.

On a Windows computer, there is a variety of terminal emulators available to select from. The device name is usually `COM3:` and the other parameters must be the same as for Linux.

If you can't resolve the problem by yourself, it is always possible to raise an issue [here](https://github.com/turgu1/EPub-InkPlate/issues). You have to explain the bad behaviour of the application and attach any information that can help the author to find what the problem is.

### 3.8 Limitations

The Inkplate devices are based on ESP32-WROVER MPU. This is a very capable chip with a fair amount of processing power and memory. The following are the limitations imposed to the EPub-Inkplate application related to the capabilities available with the device.

- *Maximum number of books:* **200**. The application must keep some information about the books to quickly build and show the directory content.
- *Maximum single book size:* **25 Mbytes**.
- *Font formats:* **TTF, OTF**.
- *Maximum memory used for fonts content:* **800 kbytes**. Fonts that are already loaded are kept for rendering. If the output is not appropriate, the user can disable the use of the fonts embeded with the book and use one of the fonts supplied with the application.
- *Maximum nested html tags in book content:* **50**. Testing the application, the author never had to deal with books having more than 15 nested tags. This limit is to track potential nested issues that would reset the device (stack overflow).
- *Image format types:* **subset of PNG and JPeg**. GIF and SVG are not supported. The subsets are imposed by libraries used to interpret the image file content. 