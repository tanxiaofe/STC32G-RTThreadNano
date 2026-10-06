/* SPDX-License-Identifier: Apache-2.0
 * Actual kernel IPC/heap/timer/lifecycle tests, with mocked CPU switching.
 * Hooks emulate the other task's action while the caller is suspended.
 * This is not execution of C251 tasks or a measurement of hardware stack use.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <rtthread.h>
#include <rthw.h>
#include "cpuport.h"

static rt_base_t irq_enabled;
static void (*switch_hook)(void);
static void (*last_exit)(void);
static struct rt_thread high,low,idle;
static unsigned char high_stack[512],low_stack[512],idle_stack[512];
static unsigned char heap[2048];
static struct rt_event event;
static struct rt_messagequeue queue;
static struct rt_mutex mutex;
static rt_uint32_t message_pool[8];
static int cleanup_calls,timer_calls;
extern void rt_test_defunct_cleanup(void);
extern void rt_soft_timer_check(void);

void rt_hw_console_output(const char *s){(void)s;}
void rt_console_lock(void){}
void rt_console_unlock(void){}
rt_base_t rt_hw_interrupt_disable(void)
{rt_base_t previous=irq_enabled;irq_enabled=0;return previous;}
void rt_hw_interrupt_enable(rt_base_t level){irq_enabled=level;}
rt_uint8_t *rt_hw_stack_init(void *entry,void *parameter,rt_uint8_t *base,void *exit_fn)
{(void)entry;(void)parameter;last_exit=(void(*)(void))exit_fn;return base+47;}
static void switched(void)
{void (*hook)(void)=switch_hook;switch_hook=0;if(hook)hook();}
void rt_hw_context_switch(rt_ubase_t from,rt_ubase_t to)
{(void)from;(void)to;switched();}
void rt_hw_context_switch_interrupt(rt_ubase_t from,rt_ubase_t to)
{(void)from;(void)to;switched();}
void rt_hw_context_switch_to(rt_ubase_t to){(void)to;irq_enabled=1;}
static void entry(void *parameter){(void)parameter;assert(!"Task bodies are not executed by the mock CPU");}
static void tick(void){rt_interrupt_enter();rt_tick_increase();rt_interrupt_leave();}
static void ticks(unsigned int n){while(n--)tick();}
static rt_size_t heap_used(void)
{rt_size_t total,used,maximum;rt_memory_info(&total,&used,&maximum);return used;}
static void send_event(void)
{assert(rt_thread_self()==&low);assert((high.stat&RT_THREAD_STAT_MASK)==RT_THREAD_SUSPEND);assert(rt_event_send(&event,3)==RT_EOK);}
static void timeout(void){ticks(3);}
static void send_message(void)
{rt_uint32_t value=42;assert(rt_thread_self()==&low);assert(rt_mq_send(&queue,&value,sizeof(value))==RT_EOK);}
static void receive_message(void)
{rt_uint32_t value;assert(rt_thread_self()==&low);assert(rt_mq_recv(&queue,&value,sizeof(value),0)==RT_EOK);assert(value==10);}
static void release_mutex(void)
{assert(rt_thread_self()==&low);assert(low.current_priority==1);assert(rt_mutex_release(&mutex)==RT_EOK);}
static void cleanup(struct rt_thread *thread)
{assert((thread->stat&RT_THREAD_STAT_MASK)==RT_THREAD_CLOSE);assert(rt_port_dynamic_stack_busy());cleanup_calls++;}
static void timer_callback(void *parameter)
{assert(parameter==&timer_calls);assert(rt_interrupt_get_nest()==0);assert(irq_enabled);timer_calls++;}

static void test_events(void)
{
    rt_uint32_t received=0;
    assert(rt_event_init(&event,"event",RT_IPC_FLAG_PRIO)==RT_EOK);
    assert(rt_event_recv(&event,1,RT_EVENT_FLAG_OR,0,&received)==-RT_ETIMEOUT);
    assert(rt_event_send(&event,5)==RT_EOK);
    assert(rt_event_recv(&event,3,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,0,&received)==RT_EOK);
    assert(received==1&&event.set==4);
    assert(rt_event_recv(&event,6,RT_EVENT_FLAG_AND,0,&received)==-RT_ETIMEOUT);
    assert(rt_event_send(&event,2)==RT_EOK);
    assert(rt_event_recv(&event,6,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,0,&received)==RT_EOK);
    assert(received==6&&event.set==0);
    switch_hook=send_event;
    assert(rt_event_recv(&event,3,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,RT_WAITING_FOREVER,&received)==RT_EOK);
    assert(received==3&&event.set==0&&rt_thread_self()==&high);
    switch_hook=timeout;
    assert(rt_event_recv(&event,8,RT_EVENT_FLAG_OR,3,&received)==-RT_ETIMEOUT);
    assert(rt_thread_self()==&high&&rt_list_isempty(&event.parent.suspend_thread));
    assert(rt_event_detach(&event)==RT_EOK);
    puts("PASS: event OR/AND/CLEAR, blocking wakeup, timeout and wait-list cleanup");
}

static void test_queue(void)
{
    rt_uint32_t value,received;unsigned int i;
    assert(rt_mq_init(&queue,"queue",message_pool,sizeof(value),sizeof(message_pool),RT_IPC_FLAG_PRIO)==RT_EOK);
    assert(queue.max_msgs==4);
    assert(rt_mq_recv(&queue,&received,sizeof(received),0)==-RT_ETIMEOUT);
    value=10;assert(rt_mq_send(&queue,&value,sizeof(value))==RT_EOK);
    value=20;assert(rt_mq_send(&queue,&value,sizeof(value))==RT_EOK);
    value=5;assert(rt_mq_urgent(&queue,&value,sizeof(value))==RT_EOK);
    assert(rt_mq_recv(&queue,&received,sizeof(received),0)==RT_EOK&&received==5);
    assert(rt_mq_recv(&queue,&received,sizeof(received),0)==RT_EOK&&received==10);
    assert(rt_mq_recv(&queue,&received,sizeof(received),0)==RT_EOK&&received==20);
    switch_hook=send_message;
    assert(rt_mq_recv(&queue,&received,sizeof(received),RT_WAITING_FOREVER)==RT_EOK&&received==42);
    assert(rt_thread_self()==&high);
    switch_hook=timeout;
    assert(rt_mq_recv(&queue,&received,sizeof(received),3)==-RT_ETIMEOUT);
    for(i=0;i<4;i++){value=10+i;assert(rt_mq_send(&queue,&value,sizeof(value))==RT_EOK);}
    value=99;assert(rt_mq_send(&queue,&value,sizeof(value))==-RT_EFULL);
    switch_hook=receive_message;
    assert(rt_mq_send_wait(&queue,&value,sizeof(value),RT_WAITING_FOREVER)==RT_EOK);
    assert(queue.entry==4&&rt_thread_self()==&high);
    switch_hook=timeout;
    assert(rt_mq_send_wait(&queue,&value,sizeof(value),3)==-RT_ETIMEOUT);
    for(i=0;i<4;i++)
    {assert(rt_mq_recv(&queue,&received,sizeof(received),0)==RT_EOK);assert(received==(i<3?11+i:99));}
    assert(queue.entry==0&&rt_list_isempty(&queue.suspend_sender_thread));
    assert(rt_mq_send(&queue,&value,sizeof(value)+1)==-RT_ERROR);
    assert(rt_mq_detach(&queue)==RT_EOK);
    puts("PASS: message queue FIFO/urgent/full/oversize, blocked sender/receiver and timeouts");
}

static void test_mutex(void)
{
    assert(rt_mutex_init(&mutex,"mutex",RT_IPC_FLAG_PRIO)==RT_EOK);
    assert(rt_mutex_take(&mutex,0)==RT_EOK&&mutex.owner==&high);
    assert(rt_mutex_take(&mutex,0)==RT_EOK&&mutex.hold==2);
    assert(rt_mutex_release(&mutex)==RT_EOK&&mutex.hold==1);
    assert(rt_mutex_release(&mutex)==RT_EOK&&mutex.owner==RT_NULL);
    rt_thread_suspend(&high);rt_schedule();assert(rt_thread_self()==&low);
    assert(rt_mutex_take(&mutex,0)==RT_EOK);
    rt_thread_resume(&high);rt_schedule();assert(rt_thread_self()==&high);
    assert(rt_mutex_release(&mutex)==-RT_ERROR); /* only owner may release */
    switch_hook=release_mutex;
    assert(rt_mutex_take(&mutex,RT_WAITING_FOREVER)==RT_EOK);
    assert(mutex.owner==&high&&low.current_priority==4);
    assert(rt_mutex_release(&mutex)==RT_EOK);
    rt_thread_suspend(&high);rt_schedule();assert(rt_mutex_take(&mutex,0)==RT_EOK);
    rt_thread_resume(&high);rt_schedule();
    assert(rt_mutex_take(&mutex,0)==-RT_ETIMEOUT);
    switch_hook=timeout;
    assert(rt_mutex_take(&mutex,3)==-RT_ETIMEOUT);
    /* Nano 4.1.1 restores inherited priority when the owner releases. */
    rt_thread_suspend(&high);rt_schedule();assert(rt_thread_self()==&low);
    assert(rt_mutex_release(&mutex)==RT_EOK&&low.current_priority==4);
    rt_thread_resume(&high);rt_schedule();
    assert(rt_mutex_detach(&mutex)==RT_EOK);
    puts("PASS: mutex recursion, ownership, priority inheritance/restoration and timeout");
}

static void test_heap_and_dynamic(void)
{
    rt_size_t baseline=heap_used();unsigned char *p,*q;unsigned int i;
    void *blocks[128];unsigned int count=0;rt_thread_t thread;void (*exit_fn)(void);
    p=rt_calloc(8,4);assert(p);for(i=0;i<32;i++)assert(p[i]==0);
    memset(p,0x5a,32);q=rt_realloc(p,96);assert(q);
    for(i=0;i<32;i++)assert(q[i]==0x5a);
    p=rt_realloc(q,16);assert(p);for(i=0;i<16;i++)assert(p[i]==0x5a);
    assert(rt_realloc(p,(rt_size_t)-1)==RT_NULL);
    for(i=0;i<16;i++)assert(p[i]==0x5a); /* failure must retain original block */
    rt_free(p);assert(heap_used()==baseline);
    assert(rt_malloc((rt_size_t)-1)==RT_NULL);
    assert(rt_calloc(((rt_size_t)-1)/2+1,2)==RT_NULL);
    assert(heap_used()==baseline);
    while(count<128&&(blocks[count]=rt_malloc(64))!=RT_NULL)count++;
    assert(count>0&&count<128);
    assert(rt_thread_create("noheap",entry,0,512,2,5)==RT_NULL);
    assert(!rt_port_dynamic_stack_busy());
    for(i=0;i<count;i++)rt_free(blocks[i]);assert(heap_used()==baseline);
    assert(rt_thread_create("bad",entry,0,127,2,5)==RT_NULL);
    assert(rt_thread_create("bad",entry,0,513,2,5)==RT_NULL);
    assert(rt_thread_create("bad",entry,0,512,8,5)==RT_NULL);
    assert(rt_thread_create("bad",RT_NULL,0,512,2,5)==RT_NULL);
    assert(rt_thread_create("bad",entry,0,512,2,0)==RT_NULL);
    assert(heap_used()==baseline);
    for(i=0;i<50;i++)
    {
        rt_size_t allocated;
        thread=rt_thread_create("dynamic",entry,0,i%2?128:512,2,5);assert(thread);
        assert(rt_port_dynamic_stack_busy()&&rt_port_dynamic_stack_guard());
        assert(!rt_object_is_systemobject((rt_object_t)thread));
        allocated=heap_used();assert(allocated>baseline);
        assert(rt_thread_create("busy",entry,0,512,2,5)==RT_NULL);
        assert(heap_used()==allocated); /* failed allocation rolls back TCB */
        rt_thread_stack_free(&mutex);assert(rt_port_dynamic_stack_busy());
        thread->cleanup=cleanup;assert(rt_thread_startup(thread)==RT_EOK);
        assert(rt_thread_find("dynamic")==thread);
        assert(rt_thread_delete(thread)==RT_EOK);
        assert(rt_port_dynamic_stack_busy()); /* deferred, not freed on caller's stack */
        rt_test_defunct_cleanup();
        assert(!rt_port_dynamic_stack_busy()&&heap_used()==baseline);
        assert(rt_thread_find("dynamic")==RT_NULL);
    }
    /* Execute the real kernel exit callback supplied to the CPU trampoline. */
    thread=rt_thread_create("returns",entry,0,512,0,5);assert(thread);exit_fn=last_exit;
    thread->cleanup=cleanup;rt_thread_startup(thread);assert(rt_thread_self()==thread);
    exit_fn();assert(rt_thread_self()==&high);assert(rt_port_dynamic_stack_busy());
    rt_test_defunct_cleanup();assert(!rt_port_dynamic_stack_busy()&&heap_used()==baseline);
    assert(cleanup_calls==51);
    puts("PASS: small heap calloc/realloc/exhaustion/coalescing; dynamic creation validation, rollback, 50 delete/reuse cycles and natural exit/idle reclamation");
}

static void test_soft_timers(void)
{
    struct rt_timer timer;rt_tick_t period=3;
    rt_system_timer_thread_init();assert(rt_thread_find("timer"));
    rt_tick_set(100);
    rt_timer_init(&timer,"soft",timer_callback,&timer_calls,2,RT_TIMER_FLAG_SOFT_TIMER|RT_TIMER_FLAG_PERIODIC);
    assert(rt_timer_start(&timer)==RT_EOK);
    ticks(2);assert(timer_calls==0); /* hard IRQ must not invoke soft callback */
    rt_soft_timer_check();assert(timer_calls==1);
    ticks(2);rt_soft_timer_check();assert(timer_calls==2);
    assert(rt_timer_stop(&timer)==RT_EOK);ticks(4);rt_soft_timer_check();assert(timer_calls==2);
    assert(rt_timer_control(&timer,RT_TIMER_CTRL_SET_TIME,&period)==RT_EOK);
    assert(rt_timer_control(&timer,RT_TIMER_CTRL_SET_ONESHOT,RT_NULL)==RT_EOK);
    assert(rt_timer_start(&timer)==RT_EOK);ticks(2);rt_soft_timer_check();assert(timer_calls==2);
    tick();rt_soft_timer_check();assert(timer_calls==3);
    ticks(4);rt_soft_timer_check();assert(timer_calls==3);
    rt_tick_set(0xfffffffeU);assert(rt_timer_start(&timer)==RT_EOK);
    ticks(3);assert(rt_tick_get()==1);rt_soft_timer_check();assert(timer_calls==4);
    assert(rt_timer_detach(&timer)==RT_EOK);
    puts("PASS: real software timer thread registration, dispatch outside IRQ, periodic/stop/period/oneshot and tick wrap");
}

static void test_dynamic_ipc(void)
{
    rt_size_t baseline=heap_used();unsigned int i;rt_uint32_t value=55,received;
    rt_event_t e;rt_mutex_t m;rt_mq_t q;rt_timer_t t;
    for(i=0;i<10;i++)
    {
        e=rt_event_create("alloc_ev",RT_IPC_FLAG_PRIO);assert(e);
        m=rt_mutex_create("alloc_mu",RT_IPC_FLAG_PRIO);assert(m);
        q=rt_mq_create("alloc_mq",sizeof(value),4,RT_IPC_FLAG_PRIO);assert(q);
        t=rt_timer_create("alloc_tm",timer_callback,&timer_calls,2,RT_TIMER_FLAG_SOFT_TIMER);assert(t);
        assert(rt_event_send(e,2)==RT_EOK);
        assert(rt_event_recv(e,2,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,0,&received)==RT_EOK&&received==2);
        assert(rt_mutex_take(m,0)==RT_EOK&&rt_mutex_release(m)==RT_EOK);
        assert(rt_mq_send(q,&value,sizeof(value))==RT_EOK);
        assert(rt_mq_recv(q,&received,sizeof(received),0)==RT_EOK&&received==value);
        assert(rt_timer_start(t)==RT_EOK&&rt_timer_delete(t)==RT_EOK);
        assert(rt_mq_delete(q)==RT_EOK&&rt_mutex_delete(m)==RT_EOK&&rt_event_delete(e)==RT_EOK);
        assert(heap_used()==baseline&&!rt_port_dynamic_stack_busy());
    }
    puts("PASS: heap-backed event/mutex/queue/software-timer create/use/delete, ten cycles without leaks");
}

int main(void)
{
    assert(sizeof(void*)==4);rt_system_timer_init();rt_system_scheduler_init();
    rt_system_heap_init(heap,heap+sizeof(heap));
    assert(rt_thread_init(&high,"high",entry,0,high_stack,sizeof(high_stack),1,10)==RT_EOK);
    assert(rt_thread_init(&low,"low",entry,0,low_stack,sizeof(low_stack),4,10)==RT_EOK);
    assert(rt_thread_init(&idle,"idle",entry,0,idle_stack,sizeof(idle_stack),7,10)==RT_EOK);
    rt_thread_startup(&high);rt_thread_startup(&low);rt_thread_startup(&idle);
    rt_system_scheduler_start();assert(rt_thread_self()==&high);
    test_events();test_queue();test_mutex();test_heap_and_dynamic();test_soft_timers();test_dynamic_ipc();
    puts("PASS: expanded RT-Thread Nano components (native kernel logic; hardware execution remains to be tested)");return 0;
}
