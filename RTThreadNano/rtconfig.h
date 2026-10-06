#ifndef RT_CONFIG_H
#define RT_CONFIG_H
#ifdef RT_HOST_TEST
#define RT_USING_LIBC
#endif
/* 板级 USB CDC 控制台，输出缓冲区 128 字节。 */
#define RT_USING_CONSOLE
#define RT_CONSOLEBUF_SIZE 128
#define RT_USING_BOARD_CONSOLE
/* 仅保留 MSH 命令模式；缩小命令历史和行缓冲区以节省 RAM。 */
#define RT_USING_FINSH
#define FINSH_USING_MSH
#define FINSH_USING_MSH_ONLY
#define FINSH_USING_SYMTAB
#define FINSH_USING_DESCRIPTION
#define FINSH_USING_HISTORY
#define FINSH_HISTORY_LINES 2
#define FINSH_CMD_SIZE 64
#define FINSH_ARG_MAX 8
#define FINSH_THREAD_PRIORITY 5
#define FINSH_THREAD_STACK_SIZE 768
#define RT_FINSH_STATIC_SYMTAB
#define RT_NAME_MAX 8
#define RT_ALIGN_SIZE 2
#define RT_DEBUG_TIMER 0
/* 八个优先级，数字越小越高；100 Hz 对应每 tick 为 10 ms。 */
#define RT_THREAD_PRIORITY_MAX 8
#define RT_TICK_PER_SECOND 100
#define RT_USING_SEMAPHORE
#define RT_USING_MUTEX
#define RT_USING_EVENT
#define RT_USING_MESSAGEQUEUE
#define RT_USING_TIMER_SOFT
#define RT_TIMER_THREAD_STACK_SIZE 384
#define RT_TIMER_THREAD_PRIO 3
#define RT_USING_HEAP
#define RT_USING_SMALL_MEM
#define RT_USING_SMALL_MEM_AS_HEAP
/* 控制块使用 XDATA 堆，动态线程栈另从 EDATA 专用池分配。 */
#define RT_USING_ARCH_DYNAMIC_STACK
#define RT_PORT_DYNAMIC_STACK_SIZE 512
#define RT_PORT_STATIC_FINSH
#define RT_USING_OVERFLOW_CHECK
#define RT_USING_TINY_FFS
#define RT_KSERVICE_USING_TINY_SIZE
#define IDLE_THREAD_STACK_SIZE 384
#define ARCH_CPU_STACK_GROWS_UPWARD
/* C251 的 int 为 16 位，long 为 32 位，必须明确内核数据类型。 */
#ifdef __C251__
#define RT_USING_ARCH_DATA_TYPE
typedef signed char rt_int8_t;
typedef unsigned char rt_uint8_t;
typedef signed short rt_int16_t;
typedef unsigned short rt_uint16_t;
typedef signed long rt_int32_t;
typedef unsigned long rt_uint32_t;
typedef unsigned int rt_size_t;
/* 硬件栈限于低地址 EDATA；普通内核数据放 XDATA，节省栈空间。 */
#define RT_STACK_STORAGE edata
#define RT_DATA_STORAGE xdata
#else
#define RT_STACK_STORAGE
#define RT_DATA_STORAGE
#endif
#endif
