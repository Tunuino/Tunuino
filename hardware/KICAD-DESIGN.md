# Tunuino KiCad working design

## First real KiCad schematic file

- [tunuino.kicad_sch](tunuino.kicad_sch): **draft external connector sheet** with J102 and J103, each 35 pins, and all 70 target net labels from [the pin allocation](../docs/proteus-f4-connectors.md).
- Both connector symbols are embedded in the KiCad schematic. Their footprints are referenced by the original library IDs `ampseal:ampseal-35` and `ampseal:ampseal-35-2`; those footprints are **not embedded** and must be provided or replaced with verified footprints before PCB layout.
- This sheet is an **electrical interface draft only**. The net labels are not connected to real driver/conditioner circuits. It is not an electrically complete ECU schematic.
- MCU STM32F429ZIT6, power, protections, injector and ignition drivers, trigger conditioning, CAN transceiver, USB, SWD, and PCB layout **remain to be drawn**.
- No `.kicad_pcb`, Gerber, BOM or pick-and-place has been released; do not manufacture from this file.

The original Proteus design is a reference only. The target net names describe a **new board design** and are not a verified mapping of the existing Proteus PCB.
