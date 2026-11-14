/* SPDX-License-Identifier: GPL-3.0-or-later */
'use strict';
(() => {
  const $ = id => document.getElementById(id);
  const demo = new URLSearchParams(location.search).has('demo');
  const state = {online:false, busy:false, uploading:false, step:1, files:[], selected:null, printer:{}, info:{}, history:{nozzle:[],bed:[]}};
  const demoPrinter = {v:2,state:'printing',busy:true,homed:7,x100:11250,y100:8630,z100:1480,nozzle:204,nozzle_target:205,bed:60,bed_target:60,fan:65,speed:100,progress:42,elapsed:2143,sd:true};
  const demoFiles = [{alias:'N1234567.GCO',name:'Корпус_датчика_PLA_0.2.gcode',size:6842812},{alias:'M6X12.GCO',name:'M6x12.gcode',size:20622890},{alias:'CUBE.GCO',name:'Калибровочный куб.gcode',size:842109}];
  function icons(root=document) {
    root.querySelectorAll('[data-icon]').forEach(el => {
      el.replaceChildren();
      const svg=document.createElementNS('http://www.w3.org/2000/svg','svg'); svg.classList.add('icon'); svg.setAttribute('aria-hidden','true');
      const use=document.createElementNS(svg.namespaceURI,'use'); use.setAttribute('href','#'+el.dataset.icon); svg.append(use);el.append(svg);
    });
  }
  function toast(message,error=false) { const el=document.createElement('div');el.className='toast'+(error?' error':'');el.textContent=message;$('toasts').append(el);setTimeout(()=>el.remove(),6000); }
  const errors={home_first:'Сначала выполните поиск нуля осей',motion_busy:'Принтер выполняет движение',print_active:'Действие недоступно во время печати',transfer_busy:'Идёт передача файла',travel_limit:'Достигнут предел перемещения',busy:'Принтер занят',offline:'Нет связи с основной платой',timeout:'Принтер не ответил. Проверьте состояние перед повтором',upgrade_mainboard:'Обновите прошивку основной платы',invalid_name:'Недопустимое имя файла',storage_busy:'SD-карта занята',session:'Обновите страницу',upload_failed:'Файл не подтверждён основной платой'};
  async function api(path,body) {
    if(demo) {
      if(body) {toast('Демонстрация: команда не отправлена');return {};}
      if(path.startsWith('/api/info'))return {session:'demo',ssid:'Home Wi-Fi',ip:'192.168.3.34',firmware:'0.2.0-dev',ap:'NEPTUNE3-ABC123'};
      if(path.startsWith('/api/status'))return {online:true,printer:demoPrinter,filename:demoFiles[0].name};
      if(path.startsWith('/api/files'))return {files:demoFiles};
      if(path.startsWith('/api/wifi/scan'))return {networks:[{ssid:'Home Wi-Fi',rssi:-48},{ssid:'Workshop',rssi:-67}]};
    }
    const abort = typeof AbortController!=='undefined' ? new AbortController():null;
    const timer=abort?setTimeout(()=>abort.abort(),15000):null;
    try {
      const response=await fetch(path,{method:body?'POST':'GET',cache:'no-store',headers:body?{'Content-Type':'application/x-www-form-urlencoded','X-Neptune-Session':state.info.session}:undefined,body:body?new URLSearchParams(body).toString():undefined,signal:abort?.signal});
      const data=await response.json(); if(!response.ok)throw new Error(errors[data.error]||data.error||'Ошибка '+response.status);return data;
    } finally {clearTimeout(timer);}
  }
  const duration=n=>{n=Math.max(0,Math.floor(n||0));return Math.floor(n/3600)+':'+String(Math.floor(n/60)%60).padStart(2,'0')+':'+String(n%60).padStart(2,'0');};
  function render() {
    const p=state.printer,on=state.online,printing=p.state==='printing',paused=p.state==='paused',active=printing||paused,idle=on&&!active&&!p.busy&&!state.busy&&!state.uploading;
    $('connection-label').textContent=on?'Принтер подключён':'Нет связи с принтером';$('connection').classList.toggle('online',on);
    $('job-state').textContent=on?(printing?'Печать':paused?'Пауза':p.busy?'Выполняется команда':'Готов к печати'):'Нет связи';
    $('job-name').textContent=state.filename||(active?'Печать с SD-карты':'Файл не выбран');
    $('job-note').textContent=on?(active?'Печать с карты памяти принтера':'Загрузите G-code или выберите файл из списка'):'Проверьте питание и подключение основной платы';
    const percent=on?Math.max(0,Math.min(100,Number(p.progress)||0)):0;
    $('progress-value').textContent=on?percent:'—';$('progress-bar').style.width=percent+'%';
    $('elapsed').textContent=on?duration(p.elapsed):'—';$('remaining').textContent=active&&percent>1&&p.elapsed>60?'≈ '+duration(p.elapsed*(100-percent)/percent):'—';
    $('pause-button').disabled=!on||!active||state.busy||state.uploading;$('pause-label').textContent=paused?'Продолжить':'Пауза';
    $('pause-symbol').dataset.icon=paused?'play':'pause';icons($('pause-button'));
    $('stop-button').disabled=!on||!active||state.busy||state.uploading;
    ['X','Y','Z'].forEach(axis=>{$('coord-'+axis).querySelector('span').textContent=on?(p[axis.toLowerCase()+'100']/100).toFixed(2):'—';});
    document.querySelectorAll('.jog').forEach(el=>el.disabled=!idle||p.homed!==7);
    document.querySelectorAll('.home').forEach(el=>el.disabled=!idle);
    $('park-button').disabled=!idle||p.homed!==7;
    $('motion-state').textContent=!on?'Недоступно':active?'Печать':p.homed===7?'Оси готовы':'Нужен поиск нуля';
    $('motion-note').textContent=p.homed===7?'Перемещения в пределах рабочей области':'Перед перемещением нажмите значок домика';
    document.querySelectorAll('.setting,[data-fan-delta]').forEach(el=>el.disabled=!on||state.busy||state.uploading);
    if(document.activeElement!==$('fan-slider'))$('fan-slider').value=p.fan||0;
    $('fan-value').textContent=on?(p.fan||0)+'%':'—';
    if(document.activeElement!==$('speed'))$('speed').value=p.speed||100;
    $('cooldown-button').disabled=!on||active||state.busy||state.uploading;
    ['nozzle','bed'].forEach(kind=>{
      $(kind+'-current').textContent=on?p[kind]:'—';$(kind+'-target').textContent=on?p[kind+'_target']:'—';
      $(kind+'-state').textContent=!on?'Нет данных':p[kind+'_target']>0?(p[kind]<p[kind+'_target']-3?'Нагрев':'Поддержание'):'Нагрев выключен';
    });
    $('files-refresh').disabled=!idle;$('upload-button').disabled=!idle||!p.sd;
    $('print-button').disabled=!idle||!state.selected;
    $('wifi-save').disabled=!idle;
    renderFiles();
  }
  function renderFiles() {
    const query=$('file-search').value.toLocaleLowerCase();$('file-list').replaceChildren();
    const files=state.files.filter(f=>f.name.toLocaleLowerCase().includes(query));
    files.forEach(file=>{
      const row=document.createElement('tr');row.classList.toggle('selected',state.selected?.alias===file.alias);
      const name=document.createElement('td'),size=document.createElement('td'),actions=document.createElement('td');
      const select=document.createElement('button');select.className='file-name';select.title=file.name;
      const symbol=document.createElement('span');symbol.dataset.icon='file';const label=document.createElement('span');label.textContent=file.name;select.append(symbol,label);
      select.onclick=()=>{state.selected=file;$('selected-name').textContent=file.name;render();};name.append(select);
      size.textContent=(file.size/1048576).toFixed(1)+' МБ';
      const play=document.createElement('button');play.className='icon-button';play.dataset.icon='play';play.setAttribute('aria-label','Печатать '+file.name);play.disabled=!state.online||state.printer.state!=='idle'||state.printer.busy||state.busy||state.uploading;play.onclick=()=>print(file);actions.append(play);
      row.append(name,size,actions);$('file-list').append(row);
    });icons($('file-list'));
    $('file-count').textContent=state.files.length+' файлов';$('files-empty').hidden=files.length>0;
  }
  async function control(action,extra={}) {
    if(state.busy||state.uploading)return;
    state.busy=true;render();try {await api('/api/control',{action,...extra});if(!demo)toast('Команда принята');}catch(e){toast(e.message,true);}finally{state.busy=false;await poll();}
  }
  function print(file) {if(file)control('PRINT',{file:file.alias});}
  async function files(refresh=false) {try{const result=await api('/api/files'+(refresh?'?refresh=1':''));state.files=result.files;renderFiles();}catch(e){toast(e.message,true);}}
  async function poll() {
    if(state.uploading)return;
    try {
      const data=await api('/api/status');state.online=data.online;state.printer=data.printer||{};state.filename=data.filename||'';
      if(state.online) ['nozzle','bed'].forEach(kind=>{
        const series=state.history[kind];series.push(state.printer[kind]);if(series.length>60)series.shift();
        const max=kind==='bed'?100:260;$(kind+'-chart').setAttribute('points',series.map((v,i)=>(i*280/59).toFixed(1)+','+(35-Math.min(max,Math.max(0,v))*33/max).toFixed(1)).join(' '));
      });
    }catch(e){state.online=false;}render();
  }
  async function info() {state.info=await api('/api/info');['ssid','ip','version'].forEach(key=>$('network-'+key).textContent=state.info[key==='version'?'firmware':key]||'—');}
  async function upload(file) {
    if(!file)return;if(demo){toast('Демонстрация: файл не отправлен');return;}
    if(new TextEncoder().encode(file.name).length>90){toast('Имя длиннее 90 байт UTF-8. Сократите его.',true);return;}
    state.uploading=true;render();$('upload-progress').hidden=false;$('upload-note').textContent='Передача '+file.name;
    try {
      await new Promise((resolve,reject)=>{
        const xhr=new XMLHttpRequest();xhr.open('POST','/api/upload?name='+encodeURIComponent(file.name));xhr.setRequestHeader('Content-Type','application/octet-stream');xhr.setRequestHeader('X-Neptune-Session',state.info.session);xhr.timeout=1800000;
        xhr.upload.onprogress=e=>{if(e.lengthComputable){$('upload-bar').style.width=Math.floor(e.loaded/e.total*100)+'%';if(e.loaded===e.total)$('upload-note').textContent='Проверка записи на SD-карту…';}};
        xhr.onload=()=>{let data={};try{data=JSON.parse(xhr.responseText);}catch{};xhr.status===200?resolve():reject(new Error(errors[data.error]||data.error||'Ошибка передачи'));};
        xhr.onerror=()=>reject(new Error('Связь прервана. Проверьте список файлов перед повтором.'));xhr.ontimeout=()=>reject(new Error('Время передачи истекло'));xhr.send(file);
      });toast('Файл записан на SD-карту');
    }catch(e){toast(e.message,true);}finally{state.uploading=false;$('upload-progress').hidden=true;$('file-input').value='';await poll();await files(true);}
  }
  document.querySelectorAll('[data-jog]').forEach(el=>el.onclick=()=>{const[axis,sign]=el.dataset.jog.split(',');control('JOG',{axis,value:state.step*Number(sign)});});
  document.querySelectorAll('[data-home]').forEach(el=>el.onclick=()=>control('HOME',{axis:el.dataset.home}));
  document.querySelectorAll('[data-step]').forEach(el=>el.onclick=()=>{state.step=Number(el.dataset.step);document.querySelectorAll('[data-step]').forEach(button=>button.classList.toggle('selected',button===el));});
  document.querySelectorAll('[data-heater]').forEach(form=>form.onsubmit=e=>{e.preventDefault();control(form.dataset.heater,{value:form.querySelector('input').value});});
  document.querySelectorAll('[data-preheat]').forEach(el=>el.onclick=()=>control('NOZZLE',{value:el.dataset.preheat}));
  document.querySelectorAll('[data-fan-delta]').forEach(el=>el.onclick=()=>control('FAN',{value:Math.max(0,Math.min(100,(state.printer.fan||0)+Number(el.dataset.fanDelta)))}));
  $('fan-slider').oninput=()=>{$('fan-value').textContent=$('fan-slider').value+'%';};$('fan-slider').onchange=()=>control('FAN',{value:$('fan-slider').value});$('speed').onchange=()=>control('SPEED',{value:$('speed').value});
  $('park-button').onclick=()=>control('PARK');$('cooldown-button').onclick=()=>control('COOLDOWN');$('pause-button').onclick=()=>control(state.printer.state==='paused'?'RESUME':'PAUSE');
  $('stop-button').onclick=()=>$('stop-dialog').showModal();$('stop-confirm').onclick=()=>{$('stop-dialog').close();control('STOP');};
  $('files-refresh').onclick=()=>files(true);$('file-search').oninput=renderFiles;$('print-button').onclick=()=>print(state.selected);$('upload-button').onclick=()=>$('file-input').click();$('file-input').onchange=()=>upload($('file-input').files[0]);
  document.querySelectorAll('[data-close]').forEach(el=>el.onclick=()=>$(el.dataset.close).close());
  $('wifi-open').onclick=()=>{$('wifi-current-ssid').textContent=state.info.ssid||'Не подключён';$('wifi-current-ip').textContent=state.info.ip||'—';$('wifi-ssid').value=state.info.ssid||'';$('ap-name').textContent=state.info.ap||'NEPTUNE3';$('wifi-dialog').showModal();};
  $('wifi-scan').onclick=async()=>{try{$('wifi-scan').disabled=true;const data=await api('/api/wifi/scan');$('wifi-networks').replaceChildren(new Option('Выберите сеть',''));data.networks.forEach(n=>$('wifi-networks').append(new Option(n.ssid+' · '+n.rssi+' dBm',n.ssid)));}catch(e){toast(e.message,true);}finally{$('wifi-scan').disabled=false;}};
  $('wifi-networks').onchange=()=>{if($('wifi-networks').value)$('wifi-ssid').value=$('wifi-networks').value;};
  $('wifi-form').onsubmit=async e=>{e.preventDefault();try{await api('/api/wifi',{ssid:$('wifi-ssid').value,password:$('wifi-password').value});$('wifi-dialog').close();toast('Настройки сохранены. IP может измениться: найдите принтер в новом роутере или подключитесь к резервной точке доступа.');}catch(error){toast(error.message,true);}};
  icons();$('upload-progress').classList.remove('hidden');$('upload-progress').hidden=true;
  if(demo)$('demo-notice').classList.remove('hidden');
  (async()=>{try{await info();}catch(e){toast('Wi-Fi модуль недоступен',true);}await poll();await files();setInterval(()=>{if(!state.busy)poll();},2000);})();
})();
