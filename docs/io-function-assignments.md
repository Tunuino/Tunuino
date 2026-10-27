# Function-based ECU assignments

The Console `Funções de I/O` page assigns functions to named ECU terminals;
MCU GPIO codes are not presented as user choices. Connector aliases AV/AT/LS/IGN
are available as terminal tooltips. Crank/cam remain dedicated read-only routes.
No actuator capability, physical pin mapping, wire byte layout or flash format
is changed by this interface. The INI revision becomes 10 for functional labels.

`IoAssignments` maintains one function per terminal and one terminal per function.
Analog choices are MAP/lambda/TPS/battery/analog ethanol/conditioned launch;
TEMP choices are CLT/IAT; low-side power outputs accept configured injectors and
pump/fan/relay/idle/boost; logical ignition outputs accept cylinder ignition only.
The offline catalog offers 12 cylinders until a profile is read. Saving actuator
assignments requires a valid motor profile; required cylinders may not be left
without injection or ignition. Moving a function clears its old terminal. A
function displaced at the destination remains unassigned and must be resolved.
Existing overlapping reservations are reported and require an explicit choice.

Sensor calibration, motor angles, timing, activation policy and mode flags are
preserved. TPS/PWM assignment requires initialized auxiliary calibration; IAT,
battery, ethanol and launch require initialized feature calibration. Assignment
does not initialize arbitrary sensor values or enable an actuator mode. Local
validation and the firmware validate the resulting combined configuration.

`IoAssignmentClient` reads page 1 and page 4 against one generation/validation
snapshot, verifies both page CRCs and the engine state, and refuses armed, stale
or dirty transactions. The candidate is restricted to routing bytes only:
page 1 offsets 6..8, 150..152, and 160+8*i/161+8*i; page 4 offsets 2,4,5,72..75.
It stages both pages, compares staged bytes, checks validation, commits once,
and verifies a single incremented generation and exact readback. Maps and all
other calibration bytes remain intact. Its own staging is discarded after a
pre-commit error; uncertain flash results require a fresh read before retrying.
A save cannot arm outputs. Configuration generation changes invalidate cached
motor/test routing so relay tests cannot use old function assignments.

The other Console selectors keep numeric protocol values internally while
rendering function labels; there is no translation of function text into a
GPIO number. Non-editable channel selectors prevent raw pin/code entry.

C/Java register-model tests exercise moving MAP, injector and IAT routes in one
atomic save, untouched calibration, no auto-arm, invalid function categories,
required cylinder routes, stale snapshots and armed-write refusal. JavaFX checks
exercise function catalogs and exact existing wire values. These checks do not
verify physical wiring, current or engine behavior.
