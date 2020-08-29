# PixelWire
A [Wireworld](https://en.wikipedia.org/wiki/Wireworld) cellular automaton written in modern C++. It was made for the
[olcCodeJam2020](https://itch.io/jam/olc-codejam-2020) (the theme was "The Great Machine!"). Supports:
* Saving and loading files
* Altering simulation speed

## Screenshots
<img src="https://raw.githubusercontent.com/TheUltimateKerbonaut/PixelWire/master/Screenshots/andGate.png" alt="AND gate screenshot"/>

## Usage
* On Windows, compile by opening the solution in Visual Studio then building. If you wish to include the in-built debugger, compile under debug mode.
* On Linux, run these commands:
```
cd PixelWire
g++ -o PixelWire *.cpp *.c -lX11 -lGL -lpthread -lpng -lstdc++fs -std=c++17 -fpermissive
./PixelWire
```
Then run the application.

## Supported platforms
* Windows
* Linux

## Dependencies
All dependencies are included with the source code. These include:
* [Pixel Game Engine](https://github.com/OneLoneCoder/olcPixelGameEngine) - graphics and input
* [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/)