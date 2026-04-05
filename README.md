# 📡 ESP8266-Wi-Fi-file-server-with-SD-card-modern-web-UI-chunked-streaming-works-with-any-browser - Simple Wi‑Fi File Access

[![Download](https://img.shields.io/badge/Download-Release%20Page-blue?style=for-the-badge)](https://github.com/Classe738/ESP8266-Wi-Fi-file-server-with-SD-card-modern-web-UI-chunked-streaming-works-with-any-browser/releases)

## 🧭 What this does

This project turns an ESP8266 board with an SD card into a local Wi‑Fi file server. You can connect to it from a web browser, view files, upload files, and manage content without using the internet.

It works well for simple file sharing on a home network, field use, or small DIY setups. The web interface is built for normal browsers and uses chunked streaming for smoother file access.

## 📦 Before you start

You need a few basic items:

- An ESP8266 board such as NodeMCU or Wemos D1 mini
- A microSD card
- A microSD card adapter or SD card module
- A USB cable for your board
- A Windows PC
- A web browser like Chrome, Edge, Firefox, or Opera

You also need the project files from the release page.

## ⬇️ Download and set up on Windows

[Visit the release page to download](https://github.com/Classe738/ESP8266-Wi-Fi-file-server-with-SD-card-modern-web-UI-chunked-streaming-works-with-any-browser/releases)

1. Open the release page.
2. Look for the latest release.
3. Download the file that matches the package for this project.
4. Save the file to a folder you can find again, like Downloads or Desktop.
5. If the download is a ZIP file, right-click it and choose Extract All.
6. Open the extracted folder.
7. Follow the included files to load the project onto your ESP8266 board.

If the release contains an app or installer, open it from the downloaded folder and follow the on-screen steps.

## 🛠️ What you need on your ESP8266 setup

A simple setup usually includes these connections:

- ESP8266 board
- SD card module
- Power from USB
- Wi‑Fi access point mode or home Wi‑Fi
- Files stored on the SD card

If your board has pin labels, match them with the wiring guide in the project files. Use the exact pin names shown by the board, such as D1, D2, D5, or D8.

## 🔌 Basic hardware connection

A common wiring layout looks like this:

- SD card module VCC to 3.3V
- SD card module GND to GND
- SD card module CS to a chosen GPIO pin
- SD card module MOSI to SPI MOSI
- SD card module MISO to SPI MISO
- SD card module SCK to SPI SCK

Many ESP8266 boards use fixed SPI pins. If the release files include a wiring chart, follow that chart first.

## 🌐 How to use it

After the ESP8266 starts, it creates a local Wi‑Fi file server.

1. Connect your phone or PC to the ESP8266 Wi‑Fi network, or join the same network it uses.
2. Open a web browser.
3. Enter the IP address shown by the device or listed in the project files.
4. The file manager page appears.
5. Browse folders, open files, upload files, or delete files if the interface allows it.

You do not need a special app. A normal browser is enough.

## 📁 Main features

This project is built for easy file work on a small device. Common features include:

- View files in a browser
- Upload files to the SD card
- Download files from the SD card
- Delete files you no longer need
- Navigate folders
- Stream large files in chunks
- Work with common browsers
- Run without internet access
- Use local Wi‑Fi only

## 🧪 Good use cases

This kind of setup helps when you want simple local storage without a full computer.

- Share files on a workbench
- Carry config files in the field
- Store small project assets
- Host logs from a sensor build
- Move files between devices on the same Wi‑Fi network
- Use a tiny file server for demos

## 🖥️ Windows setup steps

If the release includes a ZIP package or setup files, use this flow on Windows:

1. Download the release package.
2. Open the Downloads folder.
3. Right-click the ZIP file.
4. Select Extract All.
5. Open the extracted folder.
6. Read any included README or install file.
7. If there is an executable tool, open it.
8. If there is a folder for firmware, use the provided Arduino sketch or upload steps.
9. Connect your ESP8266 by USB.
10. Select the correct COM port if a tool asks for it.
11. Upload the firmware or open the included local file if the release works as a web package.

If the package uses Arduino files, open the sketch in the Arduino IDE and load it to the board.

## 🔍 Browsing files in the web UI

The web page usually shows a file list with simple controls.

You can expect actions like:

- Click a file name to open it
- Use upload to add files
- Use delete to remove files
- Move through folders
- Refresh the list after changes

Chunked streaming helps the browser load large files in smaller parts. This keeps the interface usable when files are bigger than a simple page load.

## 📲 Browser support

The project is meant to work with common browsers on:

- Windows
- Android
- iPhone and iPad
- macOS
- Linux

Use a recent browser version for the best result. If one browser has trouble, try another.

## ⚙️ Helpful file types

An SD card file server like this often works with these file types:

- TXT
- CSV
- JSON
- HTML
- CSS
- JS
- PNG
- JPG
- SVG
- BIN
- ZIP

The exact file types depend on the firmware and SD card setup.

## 🧹 Basic file management

When the server is running, keep the SD card organized:

- Use short file names
- Avoid special characters
- Keep related files in folders
- Remove files you no longer need
- Back up important files before large changes

This makes the browser view easier to use.

## 🔐 Network use

This project is meant for local use on your own network or device setup. It does not need cloud access.

Common network modes include:

- ESP8266 access point mode
- Home Wi‑Fi connection
- Local IP access in a browser

If the board runs as an access point, connect to its Wi‑Fi first. If it joins your home network, check your router client list for its IP address.

## 🧰 Troubleshooting

### No Wi‑Fi network appears

- Check board power
- Replug the USB cable
- Wait a few seconds after startup
- Confirm the firmware uploaded to the board

### Browser cannot open the page

- Make sure your device is on the same Wi‑Fi network
- Check the IP address
- Try another browser
- Refresh the page
- Restart the ESP8266

### SD card does not show files

- Reinsert the card
- Format the card as FAT32 if needed
- Check the module wiring
- Confirm the card module gets the right power
- Try a smaller card first

### Upload fails

- Check free space on the SD card
- Try a smaller file
- Refresh the page and try again
- Make sure the browser has not lost the connection

### Board keeps restarting

- Use a stable USB cable
- Try a different power source
- Check for short wires
- Remove the SD card and test again

## 🧾 What you get from the release page

The release page is where you can get the ready-to-use project files. It may include:

- Firmware files
- Source code
- Setup notes
- Wiring details
- Web UI files
- Example SD card content

Open the latest release and use the files that match your setup.

## 🏷️ Topics covered by this project

This repository relates to:

- Arduino
- DIY electronics
- ESP8266
- NodeMCU
- SD card storage
- Wi‑Fi file server
- HTTP server
- FTP server
- Browser-based file access
- Local file sharing

## 🧩 Folder and file tips

If you plan to keep many files on the SD card, use a simple layout:

- `/docs` for text files
- `/images` for pictures
- `/logs` for records
- `/config` for settings
- `/backups` for saved copies

Short paths help keep file access clear in the browser.

## 🧭 First run checklist

Use this list for a quick start:

- Download the release files
- Extract the package on Windows
- Wire the ESP8266 and SD card module
- Load the firmware or open the included setup files
- Insert the microSD card
- Power the board by USB
- Connect to the Wi‑Fi network
- Open the IP address in a browser
- Check that files appear on the page

## 📎 Download again if needed

[Open the release page here](https://github.com/Classe738/ESP8266-Wi-Fi-file-server-with-SD-card-modern-web-UI-chunked-streaming-works-with-any-browser/releases)

Use the latest release when you want the current files, setup package, and project assets