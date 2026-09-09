# Bambu P2S Round Display --- User Guide

This guide is for end users who want to build and install the Bambu P2S
Round Display. You do not need Arduino IDE, PlatformIO, or programming
experience.

## What This Project Does

The project uses an ESP32 and a 1.28-inch 240 × 240 GC9A01 round display
to show live status information from a Bambu Lab P2S over the local
network.

Depending on printer state, the display can show information such as:

-   Print progress
-   Current layer
-   Remaining print time
-   Estimated completion time
-   Nozzle temperature
-   Bed temperature
-   Printer status
-   A graphical progress ring that becomes fully green at 100%

## What You Need

You will need:

-   A supported ESP32 board
-   A 1.28-inch 240 × 240 GC9A01 SPI round display
-   A USB data cable for the ESP32
-   A Bambu Lab P2S
-   A computer with a compatible desktop web browser
-   The ESP32 and printer connected to the same local network during
    normal use

The web installer currently provides profiles for:

-   ESP32-C3 Super Mini
-   ESP32-S3 DevKit
-   ESP32 DevKit / WROOM

## Before You Start

You will need the following information from your own network and
printer:

-   Wi-Fi network name (SSID)
-   Wi-Fi password
-   Bambu P2S IP address
-   Bambu P2S serial number
-   Bambu P2S LAN Access Code
-   The hostname you want the ESP32 to use on your network

Do not share your Wi-Fi password or LAN Access Code publicly.

## Step 1 --- Open the Web Installer

Open the project's GitHub Pages installer in a compatible desktop
browser.

The installer must be opened over HTTPS. The published GitHub Pages
version already uses HTTPS.

If the browser reports that USB or serial access is unsupported, try a
Chromium-based desktop browser with Web Serial support.

## Step 2 --- Select Your ESP32 Board

In the installer, select the exact ESP32 board you are using.

This is important because different ESP32 boards use different GPIO pins
for the GC9A01 display.

The installer selects the appropriate firmware and later shows the
wiring information for the selected board.

## Step 3 --- Enter Your Configuration

Enter:

-   Wi-Fi SSID
-   Wi-Fi password
-   P2S IP address
-   Printer serial number
-   LAN Access Code
-   ESP32 hostname
-   Timezone

The default hostname is:

`Bambu-P2S-Display`

You may change it to another valid hostname.

Your Wi-Fi password and LAN Access Code are not intended to be stored in
the public GitHub repository. They are sent from the browser directly to
the connected ESP32 during configuration.

## Step 4 --- Connect the ESP32

Connect the ESP32 to your computer using a USB data cable.

Some USB cables are power-only cables. If the browser cannot detect the
ESP32, try another cable before troubleshooting anything else.

## Step 5 --- Flash the Firmware

Click:

**Connect & flash ESP32**

Select the USB device corresponding to your ESP32.

Follow the installation dialog and install the firmware.

If the installer offers an erase option for a new installation, erasing
the device is recommended when installing this project for the first
time or when troubleshooting old configuration data.

Wait until flashing has completed.

Then close the ESP Web Tools installation dialog.

## Step 6 --- Save Your Settings

After flashing, click:

**Save settings to device**

When the browser asks for a serial port, select the ESP32 again.

The installer sends your configuration directly to the ESP32 over USB
Serial.

The ESP32 stores the settings in non-volatile memory and restarts.

After restarting, it should connect to your Wi-Fi network and then
attempt to connect to the Bambu P2S.

## Step 7 --- Wire the GC9A01 Display

After successful configuration, the web installer shows the wiring for
the board you selected.

### ESP32-C3 Super Mini

  GC9A01             ESP32-C3 Super Mini
  ------------------ ---------------------
  VCC                3.3V
  GND                GND
  CLK / SCL          GPIO 4
  DIN / SDA / MOSI   GPIO 5
  DC                 GPIO 6
  CS                 GPIO 7
  RST / RES          GPIO 10
  BL / BLK           GPIO 3

MISO is not required.

### ESP32-S3 DevKit

  GC9A01             ESP32-S3 DevKit
  ------------------ -----------------
  VCC                3.3V
  GND                GND
  CLK / SCL          GPIO 12
  DIN / SDA / MOSI   GPIO 11
  DC                 GPIO 10
  CS                 GPIO 9
  RST / RES          GPIO 14
  BL / BLK           GPIO 13

Check that these GPIO pins are available on your exact ESP32-S3 board
before wiring it.

MISO is not required.

### ESP32 DevKit / WROOM

  GC9A01             ESP32 DevKit / WROOM
  ------------------ ----------------------
  VCC                3.3V
  GND                GND
  CLK / SCL          GPIO 18
  DIN / SDA / MOSI   GPIO 23
  DC                 GPIO 16
  CS                 GPIO 17
  RST / RES          GPIO 19
  BL / BLK           GPIO 21

ESP32 DevKit boards are available from many manufacturers. Verify the
printed GPIO labels on your exact board before connecting the display.

MISO is not required.

## GC9A01 Label Differences

Different GC9A01 modules sometimes use slightly different labels.

Common equivalents include:

  Function       Possible Display Labels
  -------------- -------------------------
  Power          VCC
  Ground         GND
  SPI clock      CLK, SCL, SCK
  SPI data       DIN, SDA, MOSI
  Data/command   DC
  Chip select    CS
  Reset          RST, RES
  Backlight      BL, BLK

For this project, a display pin labelled `SDA` normally refers to SPI
data/MOSI. It is not being used as an I²C SDA connection.

## Normal Startup

When powered, the ESP32 starts the display and loads its saved
configuration.

It then attempts to:

1.  Connect to the configured Wi-Fi network.
2.  Synchronize the local time.
3.  Connect to the Bambu P2S over the local network.
4.  Receive printer status information.
5.  Update the round display.

The display is designed to update individual screen elements rather than
constantly redraw the complete screen, reducing visible flicker.

## Changing Wi-Fi or Printer Settings

You do not need to recompile the firmware.

Connect the ESP32 to the computer over USB, open the web installer,
enter the new settings, and use:

**Save settings to device**

The new values are stored in the ESP32.

The installer can also read the non-secret parts of the current
configuration.

For security, stored Wi-Fi passwords and LAN Access Codes are not
returned to the browser when reading the configuration. Re-enter those
values before saving an edited configuration.

## Factory Reset

The installer includes:

**Factory reset settings**

This clears the saved project configuration from the ESP32.

After the ESP32 restarts, it waits for a new configuration.

You can then enter the settings again from the web installer.

## Troubleshooting

### The browser cannot find the ESP32

Try the following:

-   Use a USB data cable instead of a charge-only cable.
-   Try another USB port.
-   Disconnect and reconnect the ESP32.
-   Close Arduino IDE Serial Monitor or other programs that may already
    be using the serial port.
-   Close the ESP Web Tools flashing dialog before attempting the
    configuration step.
-   Try a compatible Chromium-based desktop browser.

### Flashing works, but saving settings does not

Make sure the flashing dialog has been closed. A serial port generally
cannot be used by two applications or browser components at the same
time.

Reconnect the ESP32 and try **Save settings to device** again.

### The display stays black

Check:

-   3.3V to VCC
-   GND to GND
-   CLK/SCL
-   DIN/SDA/MOSI
-   DC
-   CS
-   RST/RES
-   BL/BLK

Also verify that you selected the correct ESP32 board in the installer.

### The display works but no printer data appears

Check:

-   The ESP32 is connected to Wi-Fi.
-   The P2S is reachable on the same local network.
-   The printer IP address is correct.
-   The printer serial number is correct.
-   The LAN Access Code is correct.
-   The printer's local/LAN functionality required by the project is
    available.

If the printer receives a different IP address from DHCP, update the IP
address stored in the ESP32. Reserving an IP address for the printer in
your router can make the setup more reliable.

### The completion time is incorrect

Verify that the correct timezone was selected during configuration and
that the ESP32 has internet/network access sufficient for time
synchronization.

### The ESP32 repeatedly restarts

Disconnect the display temporarily and test the ESP32 by itself.

Also check the USB cable, USB power source, wiring, and for accidental
shorts between power and GPIO pins.

## Updating the Firmware

When a newer project firmware is published, reconnect the ESP32 to your
computer and use the web installer again.

If the update procedure erases the stored configuration, simply enter
your Wi-Fi and printer settings again and save them to the ESP32.

## Security and Privacy

The project requires credentials for your own Wi-Fi network and Bambu
printer.

Treat the following as private:

-   Wi-Fi password
-   Bambu LAN Access Code

Do not post these values in screenshots, GitHub issues, public
repositories, or forum messages.

## Project Disclaimer

This is a community project and is not an official Bambu Lab product.

Bambu Lab, P2S, ESP32, and other product names belong to their
respective owners.
