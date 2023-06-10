# PID control model
This project was done as a 3rd year project at INSA Centre Val de Loire.
The goal was to create an educational model around PID control.
All information relatives to the project can be found in the [project report (in French)](rapport.pdf)
This repository contains both the code for the Arduino Uno and the desktop application.

## How to install
### Arduino
The Arduino code is located in the `Arduino` folder. It was made using PlatformIO.
If you want to use the Arduino IDE, you can copy the content of the `src` folder into a new sketch and remove the first line of the `main.cpp` file (`#include <Arduino.h>`).
You will also need to install the following libraries:
- [G2MotorDriver](https://github.com/photodude/G2MotorDriver)
- [digitalWriteFast](https://github.com/watterott/Arduino-Libs/tree/master/digitalWriteFast)
- [FlexiTimer2](https://github.com/PaulStoffregen/FlexiTimer2)

### Desktop application
The desktop application is located in the `PC App` folder. It was made using Python (tested on 3.10).
To install the dependencies, run `pip install -r requirements.txt` in the `PC App` folder.
You can then run the application using `python main.py`.

## How to use
Check the [project report (in French)](rapport.pdf) for more information on how to use.
