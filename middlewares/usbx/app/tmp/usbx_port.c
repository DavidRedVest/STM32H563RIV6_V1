#include "ux_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* 禁止中断 */
ULONG  _ux_utility_interrupt_disable(VOID)
{
    /* 临时屏蔽中断，这里用 FreeRTOS 语义或裸机实现均可 */
    taskENTER_CRITICAL();     /* 若未使用 FreeRTOS，可直接 __disable_irq() */
    return 0;
}

/* 恢复中断 */
VOID   _ux_utility_interrupt_restore(ULONG old_posture)
{
    /* 恢复中断 */
    taskEXIT_CRITICAL();      /* 若裸机，可直接 __enable_irq() */
    (void)old_posture;
}

//如果没有启用FreeRTOS，也可以直接使用
//__disable_irq();
//__enable_irq();

