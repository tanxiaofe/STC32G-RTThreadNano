/* SPDX-License-Identifier: Apache-2.0 */
#include <rthw.h>
#include "finsh.h"
#include "shell.h"
#include "demo.h"
#include "cpuport.h"
#include "gui.h"
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
/* 锁调度复制快照，解锁后打印；防止输出阻塞时 idle 回收动态控制块。 */
struct thread_row {char name[RT_NAME_MAX+1];unsigned int priority,state,used,size;};
static struct thread_row RT_DATA_STORAGE thread_rows[8];
static int cmd_ps(int argc,char **argv)
{
    struct rt_object_information *info;rt_list_t *node;rt_thread_t t;
    unsigned int count=0,i;
    RT_UNUSED(argc);RT_UNUSED(argv);
    info=rt_object_get_information(RT_Object_Class_Thread);
    rt_enter_critical();
    for(node=info->object_list.next;node!=&info->object_list&&count<8;node=node->next)
    {
        t=rt_list_entry(node,struct rt_thread,list);
        rt_memcpy(thread_rows[count].name,t->name,RT_NAME_MAX);
        thread_rows[count].name[RT_NAME_MAX]=0;
        thread_rows[count].priority=t->current_priority;
        thread_rows[count].state=t->stat&RT_THREAD_STAT_MASK;
        thread_rows[count].size=(unsigned int)t->stack_size;
        thread_rows[count].used=stack_used(t);count++;
    }
    rt_exit_critical();
    rt_kprintf("thread   prio state stack used/size\n");
    for(i=0;i<count;i++)
        rt_kprintf("%-8s %u    %u     %u/%u\n",thread_rows[i].name,
                   thread_rows[i].priority,thread_rows[i].state,thread_rows[i].used,thread_rows[i].size);
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
/* 演示线程实际使用 IPC；命令读取快照或控制软件定时器、动态心跳。 */
static int cmd_ipc(int argc,char **argv)
{
    rt_uint32_t values[8];RT_UNUSED(argc);RT_UNUSED(argv);demo_component_stats(values);
    rt_kprintf("event: OR|CLEAR beat/key/timer; mutex: counts\n");
    rt_kprintf("mq sent=%lu recv=%lu drop=%lu pending=%lu last=%lu\n",
       (unsigned long)values[1],(unsigned long)values[2],(unsigned long)values[3],
       (unsigned long)values[5],(unsigned long)values[4]);
    rt_kprintf("soft timer=%s hits=%lu dynamic beat=%s\n",values[6]?"on":"off",
       (unsigned long)values[0],values[7]?"running/stopping":"stopped");return 0;
}
static int cmd_timer(int argc,char **argv)
{
    int milliseconds;rt_err_t result;
    if(argc!=2){rt_kprintf("usage: timer on|off|<10..1000 ms, step 10>\n");return -1;}
    if(!rt_strcmp(argv[1],"on"))milliseconds=200;
    else if(!rt_strcmp(argv[1],"off"))milliseconds=0;
    else {milliseconds=number(argv[1]);if(milliseconds<10||milliseconds>1000||milliseconds%10)return -1;}
    result=demo_soft_timer((rt_uint16_t)milliseconds);
    rt_kprintf("soft timer period=%u ms result=%d\n",(unsigned int)milliseconds,(int)result);
    return result;
}
static int cmd_dyn(int argc,char **argv)
{
    rt_uint32_t values[8];rt_err_t result;
    if(argc!=2){rt_kprintf("usage: dyn start|stop|status\n");return -1;}
    if(!rt_strcmp(argv[1],"start"))
    {
        result=demo_dynamic_start();rt_kprintf("dynamic start=%d; one EDATA stack slot\n",(int)result);
        return result;
    }
    if(!rt_strcmp(argv[1],"stop"))
    {demo_dynamic_stop();rt_kprintf("stop requested; wait <=1s for beat exit + idle reclamation\n");return 0;}
    if(!rt_strcmp(argv[1],"status"))
    {
        demo_component_stats(values);
        rt_kprintf("beat=%s EDATA stack=%s (512 B max)\n",values[7]?"running/stopping":"stopped",
                   rt_port_dynamic_stack_busy()?"busy":"free");return 0;
    }
    return -1;
}
static int cmd_mem(int argc,char **argv)
{
    rt_size_t total,used,maximum;RT_UNUSED(argc);RT_UNUSED(argv);
    rt_memory_info(&total,&used,&maximum);
    rt_kprintf("XDATA heap total=%lu used=%lu max=%lu free=%lu\n",
       (unsigned long)total,(unsigned long)used,(unsigned long)maximum,(unsigned long)(total-used));return 0;
}
/* USB 上也可切换 LCD 页面；绘图仍由 LCD 线程完成。 */
static int cmd_page(int argc,char **argv)
{
    int value;
    if(argc==1){rt_kprintf("page=%u (0=status 1=ipc 2=msh)\n",(unsigned int)gui_page());return 0;}
    if(argc!=2)return -1;
    if(!rt_strcmp(argv[1],"status"))value=0;
    else if(!rt_strcmp(argv[1],"ipc"))value=1;
    else if(!rt_strcmp(argv[1],"msh"))value=2;
    else value=number(argv[1]);
    if(value<0||value>2){rt_kprintf("usage: page status|ipc|msh|0|1|2\n");return -1;}
    gui_set_page((unsigned char)value);rt_kprintf("LCD page=%u\n",(unsigned int)value);return 0;
}
extern int msh_help(int argc,char **argv);
/* C251 不自动收集链接段命令，改为显式注册表；终端显示字符串保留 ASCII。 */
static const struct finsh_syscall commands[]={
    {"help","List commands",(syscall_func)msh_help},
    {"ps","List threads and stack usage",(syscall_func)cmd_ps},
    {"tick","Read RTOS tick",(syscall_func)cmd_tick},
    {"version","Kernel and board version",(syscall_func)cmd_version},
    {"stat","Demo counters",(syscall_func)cmd_stat},
    {"beat","Set heartbeat period in ms",(syscall_func)cmd_beat},
    {"page","LCD status|ipc|msh page",(syscall_func)cmd_page},
    {"ipc","Event, queue, mutex and timer status",(syscall_func)cmd_ipc},
    {"timer","Software timer on|off|milliseconds",(syscall_func)cmd_timer},
    {"dyn","Dynamic beat start|stop|status",(syscall_func)cmd_dyn},
    {"mem","XDATA heap usage",(syscall_func)cmd_mem},
    {"reset","Reset demo counters",(syscall_func)cmd_reset},
    {"clear","Clear ANSI terminal",(syscall_func)cmd_clear},
    {"echo","Echo arguments; quotes supported",(syscall_func)cmd_echo}
};
/* 提供命令表首尾地址，交给原版 FinSH MSH 查找和生成帮助。 */
void stc_finsh_table_init(void)
{
    finsh_system_function_init(commands,commands+sizeof(commands)/sizeof(commands[0]));
}
