# Tunuino Console — Windows Qt 6 prototype

Initial Windows-only Qt Widgets scaffold. It is **not yet connected to the ECU**. No firmware flashing, map editing, or ECU writes are implemented.

## Build (Windows x64)
Install Qt 6.4+ with MSVC 2022 64-bit, CMake 3.21+, and Visual Studio 2022 C++ tools.

```powershell
cmake -S qt_console -B build/qt_console -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build/qt_console --config Release
cmake --install build/qt_console --config Release --prefix dist/TunuinoConsole
```

Adjust CMAKE_PREFIX_PATH to the installed Qt version. The Java console remains unchanged.

## Migration modules
- UI: dockable engine status, maps, dashboard, language selection
- ECU transport: USB CDC serial first; CAN adapter support later
- Protocol: port and test existing frame codec and firmware commands before enabling writes
- Calibration: sensor assistants, injector dead time, 16x16 maps
- Logging: telemetry capture, CSV and playback
- Maintenance: firmware identity, DFU flashing and recovery

No ECU control feature is considered implemented until tested against firmware and hardware.
