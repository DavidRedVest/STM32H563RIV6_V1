#include "ux_api.h"
#include "ux_device_stack.h"
#include "ux_device_class_cdc_acm.h"
#include "ux_dcd_stm32.h"
#include "bsp_usb.h"

extern void Error_Handler(void);



/* 来自 usbx_descriptors.c */

extern UCHAR device_framework_full_speed[];
extern ULONG device_framework_full_speed_length;
extern UCHAR device_framework_high_speed[];
extern ULONG device_framework_high_speed_length;
extern UCHAR string_framework[];
extern ULONG string_framework_length;
extern UCHAR language_id_framework[];
extern ULONG language_id_framework_length;


/* 给应用层的 CDC 实例 */
UX_SLAVE_CLASS_CDC_ACM *cdc_acm_instance = UX_NULL;

/* USBX 内存池 */
#define USBX_MEM_SZ (32 * 1024)


/* CDC 回调 */
static VOID cdc_activate(VOID *inst)   { cdc_acm_instance = (UX_SLAVE_CLASS_CDC_ACM*)inst; }
static VOID cdc_deactivate(VOID *inst) { (void)inst; cdc_acm_instance = UX_NULL; }

/* 设备状态变化回调（你的头文件若要求最后参数是回调则传这个；否则传 UX_NULL） */
//static UINT usbx_change(ULONG state) { (void)state; return UX_SUCCESS; }

VOID MX_USBX_Device_Init(VOID)
{
    UINT status;

	static UCHAR usbx_mem[USBX_MEM_SZ];


    /* 1) USBX 系统内存 */
    status = ux_system_initialize(usbx_mem, USBX_MEM_SZ, UX_NULL, 0);
    if (status != UX_SUCCESS) Error_Handler();



#if 1
    /* 2) 设备栈（按你环境的原型：若函数最后需要回调，传 usbx_change；否则传 UX_NULL） */
    status = _ux_device_stack_initialize(
                 device_framework_high_speed, device_framework_high_speed_length,
                 device_framework_full_speed,  device_framework_full_speed_length,
                 string_framework,             string_framework_length,
                 language_id_framework,        language_id_framework_length,UX_NULL);
                 //usbx_change);
    if (status != UX_SUCCESS) Error_Handler();
#endif
    /* 3) 注册 CDC 类 */
    UX_SLAVE_CLASS_CDC_ACM_PARAMETER p;
    ux_utility_memory_set(&p, 0, sizeof(p));
    p.ux_slave_class_cdc_acm_instance_activate   = cdc_activate;
    p.ux_slave_class_cdc_acm_instance_deactivate = cdc_deactivate;
    p.ux_slave_class_cdc_acm_parameter_change    = UX_NULL;

    status = _ux_device_stack_class_register(_ux_system_slave_class_cdc_acm_name,
                                             _ux_device_class_cdc_acm_entry,
                                             1, 0, &p);
    if (status != UX_SUCCESS) Error_Handler();

	/* 3) 把 HAL 的 PCD 注册给 USBX（★关键★） */
//	status = ux_device_stack_pcd_register((VOID *)&hpcd_USB_DRD_FS);
//	if (status != UX_SUCCESS) Error_Handler();


    /* 4) 绑定 ST DCD 驱动（把 HAL 和 USBX 连接起来） */
    status = _ux_dcd_stm32_initialize((ULONG)USB_DRD_FS, (ULONG)&hpcd_USB_DRD_FS);
    if (status != UX_SUCCESS) Error_Handler();

    /* 5) 启动控制器（真正开始对主机信号） */
    HAL_PCD_Start(&hpcd_USB_DRD_FS);
}

