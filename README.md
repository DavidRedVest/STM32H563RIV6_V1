# 说明 

- STM32H563RIV6

- HAL V1.5.0
- gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
- Ubuntu24.04

搭建Linux下的开发环境

提交版本说明：

- 完成基础功能搭建

- 使用.gitkeep上传空文件夹

- obj目录依然没有上传成功，在试一次

- 修改.gitignore规则重新上传obj目录

- 实现LED和LCD驱动，测试程序OK；同时解决了编译警告

- 增加串口DMA使用

- 增加rt_printf功能，可以绑定UART2和UART4，支持浮点数

- 增加FreeRTOS，实现基本功能

- 测试UART封装，注意FreeRTOSConfig.h文件中的configTOTAL_HEAP_SIZE参数，设置小了会导致堆空间用尽，然后任务创建失败

  ```C
  #define configTOTAL_HEAP_SIZE                        ( ( size_t ) ( 20 * 1024 ) )
  ```

- FreeRTOS启动之后，使用vTaskDelay函数，不要使用HAL_Delay

- 实现USB虚拟串口功能

- 实现rt_kprintf绑定USB虚拟串口输出，不能发送单个字节，开始初始化会比较慢，最前面的输出会丢掉