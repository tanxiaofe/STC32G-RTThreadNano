; SPDX-License-Identifier: Apache-2.0
; C251 source mode, INTR=1. Timer0 and software yield share a 4-byte frame.
; STC implements TRAP as NOP, so normal LCALL frames are expanded in software.
; Stack is upward, generic saved SP is four bytes in big-endian memory.
; 现场共 46 字节：硬件返回帧 4 字节，软件保存 42 字节；弹栈顺序必须相反。
$MODSRC
$CASE
NAME STC_RT_CONTEXT
PSW0 DATA 0D0H
IE DATA 0A8H
DPS DATA 0E3H
DPL1 DATA 0E4H
DPH1 DATA 0E5H
P_SW2 DATA 0BAH
EA BIT 0AFH
EXTRN EDATA (rt_port_from,rt_port_to,rt_port_pending)
EXTRN CODE : NEAR (rt_port_tick?_,rt_port_usb_irq?_)
PUBLIC rt_port_yield?_
?RT?CONTEXT SEGMENT CODE
CSEG AT 000BH
    LJMP timer0_entry
CSEG AT 00CBH
    LJMP usb_entry
RSEG ?RT?CONTEXT
; 先保存 IE 再关闭 EA，保留线程原中断状态；硬件已经压入四字节返回帧。
timer0_entry:
    PUSH PSW0
    PUSH IE
    CLR EA
    PUSH DPS
    PUSH DPL1
    PUSH DPH1
    PUSH P_SW2
    PUSH DR56
    PUSH DR0
    PUSH DR4
    PUSH DR8
    PUSH DR12
    PUSH DR16
    PUSH DR20
    PUSH DR24
    PUSH DR28
    LCALL rt_port_tick?_
    SJMP switch_check

; USB 保存完整现场，支持在中断尾部执行内核请求的线程切换。
usb_entry:
    PUSH PSW0
    PUSH IE
    CLR EA
    PUSH DPS
    PUSH DPL1
    PUSH DPH1
    PUSH P_SW2
    PUSH DR56
    PUSH DR0
    PUSH DR4
    PUSH DR8
    PUSH DR12
    PUSH DR16
    PUSH DR20
    PUSH DR24
    PUSH DR28
    LCALL rt_port_usb_irq?_
    SJMP switch_check

; LCALL 仅压入两字节 PC，补齐并重排为 RETI 所需的硬件返回帧。
rt_port_yield?_:
    ; Incoming LCALL has pushed PCL,PCH. Add PSW1 and one spare byte.
    ; The caller has already disabled EA.
    PUSH 0D1H
    PUSH #0
    PUSH PSW0
    PUSH IE
    CLR EA
    PUSH DPS
    PUSH DPL1
    PUSH DPH1
    PUSH P_SW2
    PUSH DR56
    PUSH DR0
    PUSH DR4
    PUSH DR8
    PUSH DR12
    PUSH DR16
    PUSH DR20
    PUSH DR24
    PUSH DR28
    ; Reorder the first four bytes to PSW1,bank,PCL,PCH for RETI.
    MOV DR0,DR60
    SUB WR2,#45
    MOV R4,@DR0
    MOV R5,@DR0+1
    MOV R6,@DR0+2
    MOV @DR0,R6
    MOV R7,#0FFH
    MOV @DR0+1,R7
    MOV @DR0+2,R4
    MOV @DR0+3,R5

; 无请求则恢复原线程；有请求则保存当前 DR60 到来源线程的 sp 字段。
switch_check:
    MOV R0,rt_port_pending
    CMP R0,#0
    JE restore_context
    MOV R0,#0
    MOV rt_port_pending,R0
    MOV DR0,rt_port_from
    MOV DR4,#0
; 比较完整 32 位地址；0x00010000 的低 16 位为零，但不是空指针。
    CMP DR0,DR4
    JE load_context
    MOV DR4,DR60
    MOV @DR0,WR4
    MOV @DR0+2,WR6
; 加载目标线程的四字节栈指针，指向最后一个已占用字节。
load_context:
    MOV DR0,rt_port_to
    MOV WR4,@DR0
    MOV WR6,@DR0+2
    MOV DR60,DR4
; 反序恢复现场，RETI 最后恢复 PSW1 和 PC，继续执行目标线程。
restore_context:
    POP DR28
    POP DR24
    POP DR20
    POP DR16
    POP DR12
    POP DR8
    POP DR4
    POP DR0
    POP DR56
    POP P_SW2
    POP DPH1
    POP DPL1
    POP DPS
    POP IE
    POP PSW0
    RETI
END
