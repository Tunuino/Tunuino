#pragma once
#include "ConnectorPin.h"
#include <QStringList>
namespace tunuino {
// UAefi F3, Tunuino/uaefi commit fefb25a22f8b1968a828ce377363717c529c712f.
// 68 connector nets checked against the schematic and PCB.
// Photo coordinates correspond to the unmodified photographs and their original pin map.
inline QString uaefiPcbConnector(int index) {return QStringList{"J4","J5","J2","J10","J3"}.value(index);}
inline QVector<ConnectorPin> uaefiPins(int connector) {
 switch(connector) {
 case 0: return {
  {1, {420, 221}, "DC1-", "Motor H-bridge 1 — negative", "etb"},
  {2, {306, 221}, "DC2-", "Motor H-bridge 2 — negative", "etb"},
  {3, {207, 221}, "Power GND", "Power / chassis ground", "gnd"},
  {4, {97, 221}, "Power GND", "Power / chassis ground", "gnd"},
  {5, {420, 115}, "DC1+", "Motor H-bridge 1 — positive", "etb"},
  {6, {306, 115}, "DC2+", "Motor H-bridge 2 — positive", "etb"},
  {7, {207, 115}, "12V KEY", "Ignition-switched ECU power input", "12v"},
  {8, {97, 115}, "12V MR", "Main relay power input", "12v"},
 };
 case 1: return {
  {1, {927, 230}, "INJ6", "Low-side injector output 6", "ls"},
  {2, {821, 230}, "INJ5", "Low-side injector output 5", "ls"},
  {3, {719, 230}, "INJ4", "Low-side injector output 4", "ls"},
  {4, {615, 230}, "INJ3", "Low-side injector output 3", "ls"},
  {5, {518, 230}, "INJ2", "Low-side injector output 2", "ls"},
  {6, {415, 230}, "INJ1", "Low-side injector output 1", "ls"},
  {7, {315, 230}, "LS1", "Low-side solenoid / relay output", "ls"},
  {8, {208, 230}, "LS HOT2", "Weak low-side relay output; no flyback diode", "ls"},
  {9, {109, 230}, "LS HOT1", "Weak low-side relay output; no flyback diode", "ls"},
  {10, {927, 130}, "IGN6", "Logic signal for smart ignition coil 6", "ign"},
  {11, {821, 130}, "IGN4", "Logic signal for smart ignition coil 4", "ign"},
  {12, {719, 130}, "IGN3", "Logic signal for smart ignition coil 3", "ign"},
  {13, {615, 130}, "IGN5", "Logic signal for smart ignition coil 5", "ign"},
  {14, {518, 130}, "IGN2", "Logic signal for smart ignition coil 2", "ign"},
  {15, {415, 130}, "IGN1", "Logic signal for smart ignition coil 1", "ign"},
  {16, {315, 130}, "LS4", "Low-side solenoid / relay output", "ls"},
  {17, {208, 130}, "LS3", "Low-side solenoid / relay output", "ls"},
  {18, {109, 130}, "LS2", "Low-side solenoid / relay output", "ls"},
 };
 case 2: return {
  {1, {1655, 400}, "5V SENSOR", "5 V sensor power supply", "5v"},
  {2, {1500, 400}, "5V SENSOR", "5 V sensor power supply", "5v"},
  {3, {1341, 400}, "AUX2", "Auxiliary analog sensor input 2", "av"},
  {4, {1185, 400}, "PPS2", "Analog accelerator pedal sensor input 2", "av"},
  {5, {1031, 400}, "HALL1", "Hall / logic sensor input 1", "din"},
  {6, {876, 400}, "HALL2", "Hall / logic sensor input 2", "din"},
  {7, {716, 400}, "HALL3", "Hall / logic sensor input 3", "din"},
  {8, {561, 400}, "Power GND", "Power / chassis ground", "gnd"},
  {9, {405, 400}, "BUTTON3", "Switch / button signal input 3", "din"},
  {10, {246, 400}, "EGT+", "EGT thermocouple positive; optional converter", "at"},
  {11, {1655, 245}, "Sensor GND", "Reference ground for sensor signals", "agnd"},
  {12, {1500, 245}, "Sensor GND", "Reference ground for sensor signals", "agnd"},
  {13, {1341, 245}, "Sensor GND", "Reference ground for sensor signals", "agnd"},
  {14, {1185, 245}, "TPS2", "Analog throttle position sensor input 2", "av"},
  {15, {1031, 245}, "AUX3", "Auxiliary analog sensor input 3", "av"},
  {16, {876, 245}, "VR2+", "MAX9924 VR input — positive", "vr"},
  {17, {716, 245}, "VR2-", "MAX9924 VR input — negative", "vr"},
  {18, {561, 245}, "VR1+", "Discrete VR input — positive", "vr"},
  {19, {405, 245}, "VR1-", "Discrete VR input — negative", "vr"},
  {20, {246, 245}, "EGT-", "EGT thermocouple negative; optional converter", "at"},
 };
 case 3: return {
  {1, {977, 250}, "AUX1", "Auxiliary analog sensor input 1", "av"},
  {2, {862, 250}, "BUTTON1", "Switch / button signal input 1", "din"},
  {3, {743, 250}, "5V SENSOR", "5 V sensor power supply", "5v"},
  {4, {626, 250}, "5V SENSOR", "5 V sensor power supply", "5v"},
  {5, {511, 250}, "FLEX", "Flex fuel / digital pulse input", "din"},
  {6, {393, 250}, "PPS1", "Analog accelerator pedal sensor input 1", "av"},
  {7, {276, 250}, "CAN+", "CAN bus — High", "can"},
  {8, {160, 250}, "CAN-", "CAN bus — Low", "can"},
  {9, {977, 134}, "MAP", "External manifold pressure sensor input", "av"},
  {10, {862, 134}, "BUTTON2", "Switch / button signal input 2", "din"},
  {11, {743, 134}, "Sensor GND", "Reference ground for sensor signals", "agnd"},
  {12, {626, 134}, "Sensor GND", "Reference ground for sensor signals", "agnd"},
  {13, {511, 134}, "TPS1", "Analog throttle position sensor input 1", "av"},
  {14, {393, 134}, "KNOCK", "Knock sensor signal input", "knock"},
  {15, {276, 134}, "IAT", "Intake air temperature sensor input", "at"},
  {16, {160, 134}, "CLT", "Coolant temperature sensor input", "at"},
 };
 case 4: return {
  {1, {361, 221}, "WBO heater +", "Wideband sensor heater positive; sensor pin 4", "wbo"},
  {2, {254, 221}, "WBO CalR", "Wideband calibration resistor; sensor pin 5", "wbo"},
  {3, {153, 221}, "WBO heater -", "Wideband sensor heater negative; sensor pin 3", "wbo"},
  {4, {361, 116}, "WBO Vs", "Wideband Vs (Un) signal; sensor pin 6", "wbo"},
  {5, {254, 116}, "WBO Ip", "Wideband pump current signal; sensor pin 1", "wbo"},
  {6, {153, 116}, "WBO Vm", "Wideband Vs/Ip (Vm) signal; sensor pin 2", "wbo"},
 };
 default: return {};
 }
}
}
