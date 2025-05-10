# ESP32-S3-Template

Welcome to the lab using the ESP32-S3 LilyGO!

This repository serves as a foundation for software engineering projects aimed at the ESP32 hardware.
This document will guide you through the setup process and help you prepare to work with your ESP32 hardware.

---

# How to get started

This project will work regardless of the operating system you use, and there are appended guides for each below.
Each of these tutorials is done on a fresh install of Visual Studio Code, which is the IDE you must use to complete this project.
Setting up and configuring the project will take multiple minutes, up to 15 min so be patient with the software.

## General

1. Install Visual Studio Code
   * Visit [Visual Studio Code's website](https://code.visualstudio.com/download) and download the latest version or use the package manager of your system.
   * Run `Visual Studio Code` and follow the steps.
2. Install the correct extension.
   * Head over to the extensions tab on your left.
   * Search for ["PlatformIO IDE"](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) and install it.

## Windows

3. Install the Clang compiler and C++ extension for Visual Studio Code.
   * (optional) If you want/need to use Windows Subsystem for Linux (WSL) have at the [documentation](https://code.visualstudio.com/docs/remote/wsl).

## Linux

3. Install the Clang compiler and C++ extension for Visual Studio Code.

4. Run the following command to add access to the `ttyACM0` port in order to avoid complications [1]
   * `sudo adduser $USER $(stat --format="%G" /dev/ttyACM0 )`

## MacOS

3. Run `git -v` in the Terminal app to check whether git and the macOS command line tools are installed. If the command line tools are not installed, a pop up will appear to install it.

## How to run the program

1. Open the locally cloned repository with Visual Studio Code
    * If the "Do you trust the authors of the files in this folder?" dialog appears, click on "Yes, I trust the authors"
2. Open up the file `project/project.ino`
3. Connect your ESP32 to your computer via a USB cable.
4. Build and upload the project to the device. See screenshot.

![[screenshot](./assets/screenshot.png)](./assets/screenshot.png)

---

## What to add where

Your code belongs in one file and one file only. The only place you should add, change, or remove things from is the [**project.ino**](project/project.ino) file in the `project` folder. Changing anything else might break the code and cause a lot of headaches for all involved parties.

## Areas of addition

You are meant to write a small application for the computer that retrieves the weather for a given location, and most of that code belongs in your own functions and the `void loop()` function at the bottom of the `project.ino` file.

## How to Connect to WiFi

The ESP32-S3 LilyGO has a built-in WiFi module, which allows you to connect to the internet.
However, it does not work with eduroam, so you will need to connect to a different WiFi network.
To connect to WiFi, you need to provide your SSID and password in the [project/project.ino](./project/project.ino) file.

## References

[1] This command adds the current user ($USER) to the group that owns the /dev/ttyACM0 device.
**Breakdown**:

* `sudo`: Runs the command with superuser (root) privileges.
* `adduser` `$USER`: Adds the current user to a specified group.
* `$(stat --format="%G" /dev/ttyACM0)`: retrieves the group name that owns the /dev/ttyACM0 device.
* `$()`: executes this command and substitutes its output.

Purpose: This is useful when working with serial devices (like Arduino or other USB-connected hardware). By adding yourself to the device's group, you gain permission to access `/dev/ttyACM0` without needing sudo every time.

## Features

[]=Done
[x]=Ongoing

[]U.S 1.1 - As a user, I want to see a boot screen to display the current program version and team number for at least 3 seconds.

[x]U.S1.2B - As a user, I want to see temperature and weather conditions with symbols (e.g., clear sky, rain, snow, thunder) for the next 24 hours on the starting screen.

[]U.S 2.1 - As a user, I want a menu to navigate between different screens using the two buttons, like forecast and settings screen.

[]U.S 2.2 - As a user, I want to access the menu (overview of screens) from anywhere in the program.

[]U.S 2.2B - As a user, I want to access the menu from anywhere in the program by holding both buttons simultaneously.

[]U.S 3.1 - As a user, I want to have a menu option and screen to view historical weather data.

[x]U.S 3.2B - As a user, I want to view historical temperature data (latest months) for a selected city and interact with the historical graph using the two buttons to 1. View 7 days of data 2. Scroll backward or forward by 1 day.

[]U.S 4.1 - As a user, I want to access a settings menu to configure weather data display options.

[]U.S 4.2 - As a user, I want to select weather parameters (e.g., temperature, humidity, wind speed) in the settings menu to customize the historical graph.

[]U.S 4.3 - As a user, I want to select different cities to view their weather data for the historical data and starting screen forecast.

[]U.S 4.4 - As a user, I want to reset settings to default via a menu option.

[]U.S 4.5 - As a user, I want to change my default city and weather parameter, so they are automatically selected when I start the device.

[]U.S 4.6 - As a user, I want the microcontroller to store my default settings so that they are retained even after a restart.

[x]U.S 5.1 - As a user, I want to access a menu option and screen to view the weather forecast for all of Sweden.

[x]U.S 5.2 - As a user, I want to see the forecast for each administrative area of Sweden (“Landskap”) on a map, with: o Color-coded temperature zones. o Weather symbols for each area. o A looped animation displaying the hourly forecast for the next 24 hours.
