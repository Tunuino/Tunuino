# STM32F407 CKP/CMP capture integration

The initial `trigger_timer` adapter stores timestamped CKP and CMP edges and counts events. It is a **software interface**, not a hardware input-capture implementation. No GPIO, timer channel, AF mux, NVIC interrupt or sensor input is configured yet.

Next hardware requirements:
1. Confirm the actual CKP/CMP pins on the PCB and the input conditioning electronics (VR/Hall, logic voltage, polarity, protection).
2. Choose STM32F407 timer input capture channels available on those pins; confirm alternate-function mappings against the datasheet.
3. Configure a free-running hardware timer and its prescaler for 1-us tick resolution, with validated overflow handling.
4. Read hardware CCR latches inside IRQ handlers; record overflow and capture race conditions.
5. Connect captured CKP/CMP timestamps to `engine_state`, validate with an external signal generator and oscilloscope.
6. Keep injector and ignition drivers disabled until a separate hardware fail-safe and verified sync logic are implemented.

**Do not treat this adapter as physical timer capture.**
