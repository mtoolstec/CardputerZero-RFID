#!/usr/bin/env python3
"""CardputerZero framebuffer + evdev debug bridge. Reach through SSH only."""
import argparse, glob, json, mmap, os, secrets, struct, threading, zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HTML = r'''<!doctype html><meta charset="utf-8"><title>CardputerZero 调试</title>
<style>body{background:#14171d;color:#eee;font:16px system-ui;max-width:1000px;margin:32px auto;padding:0 20px}canvas{width:960px;max-width:100%;image-rendering:pixelated;border:2px solid #485366;background:#000}button{padding:12px 20px;margin:5px;background:#293443;color:white;border:1px solid #627389;border-radius:6px;touch-action:none}small{color:#aab8c9}#state{color:#86d7ac}</style>
<h2>CardputerZero 远程调试</h2><p id="state">连接中…</p><canvas id="screen" width="320" height="170" tabindex="0"></canvas>
<p><small>点击页面后使用键盘。支持方向键、Enter、Esc、Tab、字母、数字、Shift / Ctrl / Alt。切换窗口会自动松开按键。画面仅镜像，点击屏幕不产生触摸。</small></p>
<div id="buttons"></div><p><button onclick="location.href='/frame'">查看原尺寸截图</button></p>
<script>
const token='TOKEN', held=new Set(), canvas=document.querySelector('canvas'), ctx=canvas.getContext('2d');
const codes={Escape:1,Digit1:2,Digit2:3,Digit3:4,Digit4:5,Digit5:6,Digit6:7,Digit7:8,Digit8:9,Digit9:10,Digit0:11,Minus:12,Equal:13,Backspace:14,Tab:15,KeyQ:16,KeyW:17,KeyE:18,KeyR:19,KeyT:20,KeyY:21,KeyU:22,KeyI:23,KeyO:24,KeyP:25,BracketLeft:26,BracketRight:27,Enter:28,ControlLeft:29,ControlRight:97,KeyA:30,KeyS:31,KeyD:32,KeyF:33,KeyG:34,KeyH:35,KeyJ:36,KeyK:37,KeyL:38,Semicolon:39,Quote:40,Backquote:41,ShiftLeft:42,Backslash:43,KeyZ:44,KeyX:45,KeyC:46,KeyV:47,KeyB:48,KeyN:49,KeyM:50,Comma:51,Period:52,Slash:53,ShiftRight:54,AltLeft:56,AltRight:100,Space:57,CapsLock:58,ArrowUp:103,ArrowLeft:105,ArrowRight:106,ArrowDown:108,Home:102,End:107,Delete:111,PageUp:104,PageDown:109};
let queue=Promise.resolve();
function key(code,value){queue=queue.then(()=>fetch('/key',{method:'POST',headers:{'Content-Type':'application/json','X-Debug-Token':token},body:JSON.stringify({code,value})})).catch(()=>{document.querySelector('#state').textContent='按键发送失败';});}
function down(code,repeat=false){if(!held.has(code)){held.add(code);key(code,1)}else if(repeat)key(code,2)}
function up(code){if(held.delete(code))key(code,0)}
function release(){for(const code of [...held])up(code)}
addEventListener('keydown',e=>{let c=codes[e.code];if(c){e.preventDefault();down(c,e.repeat)}});
addEventListener('keyup',e=>{let c=codes[e.code];if(c){e.preventDefault();up(c)}});
addEventListener('blur',release);document.addEventListener('visibilitychange',()=>{if(document.hidden)release()});
for(const [label,code] of [['Esc / 返回',1],['Tab',15],['↑',103],['↓',108],['←',105],['→',106],['Enter / 确认',28],['空格',57]]){let b=document.createElement('button');b.textContent=label;b.onpointerdown=e=>{e.preventDefault();b.setPointerCapture(e.pointerId);down(code)};b.onpointerup=()=>up(code);b.onpointercancel=()=>up(code);document.querySelector('#buttons').append(b)}
async function frame(){try{let r=await fetch('/frame',{cache:'no-store'});if(!r.ok)throw Error(r.status);let img=await createImageBitmap(await r.blob());ctx.drawImage(img,0,0);img.close();document.querySelector('#state').textContent='已连接 · 320 × 170 · SSH 隧道';}catch(e){document.querySelector('#state').textContent='连接中断，正在重试…'}setTimeout(frame,180)}frame();
</script>'''

def chunk(kind, data):
    return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data) & 0xffffffff)

class Bridge:
    def __init__(self, fb, keyboard):
        name = Path(fb).name
        root = Path('/sys/class/graphics') / name
        self.width, self.height = map(int, (root/'virtual_size').read_text().strip().split(','))
        self.stride = int((root/'stride').read_text())
        if int((root/'bits_per_pixel').read_text()) != 16:
            raise RuntimeError('This bridge currently requires RGB565 framebuffer')
        self.fb_fd = os.open(fb, os.O_RDONLY)
        self.mem = mmap.mmap(self.fb_fd, self.stride*self.height, access=mmap.ACCESS_READ)
        self.keyboard = os.open(keyboard, os.O_WRONLY | os.O_NONBLOCK)
        self.lock = threading.Lock()
        self.held = set()
        self.token = secrets.token_urlsafe(32)
        self.last_key = 0.0
        threading.Thread(target=self.watchdog, daemon=True).start()

    def png(self):
        rows = bytearray()
        for y in range(self.height):
            rows.append(0)
            for (pixel,) in struct.iter_unpack('<H', self.mem[y*self.stride:y*self.stride+self.width*2]):
                r, g, b = (pixel>>11)&31, (pixel>>5)&63, pixel&31
                rows.extend(((r<<3)|(r>>2), (g<<2)|(g>>4), (b<<3)|(b>>2)))
        return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',self.width,self.height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows,1))+chunk(b'IEND',b'')

    def key(self, code, value):
        import time
        with self.lock:
            self.last_key = time.monotonic()
            self._write(code, value)

    def _write(self, code, value):
        os.write(self.keyboard, struct.pack('llHHi',0,0,1,code,value)+struct.pack('llHHi',0,0,0,0,0))
        if value: self.held.add(code)
        else: self.held.discard(code)

    def watchdog(self):
        import time
        while True:
            time.sleep(1)
            with self.lock:
                if self.held and time.monotonic()-self.last_key > 8:
                    for code in list(self.held): self._write(code,0)

class Handler(BaseHTTPRequestHandler):
    def respond(self, status, data, mime='text/plain'):
        self.send_response(status); self.send_header('Content-Type',mime)
        self.send_header('Content-Length',str(len(data))); self.send_header('Cache-Control','no-store')
        self.end_headers(); self.wfile.write(data)
    def do_GET(self):
        try:
            if self.path == '/frame': self.respond(200,self.server.bridge.png(),'image/png')
            elif self.path == '/': self.respond(200,HTML.replace('TOKEN',self.server.bridge.token).encode(),'text/html; charset=utf-8')
            else: self.respond(404,b'Not found')
        except (BrokenPipeError, ConnectionResetError): pass
    def do_POST(self):
        if self.path != '/key' or self.headers.get('X-Debug-Token') != self.server.bridge.token:
            return self.respond(403,b'Forbidden')
        try:
            length=int(self.headers.get('Content-Length','0'))
            if not 0 < length <= 128: raise ValueError('invalid size')
            data=json.loads(self.rfile.read(length)); code=data['code']; value=data['value']
            if type(code) is not int or not 1<=code<=127 or type(value) is not int or value not in (0,1,2): raise ValueError('invalid key')
            self.server.bridge.key(code,value); self.respond(200,b'OK')
        except (ValueError,KeyError,TypeError): self.respond(400,b'Invalid key')
    def log_message(self,*args): pass

if __name__ == '__main__':
    ap=argparse.ArgumentParser(); ap.add_argument('--port',type=int,default=18765); ap.add_argument('--fb',default='/dev/fb0')
    ap.add_argument('--keyboard',default='/dev/input/by-path/platform-3f804000.i2c-event'); args=ap.parse_args()
    server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler)
    server.bridge=Bridge(args.fb,args.keyboard)
    print('Debug bridge ready on loopback port',args.port,flush=True)
    server.serve_forever()
