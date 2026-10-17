# STM32F429ZIT6 CKP/CMP capture integration

Target: **Tunuino STM32F429ZIT6**, using external Proteus F4 connectors **J102 and J103 only**. The former STM32F407 target in this document is obsolete.

The existing `trigger_timer` adapter stores timestamped CKP/CMP edges and counts events. The `tim2_capture` module currently starts a 1 MHz **free-running counter only**: it does not configure timer capture channels, GPIO alternate functions, input filters, or IRQs. Therefore no physical CKP/CMP edges reach the engine logic yet.

## Hardware prerequisites

1. Resolve all nets on J102/J103, including unlabelled positions, power and ground, using the Proteus KiCad schematic **and PCB**. See [connector map](proteus-f4-connectors.md). Do not assume a spare pin is electrically unconnected.
2. Locate protected crank/cam input-conditioning paths. The original Proteus design has three external connectors; J101 is excluded here. Any VR+/VR- nets originally on J101 must be **redesigned and rerouted**, not assigned by name to an arbitrary J102/J103 pin.
3. Confirm whether the intended sensors are VR or Hall, the conditioner output levels and polarity, pull-ups, transient protection, and grounding.
4. Select two independent STM32F429 timer input capture channels compatible with the **actual PCB netlist**. Check alternate-function pin mux and conflicts with ADC, CAN, USART/USB and injector/ignition outputs.
5. Configure the capture timer with verified clock, overflow extension, CCR latch reading, NVIC priorities and race handling. The 1 MHz free-running TIM2 counter is **not** an input-capture implementation.
6. Connect timestamped events to tooth-pattern decoding, cam-phase correlation and stall/loss-of-sync fail-safe. Validate on a bench with a signal generator and oscilloscope.

## Safety gate

Until all the above are implemented and tested, keep all injector and ignition drivers physically disabled. A successful CI build or a running free-running timer is not evidence of a safe engine controller.
