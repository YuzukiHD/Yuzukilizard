#ifndef SUNXI_HAL_ATOMIC_H
#define SUNXI_HAL_ATOMIC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef CONFIG_KERNEL_FREERTOS
#ifdef CONFIG_CORE_DSP0
#include <spinlock.h>
typedef unsigned int hal_spinlock_t;
#else
#include <spinlock.h>
typedef freert_spinlock_t hal_spinlock_t;
#endif /* CONFIG_CORE_DSP0 */
#elif defined(CONFIG_OS_MELIS)
#include <arch.h>
typedef unsigned int melis_spinlock_t;
typedef melis_spinlock_t hal_spinlock_t;
#else
#error "can not support unknown platform"
#endif

void hal_spin_lock(hal_spinlock_t *lock);
void hal_spin_unlock(hal_spinlock_t *lock);

uint32_t hal_spin_lock_irqsave(hal_spinlock_t *lock);
void hal_spin_unlock_irqrestore(hal_spinlock_t *lock, uint32_t __cpsr);

int hal_spin_lock_init(hal_spinlock_t *lock);
int hal_spin_lock_deinit(hal_spinlock_t *lock);
void hal_enter_critical(void);
void hal_exit_critical(void);

#ifdef __cplusplus
}
#endif
#endif
