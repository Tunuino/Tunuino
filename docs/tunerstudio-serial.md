# Experimental TunerStudio serial command adapter

`tunuino_ts_serial_receive` accepts one command byte at a time and responds through a caller-supplied callback. The only supported commands are `S` (signature) and `A` (eight-byte read-only channel snapshot). Other commands are rejected and counted.

This is a **host-testable adapter**, not an STM32 UART driver and not a confirmed TunerStudio transport implementation. It does not buffer partial writes, distinguish Tunuino Console framing from TunerStudio commands, parse packet-based TunerStudio variants, or negotiate baud rate.

A dedicated physical serial port or explicit transport arbitration is required before using it with the Console protocol. The current firmware `main.c` is unchanged. All write operations remain disabled.
