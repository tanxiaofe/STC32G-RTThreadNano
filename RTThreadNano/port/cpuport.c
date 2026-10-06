/* SPDX-License-Identifier: Apache-2.0
 * STC32G C251 port. All C functions must be built FUNCTIONS(REENTRANT).
 * XSMALL generic data pointers are 32 bits; near function pointers 16 bits.
 */
#include <rthw.h>
#include "cpuport.h"
#include "STC32G.h"
/* 保存的是线程控制块中 sp 字段的地址，而不是栈顶本身；通用指针必须保留全部 32 位。 */
volatile rt_ubase_t rt_port_from;
volatile rt_ubase_t rt_port_to;
volatile rt_uint8_t rt_port_pending;
volatile rt_uint8_t rt_port_fault;
typedef char check_u32[(sizeof(rt_uint32_t)==4)?1:-1];
typedef char check_pointer[(sizeof(void*)==4)?1:-1];
typedef char check_function[(sizeof(void (*)(void))==2)?1:-1];

/* 故障时关闭中断并停机，可在调试器查看 rt_port_fault；避免错误栈继续运行。 */
void rt_port_panic(unsigned char reason)
{
    EA=0;
    rt_port_fault=reason;
    while (1) ; /* inspect rt_port_fault in debugger */
}

/* 临界区保存原 EA 状态，退出时恢复原值；嵌套调用不能直接无条件开中断。 */
rt_base_t rt_hw_interrupt_disable(void)
{
    rt_base_t previous=EA;
    EA=0;
    return previous;
}
void rt_hw_interrupt_enable(rt_base_t previous)
{
    if (previous) EA=1;
    else EA=0;
}

/* texit is kept below the CPU frame, so each thread owns its exit callback. */
/* 首次启动线程通过此入口读取控制块参数；栈底额外两个字节保存线程退出回调。 */
static void thread_trampoline(void)
{
    rt_thread_t thread=rt_thread_self();
    void (*entry)(void *);
    void (*exit_fn)(void);
    entry=(void (*)(void *))thread->entry;
    exit_fn=(void (*)(void))((rt_uint16_t*)thread->stack_addr)[0];
    entry(thread->parameter);
    exit_fn();
    rt_port_panic(3);
}

/* 构造与汇编恢复过程一致的 46 字节现场。硬件栈向高地址增长，只能放在 EDATA。 */
rt_uint8_t *rt_hw_stack_init(void *entry, void *parameter,
                           rt_uint8_t *stack_addr, void *texit)
{
    rt_uint8_t *frame=stack_addr+2;
    rt_uint16_t pc=(rt_uint16_t)thread_trampoline;
    RT_UNUSED(entry); RT_UNUSED(parameter);
    /* Hardware SP is restricted to bank 00; reject XDATA stacks. */
    if ((rt_ubase_t)stack_addr >= 0x1000UL) rt_port_panic(1);
    ((rt_uint16_t*)stack_addr)[0]=(rt_uint16_t)texit;
    rt_memset(frame,0,RT_PORT_FRAME_SIZE);
/* 帧偏移：0..3 为 PSW1/代码银行/PC；4..9 为状态寄存器；10..45 为 DR56 和 DR0..DR28。 */
    frame[0]=0;                    /* hardware PSW1 */
    frame[1]=0xff;                 /* code bank */
    frame[2]=(rt_uint8_t)pc;       /* PC low, then PC high */
    frame[3]=(rt_uint8_t)(pc>>8);
    frame[4]=0;                    /* PSW0 */
    frame[5]=0x82;                 /* EA + Timer0 enabled */
    frame[9]=P_SW2;
    frame[11]=1;                   /* DR56=00010000, DPXL=01 */
    return frame+RT_PORT_FRAME_SIZE-1; /* upward stack, last occupied byte */
}
/* 主动调度通过 LCALL 进入汇编；STC 的 TRAP 是空操作，不能用作软件中断。 */
void rt_hw_context_switch(rt_ubase_t from,rt_ubase_t to)
{
    rt_port_from=from; rt_port_to=to; rt_port_pending=1;
    rt_port_yield(); /* assembly expands LCALL frame; STC TRAP is a NOP */
}
void rt_hw_context_switch_to(rt_ubase_t to)
{
    EA=0; rt_port_from=0; rt_port_to=to; rt_port_pending=1;
    rt_port_yield();
    rt_port_panic(2);
}
/* 中断内只登记切换请求：保留最初的来源，更新最终目标，在汇编中断尾部切换。 */
void rt_hw_context_switch_interrupt(rt_ubase_t from,rt_ubase_t to)
{
    if (!rt_port_pending) rt_port_from=from;
    rt_port_to=to; rt_port_pending=1;
}
void rt_port_tick(void)
{
    rt_interrupt_enter();
    rt_tick_increase();
    rt_interrupt_leave();
}
/* C251 不支持这里所需的静态自引用链表初始化，因此启动时建立各对象链表。 */
void rt_port_object_init(void)
{
    rt_uint8_t i;
    struct rt_object_information *info;
    for (i=1;i<RT_Object_Class_Unknown;i++)
    {
        info=rt_object_get_information((enum rt_object_class_type)i);
        if (info) rt_list_init(&info->object_list);
    }
}

/* USB 与系统节拍共用完整现场保护；中断嵌套计数让内核识别中断上下文。 */
void rt_port_usb_irq(void)
{
    extern void usb_irq_service(void);
    rt_interrupt_enter();
    usb_irq_service();
    rt_interrupt_leave();
}
