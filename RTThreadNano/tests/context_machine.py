"""Execute the actual linked context-switch opcodes in a small C251 ISA model.
Not a full STC simulator: the C tick handler is replaced with a callback.
Instruction and stack byte order follow STC32G manual appendix P.
"""
from pathlib import Path
import re, random
ROOT=Path(__file__).resolve().parents[1]
MAP=(ROOT/'build/RTThreadNano.map').read_text(errors='replace')
ROM={}; upper=0
for line in (ROOT/'build/RTThreadNano.hex').read_text().splitlines():
    raw=bytes.fromhex(line[1:]);assert sum(raw)%256==0
    size,addr,kind=raw[0],int.from_bytes(raw[1:3],'big'),raw[3];data=raw[4:4+size]
    if kind==4:upper=int.from_bytes(data,'big')<<16
    elif kind==0:
        for i,b in enumerate(data):
            address=upper+addr+i;assert 0xfe0000<=address<=0xffffff
            assert address not in ROM;ROM[address]=b
    elif kind==1:break

def symbol(name):
    m=re.search(r'^\s*([0-9A-F]+)H\s+(?:SYMBOL\s+)?(?:CODE|EDATA)\s+.*?\s'+re.escape(name)+r'\s*$',MAP,re.M)
    assert m,name
    return int(m[1],16)

class CPU:
    def __init__(self):
        self.r=bytearray(64);self.ram=bytearray(0x12000);self.sf={0xd0:0,0xd1:0,0xa8:0x82,0xe3:0,0xe4:0,0xe5:0,0xba:0x80}
        self.pc=0;self.zero=False;self.tick=lambda:None;self.steps=0;self.returned=False
    def reg(self,n,size):return int.from_bytes(self.r[n:n+size],'big')
    def setreg(self,n,size,v):self.r[n:n+size]=int(v&((1<<(size*8))-1)).to_bytes(size,'big')
    @property
    def sp(self):return self.reg(60,4)
    @sp.setter
    def sp(self,v):self.setreg(60,4,v)
    def direct(self,n):
        aliases={0x81:63,0x85:62,0x82:59,0x83:58,0x84:57,0xe0:11,0xf0:10}
        return self.r[aliases[n]] if n in aliases else self.sf.get(n,0)
    def setdirect(self,n,v):
        aliases={0x81:63,0x85:62,0x82:59,0x83:58,0x84:57,0xe0:11,0xf0:10}
        if n in aliases:self.r[aliases[n]]=v
        else:self.sf[n]=v
    def push(self,b):self.sp+=1;assert self.sp<4096;self.ram[self.sp]=b
    def pop(self):b=self.ram[self.sp];self.sp-=1;return b
    def fetch(self):b=ROM[self.pc];self.pc+=1;return b
    def u16(self):return (self.fetch()<<8)|self.fetch()
    def mem(self,a,size):return int.from_bytes(self.ram[a:a+size],'big')
    def store(self,a,size,v):self.ram[a:a+size]=int(v).to_bytes(size,'big')
    def step(self):
        at=self.pc;op=self.fetch();self.steps+=1
        if op in (0xc0,0xd0):
            n=self.fetch()
            if op==0xc0:self.push(self.direct(n))
            else:self.setdirect(n,self.pop())
        elif op==0xc2:
            bit=self.fetch();assert bit==0xaf;self.sf[0xa8]&=0x7f
        elif op in (0xca,0xda):
            m=self.fetch()
            if m==2:assert op==0xca;self.push(self.fetch())
            else:
                assert m&15==11;n=(m>>4)*4
                if op==0xca:
                    for b in self.r[n:n+4]:self.push(b)
                else:
                    for i in range(3,-1,-1):self.r[n+i]=self.pop()
        elif op==0x7f:
            m=self.fetch();self.setreg((m>>4)*4,4,self.reg((m&15)*4,4))
        elif op==0x9e:
            m=self.fetch();assert m&15==4;n=(m>>4)*2
            self.setreg(n,2,self.reg(n,2)-self.u16())
        elif op==0x7e:
            m=self.fetch();mode=m&15;n=m>>4
            if mode==0:self.r[n]=self.fetch()
            elif mode==3:self.r[n]=self.ram[self.u16()]
            elif mode==8:self.setreg(n*4,4,self.u16())
            elif mode==15:self.setreg(n*4,4,self.mem(self.u16(),4))
            elif mode==11:
                dest=self.fetch()>>4;self.r[dest]=self.ram[self.reg(n*4,4)]
            else:raise AssertionError(('7e',m,hex(at)))
        elif op==0x7a:
            m=self.fetch();mode=m&15;n=m>>4
            if mode==3:self.ram[self.u16()]=self.r[n]
            elif mode==11:
                src=self.fetch()>>4;self.ram[self.reg(n*4,4)]=self.r[src]
            else:raise AssertionError(('7a',m,hex(at)))
        elif op==0xbf:
            m=self.fetch();self.zero=self.reg((m>>4)*4,4)==self.reg((m&15)*4,4)
            self.sf[0xd1]=(self.sf[0xd1]&~1)|int(self.zero)
        elif op==0xbe:
            m=self.fetch();n=m>>4;mode=m&15
            if mode==0:self.zero=self.r[n]==self.fetch()
            elif mode==4:self.zero=self.reg(n*2,2)==self.u16()
            else:raise AssertionError(('be',m))
            self.sf[0xd1]=(self.sf[0xd1]&~1)|int(self.zero)
        elif op in (0x68,0x80):
            offset=self.fetch();offset=offset if offset<128 else offset-256
            if op==0x80 or self.zero:self.pc+=offset
        elif op in (0x29,0x39,0x69,0x79):
            m=self.fetch();base=self.reg((m&15)*4,4);addr=base+self.u16();n=m>>4
            if op==0x29:self.r[n]=self.ram[addr]
            elif op==0x39:self.ram[addr]=self.r[n]
            elif op==0x69:self.setreg(n*2,2,self.mem(addr,2))
            else:self.store(addr,2,self.reg(n*2,2))
        elif op in (0x0b,0x1b):
            m=self.fetch();assert m&15==10;addr=self.reg((m>>4)*4,4);n=(self.fetch()>>4)*2
            if op==0x0b:self.setreg(n,2,self.mem(addr,2))
            else:self.store(addr,2,self.reg(n,2))
        elif op==0x12:
            dest=0xff0000|self.u16();assert dest in (symbol('rt_port_tick?_'),symbol('rt_port_usb_irq?_'))
            self.push(self.pc&255);self.push((self.pc>>8)&255)
            self.tick()
            high=self.pop();low=self.pop();self.pc=0xff0000|(high<<8)|low
        elif op==0x02:self.pc=0xff0000|self.u16()
        elif op==0x32:
            high=self.pop();low=self.pop();bank=self.pop();self.sf[0xd1]=self.pop()
            self.pc=(bank<<16)|(high<<8)|low;self.returned=True
        else:raise AssertionError(('opcode',hex(op),hex(at)))
    def run(self):
        for _ in range(400):
            self.step()
            if self.returned:return
        raise AssertionError('context never returned')
    def state(self):return bytes(self.r[:32]),bytes(self.r[56:60]),dict(self.sf),self.sp
    def randomize(self,rng):
        for i in list(range(32))+list(range(56,60)):self.r[i]=rng.randrange(256)
        for i in self.sf:self.sf[i]=rng.randrange(256)
        self.sf[0xa8]|=0x80
    def request(self,from_field,to_field,target_sp):
        self.store(symbol('rt_port_from'),4,from_field)
        self.store(symbol('rt_port_to'),4,to_field)
        self.ram[symbol('rt_port_pending')]=1
        self.store(to_field,4,target_sp)
    def interrupt(self,return_pc):
        for b in [self.sf[0xd1],return_pc>>16,return_pc&255,(return_pc>>8)&255]:self.push(b)
        self.pc=0xff000b
    def yield_call(self,return_pc):
        self.sf[0xa8]&=0x7f
        self.push(return_pc&255);self.push((return_pc>>8)&255)
        self.pc=symbol('rt_port_yield?_')

def fresh_frame(cpu,base,pc=0xff2345):
    # Exact layout built by rt_hw_stack_init: exit callback16 then context46.
    frame=bytearray(46);frame[1]=0xff;frame[2]=pc&255;frame[3]=(pc>>8)&255
    frame[5]=0x82;frame[9]=0x80;frame[11]=1
    cpu.ram[base+2:base+48]=frame
    return base+47

def tests():
    rng=random.Random(251)
    for _ in range(150):
        # No scheduling: real Timer0 opcodes preserve every register and flag.
        c=CPU();c.randomize(rng);c.sp=0x600;expected=c.state();c.interrupt(0xff4567)
        c.tick=lambda: [c.setreg(0,4,0xdeadbeef),c.setreg(56,4,0x1020304)]
        c.run();assert c.state()==expected;assert c.pc==0xff4567
        # Normal software yield converts the 2-byte call frame correctly.
        c=CPU();c.randomize(rng);c.sp=0x500;c.sf[0xa8]&=0x7f;expected=c.state()
        c.yield_call(0xff3456);c.run();assert c.state()==expected;assert c.pc==0xff3456
        # Initial scheduler starts a fresh frame without saving bootstrap SP.
        c=CPU();c.randomize(rng);c.sp=0x800
        target=fresh_frame(c,0x200);c.request(0,0xe90,target);c.yield_call(0xff1122);c.run()
        assert c.pc==0xff2345 and c.sp==0x201
        assert c.reg(56,4)==0x10000 and c.sf[0xa8]==0x82 and not any(c.r[:32])
        # Suspend through yield, later resume through interrupt exit.
        c=CPU();c.randomize(rng);c.sp=0x600;c.sf[0xa8]&=0x7f;expected=c.state()
        target=fresh_frame(c,0x200);c.request(0xe80,0xe90,target)
        c.yield_call(0xff4567);c.run();saved=c.mem(0xe80,4)
        assert saved==0x600+46 and c.pc==0xff2345
        c.returned=False;c.request(0xe90,0xe80,saved);c.interrupt(0xff6789);c.run()
        assert c.pc==0xff4567 and c.state()==expected
        # Save a running thread from Timer0, resume it through software yield.
        c=CPU();c.randomize(rng);c.sp=0x600;expected=c.state();target=fresh_frame(c,0x200)
        c.tick=lambda:c.request(0xe80,0xe90,target)
        c.interrupt(0xff5678);c.run();saved=c.mem(0xe80,4);assert saved==0x600+46
        c.returned=False;c.request(0xe90,0xe80,saved);c.yield_call(0xff6789);c.run()
        assert c.pc==0xff5678 and c.state()==expected
    for vector in [0xff000b,0xff00cb]:
        c=CPU();c.randomize(rng);c.sp=0x600;expected=c.state()
        c.interrupt(0xff3210);c.pc=vector;c.run()
        assert c.state()==expected and c.pc==0xff3210
        c=CPU();c.randomize(rng);c.sp=0x600;expected=c.state();target=fresh_frame(c,0x200)
        c.tick=lambda:c.request(0x10000,0x10200,target)
        c.interrupt(0xff3210);c.pc=vector;c.run();saved=c.mem(0x10000,4)
        assert saved==0x600+46
        c.returned=False;c.request(0x10200,0x10000,saved);c.yield_call(0xff5678);c.run()
        assert c.state()==expected and c.pc==0xff3210
    assert ROM[0xff0000]==0x02 # actual reset vector
    assert ROM[0xff000b]==0x02 # actual Timer0 vector
    assert ROM[0xff00cb]==0x02 # actual USB vector
    for name,size in [('heartbeat_stack',512),('key_stack',512),('lcd_stack',768),('rt_thread_stack',384),('finsh_thread_stack',1024)]:
        addr=symbol(name);assert 8<=addr and addr+size<=0x1000,(name,addr)
    assert 'INTR FRAME:   4 BYTES' in MAP
    assert 'MEMORY MODEL: XSMALL' in MAP
    print('PASS: 750 randomized context scenarios plus Timer0/USB/XDATA pointer cases; all GP registers, DPX, SFR state, PSW0/1, EA, PC, SP; boot/yield/IRQ switching; HEX checksums and EDATA stack bounds')
if __name__=='__main__':tests()
