/* SPDX-License-Identifier: Apache-2.0
 * Native kernel state-machine tests. No real C251 task execution here. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "shell.h"
#include <rthw.h>
#include <rtthread.h>
#include "finsh.h"
#include "msh.h"
static char console_capture[4096];
void rt_hw_console_output(const char *s){strncat(console_capture,s,sizeof(console_capture)-strlen(console_capture)-1);}
volatile unsigned char rt_port_fault;
static rt_uint16_t demo_period=500;

void demo_stats(rt_uint32_t *b,rt_uint32_t *k,rt_uint32_t *f,rt_uint16_t *ms)
{*b=12;*k=34;*f=56;*ms=demo_period;}
void demo_beat_period(rt_uint16_t ms){demo_period=ms;}
void demo_reset(void){}
static unsigned char lcd_page;
void gui_set_page(unsigned char p){lcd_page=p;}
unsigned char gui_page(void){return lcd_page;}
static int dyn_running=1;static rt_uint16_t timer_period=200;
void demo_component_stats(rt_uint32_t *v){int i;for(i=0;i<8;i++)v[i]=i;v[6]=timer_period!=0;v[7]=dyn_running;}
rt_err_t demo_dynamic_start(void){if(dyn_running)return -RT_ERROR;dyn_running=1;return RT_EOK;}
void demo_dynamic_stop(void){dyn_running=0;}
rt_err_t demo_soft_timer(rt_uint16_t ms){timer_period=ms;return RT_EOK;}
static unsigned char host_heap[4096];
extern void stc_finsh_table_init(void);
extern void finsh_thread_entry(void *parameter);
static const char *terminal_input;
static jmp_buf terminal_end;
char rt_hw_console_getchar(void){if(!*terminal_input)longjmp(terminal_end,1);return *terminal_input++;}
static void terminal(const char *text)
{terminal_input=text;console_capture[0]=0;if(!setjmp(terminal_end))finsh_thread_entry(0);}
static int command(const char *text)
{char line[128];strcpy(line,text);console_capture[0]=0;return msh_exec(line,strlen(line));}
void rt_console_lock(void){}
void rt_console_unlock(void){}
static rt_base_t irq_enabled;
static int normal_switches,irq_switches;
static struct rt_thread a,b,c,idle_test;
static unsigned char sa[512],sb[512],sc[512],si[512];
static struct rt_semaphore sem;
static struct rt_timer timer;
static int timer_fired;
rt_base_t rt_hw_interrupt_disable(void){rt_base_t x=irq_enabled;irq_enabled=0;return x;}
void rt_hw_interrupt_enable(rt_base_t x){irq_enabled=x;}
rt_uint8_t *rt_hw_stack_init(void *entry,void *param,rt_uint8_t *base,void *exit_fn)
{(void)entry;(void)param;(void)exit_fn;return base+48;}
void rt_hw_context_switch(rt_ubase_t from,rt_ubase_t to)
{(void)from;(void)to;normal_switches++;}
void rt_hw_context_switch_to(rt_ubase_t to){(void)to;irq_enabled=1;}
void rt_hw_context_switch_interrupt(rt_ubase_t from,rt_ubase_t to)
{(void)from;(void)to;irq_switches++;}
static void entry(void *p){(void)p;assert(!"mock must never execute task entry");}
static void fired(void *p){(void)p;timer_fired++;}
static void tick(void){rt_interrupt_enter();rt_tick_increase();rt_interrupt_leave();}
int main(void)
{
 int i;char text[32];
 assert(sizeof(void*)==4&&sizeof(rt_uint32_t)==4);
 for(i=0;i<32;i++)assert(__rt_ffs((rt_uint32_t)1<<i)==i+1);
 assert(__rt_ffs(0)==0);assert(__rt_ffs(0x80000004U)==3);
 rt_system_timer_init();rt_system_scheduler_init();rt_system_heap_init(host_heap,host_heap+sizeof(host_heap));
 assert(rt_thread_init(&a,"high",entry,0,sa,sizeof(sa),1,2)==RT_EOK);
 assert(rt_thread_init(&b,"peer",entry,0,sb,sizeof(sb),1,2)==RT_EOK);
 assert(rt_thread_init(&c,"low",entry,0,sc,sizeof(sc),4,2)==RT_EOK);
 assert(rt_thread_init(&idle_test,"idle",entry,0,si,sizeof(si),7,2)==RT_EOK);
 rt_thread_startup(&b);rt_thread_startup(&a);rt_thread_startup(&c);rt_thread_startup(&idle_test);
 rt_system_scheduler_start();assert(rt_thread_self()==&a);
 tick();assert(rt_thread_self()==&a);
 tick();assert(rt_thread_self()==&b&&irq_switches>0);
 rt_thread_yield();assert(rt_thread_self()==&a&&normal_switches>0);
 rt_thread_delay(3);assert(rt_thread_self()==&b);
 rt_thread_delay(5);assert(rt_thread_self()==&c);
 tick();tick();assert(rt_thread_self()==&c);
 tick();assert(rt_thread_self()==&a); /* timer wakeup preempts low priority */
 assert(rt_sem_init(&sem,"test",0,RT_IPC_FLAG_PRIO)==RT_EOK);
 rt_sem_take(&sem,RT_WAITING_FOREVER);assert(rt_thread_self()==&c);
 assert((a.stat&RT_THREAD_STAT_MASK)==RT_THREAD_SUSPEND);
 rt_sem_release(&sem);assert(rt_thread_self()==&a);
 assert(a.error==RT_EOK);
 rt_timer_init(&timer,"oneshot",fired,0,2,RT_TIMER_FLAG_ONE_SHOT);
 rt_timer_start(&timer);tick();assert(timer_fired==0);tick();assert(timer_fired==1);
 tick();tick();assert(timer_fired==1);
 rt_tick_set(0xfffffffeU);assert(rt_tick_get()==0xfffffffeU);
 tick();assert(rt_tick_get()==0xffffffffU);tick();assert(rt_tick_get()==0);
 rt_snprintf(text,sizeof(text),"%u %lu",(unsigned int)123,(unsigned long)456);
 assert(rt_strcmp(text,"123 456")==0);
 stc_finsh_table_init();
 assert(command("help")==0&&strstr(console_capture,"beat"));
 assert(command("echo one \"two words\"")==0&&strstr(console_capture,"one two words"));
 assert(command("beat 200")==0&&demo_period==200);
 assert(command("beat 1")==-1&&demo_period==200);
 assert(command("stat")==0&&strstr(console_capture,"beat=12 keys=34 lcd=56 period=200"));
 assert(command("ps")==0&&strstr(console_capture,"high")&&strstr(console_capture,"peer"));
 assert(command("page msh")==0&&lcd_page==2);
 assert(command("page 3")==-1&&lcd_page==2);
 assert(command("page ipc")==0&&lcd_page==1);
 assert(command("page status")==0&&lcd_page==0);
 assert(command("ipc")==0&&strstr(console_capture,"mq sent="));
 assert(command("timer off")==0&&timer_period==0);
 assert(command("timer 150")==0&&timer_period==150);
 assert(command("timer 11")==-1&&timer_period==150);
 assert(command("timer on")==0&&timer_period==200);
 assert(command("dyn stop")==0&&dyn_running==0);
 assert(command("dyn status")==0&&strstr(console_capture,"stopped"));
 assert(command("dyn start")==0&&dyn_running==1);
 assert(command("dyn start")==-RT_ERROR);
 assert(command("dyn bad")==-1);
 assert(command("mem")==0&&strstr(console_capture,"XDATA heap total="));
 assert(command("missing_cmd")==-1);
 assert(finsh_system_init()==RT_EOK);
 terminal("echo hello\r");assert(strstr(console_capture,"hello\nmsh >"));
 terminal("echo a\bB\r");assert(strstr(console_capture,"B\nmsh >"));
 terminal("sta\t\r");assert(strstr(console_capture,"beat=12"));
 terminal("beat 9\003stat\r");assert(demo_period==200&&strstr(console_capture,"period=200"));
 terminal("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAbeat 900\r");
 assert(demo_period==200&&strstr(console_capture,"Command too long; discarded."));
 terminal("beat 300\r\033[A\r");assert(demo_period==300);
 puts("PASS: real FinSH editor/backspace/tab/history/Ctrl-C/overflow discard");
 puts("PASS: MSH help/table/quoted args/beat validation/stat/ps/unknown command");
 puts("PASS: priorities, IRQ time slicing, yield, delay/preemption, semaphore wakeup, timer, tick wrap, ffs32");
 return 0;
}
