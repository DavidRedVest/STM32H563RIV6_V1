/**
  ******************************************************************************
  * @file    usbx_descriptors.c
  * @brief   USBX CDC-ACM descriptors for STM32H563 (FS device).
  *          - Device Class uses Composite (0xEF/0x02/0x01) + IAD
  *          - CDC Communication (IF0) + CDC Data (IF1)
  *          - EP: 0x83 (INT IN 8B), 0x01 (BULK OUT 64B), 0x82 (BULK IN 64B)
  *          - FS/HS frameworks both provided (same topology) for robust host queries
  *          - String & Language frameworks in USBX format
  ******************************************************************************
  */

#include "ux_api.h"
#include "ux_device_stack.h"
#include "ux_device_class_cdc_acm.h"

/* ============================= Device Descriptors ============================= */
/* 建议换成你自己的 VID/PID；先用 ST VID/PID 也能枚举（若系统装有通用 CDC 驱动）。 */
#define MY_VID_LO   0x83
#define MY_VID_HI   0x04
#define MY_PID_LO   0x40
#define MY_PID_HI   0x57

/* -------------------------- Full-Speed device + config ----------------------- */
/* wTotalLength(FS) = 75 (0x004B) */
UCHAR device_framework_full_speed[] =
{
    /* Device (18 bytes) */
    0x12,0x01,          /* bLength, bDescriptorType=DEVICE */
    0x00,0x02,          /* bcdUSB 2.00 */
    0xEF,               /* bDeviceClass    = Miscellaneous (Composite) */
    0x02,               /* bDeviceSubClass = Common Class */
    0x01,               /* bDeviceProtocol = Interface Association Descriptor */
    0x40,               /* bMaxPacketSize0 = 64 */
    MY_VID_LO, MY_VID_HI,  /* idVendor  */
    MY_PID_LO, MY_PID_HI,  /* idProduct */
    0x00,0x02,          /* bcdDevice 2.00 */
    0x01,               /* iManufacturer = 1 */
    0x02,               /* iProduct      = 2 */
    0x03,               /* iSerialNumber = 3 */
    0x01,               /* bNumConfigurations = 1 */

    /* Configuration (9 bytes) */
    0x09,0x02,          /* bLength, bDescriptorType=CONFIGURATION */
    0x4B,0x00,          /* wTotalLength = 75 */
    0x02,               /* bNumInterfaces = 2 (Comm + Data) */
    0x01,               /* bConfigurationValue */
    0x00,               /* iConfiguration */
    0xC0,               /* bmAttributes: Self Powered */
    0x32,               /* bMaxPower: 100 mA */

    /* IAD: first IF = 0, count = 2, CDC(0x02/0x02/0x01) */
    0x08,0x0B, 0x00,0x02, 0x02,0x02,0x01,0x00,

    /* IF0: CDC Communication (1 EP - INT IN) */
    0x09,0x04, 0x00,0x00,0x01, 0x02,0x02,0x01, 0x00,

    /* CDC Header */
    0x05,0x24,0x00, 0x10,0x01,
    /* CDC Call Management: no call mgmt over Data IF, DataIF=1 */
    0x05,0x24,0x01, 0x00,0x01,
    /* CDC ACM */
    0x04,0x24,0x02, 0x02,
    /* CDC Union: master=0, slave=1 */
    0x05,0x24,0x06, 0x00,0x01,

    /* EP 0x83: INT IN, 8 bytes, interval 16ms */
    0x07,0x05, 0x83,0x03, 0x08,0x00, 0x10,

    /* IF1: CDC Data (2 EP - BULK OUT/IN) */
    0x09,0x04, 0x01,0x00,0x02, 0x0A,0x00,0x00, 0x00,

    /* EP 0x01: BULK OUT, 64 bytes */
    0x07,0x05, 0x01,0x02, 0x40,0x00, 0x00,

    /* EP 0x82: BULK IN, 64 bytes */
    0x07,0x05, 0x82,0x02, 0x40,0x00, 0x00,
};
ULONG device_framework_full_speed_length = sizeof(device_framework_full_speed);

/* -------------------------- High-Speed device + config ----------------------- */
/* 结构完全镜像 FS；Bulk 的 wMaxPacketSize 改为 512（规范值）。wTotalLength(HS)=75 一致。 */
#if 0
static const UCHAR device_desc_hs[] =
{
    0x12,0x01, 0x00,0x02,
    0xEF,0x02,0x01, 0x40,
    MY_VID_LO, MY_VID_HI, MY_PID_LO, MY_PID_HI,
    0x00,0x02, 0x01,0x02,0x03,0x01
};
static const UCHAR config_tree_hs[] =
{
    0x09,0x02, 0x4B,0x00, 0x02,0x01,0x00,0xC0,0x32,
    0x08,0x0B, 0x00,0x02, 0x02,0x02,0x01,0x00,
    0x09,0x04, 0x00,0x00,0x01, 0x02,0x02,0x01, 0x00,
    0x05,0x24,0x00, 0x10,0x01,
    0x05,0x24,0x01, 0x00,0x01,
    0x04,0x24,0x02, 0x02,
    0x05,0x24,0x06, 0x00,0x01,
    0x07,0x05, 0x83,0x03, 0x08,0x00, 0x10,
    0x09,0x04, 0x01,0x00,0x02, 0x0A,0x00,0x00, 0x00,
    /* HS bulk OUT 512 */
    0x07,0x05, 0x01,0x02, 0x00,0x02, 0x00,
    /* HS bulk IN 512 */
    0x07,0x05, 0x82,0x02, 0x00,0x02, 0x00,
};

/* 组合为 HS framework：设备 + 配置树（USBX 读取时按这顺序返回） */
UCHAR device_framework_high_speed[sizeof(device_desc_hs) + sizeof(config_tree_hs)];
ULONG device_framework_high_speed_length = sizeof(device_framework_high_speed);
#endif

#if 1
const UCHAR device_framework_high_speed[] =
{
    /* Device (HS) */
    0x12,0x01, 0x00,0x02,
    0xEF,0x02,0x01, 0x40,
    MY_VID_LO, MY_VID_HI, MY_PID_LO, MY_PID_HI,
    0x00,0x02, 0x01,0x02,0x03,0x01,

    /* Config Tree (HS) — 与 FS 拓扑一致；HS Bulk MPS=512 */
    0x09,0x02, 0x4B,0x00, 0x02,0x01,0x00,0xC0,0x32,
    0x08,0x0B, 0x00,0x02, 0x02,0x02,0x01,0x00,
    0x09,0x04, 0x00,0x00,0x01, 0x02,0x02,0x01, 0x00,
    0x05,0x24,0x00, 0x10,0x01,
    0x05,0x24,0x01, 0x00,0x01,
    0x04,0x24,0x02, 0x02,
    0x05,0x24,0x06, 0x00,0x01,
    0x07,0x05, 0x83,0x03, 0x08,0x00, 0x10,
    0x09,0x04, 0x01,0x00,0x02, 0x0A,0x00,0x00, 0x00,
    /* HS bulk OUT 512 */
    0x07,0x05, 0x01,0x02, 0x00,0x02, 0x00,
    /* HS bulk IN 512 */
    0x07,0x05, 0x82,0x02, 0x00,0x02, 0x00,
};
ULONG device_framework_high_speed_length = sizeof(device_framework_high_speed);


#endif
#if 0
/* 构造 HS framework（也可在初始化函数里 memcpy） */
__attribute__((constructor))
static void build_hs_fw(void)
{
    UCHAR *p = device_framework_high_speed;
    for (UINT i=0;i<sizeof(device_desc_hs); ++i) *p++ = device_desc_hs[i];
    for (UINT i=0;i<sizeof(config_tree_hs); ++i) *p++ = config_tree_hs[i];
}
#endif
/* ====================== String / Language frameworks (for USBX) ====================== */
/* USBX 的字符串 framework 是扁平表项：LANGID(2B), index(2B), length_in_chars(1B), chars... */
UCHAR string_framework[] =
{
    /* iManufacturer = 1 */
    0x09,0x04,  0x01,0x00,  0x0B,
    'S','T','M','i','c','r','o','e','l','e','c',

    /* iProduct = 2 */
    0x09,0x04,  0x02,0x00,  0x11,
    'S','T','M','3','2','H','5','6','3',' ','C','D','C','-','A','C','M',

    /* iSerial = 3 */
    0x09,0x04,  0x03,0x00,  0x10,
    '0','0','0','0','0','0','0','1','A','B','C','D','1','2','3','4',
};
ULONG string_framework_length = sizeof(string_framework);

/* 语言 ID（仅英语-美国 0x0409） */
UCHAR language_id_framework[] = { 0x09,0x04 };
ULONG language_id_framework_length = sizeof(language_id_framework);

