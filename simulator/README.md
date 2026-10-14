# Tunuino simulator

This directory provides a **working native validation target** for Tunuino's firmware logic. Run `make -C simulator` from the repository root to configure, build, and execute the CMake/CTest host suite.

This is not yet a complete ECU simulator: there is no emulated STM32 peripheral model, engine event scheduler, or live Console connection. Those features require integration with the firmware modules and must be implemented and tested separately.
