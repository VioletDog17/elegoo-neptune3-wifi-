// SPDX-License-Identifier: GPL-3.0-or-later
// Headless Chrome must be running with --remote-debugging-port=9223.
import {writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import assert from 'node:assert/strict';
const targets=await (await fetch('http://127.0.0.1:9223/json')).json();
const target=targets.find(t=>t.type==='page');
const ws=new WebSocket(target.webSocketDebuggerUrl);await new Promise(r=>ws.addEventListener('open',r,{once:true}));
let sequence=0;const pending=new Map(),exceptions=[];
ws.addEventListener('message',e=>{const m=JSON.parse(e.data);if(m.id){const cb=pending.get(m.id);pending.delete(m.id);m.error?cb.reject(m.error):cb.resolve(m.result);}if(m.method==='Runtime.exceptionThrown')exceptions.push(m.params.exceptionDetails.text);});
function cdp(method,params={}){return new Promise((resolve,reject)=>{const id=++sequence;pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params}));});}
async function evaluate(expression){const r=await cdp('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value;}
const wait=ms=>new Promise(r=>setTimeout(r,ms));
const output=resolve('docs/screenshots');await mkdir(output,{recursive:true});
await cdp('Runtime.enable');await cdp('Page.enable');
await cdp('Emulation.setDeviceMetricsOverride',{width:1440,height:1250,deviceScaleFactor:1,mobile:false});
const url='file:///'+resolve('web-ui/index.html').replaceAll('\\','/')+'?demo=1';
await cdp('Page.navigate',{url});await wait(1000);
assert.equal(await evaluate('document.getElementById("job-name").textContent'),'Корпус_датчика_PLA_0.2.gcode');
assert.equal(await evaluate('document.querySelectorAll("#file-list tr").length'),3);
assert.equal(await evaluate('document.querySelector(".jog").disabled'),true);
assert.notEqual(await evaluate('getComputedStyle(document.getElementById("demo-notice")).display'),'none');
await evaluate('document.getElementById("file-search").value="Куб";document.getElementById("file-search").dispatchEvent(new Event("input"))');
assert.equal(await evaluate('document.querySelectorAll("#file-list tr").length'),1);
await evaluate('document.getElementById("file-search").value="";document.getElementById("file-search").dispatchEvent(new Event("input"))');
async function capture(name){const r=await cdp('Page.captureScreenshot',{format:'png',captureBeyondViewport:true});await writeFile(resolve(output,name),Buffer.from(r.data,'base64'));}
await capture('web-panel-demo.png');
await evaluate('document.getElementById("wifi-open").click()');await wait(300);assert.equal(await evaluate('document.getElementById("wifi-dialog").open'),true);
await capture('web-wifi-demo.png');
await evaluate('document.getElementById("wifi-dialog").close()');
await cdp('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:true});await wait(300);
assert.equal(await evaluate('document.documentElement.scrollWidth <= innerWidth'),true);
await capture('web-mobile-demo.png');
assert.deepEqual(exceptions,[]);
// Exercise actual request construction against a browser-local fake device.
// No request in this phase can reach the network or a physical printer.
await cdp('Page.addScriptToEvaluateOnNewDocument',{source:`
window.calls=[];window.fixture={v:2,state:'idle',busy:false,homed:7,x100:0,y100:0,z100:0,nozzle:22,nozzle_target:0,bed:22,bed_target:0,fan:0,speed:100,progress:0,elapsed:0,sd:true};
window.fetch=async(path,options={})=>{
 if(options.method==='POST'){window.calls.push({path,body:options.body,headers:options.headers});return new Response('{"ok":true}');}
 const data=path.includes('/info')?{session:'test-session',ip:'192.0.2.1',ssid:'Test',firmware:'test'}:path.includes('/status')?{online:window.deviceOnline!==false,printer:window.fixture}:path.includes('/files')?{files:[{alias:'TEST.GCO',name:'Русское имя.gcode',size:100}]}:{networks:[]};return new Response(JSON.stringify(data));
};`});
await cdp('Page.navigate',{url:url.replace('?demo=1','')});await wait(300);
await evaluate('document.querySelector("[data-home=A]").click()');await wait(100);
assert.equal(await evaluate('new URLSearchParams(calls[0].body).get("action")'),'HOME');
assert.equal(await evaluate('calls[0].headers["X-Neptune-Session"]'),'test-session');
await evaluate('document.querySelector(".file-name").click();document.getElementById("print-button").click()');await wait(100);
assert.equal(await evaluate('new URLSearchParams(calls[1].body).get("file")'),'TEST.GCO');
await evaluate('window.fixture.state="paused"');await wait(2100);
assert.equal(await evaluate('document.querySelector(".jog").disabled'),true);
await evaluate('document.getElementById("pause-button").click()');await wait(100);
assert.equal(await evaluate('new URLSearchParams(calls[2].body).get("action")'),'RESUME');
await evaluate('window.deviceOnline=false');await wait(2100);
assert.equal(await evaluate('document.querySelector("[data-preheat]").disabled'),true);
assert.equal(await evaluate('document.getElementById("print-button").disabled'),true);
assert.deepEqual(exceptions,[]);
console.log('PASS: Cyrillic filename, filter, Wi-Fi dialog, mobile layout, tagged session requests, print/resume, paused/offline locks, no JavaScript exceptions');
ws.close();
