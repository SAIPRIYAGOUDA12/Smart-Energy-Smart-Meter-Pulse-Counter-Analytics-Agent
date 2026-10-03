#pragma once
// Interactive web dashboard, embedded in the binary and served by smeterd at "/".
// Self-contained (no external libraries / network needed): canvas charts + fetch() polling.
namespace sm {

inline const char* dashboard_html() {
    return R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Meter Agent</title>
<style>
:root{--bg:#0f1420;--card:#171e2e;--fg:#e8ecf4;--mut:#8b96ad;--line:#2a3550;--acc:#4da3ff;--ok:#3ddc97;--warn:#ffb020;--bad:#ff5d5d}
@media (prefers-color-scheme:light){:root{--bg:#f1f4fa;--card:#fff;--fg:#1a2233;--mut:#6a7690;--line:#dde3f0;--acc:#1f78ff;--ok:#0aa468;--warn:#d98500;--bad:#d93636}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:14px system-ui,Segoe UI,Roboto,sans-serif}
header{display:flex;flex-wrap:wrap;gap:10px;align-items:center;padding:14px 20px;border-bottom:1px solid var(--line)}
h1{font-size:18px;margin:0;flex:1}.badge{padding:4px 12px;border-radius:99px;font-weight:700;font-size:12px;background:var(--line)}
.MEASURING{background:var(--ok);color:#06281a}.IDLE{background:var(--mut);color:#fff}.OVERLOAD{background:var(--bad);color:#fff;animation:p 1s infinite}.FAULT{background:var(--warn);color:#241700}.INIT{background:var(--line)}
@keyframes p{50%{opacity:.55}}
main{padding:16px 20px;display:grid;gap:14px;grid-template-columns:repeat(4,1fr)}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px}
.card h3{margin:0 0 6px;font-size:12px;color:var(--mut);font-weight:600;text-transform:uppercase;letter-spacing:.05em}
.big{font-size:30px;font-weight:700}.sub{color:var(--mut);font-size:12px;margin-top:2px}
.wide{grid-column:span 4}.half{grid-column:span 2}
.bar{height:8px;background:var(--line);border-radius:6px;margin-top:10px;overflow:hidden}.bar>i{display:block;height:100%;width:0;background:var(--ok);transition:width .4s,background .4s}
canvas{width:100%;height:220px;display:block;cursor:crosshair}
.tools{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin-bottom:8px}.tools h3{margin:0;flex:1}
button{background:var(--card);color:var(--fg);border:1px solid var(--line);border-radius:8px;padding:5px 11px;cursor:pointer;font:inherit}
button:hover{border-color:var(--acc)}button.on{background:var(--acc);border-color:var(--acc);color:#fff}
ul{list-style:none;margin:0;padding:0;max-height:210px;overflow:auto}li{padding:5px 8px;border-left:3px solid var(--acc);margin-bottom:4px;background:var(--bg);border-radius:4px;font-size:13px}
li.a{border-color:var(--warn)}li.o{border-color:var(--bad)}li.g{border-color:var(--ok)}
#err{display:none;background:var(--bad);color:#fff;padding:8px 20px;font-weight:600}
@media(max-width:900px){main{grid-template-columns:1fr 1fr}.wide,.half{grid-column:span 2}}
</style></head><body>
<div id="err">Cannot reach smeterd - retrying...</div>
<header><h1>&#9889; Smart Meter Agent</h1><span id="src" class="sub"></span><span id="clock" class="sub"></span>
<span id="state" class="badge INIT">INIT</span><button id="pause">&#10074;&#10074; Pause</button><button id="reset">&#10226; Reset counters</button><button id="restart" style="border-color:var(--bad);color:var(--bad)">&#8635; Restart daemon</button></header>
<main>
<div class="card"><h3>Power now</h3><div class="big" id="pw">-</div><div class="sub" id="sm"></div><div class="bar"><i id="bar"></i></div><div class="sub" id="lim"></div></div>
<div class="card"><h3>Energy</h3><div class="big" id="en">-</div><div class="sub" id="pl"></div></div>
<div class="card"><h3>Cost</h3><div class="big" id="co">-</div><div class="sub" id="ra"></div></div>
<div class="card"><h3>Peak demand</h3><div class="big" id="pk">-</div><div class="sub" id="qa"></div></div>
<div class="card wide"><div class="tools"><h3>Live power (last 2 min, 1 s samples) - hover for values</h3></div><canvas id="c1"></canvas></div>
<div class="card wide"><div class="tools"><h3>Per-minute history</h3>
 <span id="metric"><button data-m="avg_w" class="on">Avg W</button><button data-m="peak_w">Peak W</button><button data-m="energy_wh">Energy Wh</button></span>
 <span id="range"><button data-n="15">15m</button><button data-n="60" class="on">1h</button><button data-n="240">4h</button><button data-n="1440">24h</button></span>
 <button id="csv">&#11015; CSV</button></div><canvas id="c2"></canvas></div>
<div class="card half"><h3>Event log</h3><ul id="ev"></ul></div>
<div class="card half"><h3>Counters</h3><div id="cnt" class="sub" style="line-height:1.9"></div></div>
</main>
<script>
const $=i=>document.getElementById(i),cs=n=>getComputedStyle(document.documentElement).getPropertyValue(n).trim();
class Chart{
 constructor(cv,unit){this.cv=cv;this.unit=unit;this.ys=[];this.xs=[];this.hi=null;this.limit=null;this.bars=false;
  cv.addEventListener('mousemove',e=>{const r=cv.getBoundingClientRect(),n=this.ys.length;if(!n)return;
   const f=(e.clientX-r.left-46)/(r.width-56);this.hi=Math.max(0,Math.min(n-1,Math.round(f*(n-1))));this.draw();});
  cv.addEventListener('mouseleave',()=>{this.hi=null;this.draw();});addEventListener('resize',()=>this.draw());}
 set(ys,xs){this.ys=ys;this.xs=xs;this.draw();}
 draw(){const cv=this.cv,r=cv.getBoundingClientRect(),d=devicePixelRatio||1;cv.width=r.width*d;cv.height=r.height*d;
  const c=cv.getContext('2d');c.setTransform(d,0,0,d,0,0);const W=r.width,H=r.height,L=46,R=10,T=10,B=22,n=this.ys.length;
  c.font='11px system-ui';c.fillStyle=cs('--mut');c.strokeStyle=cs('--line');
  let mx=Math.max(1,...this.ys,this.limit||0)*1.1;
  for(let i=0;i<=4;i++){const y=T+(H-T-B)*i/4;c.beginPath();c.moveTo(L,y);c.lineTo(W-R,y);c.stroke();c.fillText(Math.round(mx*(1-i/4)),4,y+4);}
  if(!n){c.fillText('waiting for data...',L+10,H/2);return;}
  const X=i=>L+(n>1?i/(n-1):.5)*(W-L-R),Y=v=>T+(H-T-B)*(1-v/mx),acc=cs('--acc');
  if(this.limit){c.strokeStyle=cs('--bad');c.setLineDash([6,4]);c.beginPath();c.moveTo(L,Y(this.limit));c.lineTo(W-R,Y(this.limit));c.stroke();c.setLineDash([]);c.fillStyle=cs('--bad');c.fillText('overload limit',L+4,Y(this.limit)-4);}
  c.strokeStyle=acc;c.fillStyle=acc;
  if(this.bars){const bw=Math.max(1,(W-L-R)/n-1);this.ys.forEach((v,i)=>c.fillRect(X(i)-bw/2,Y(v),bw,H-B-Y(v)));}
  else{c.lineWidth=2;c.beginPath();this.ys.forEach((v,i)=>i?c.lineTo(X(i),Y(v)):c.moveTo(X(i),Y(v)));c.stroke();c.lineWidth=1;
   const g=c.createLinearGradient(0,T,0,H-B);g.addColorStop(0,acc+'55');g.addColorStop(1,acc+'00');c.lineTo(X(n-1),H-B);c.lineTo(X(0),H-B);c.fillStyle=g;c.fill();}
  c.fillStyle=cs('--mut');c.fillText(this.xs[0]||'',L,H-6);const l=this.xs[n-1]||'';c.fillText(l,W-R-c.measureText(l).width,H-6);
  if(this.hi!=null){const i=this.hi,x=X(i),y=Y(this.ys[i]);c.strokeStyle=cs('--fg');c.beginPath();c.moveTo(x,T);c.lineTo(x,H-B);c.stroke();
   c.fillStyle=acc;c.beginPath();c.arc(x,y,4,0,7);c.fill();
   const t=this.ys[i].toFixed(1)+' '+this.unit+'  '+(this.xs[i]||''),w=c.measureText(t).width+14,bx=Math.min(Math.max(x-w/2,L),W-R-w);
   c.fillStyle=cs('--card');c.strokeStyle=cs('--line');c.fillRect(bx,T+2,w,22);c.strokeRect(bx,T+2,w,22);c.fillStyle=cs('--fg');c.fillText(t,bx+7,T+17);}}
}
const live=new Chart($('c1'),'W'),hist=new Chart($('c2'),'W');hist.bars=true;
let liveY=[],liveX=[],metric='avg_w',range=60,paused=false,lastHist=null,lastStatus=null;
const hhmm=s=>{const d=new Date(s*1000);return d.toTimeString().slice(0,8);};
const units={avg_w:'W',peak_w:'W',energy_wh:'Wh'};
function renderHist(){if(!lastHist)return;hist.unit=units[metric];hist.set(lastHist[metric],lastHist.t.map(t=>hhmm(t)));}
async function poll(){
 try{
  const s=await (await fetch('/api/status')).json();lastStatus=s;$('err').style.display='none';restarting=false;restarting=false;
  $('state').textContent=s.state;$('state').className='badge '+s.state;$('src').textContent=s.source;$('clock').textContent='meter time '+hhmm(s.wall_s);
  $('pw').textContent=s.power_w.toFixed(0)+' W';$('sm').textContent='smoothed '+s.smooth_w.toFixed(0)+' W';
  const pct=Math.min(100,s.power_w/s.overload_w*100);$('bar').style.width=pct+'%';$('bar').style.background=pct>90?cs('--bad'):pct>60?cs('--warn'):cs('--ok');
  $('lim').textContent=pct.toFixed(0)+'% of '+s.overload_w+' W limit';
  $('en').textContent=s.energy_kwh.toFixed(3)+' kWh';$('pl').textContent=s.pulses+' pulses';
  $('co').textContent=s.cost.toFixed(2);$('ra').textContent='current rate '+s.rate.toFixed(2)+' / kWh';
  $('pk').textContent=s.peak_w.toFixed(0)+' W';$('qa').textContent=s.spikes+' anomalies flagged';
  $('cnt').innerHTML='Pulses counted: <b>'+s.pulses+'</b><br>Bounce rejected: <b>'+s.debounced+'</b><br>Out-of-order: <b>'+s.out_of_order+'</b><br>Spikes: <b>'+s.spikes+'</b>';
  $('ev').innerHTML=s.events.slice().reverse().map(e=>'<li class="'+(e.includes('OVERLOAD')?'o':e.includes('anomaly')?'a':e.includes('MEASURING')?'g':'')+'">'+e+'</li>').join('')||'<li>no events yet</li>';
  liveY.push(s.power_w);liveX.push(new Date().toTimeString().slice(0,8));if(liveY.length>120){liveY.shift();liveX.shift();}
  live.limit=s.overload_w;live.set(liveY.slice(),liveX.slice());
  const h=await (await fetch('/api/history?n='+range)).json();lastHist=h;renderHist();
 }catch(e){$('err').textContent=restarting?'Restarting daemon... reconnecting automatically':'Cannot reach smeterd - retrying...';$('err').style.display='block';}
}
let restarting=false;
const post=async p=>{const r=await fetch(p,{method:'POST',headers:{'X-Smeter':'1'}});if(!r.ok)throw 0;};
$('restart').onclick=async()=>{
 if(!confirm('Restart the smeterd daemon?\n\nThe whole program restarts (a few seconds). Energy and cost totals are kept; live charts and history start fresh.'))return;
 try{await post('/api/restart');restarting=true;liveY=[];liveX=[];lastHist=null;live.set([],[]);hist.set([],[]);
  $('err').textContent='Restarting daemon... reconnecting automatically';$('err').style.display='block';}
 catch(e){alert('Restart failed - is smeterd running?');}};
$('reset').onclick=async()=>{
 if(!confirm('Reset counters?\n\nEnergy, cost, history and events go back to zero and the simulation restarts.'))return;
 try{await post('/api/reset');liveY=[];liveX=[];lastHist=null;live.set([],[]);hist.set([],[]);setTimeout(poll,300);}
 catch(e){alert('Reset failed - is smeterd running?');}};
$('pause').onclick=()=>{paused=!paused;$('pause').innerHTML=paused?'&#9654; Resume':'&#10074;&#10074; Pause';$('pause').classList.toggle('on',paused);};
document.querySelectorAll('#metric button').forEach(b=>b.onclick=()=>{metric=b.dataset.m;document.querySelectorAll('#metric button').forEach(x=>x.classList.toggle('on',x===b));renderHist();});
document.querySelectorAll('#range button').forEach(b=>b.onclick=()=>{range=+b.dataset.n;document.querySelectorAll('#range button').forEach(x=>x.classList.toggle('on',x===b));poll();});
$('csv').onclick=()=>{if(!lastHist)return;let t='time,avg_w,peak_w,energy_wh\n';lastHist.t.forEach((x,i)=>t+=hhmm(x)+','+lastHist.avg_w[i]+','+lastHist.peak_w[i]+','+lastHist.energy_wh[i]+'\n');
 const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([t],{type:'text/csv'}));a.download='smeter_history.csv';a.click();};
poll();setInterval(()=>{if(!paused)poll();},1000);
</script></body></html>)HTML";
}

}  // namespace sm
