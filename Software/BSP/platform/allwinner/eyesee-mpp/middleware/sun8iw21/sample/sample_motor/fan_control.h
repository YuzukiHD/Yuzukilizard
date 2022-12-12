
#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif
#include <utils/plat_log.h>

int AW_MPI_InitMotor();
int AW_MPI_EnableMotor(int Group, int angle, int polarity);
int AW_MPI_SetMotorStepSpeed(int Group, int run_count, int polarity);
int AW_MPI_DisableMotor(int Group);

#ifdef __cplusplus
}
#endif

#endif
