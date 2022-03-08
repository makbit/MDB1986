## ModBus_RS232 - FreeModBus RTU Slave demonstration firmware for Milandr 1986BE1T (QI) board.
> Platform: Milandr MCU K1986BE1QI.

The source code is base on the [freemodbus library](https://github.com/cwalter-at/freemodbus) by Christian Walter.

### Important note: Add call to "xMBRTUTransmitFSM ();" in mbrtu.c file, Line 215.

This Demo firmware runs at 80MHz and uses UART#2 (Port D 13, Port D 14) and Timer #1 with prescaler set to 8.
UART is configured to 115200 baudrate, parity none, 2 stop bits (as recommended in standard).
Slave address is set to 0x0A (10 decimal).
![Modbus-1.png](Modbus-1.png)

The project folder contains *modbus* subfolder with **port_1986BE1T** MCU specific.
![Modbus-3.png](Modbus-3.png)

##### Good luck!
