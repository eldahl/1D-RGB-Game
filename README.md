
# 1D RGB Game
This is a marvelous game that grabs the attention of the player from the get-go!  
It features an WS2812 RGB LED chain for the display, and a 3D printed box with 5 buttons for the controls.

# Images
![1](https://github.com/user-attachments/assets/318a09fc-df99-4c7e-8e53-160ea434e11a)

<p align="center">
  <img width="49%" src="https://github.com/user-attachments/assets/c413355b-ad98-41d2-8f02-de84bbbf2353">
  <img width="49%" src="https://github.com/user-attachments/assets/d6d27c59-31f4-469b-9dab-b27c4051bd9b">
</p>

<p align="center">
  <img width="49%" alt="5" src="https://github.com/user-attachments/assets/543ee47f-2a68-4630-afc4-720687e9f1d4" />
</p>

# Making it yourself
## What you need:
- a 3D printer and some filament.
- 5x arcade buttons.
- 1x Raspberry Pi Pico (or other suitable arduino compatible microcontroller).
- 1x USB cable.
- 14x 10cm wires.
- 10x cable shoes.
- 1x 3-pin male headers.
- 1x 3-pin female headers.
- 1x screw terminal wire connector.

## Hardware walkthrough
First off, grab the 3D file for the case on Thingiverse, and get it started with the printing: [https://www.thingiverse.com/thing:7273005](https://www.thingiverse.com/thing:7273005)

While that is printing, go ahead with the wiring:
- Assemble the arcade buttons if needed.
- Strip insulation from both ends of all the wires.
- Put cable shoes on one side of 10 of the wires.
<p align="center">
  <img width="49%" src="https://github.com/user-attachments/assets/c80b30e9-b21b-4e76-b701-649ed163ec53" />
  <img width="49%" src="https://github.com/user-attachments/assets/a47ea654-ef25-4f5a-ad28-ef42598852a9" />
</p>

### Button connections
- Join 1 plain wire and 5 wires with cable shoes attached, together in a screw terminal wire connector. This will be the ground connections for the buttons. Connect the 1 plain wire to the GND on the Pico.
- Connect 5 wires with cable shoes attached to each of these: GP6, GP7, GP8, GP9, GP10.

**Note**: See the pin diagram below.

### WS2812 LED chain connector
<p align="center">
  <img width="300" alt="6" src="https://github.com/user-attachments/assets/bb679de6-1904-4573-bd7a-037b3ae9c640" />
</p>

- Solder the 3-pin female header to the connection point on the WS2812 LED chain. Remember that the LED chain is directional, so it has a right and a wrong end. Look for the arrow point in the right direction, or make sure it is the side that says `DI` and not `DO`.
- Connect 1 wire to GND on the Pico, and the oher end to the left side of the 3-pin male header.
- Connect 1 wire to GP4 on the Pico, and the other end to the middle of the 3-pin male header.
- Connect 1 wire to VBUS on the Pico, and the other end to the right side of the 3-pin male header.

### Pin diagram
Here is a pin connection overview for the Pico:
<p align="center">
  <img width="600" alt="pico-wires2" src="https://github.com/user-attachments/assets/68ad8de8-c405-4c9d-965d-da6a360d61b3" />
</p>


### Wiring end result
A completed wiring can be seen here:
![pico-wires1](https://github.com/user-attachments/assets/c568f63e-e886-4937-ba0e-f12787059869)


## Software 
To upload the Arduino sketch to the Raspberry Pi Pico, first download this repository.

Next, refer to this repository for adding the Raspberry Pi Pico as a compilation target from Arduino: [https://github.com/user-attachments/assets/a47ea654-ef25-4f5a-ad28-ef42598852a9](https://github.com/user-attachments/assets/a47ea654-ef25-4f5a-ad28-ef42598852a9).
- Go to File > Preferences, and add the URL: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`.
- Go to the boards manager, and search for `Pico` and add support for the Raspberry Pi Pico:
<p align="center">
  <img width="376" height="246" alt="arduino-board-add-pico" src="https://github.com/user-attachments/assets/0603bfbe-b72a-4786-ac28-4420c50febc9" />
</p>

Next, simply select the correct COM port, and set the board as `Raspberry Pi Pico`, and upload the sketch.

# Credits
Made by Esben Christensen / Eldahl for InnoPixel Aps

