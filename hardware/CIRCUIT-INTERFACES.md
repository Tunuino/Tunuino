# Hardware circuit interface register

The machine-readable [circuit interface register](circuit-interfaces.csv) connects the chosen external J102/J103 pin allocation to **required circuit functions**. It does **not** assert a selected component, an MCU GPIO pad, a completed schematic or a proven current rating.

The first design phase covers the high-risk interfaces: ECU supply and return, eight injector drivers, eight smart-coil logic interfaces, two differential engine position sensor channels and CAN transceiver. Sensor inputs and regulated 5 V rails remain in the [70-pin connector map](connector-pinmap.csv) and need their own analogue design.

Before choosing STM32F429ZIT6 pads, draw each conditioner/driver and then document: connector net, circuit output/input net, MCU pad, alternate function or ADC channel, reset state, fault response and measurement evidence. Never connect raw battery, raw VR or injector loads directly to MCU pads.

**Status:** interface planning only; no physical PCB or power circuitry has been validated.
