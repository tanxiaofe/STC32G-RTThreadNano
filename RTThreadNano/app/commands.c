/* SPDX-License-Identifier: Apache-2.0 */
#include <rthw.h>
#include "finsh.h"
#include "shell.h"
#include "demo.h"
#include "cpuport.h"
/* 解析有界十进制数，拒绝非法字符。 */
static int number(const char *s)
{
    unsigned int n=0;
    if(!*s)return -1;
    while(*s){if(*s<'0'||*s>'9'||n>1000)return -1;n=n*10+(*s++-'0');}
    return n;
}
/* 扫描栈高地址的填充值，估算历史占用量；并非实时精确栈深度。 */
static unsigned int stack_used(rt_thread_t t)
{
    unsigned int free_bytes=0;
    unsigned char *end=(unsigned char*)t->stack_addr+t->stack_size;
    while(free_bytes<t->stack_size && end[-1-free_bytes]=='#')free_bytes++;
    return (unsigned int)(t->stack_size-free_bytes);
}
/* 演示线程均为静态且不销毁，遍历对象链表期间地址保持有效。 */
static int cmd_ps(int argc,char **argv)
{
    struct rt_object_information *info;
    rt_list_t *node;
    rt_thread_t t;
    char name[RT_NAME_MAX+1];
    unsigned int priority,state,used,size;
    rt_base_t level;
    RT_UNUSED(argc);RT_UNUSED(argv);
    info=rt_object_get_information(RT_Object_Class_Thread);
    rt_kprintf("thread   prio state stack used/size\n");
    /* Only static never-exiting demo tasks are registered. */
    for(node=info->object_list.next;node!=&info->object_list;node=node->next)
    {
        t=rt_list_entry(node,struct rt_thread,list);
        level=rt_hw_interrupt_disable();
        rt_memcpy(name,t->name,RT_NAME_MAX);name[RT_NAME_MAX]=0;
        priority=t->current_priority;state=t->stat&RT_THREAD_STAT_MASK;
        size=(unsigned int)t->stack_size;
        rt_hw_interrupt_enable(level);
        used=stack_used(t);
        rt_kprintf("%-8s %u    %u     %u/%u\n",name,priority,state,used,size);
    }
    rt_kprintf("state: ready=1 suspend=2 running=3; stack grows upward\n");
    return 0;
}
static int cmd_tick(int argc,char **argv)
{RT_UNUSED(argc);RT_UNUSED(argv);rt_kprintf("tick=%lu (100Hz)\n",(unsigned long)rt_tick_get());return 0;}
static int cmd_version(int argc,char **argv)
{RT_UNUSED(argc);RT_UNUSED(argv);rt_show_version();rt_kprintf("STC32G12K128 / C251 35MHz / USB CDC\n");return 0;}
static int cmd_stat(int argc,char **argv)
{
    rt_uint32_t beat,key,frame;rt_uint16_t milliseconds;
    RT_UNUSED(argc);RT_UNUSED(argv);
    demo_stats(&beat,&key,&frame,&milliseconds);
    rt_kprintf("beat=%lu keys=%lu lcd=%lu period=%u ms fault=%u\n",
               (unsigned long)beat,(unsigned long)key,(unsigned long)frame,
               (unsigned int)milliseconds,(unsigned int)rt_port_fault);
    return 0;
}
/* 周期限制为 100..1000 ms，且为 10 ms 整数倍。 */
static int cmd_beat(int argc,char **argv)
{
    int milliseconds;
    if(argc!=2 || (milliseconds=number(argv[1]))<100 || milliseconds>1000 || milliseconds%10)
    {rt_kprintf("usage: beat <100..1000 ms, step 10>\n");return -1;}
    demo_beat_period((rt_uint16_t)milliseconds);
    rt_kprintf("beat period=%u ms\n",(unsigned int)milliseconds);return 0;
}
static int cmd_reset(int argc,char **argv)
{RT_UNUSED(argc);RT_UNUSED(argv);demo_reset();rt_kprintf("demo counters reset\n");return 0;}
static int cmd_clear(int argc,char **argv)
{RT_UNUSED(argc);RT_UNUSED(argv);rt_kprintf("\033[2J\033[H");return 0;}
static int cmd_echo(int argc,char **argv)
{int i;for(i=1;i<argc;i++)rt_kprintf("%s%s",argv[i],i+1<argc?" ":"\n");return 0;}
extern int msh_help(int argc,char **argv);
/* C251 不自动收集链接段命令，改为显式注册表；终端显示字符串保留 ASCII。 */
static const struct finsh_syscall commands[]={
    {"help","List commands",(syscall_func)msh_help},
    {"ps","List threads and stack usage",(syscall_func)cmd_ps},
    {"tick","Read RTOS tick",(syscall_func)cmd_tick},
    {"version","Kernel and board version",(syscall_func)cmd_version},
    {"stat","Demo counters",(syscall_func)cmd_stat},
    {"beat","Set heartbeat period in ms",(syscall_func)cmd_beat},
    {"reset","Reset demo counters",(syscall_func)cmd_reset},
    {"clear","Clear ANSI terminal",(syscall_func)cmd_clear},
    {"echo","Echo arguments; quotes supported",(syscall_func)cmd_echo}
};
/* 提供命令表首尾地址，交给原版 FinSH MSH 查找和生成帮助。 */
void stc_finsh_table_init(void)
{
    finsh_system_function_init(commands,commands+sizeof(commands)/sizeof(commands[0]));
}
