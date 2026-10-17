// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#ifndef TUNUINO_OUTPUT_CONFIG_H
#define TUNUINO_OUTPUT_CONFIG_H
#include <stdint.h>
typedef enum { TUNUINO_OUT_DISABLED=0, TUNUINO_OUT_DIGITAL=1, TUNUINO_OUT_PWM=2, TUNUINO_OUT_STEPPER=3 } tunuino_output_mode_t;
enum { TUNUINO_CAP_DIGITAL=1u, TUNUINO_CAP_PWM=2u, TUNUINO_CAP_STEPPER=4u };
typedef struct { uint8_t capabilities; uint8_t reserved; } tunuino_output_port_t;
typedef struct { uint8_t port; tunuino_output_mode_t mode; uint16_t pwm_hz; } tunuino_output_assignment_t;
// Hardware capability table is deliberately provisional and has no GPIO mapping.
// Applications must never arm outputs based solely on this validation.
uint8_t tunuino_output_validate(const tunuino_output_port_t *ports, uint8_t port_count, const tunuino_output_assignment_t *assignments, uint8_t assignment_count);
#endif
