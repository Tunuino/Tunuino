# Engine RPM partial telemetry

`tunuino_console_engine_partial` builds an RPM-only partial telemetry snapshot from `engine_state`.

- When engine_running and speed.valid are both true, a nonzero RPM up to 65535 is reported as valid.
- When the engine state is invalid or unsynchronized, RPM is **unavailable** (valid_mask=0).
- RPM=0 is marked valid **only if an independent caller explicitly confirms the engine has stopped**. Loss of CKP sync alone does not prove engine stop.
- Other sensor fields are always unavailable.
- `tunuino_console_partial_service_step` queues type-3 frames at no more than 10 Hz and drains one UART byte per call.

These functions are host-testable and do not enable STM32 USART, CKP GPIO, ignition or injection. The caller must ensure a serialized context and genuine sensor inputs. The current STM32 `main.c` remains unchanged.
