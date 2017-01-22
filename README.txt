How To Setup Equipment/Wiring
------------------------------
TV CODE: 1088

Connect MOSI (pin 7 on cc3200) to SI on the OLED display.
Connect SCLK (pin 5 on cc3200) to CL on the OLED display.
Connect GPIO (pin 62 on cc3200) to DC on the OLED display.
Connect GPIO (pin 61 on cc3200) to R on the OLED display.
Connect CS (pin 8 on cc3200) to OC on the OLED display.
Connect 3.3V to Vin(+) on the OLED display.
Connect GND to GND(G) on the OLED display.

Pin 63 is the GPIO Pin used to connect to the Vishay IR sensor.


How To Use Program
-------------------
When the program is running the sender simply types out the 
message that he or she wants using the IR remote and the 
standard T9 texting scheme. Each key press will be displayed
in real-time on the sender side of the OLED display (the 
top half). You may use the 'CH-' key to backspace and
delete any character. Once you're satisfied with the message
simply hit the 'CH+' to begin message transmission.

CH- == Backspace.
CH+ == Enter.

Press the 'MUTE' key to begin the Tic-Tac-Toe Mini-game.

