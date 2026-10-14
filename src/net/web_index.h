// Generato da tools/web/embed.py a partire da tools/web/index.html: non modificare a mano.
#pragma once

static const char kWebIndex[] = R"ONDAWEB(<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#121212">
<title>onda · File</title>
<style>
:root{--bg:#121212;--bg2:#181818;--el:#242424;--el2:#2e2e2e;--line:#2a2a2a;--tx:#fff;--sub:#b3b3b3;--dim:#7a7a7a;--g:#1ed760;--g2:#1db954;--red:#f15e6c;--r:10px}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;-webkit-font-smoothing:antialiased}
button{font:inherit;color:inherit;cursor:pointer;border:0;background:none}
svg{width:20px;height:20px;fill:currentColor;flex:none}
.wrap{max-width:980px;margin:0 auto;padding:0 16px 140px}
header{display:flex;align-items:center;gap:16px;padding:20px 0 12px;flex-wrap:wrap}
.logo{font-weight:800;font-size:26px;letter-spacing:-.5px}.logo b{color:var(--g)}
.logo small{display:block;font-size:13px;font-weight:500;color:var(--sub);letter-spacing:0}
.disk{margin-left:auto;min-width:220px;font-size:13px;color:var(--sub)}
.bar{height:6px;border-radius:3px;background:var(--el2);overflow:hidden;margin-top:6px}
.bar i{display:block;height:100%;background:var(--g);width:0;transition:width .3s}
.tools{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin:8px 0 14px}
.btn{display:inline-flex;align-items:center;gap:8px;padding:9px 16px;border-radius:999px;background:var(--el);font-weight:600;font-size:14px;transition:background .15s,transform .1s}
.btn:hover{background:var(--el2)}.btn:active{transform:scale(.97)}
.btn.pri{background:var(--g);color:#000}.btn.pri:hover{background:#3be477}
.btn.ghost{background:none;border:1px solid #535353}.btn.ghost:hover{border-color:#fff}
.btn.danger{background:var(--red);color:#000}
.btn[disabled]{opacity:.4;pointer-events:none}
.sp{flex:1}
.chk{display:inline-flex;align-items:center;gap:6px;color:var(--sub);font-size:13px;cursor:pointer;user-select:none}
.crumbs{display:flex;flex-wrap:wrap;align-items:center;gap:2px;font-size:22px;font-weight:700;margin:6px 0 4px}
.crumbs a{color:var(--sub);text-decoration:none;padding:2px 6px;border-radius:6px}
.crumbs a:hover{color:var(--tx);background:var(--el)}
.crumbs a:last-child{color:var(--tx)}
.crumbs span{color:var(--dim);font-weight:400}
.meta{color:var(--sub);font-size:13px;margin:0 6px 10px}
.sel{display:none;align-items:center;gap:10px;background:var(--el);border-radius:var(--r);padding:8px 10px 8px 14px;margin-bottom:10px}
.sel.on{display:flex}
.list{border-top:1px solid var(--line)}
.row{display:grid;grid-template-columns:28px 36px 1fr 96px 120px 40px;align-items:center;gap:10px;padding:6px 8px;border-bottom:1px solid var(--line);border-radius:6px;min-height:52px}
.row:hover{background:#1f1f1f}
.row.dir .nm{cursor:pointer}
.ic{width:36px;height:36px;border-radius:6px;display:grid;place-items:center;background:var(--el2);color:var(--sub)}
.row.dir .ic{background:#2a2a2a;color:var(--g)}
.row.flac .ic{background:linear-gradient(135deg,#450af5,#8e8ee5);color:#fff}
.row.img .ic{background:#3a2c1f;color:#e8a24c}
.nm{min-width:0}.nm div{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.nm small{color:var(--sub);font-size:12px}
.sz,.dt{color:var(--sub);font-size:13px;text-align:right;white-space:nowrap}
.more{width:36px;height:36px;border-radius:50%;display:grid;place-items:center;color:var(--sub)}
.more:hover{background:var(--el2);color:var(--tx)}
input[type=checkbox]{width:17px;height:17px;accent-color:var(--g);cursor:pointer}
.empty{padding:60px 20px;text-align:center;color:var(--sub);border:2px dashed #333;border-radius:16px;margin-top:16px}
.empty svg{width:44px;height:44px;color:var(--dim);margin-bottom:8px}
.empty b{display:block;color:var(--tx);font-size:18px;margin-bottom:4px}
.drop{position:fixed;inset:0;background:rgba(18,18,18,.92);display:none;place-items:center;z-index:30;border:3px dashed var(--g);margin:10px;border-radius:20px}
.drop.on{display:grid}
.drop div{text-align:center;font-size:20px;font-weight:700}.drop small{display:block;color:var(--sub);font-weight:400;font-size:14px;margin-top:6px}
.drop svg{width:56px;height:56px;color:var(--g)}
.menu{position:fixed;z-index:40;background:#282828;border-radius:8px;padding:4px;min-width:190px;box-shadow:0 16px 24px rgba(0,0,0,.5);display:none}
.menu.on{display:block}
.menu button{display:flex;gap:12px;align-items:center;width:100%;padding:10px 12px;border-radius:4px;text-align:left;font-size:14px}
.menu button:hover{background:#3e3e3e}.menu .red{color:var(--red)}
.modal{position:fixed;inset:0;background:rgba(0,0,0,.6);display:none;place-items:center;z-index:50;padding:16px}
.modal.on{display:grid}
.dlg{background:#282828;border-radius:12px;padding:22px;width:min(420px,100%);box-shadow:0 20px 40px rgba(0,0,0,.6)}
.dlg h3{margin:0 0 6px;font-size:20px}.dlg p{margin:0 0 16px;color:var(--sub);font-size:14px;word-break:break-word}
.dlg input[type=text]{width:100%;padding:12px;border-radius:6px;border:1px solid #535353;background:#3e3e3e;color:#fff;font:inherit;margin-bottom:16px;outline:none}
.dlg input[type=text]:focus{border-color:#fff}
.dlg .acts{display:flex;justify-content:flex-end;gap:8px;flex-wrap:wrap}
.up{position:fixed;right:16px;bottom:84px;width:min(380px,calc(100% - 32px));background:#202020;border:1px solid #333;border-radius:12px;box-shadow:0 16px 32px rgba(0,0,0,.55);z-index:20;display:none;overflow:hidden}
.up.on{display:block}
.uph{display:flex;align-items:center;gap:10px;padding:12px 14px;cursor:pointer}
.uph b{flex:1;font-size:14px}.uph small{color:var(--sub);font-size:12px;display:block;font-weight:400}
.upbar{height:3px;background:#333}.upbar i{display:block;height:100%;width:0;background:var(--g);transition:width .2s}
.upl{max-height:260px;overflow:auto;padding:4px 0}
.up.min .upl{display:none}
.ui{display:grid;grid-template-columns:1fr auto;gap:2px 10px;padding:7px 14px;font-size:13px}
.ui div{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ui small{color:var(--sub);text-align:right}
.ui .ok{color:var(--g)}.ui .err{color:var(--red)}
.ui .pb{grid-column:1/3;height:3px;background:#333;border-radius:2px;overflow:hidden}.ui .pb i{display:block;height:100%;background:var(--sub);width:0}
.foot{position:fixed;left:0;right:0;bottom:0;background:linear-gradient(transparent,#121212 30%);padding:28px 16px 16px;z-index:10}
.foot .in{max-width:980px;margin:0 auto;display:flex;align-items:center;gap:14px;background:#1e1e1e;border:1px solid #2c2c2c;border-radius:12px;padding:10px 12px 10px 16px}
.foot p{margin:0;flex:1;font-size:13px;color:var(--sub)}.foot p b{color:var(--tx)}
.toast{position:fixed;left:50%;top:18px;transform:translateX(-50%) translateY(-16px);opacity:0;pointer-events:none;background:#fff;color:#000;padding:10px 18px;border-radius:8px;font-weight:600;font-size:14px;z-index:60;transition:transform .25s,opacity .25s;max-width:90%}
.toast.on{transform:translateX(-50%) translateY(0);opacity:1}.toast.err{background:var(--red)}
.off{position:fixed;inset:0;background:rgba(18,18,18,.96);display:none;place-items:center;z-index:70;text-align:center;padding:20px}
.off.on{display:grid}.off b{font-size:22px;display:block;margin-bottom:8px}.off p{color:var(--sub)}
@media (max-width:640px){
.row{grid-template-columns:24px 36px 1fr 40px}.sz,.dt{display:none}
.nm small.m{display:inline}.disk{min-width:100%;margin-left:0}
.crumbs{font-size:19px}.foot p{font-size:12px}
}
@media (min-width:641px){.nm small.m{display:none}}
</style>
</head>
<body>
<div class="wrap">
<header>
<div class="logo">onda<b>.</b><small>File sulla scheda microSD</small></div>
<div class="disk" id="disk"><span id="diskt">&nbsp;</span><div class="bar"><i id="diskb"></i></div></div>
</header>
<div class="tools">
<button class="btn pri" id="bFiles"><svg viewBox="0 0 24 24"><path d="M11 16V7.85l-2.6 2.6L7 9l5-5 5 5-1.4 1.45-2.6-2.6V16zm-5 4q-.825 0-1.412-.587T4 18v-3h2v3h12v-3h2v3q0 .825-.587 1.413T18 20z"/></svg>Carica file</button>
<button class="btn" id="bDir"><svg viewBox="0 0 24 24"><path d="M4 20q-.825 0-1.412-.587T2 18V6q0-.825.588-1.412T4 4h6l2 2h8q.825 0 1.413.588T22 8v10q0 .825-.587 1.413T20 20zm7-3h2v-4.2l1.6 1.6L16 13l-4-4-4 4 1.4 1.4 1.6-1.6z"/></svg>Carica cartella</button>
<button class="btn" id="bMk"><svg viewBox="0 0 24 24"><path d="M14 16h2v-2h2v-2h-2v-2h-2v2h-2v2h2zM4 20q-.825 0-1.412-.587T2 18V6q0-.825.588-1.412T4 4h6l2 2h8q.825 0 1.413.588T22 8v10q0 .825-.587 1.413T20 20z"/></svg>Nuova cartella</button>
<span class="sp"></span>
<label class="chk"><input type="checkbox" id="hid">File nascosti</label>
</div>
<nav class="crumbs" id="crumbs"></nav>
<div class="meta" id="meta"></div>
<div class="sel" id="sel"><b id="selN" style="flex:1"></b>
<button class="btn ghost" id="selMv">Sposta</button>
<button class="btn danger" id="selRm">Elimina</button>
<button class="more" id="selX" title="Annulla"><svg viewBox="0 0 24 24"><path d="M6.4 19 5 17.6l5.6-5.6L5 6.4 6.4 5l5.6 5.6L17.6 5 19 6.4 13.4 12l5.6 5.6-1.4 1.4-5.6-5.6z"/></svg></button>
</div>
<div class="list" id="list"></div>
</div>
<input type="file" id="fFiles" multiple hidden>
<input type="file" id="fDir" webkitdirectory multiple hidden>
<div class="drop" id="drop"><div><svg viewBox="0 0 24 24"><path d="M11 16V7.85l-2.6 2.6L7 9l5-5 5 5-1.4 1.45-2.6-2.6V16zm-5 4q-.825 0-1.412-.587T4 18v-3h2v3h12v-3h2v3q0 .825-.587 1.413T18 20z"/></svg><br>Rilascia per caricare<small id="dropTo"></small></div></div>
<div class="menu" id="menu"></div>
<div class="modal" id="modal"><div class="dlg"><h3 id="mT"></h3><p id="mP"></p><input type="text" id="mI" autocomplete="off" spellcheck="false"><div class="acts" id="mA"></div></div></div>
<div class="up" id="up"><div class="uph" id="uph"><div style="flex:1"><b id="upT">Caricamento</b><small id="upS"></small></div><button class="btn ghost" id="upX" style="padding:6px 12px">Annulla</button></div><div class="upbar"><i id="upB"></i></div><div class="upl" id="upl"></div></div>
<div class="foot"><div class="in"><p><b>Hai finito?</b> Il lettore chiude il Wi-Fi e aggiorna la libreria con i nuovi brani.</p><button class="btn pri" id="bDone">Aggiorna libreria</button></div></div>
<div class="toast" id="toast"></div>
<div class="off" id="off"><div><b>Trasferimento chiuso</b><p>Il lettore sta aggiornando la libreria. Puoi chiudere questa pagina.</p></div></div>
<script>
(()=>{
const $=id=>document.getElementById(id);
const IC={
dir:'<svg viewBox="0 0 24 24"><path d="M4 20q-.825 0-1.412-.587T2 18V6q0-.825.588-1.412T4 4h6l2 2h8q.825 0 1.413.588T22 8v10q0 .825-.587 1.413T20 20z"/></svg>',
flac:'<svg viewBox="0 0 24 24"><path d="M10 21q-1.65 0-2.825-1.175T6 17t1.175-2.825T10 13q.575 0 1.063.138T12 13.55V3h6v4h-4v10q0 1.65-1.175 2.825T10 21"/></svg>',
img:'<svg viewBox="0 0 24 24"><path d="M5 21q-.825 0-1.412-.587T3 19V5q0-.825.588-1.412T5 3h14q.825 0 1.413.588T21 5v14q0 .825-.587 1.413T19 21zm1-4h12l-3.75-5-3 4L9 13z"/></svg>',
list:'<svg viewBox="0 0 24 24"><path d="M3 18v-2h10v2zm0-5v-2h18v2zm0-5V6h18v2zm15 12v-3h-3v-2h3v-3h2v3h3v2h-3v3z"/></svg>',
file:'<svg viewBox="0 0 24 24"><path d="M6 22q-.825 0-1.412-.587T4 20V4q0-.825.588-1.412T6 2h8l6 6v12q0 .825-.587 1.413T18 22zm7-13h5l-5-5z"/></svg>',
more:'<svg viewBox="0 0 24 24"><path d="M6 14q-.825 0-1.412-.587T4 12t.588-1.412T6 10t1.413.588T8 12t-.587 1.413T6 14m6 0q-.825 0-1.412-.587T10 12t.588-1.412T12 10t1.413.588T14 12t-.587 1.413T12 14m6 0q-.825 0-1.412-.587T16 12t.588-1.412T18 10t1.413.588T20 12t-.587 1.413T18 14"/></svg>',
dl:'<svg viewBox="0 0 24 24"><path d="m12 16-5-5 1.4-1.45 2.6 2.6V4h2v8.15l2.6-2.6L17 11zm-6 4q-.825 0-1.412-.587T4 18v-3h2v3h12v-3h2v3q0 .825-.587 1.413T18 20z"/></svg>',
ren:'<svg viewBox="0 0 24 24"><path d="M5 19h1.425L16.2 9.225 14.775 7.8 5 17.575zm-2 2v-4.25L16.2 3.575q.3-.275.663-.425t.762-.15.775.15.65.45L20.425 5q.3.275.437.65T21 6.4q0 .4-.138.763t-.437.662L7.25 21z"/></svg>',
mv:'<svg viewBox="0 0 24 24"><path d="M4 20q-.825 0-1.412-.587T2 18V6q0-.825.588-1.412T4 4h6l2 2h8q.825 0 1.413.588T22 8v10q0 .825-.587 1.413T20 20zm8.15-6H8v2h4.15l-1.6 1.6L12 19l4-4-4-4-1.45 1.4z"/></svg>',
rm:'<svg viewBox="0 0 24 24"><path d="M7 21q-.825 0-1.412-.587T5 19V6H4V4h5V3h6v1h5v2h-1v13q0 .825-.587 1.413T17 21zm2-4h2V8H9zm4 0h2V8h-2z"/></svg>',
up:'<svg viewBox="0 0 24 24"><path d="M11 16V7.85l-2.6 2.6L7 9l5-5 5 5-1.4 1.45-2.6-2.6V16zm-5 4q-.825 0-1.412-.587T4 18v-3h2v3h12v-3h2v3q0 .825-.587 1.413T18 20z"/></svg>'
};
let cwd='/',items=[],selected=new Set(),busy=false;
const esc=s=>s.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const join=(a,b)=>(a==='/'?'':a)+'/'+b;
const fmt=n=>{if(n<1024)return n+' B';const u=['KB','MB','GB','TB'];let i=-1;do{n/=1024;i++}while(n>=1024&&i<3);return n.toLocaleString('it-IT',{maximumFractionDigits:n<10?1:0})+' '+u[i]};
const kind=n=>{const e=n.toLowerCase().split('.').pop();return e==='flac'?'flac':['jpg','jpeg','png','bmp','gif','webp'].includes(e)?'img':['m3u','m3u8'].includes(e)?'list':'file'};
const api=(path,params,opts)=>{const q=new URLSearchParams(params).toString();return fetch(path+(q?'?'+q:''),opts).then(async r=>{let j={};try{j=await r.json()}catch(e){}if(!r.ok||j.ok===false)throw new Error(j.error||('Errore '+r.status));return j})};
let tt;function toast(m,err){const t=$('toast');t.textContent=m;t.className='toast on'+(err?' err':'');clearTimeout(tt);tt=setTimeout(()=>t.className='toast'+(err?' err':''),3200)}
// ---------- dialoghi ----------
function dialog({title,text='',input=null,ok='OK',danger=false,cancel='Annulla'}){return new Promise(res=>{
$('mT').textContent=title;$('mP').textContent=text;$('mP').style.display=text?'':'none';
const i=$('mI');i.style.display=input===null?'none':'';i.value=input||'';
const a=$('mA');a.innerHTML='';
const done=v=>{$('modal').className='modal';document.removeEventListener('keydown',key);res(v)};
const bc=document.createElement('button');bc.className='btn ghost';bc.textContent=cancel;bc.onclick=()=>done(null);
const bo=document.createElement('button');bo.className='btn '+(danger?'danger':'pri');bo.textContent=ok;bo.onclick=()=>done(input===null?true:i.value.trim());
a.append(bc,bo);
const key=e=>{if(e.key==='Escape')done(null);if(e.key==='Enter')bo.click()};document.addEventListener('keydown',key);
$('modal').className='modal on';
if(input!==null){setTimeout(()=>{i.focus();const d=i.value.lastIndexOf('.');i.setSelectionRange(0,d>0?d:i.value.length)},30)}else bo.focus();
})}
$('modal').onclick=e=>{if(e.target.id==='modal')$('mA').firstChild.click()};
const badName=n=>!n||/[\\/:*?"<>|]/.test(n)||n==='.'||n==='..';
// ---------- elenco ----------
async function info(){try{const j=await api('/api/info');const used=j.used,tot=j.total;$('diskt').textContent=fmt(tot-used)+' liberi di '+fmt(tot);$('diskb').style.width=(tot?used*100/tot:0)+'%'}catch(e){}}
async function load(p){
try{const j=await api('/api/ls',{p,h:$('hid').checked?1:0});cwd=j.path||p;items=j.entries;selected.clear();render();
history.replaceState(null,'','#'+encodeURIComponent(cwd))}
catch(e){toast(e.message,true);if(p!=='/')load('/')}
}
function render(){
const c=$('crumbs');c.innerHTML='';const parts=cwd.split('/').filter(Boolean);
const add=(label,path)=>{const a=document.createElement('a');a.href='#';a.textContent=label;a.onclick=e=>{e.preventDefault();load(path)};c.append(a)};
add('Scheda SD','/');let acc='';for(const p of parts){acc+='/'+p;c.insertAdjacentHTML('beforeend','<span>/</span>');add(p,acc)}
items.sort((a,b)=>b.d-a.d||a.n.localeCompare(b.n,'it',{numeric:true,sensitivity:'base'}));
const nd=items.filter(x=>x.d).length,nf=items.length-nd,tot=items.reduce((s,x)=>s+(x.d?0:x.s),0);
$('meta').textContent=(nd?nd+(nd==1?' cartella':' cartelle'):'')+(nd&&nf?' · ':'')+(nf?nf+(nf==1?' file':' file')+' · '+fmt(tot):'');
const L=$('list');
if(!items.length){L.innerHTML='<div class="empty">'+IC.up+'<b>Cartella vuota</b>Trascina qui file o cartelle, oppure usa «Carica».</div>';updSel();return}
L.innerHTML=items.map((x,i)=>{const k=x.d?'dir':kind(x.n);const d=x.t?new Date(x.t*1000).toLocaleDateString('it-IT',{day:'2-digit',month:'short',year:'numeric'}):'';
return '<div class="row '+k+'" data-i="'+i+'"><input type="checkbox" data-c="'+i+'"'+(selected.has(x.n)?' checked':'')+'><div class="ic">'+IC[k]+'</div><div class="nm"><div>'+esc(x.n)+'</div>'+(x.d?'':'<small class="m">'+fmt(x.s)+'</small>')+'</div><div class="sz">'+(x.d?'—':fmt(x.s))+'</div><div class="dt">'+d+'</div><button class="more" data-m="'+i+'" title="Altro">'+IC.more+'</button></div>'}).join('');
updSel();
}
$('list').onclick=e=>{
const m=e.target.closest('[data-m]');if(m){openMenu(items[+m.dataset.m],m);return}
const c=e.target.closest('[data-c]');if(c){const x=items[+c.dataset.c];c.checked?selected.add(x.n):selected.delete(x.n);updSel();return}
const r=e.target.closest('.row');if(!r)return;const x=items[+r.dataset.i];
if(x.d&&e.target.closest('.nm,.ic'))load(join(cwd,x.n));
};
function updSel(){const n=selected.size;$('sel').className='sel'+(n?' on':'');$('selN').textContent=n+(n==1?' elemento selezionato':' elementi selezionati')}
$('selX').onclick=()=>{selected.clear();render()};
$('hid').onchange=()=>load(cwd);
// ---------- menu e azioni ----------
const menu=$('menu');
function openMenu(x,btn){
const acts=[];
if(!x.d)acts.push(['dl','Scarica',()=>{location.href='/api/dl?'+new URLSearchParams({p:join(cwd,x.n)})}]);
acts.push(['ren','Rinomina',()=>rename(x)],['mv','Sposta in…',()=>move([x.n])],['rm','Elimina',()=>remove([x.n]),'red']);
menu.innerHTML='';for(const[ic,l,f,cl]of acts){const b=document.createElement('button');b.innerHTML=IC[ic]+l;if(cl)b.className=cl;b.onclick=()=>{closeMenu();f()};menu.append(b)}
menu.className='menu on';const r=btn.getBoundingClientRect(),mh=menu.offsetHeight,mw=menu.offsetWidth;
menu.style.left=Math.max(8,Math.min(r.right-mw,innerWidth-mw-8))+'px';
menu.style.top=(r.bottom+mh+8>innerHeight?r.top-mh-4:r.bottom+4)+'px';
}
function closeMenu(){menu.className='menu'}
document.addEventListener('click',e=>{if(!e.target.closest('.menu,[data-m]'))closeMenu()});
addEventListener('scroll',closeMenu,{passive:true});
async function rename(x){
const n=await dialog({title:'Rinomina',input:x.n,ok:'Rinomina'});if(n===null||n===x.n)return;
if(badName(n))return toast('Nome non valido',true);
try{await api('/api/mv',{from:join(cwd,x.n),to:join(cwd,n)},{method:'POST'});toast('Rinominato');load(cwd)}catch(e){toast(e.message,true)}
}
async function move(names){
const dest=await dialog({title:'Sposta in…',text:'Percorso della cartella di destinazione (viene creata se non esiste).',input:cwd==='/'?'/':cwd+'/',ok:'Sposta'});
if(dest===null)return;let d=('/'+dest).replace(/\/+/g,'/').replace(/\/$/,'')||'/';
if(d.split('/').some(p=>p==='..'))return toast('Percorso non valido',true);
let ok=0;for(const n of names){try{await api('/api/mv',{from:join(cwd,n),to:join(d,n)},{method:'POST'});ok++}catch(e){toast(n+': '+e.message,true)}}
if(ok)toast(ok==1?'Spostato':ok+' elementi spostati');load(cwd);info();
}
async function remove(names){
const one=names.length==1;const x=one?items.find(i=>i.n===names[0]):null;
const okk=await dialog({title:one?'Eliminare «'+names[0]+'»?':'Eliminare '+names.length+' elementi?',text:(x&&x.d)||!one?'Le cartelle vengono eliminate con tutto il loro contenuto. Non si può annullare.':'Non si può annullare.',ok:'Elimina',danger:true});
if(!okk)return;let n=0;for(const nm of names){try{await api('/api/rm',{p:join(cwd,nm)},{method:'POST'});n++}catch(e){toast(nm+': '+e.message,true)}}
if(n)toast(n==1?'Eliminato':n+' elementi eliminati');load(cwd);info();
}
$('selRm').onclick=()=>remove([...selected]);
$('selMv').onclick=()=>move([...selected]);
$('bMk').onclick=async()=>{const n=await dialog({title:'Nuova cartella',input:'',ok:'Crea'});if(!n)return;if(badName(n))return toast('Nome non valido',true);
try{await api('/api/mkdir',{p:join(cwd,n)},{method:'POST'});load(join(cwd,n))}catch(e){toast(e.message,true)}};
// ---------- caricamento ----------
const queue=[];let cur=null,xhr=null,stats={done:0,bytes:0,total:0,t0:0,sent:0};
const junk=n=>/^(\.DS_Store|Thumbs\.db|desktop\.ini)$/i.test(n)||n.startsWith('._');
function enqueue(list){ // list: [{file, rel}]
list=list.filter(x=>!junk(x.file.name));if(!list.length)return;
const base=cwd;
const clash=list.filter(x=>!x.rel.includes('/')&&items.some(i=>!i.d&&i.n===x.rel));
const go=over=>{
for(const x of list){if(!over&&clash.includes(x))continue;const el=document.createElement('div');el.className='ui';
el.innerHTML='<div>'+esc(x.rel)+'</div><small>in coda</small><div class="pb"><i></i></div>';$('upl').append(el);
queue.push({file:x.file,path:join(base,x.rel),el});stats.total+=x.file.size}
$('up').className='up on';next()};
if(clash.length){dialog({title:clash.length==1?'Il file esiste già':clash.length+' file esistono già',text:clash.length==1?'«'+clash[0].rel+'» è già in questa cartella.':'Alcuni file sono già in questa cartella.',ok:'Sostituisci',cancel:'Salta'}).then(r=>go(!!r))}
else go(true);
}
function next(){
if(cur)return;const j=queue.shift();
if(!j){busy=false;$('upT').textContent=stats.done?'Caricamento completato':'Nessun file caricato';$('upS').textContent=stats.done+' file · '+fmt(stats.bytes);$('upX').textContent='Chiudi';$('upB').style.width='100%';load(cwd);info();return}
busy=true;cur=j;if(!stats.t0)stats.t0=performance.now();$('upX').textContent='Annulla';
const s=j.el.querySelector('small'),b=j.el.querySelector('i');s.textContent='0%';
xhr=new XMLHttpRequest();xhr.open('POST','/api/up?'+new URLSearchParams({p:j.path}));
xhr.upload.onprogress=e=>{const p=e.loaded/j.file.size*100;b.style.width=p+'%';s.textContent=Math.floor(p)+'%';stats.sent=e.loaded;upd()};
const end=(ok,msg)=>{cur=null;xhr=null;s.textContent=ok?'✓':msg;s.className=ok?'ok':'err';b.style.width=ok?'100%':b.style.width;
if(ok){stats.done++;stats.bytes+=j.file.size}stats.sent=0;upd();
if(ok&&j.path.slice(0,j.path.lastIndexOf('/')||1)===cwd)debLoad();next()};
xhr.onload=()=>{let m='errore';try{m=JSON.parse(xhr.responseText).error||m}catch(e){}end(xhr.status==200,m)};
xhr.onerror=()=>end(false,'connessione persa');xhr.onabort=()=>end(false,'annullato');
xhr.send(j.file);
}
let dl;const debLoad=()=>{clearTimeout(dl);dl=setTimeout(()=>{if(!selected.size)load(cwd)},400)};
function upd(){
const dt=(performance.now()-stats.t0)/1000,sent=stats.bytes+stats.sent,sp=dt>0.5?sent/dt:0;
$('upT').textContent='Caricamento '+(stats.done+(cur?1:0))+' di '+(stats.done+queue.length+(cur?1:0));
$('upS').textContent=fmt(sent)+' di '+fmt(stats.total)+(sp?' · '+fmt(sp)+'/s':'');
$('upB').style.width=(stats.total?sent*100/stats.total:0)+'%';
}
$('upX').onclick=e=>{e.stopPropagation();
if(!busy){$('up').className='up';$('upl').innerHTML='';stats={done:0,bytes:0,total:0,t0:0,sent:0};return}
while(queue.length){const j=queue.shift();j.el.querySelector('small').textContent='annullato'}if(xhr)xhr.abort()};
$('uph').onclick=()=>$('up').classList.toggle('min');
$('bFiles').onclick=()=>$('fFiles').click();
$('bDir').onclick=()=>$('fDir').click();
$('fFiles').onchange=e=>{enqueue([...e.target.files].map(f=>({file:f,rel:f.name})));e.target.value=''};
$('fDir').onchange=e=>{enqueue([...e.target.files].map(f=>({file:f,rel:f.webkitRelativePath||f.name})));e.target.value=''};
// trascina e rilascia (anche cartelle)
let dc=0;
addEventListener('dragenter',e=>{if(![...e.dataTransfer.types].includes('Files'))return;e.preventDefault();dc++;$('dropTo').textContent='in '+(cwd==='/'?'Scheda SD':cwd);$('drop').className='drop on'});
addEventListener('dragleave',e=>{if(--dc<=0){dc=0;$('drop').className='drop'}});
addEventListener('dragover',e=>e.preventDefault());
addEventListener('drop',async e=>{e.preventDefault();dc=0;$('drop').className='drop';
const its=[...(e.dataTransfer.items||[])].map(i=>i.webkitGetAsEntry&&i.webkitGetAsEntry()).filter(Boolean);
if(!its.length){enqueue([...e.dataTransfer.files].map(f=>({file:f,rel:f.name})));return}
const out=[];
const walk=async(en,pre)=>{if(en.isFile){const f=await new Promise((r,j)=>en.file(r,j));out.push({file:f,rel:pre+f.name})}
else if(en.isDirectory){const rd=en.createReader();let batch;do{batch=await new Promise((r,j)=>rd.readEntries(r,j));for(const c of batch)await walk(c,pre+en.name+'/')}while(batch.length)}};
for(const en of its)await walk(en,'');enqueue(out)});
// ---------- fine ----------
$('bDone').onclick=async()=>{
if(busy&&!await dialog({title:'Caricamento in corso',text:'Se chiudi adesso i file non ancora caricati andranno persi.',ok:'Chiudi comunque',danger:true}))return;
if(xhr)xhr.abort();try{await api('/api/done',{},{method:'POST'})}catch(e){}$('off').className='off on'};
addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue=''}});
const h=decodeURIComponent(location.hash.slice(1));load(h&&h[0]==='/'?h:'/');info();
})();
</script>
</body>
</html>)ONDAWEB";
