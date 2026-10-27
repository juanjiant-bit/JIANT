# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
from playwright.sync_api import sync_playwright
import json
root=Path(__file__).resolve().parents[1]/'web'
with sync_playwright() as p:
 b=p.chromium.launch(headless=True,args=['--no-sandbox'])
 page=b.new_page()
 def route(r):
  path=r.request.url.split('https://jiant.test/',1)[-1].split('?')[0]
  f=root/path
  if not f.is_file(): r.fulfill(status=404,body='');return
  body=f.read_bytes()
  if path=='editor.html':
   body=body.decode().replace('</script>','\nwindow.__test={makeMockDevice,smpBank,connect,disconnect,getDevice:()=>dev};\n</script>').encode()
  mime='text/html' if path.endswith('html') else 'application/javascript' if path.endswith('js') else 'application/json' if path.endswith('json') else 'font/ttf'
  r.fulfill(status=200,body=body,content_type=mime)
 page.route('https://jiant.test/**',route)
 errors=[];page.on('pageerror',lambda e:(errors.append(str(e)),print('PAGE ERROR',e)))
 page.goto('https://jiant.test/editor.html');page.wait_for_function('!!window.__test')
 print('audio support',page.evaluate('''() => [2000,3000,4000,8000].map(rate=>{try{new OfflineAudioContext(1,100,rate);return[rate,true]}catch(e){return[rate,e.name]}})'''))
 page.evaluate('''() => { const m=__test.makeMockDevice({auto:false});Object.defineProperty(navigator,'requestMIDIAccess',{value:async()=>m.access});window.mock=m; }''')
 print('low-rate decode',page.evaluate("""async()=>{
 const n=800, a=new ArrayBuffer(44+2*n),d=new DataView(a);const put=(o,s)=>[...s].forEach((c,i)=>d.setUint8(o+i,c.charCodeAt(0)));
 put(0,'RIFF');d.setUint32(4,36+2*n,true);put(8,'WAVEfmt ');d.setUint32(16,16,true);d.setUint16(20,1,true);d.setUint16(22,1,true);d.setUint32(24,8000,true);d.setUint32(28,16000,true);d.setUint16(32,2,true);d.setUint16(34,16,true);put(36,'data');d.setUint32(40,n*2,true);
 for(let i=0;i<n;i++)d.setInt16(44+2*i,20000*Math.sin(2*Math.PI*200*i/8000),true);
 const x=await __test.smpBank.decode(new File([a],'tone.wav'),2000);if(x.length<100 || ![...x].some(v=>v))throw Error('invalid low-rate audio');return x.length;
}"""))
 page.click('#connect');page.wait_for_function('__test.getDevice()?.dump && !document.querySelector("#connect").disabled',timeout=30000)
 print('connected',page.locator('#version').inner_text())
 page.click('[data-tab="sound"]');page.click('#smpfactory');page.wait_for_function('document.querySelector("#smplist").textContent.includes("CHORD")')
 page.on('dialog',lambda dialog:dialog.accept())
 page.click('#smpsend');page.wait_for_function('document.querySelector("#smplist").textContent.includes("1 KICK · 2 SNARE")',timeout=15000)
 print('factory transfer browser PASS')
 assert not errors,errors
 page.click('#connect');page.click('#connect');page.wait_for_function('__test.getDevice()?.dump && !document.querySelector("#connect").disabled',timeout=30000)
 print('reconnect PASS')
 b.close()
