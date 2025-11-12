# STM32 MCU Makefile

CROSS_COMPILE := arm-none-eabi-
CC      =$(CROSS_COMPILE)gcc
CPP     =$(CROSS_COMPILE)g++
LD      =$(CROSS_COMPILE)ld
AR      =$(CROSS_COMPILE)ar
OBJCOPY =$(CROSS_COMPILE)objcopy
OBJDUMP =$(CROSS_COMPILE)objdump
SIZE    =$(CROSS_COMPILE)size 

#设置全局变量
#固件库全局变量定义
#DEFS := -DUSE_STDPERIPH_DRIVER 
#HAL库全局变量定义
DEFS := -DUSE_HAL_DRIVER -DSTM32H563xx -DHSE_VALUE=25000000U -DHSI_VALUE=25000000U
DEFS += -DUX_INCLUDE_USER_DEFINE_FILE
#DEFS += -DUX_DEVICE_SIDE_ONLY -DUX_STANDALONE -DUX_INCLUDE_USER_DEFINE_FILE

# 操作系统平台 实现跨平台malloc函数
OS ?= FREERTOS

OS_SRC_DIR = modules/osal_mem
OS_INC_DIR = modules/osal_mem

TARGET := firmware
LINKER := stlib/STM32H563xx_FLASH.ld

# user FreeRTOS code path
FREERTOS_INC := middlewares/freertos middlewares/freertos/include 
FREERTOS_INC += middlewares/freertos/portable/GCC/ARM_CM33_NTZ/non_secure
FREERTOS_SRC := middlewares/freertos 
FREERTOS_SRC += middlewares/freertos/portable/GCC/ARM_CM33_NTZ/non_secure
FREERTOS_SRC += middlewares/freertos/portable/MemMang

# user USBX code path
USBX_INC := middlewares/usbx/common/core/inc 
USBX_INC += middlewares/usbx/common/usbx_device_classes/inc 
USBX_INC += middlewares/usbx/common/usbx_stm32_device_controllers middlewares/usbx/app
USBX_INC += middlewares/usbx/ports/cortex_m33/gnu/inc 
USBX_SRC := middlewares/usbx/common/core/src 
USBX_SRC += middlewares/usbx/common/usbx_device_classes/src 
USBX_SRC += middlewares/usbx/common/usbx_stm32_device_controllers middlewares/usbx/app

#use libmodbus
MODBUS_INC := middlewares/libmodbus
MODBUS_SRC := middlewares/libmodbus

#设置编译参数和编译选项
CFLAGS := -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard -std=c11 -ffunction-sections -fdata-sections 
#-O2表示开启优化，-Og便于调试，-g3启用最高等级调试信息（配合GDB使用）
CFLAGS += -g3 -O2
CFLAGS += -Wall -Wextra -Wno-old-style-declaration
CFLAGS += -Wno-unused-parameter
LDFLAGS := -mcpu=cortex-m33 -mthumb -Wl,--gc-sections -Wl,-Map=$(TARGET).map -Wl,--print-memory-usage
LDFLAGS += -specs=nosys.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard

#-specs=nano.specs
#LDFLAGS += -Wl,--start-group -lc -lm -Wl,--end-group

# 当 QEMU 调试时启用
#ifeq ($(QEMU_DEBUG),1)
#CFLAGS += -DQEMU_DEBUG
#endif

#添加文件路径
INCDIRS := stlib/cminc \
            stlib/inc \
            stlib \
            user \
            modules/led \
            modules/lcd \
            modules/uart \
            modules/usb \
            $(OS_INC_DIR) \
			$(FREERTOS_INC) \
			$(USBX_INC) \
			$(MODBUS_INC)
			
SRCDIRS := stlib \
            stlib/src \
            user \
            modules/led \
            modules/lcd \
            modules/uart \
            modules/usb \
            $(FREERTOS_SRC) \
            $(USBX_SRC) \
            $(MODBUS_SRC)

VPATH := $(SRCDIRS) $(INCDIRS) 

#链接头文件
INCLUDE := $(patsubst %, -I %, $(INCDIRS))

# 不存在obj目录会报错，创建一个
$(shell mkdir -p obj)

#找到.S和.c文件
SFILES := $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.S))
CFILES := $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.c))


# === OSAL_MEM selective compilation ===
# 移除 osal_mem 目录中的所有 c 文件（避免被自动扫描编译）
CFILES := $(filter-out $(OS_SRC_DIR)/%.c, $(CFILES))

# 根据 OS 选择一个指定源文件加入编译
ifeq ($(OS),FREERTOS) 
CFILES += $(OS_SRC_DIR)/osal_mem_freertos.c
else ifeq ($(OS),RTTHREAD) 
CFILES += $(OS_SRC_DIR)/osal_mem_rtthread.c
else ifeq ($(OS),THREADX) 
CFILES += $(OS_SRC_DIR)/osal_mem_threadx.c
else ifeq ($(OS),LINUX) 
CFILES += $(OS_SRC_DIR)/osal_mem_linux.c
else ifeq ($(OS),MACOS) 
CFILES += $(OS_SRC_DIR)/osal_mem_macos.c
else
$(error Unknown OS type! Define OS = FREERTOS/RTTHREAD/THREADX/LINUX/MACOS)
endif
# === end OSAL_MEM selective compilation ===


#去掉文件的路径
SFILENDIR := $(notdir $(SFILES))
CFILENDIR := $(notdir $(CFILES))
#将文件替换为.o文件
COBJS := $(patsubst %, obj/%, $(CFILENDIR:.c=.o))
SOBJS := $(patsubst %, obj/%, $(SFILENDIR:.S=.o))
OBJS := $(sort $(SOBJS) $(COBJS))

#头文件依赖
DEPS := $(patsubst obj/%, obj/%.d, $(OBJS))
DEPS := $(wildcard $(DEPS))

.PHONY: all clean upload

all:start_recursive_build $(TARGET) 
	@echo $(TARGET) has been build!


start_recursive_build:
	@echo Hello
	$(info [BUILD] Target OS = $(OS))
#   @make -C ./ -f $(TOPDIR)/Makefile.build

$(TARGET):$(OBJS)
	@$(CC) $^ -T$(LINKER) -o $(TARGET).elf $(LDFLAGS)
	@$(OBJCOPY) -O binary -S $(TARGET).elf $(TARGET).bin
	@$(SIZE) $(TARGET).elf
	@$(OBJDUMP) -S $(TARGET).elf > $(TARGET).dis

#判断一下，防止重复包含头文件
ifneq ($(DEPS),)
include $(DEPS)
endif

#编译文件
$(SOBJS) : obj/%.o : %.S
	@$(CC) $(CFLAGS) $(INCLUDE) $(DEFS) -c -o $@ $< -MD -MF obj/$(notdir $@).d

$(COBJS) : obj/%.o : %.c
	@$(CC) $(CFLAGS) $(INCLUDE) $(DEFS) -c -o $@ $< -MD -MF obj/$(notdir $@).d


clean:
	@rm -f *.o *.bin *.elf *.dis .*.d obj/* *.map


upload:all
	@echo "Flashing firmware to STM32 via J-Link..."
	@JLinkExe jlink.cfg
	@echo "Done."
	
.PHONY:gdbserver
gdbserver:
	@JLinkGDBServer -device stm32h563ri -if SWD -speed 4000



