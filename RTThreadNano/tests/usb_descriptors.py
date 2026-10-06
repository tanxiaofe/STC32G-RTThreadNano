from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
s=(root/'bsp/usb/usb_desc.c').read_text(encoding='gbk')
def array(name):
 m=re.search(r'char code '+name+r'\[(\d+)\]\s*=\s*\{(.*?)\};',s,re.S)
 data=re.sub(r'//[^\n]*','',m[2]);items=[x.strip() for x in data.split(',') if x.strip()]
 values=bytes(ord(x[1]) if x.startswith("'") else int(x,0) for x in items)
 assert len(values)==int(m[1]);return values
device=array('DEVICEDESC');config=array('CONFIGDESC')
assert device[:2]==b'\x12\x01' and device[4:7]==b'\x02\x02\x01' and device[7]==64
assert config[2]|config[3]<<8==len(config)==67
index=0;interfaces=[];endpoints=[]
while index<len(config):
 length,kind=config[index:index+2];assert length>=2 and index+length<=len(config)
 d=config[index:index+length]
 if kind==4:interfaces.append((d[2],d[5],d[6],d[7]))
 if kind==5:endpoints.append((d[2],d[3],d[4]|d[5]<<8))
 index+=length
assert interfaces==[(0,2,2,1),(1,10,0,0)]
assert endpoints==[(0x82,3,64),(0x81,2,64),(1,2,64)]
product=array('PRODUCTDESC');assert product[0]==len(product) and product[1]==3
assert product[2:].decode('utf-16le')=='STC RT-Thread CDC'
print('PASS: CDC ACM class/subclass/protocol, configuration lengths/interfaces/endpoints, UTF16 string descriptor')
