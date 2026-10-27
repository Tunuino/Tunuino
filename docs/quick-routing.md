# Proteus quick routing

The normal Console flow is function-first. A routing selector contains only
Disabled, the conventional recommendation (or an explicitly named application
proposal), and the current saved alternative when different. Numeric wire
values remain internal. Personalizar opens the complete compatible contact
list; the terminal matrix stays collapsed under Avançado.

`ProteusRouting` maps wire channels to physical connector contacts, checked
against J102/J103 in the pinned PCB. The compact source-derived record is
`hardware/review/proteus-v07-connector-routing.json`.

| Function | Wire channel | Connector / contact |
|---|---|---|
| MAP | AV1 | White35 / 13 |
| TPS | AV2 | White35 / 24 |
| Coolant | AT3 | White35 / 20 |
| Intake temperature | AT2 | White35 / 30 |
| Wideband analog lambda | AV10 | White35 / 28 |
| Pump relay | LS10 | Black35 / 20 |
| Fan relay | LS11 | Black35 / 9 |
| Main relay | LS12 | Black35 / 21 |

Injector and ignition cylinder N use their corresponding output N. Pump/fan/
main-relay defaults for the existing 1..8-cylinder structural templates now
use the conventional contacts above, rather than consecutive free outputs
following the injectors. This changes only new local templates. Existing
saved routes remain unchanged, including custom routes outside the short list.

PWM idle LS13/Black35-10, boost LS14/Black35-11, analog ethanol AV4/White35-25
and conditioned launch AV6/White35-26 are application proposals, not dedicated
factory connector functions. Battery compensation has no conventional route
provided by this profile; the current external analog calibration remains
custom. The independent supply ADC is not presented as supported feedback for
that control. Unsupported controls are not added to the lists.

The quick page displays one row per function. Selecting an occupied contact
leaves the model unchanged. The recommendation action first prepares a batch
in a separate model, then replaces the edit only if every selected route is
free after moving the participating functions. A conflict leaves all prior
assignments intact. It changes already assigned functions only, not control
mode flags or unconfigured sensors. The existing atomic save/readback client
remains authoritative; outputs must be disarmed.

Route widgets retain at most three normal choices and preserve arbitrary
valid existing channel values. Rebuilding a short list is guarded against
intermediate null/value events, so it cannot remove an assignment or corrupt
an encoder's canonical numeric editor. Full lists are created only in the
custom dialog. Actuator tests list actual cylinders from the saved profile;
the firmware's output routing and protocol are unchanged.

Source checks and JavaFX tests verify connector recommendations, short-list
counts, preservation of a custom route, occupied-input rejection, collapsed
advanced UI and exact profile encoding. Compilation and register models are
not physical continuity or engine validation.
