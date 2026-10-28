(function(root){
  'use strict';
  const kinds=['sequence','state','ring','slots','queue','bytes','graph','layers'];
  const esc=value=>String(value??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const num=(value,fallback=0)=>Number.isFinite(Number(value))?Number(value):fallback;
  const text=(x,y,value,anchor='middle',extra='')=>`<text x="${x}" y="${y}" text-anchor="${anchor}" ${extra}>${esc(value)}</text>`;
  const lines=(x,y,value,anchor='middle')=>{
    const raw=String(value??'');const parts=[];
    for(let i=0;i<raw.length;i+=18)parts.push(raw.slice(i,i+18));
    return parts.map((part,i)=>text(x,y+i*20,part,anchor)).join('');
  };
  const box=(x,y,w,h,label,sub='',active=false,attrs='')=>`<g class="figure-node${active?' active':''}" ${attrs}><rect x="${x}" y="${y}" width="${w}" height="${h}" rx="5" style="fill:var(${active?'--soft':'--paper'});stroke:var(${active?'--accent':'--line'});stroke-width:${active?2:1}"/>${lines(x+w/2,y+25,label)}${sub?text(x+w/2,y+h-12,sub,'middle','class="figure-sub"'):''}</g>`;
  function arrow(x1,y1,x2,y2,label='',kind='call',opacity=1){
    const color=kind==='blocked'?'--danger':kind==='callback'?'--gold':'--accent';
    const dashed=kind==='callback'?'stroke-dasharray="7 5"':'';
    const style=`style="stroke:var(${color});fill:none;stroke-width:${kind==='data'?4:2}"`;
    const head=x2>=x1?`${x2-8},${y2-5} ${x2},${y2} ${x2-8},${y2+5}`:`${x2+8},${y2-5} ${x2},${y2} ${x2+8},${y2+5}`;
    const path=x1===x2?`M${x1} ${y1} h45 v22 h-45`:`M${x1} ${y1} L${x2} ${y2}`;
    return `<g opacity="${opacity}"><path d="${path}" ${style} ${dashed}/>${kind==='completion'?`<path d="M${x1} ${y1+5} L${x2} ${y2+5}" ${style}/>`:''}<polyline points="${head}" ${style}/>${text((x1+x2)/2+(x1===x2?50:0),(y1+y2)/2-10,label)}${kind==='blocked'?`<path d="M${(x1+x2)/2-7} ${y1-7} l14 14 m0 -14 l-14 14" ${style}/>`:''}</g>`;
  }
  const stepCount=(_kind,spec)=>Math.max(1,spec.steps?.length??0);
  function sequence(spec,step){
    const lanes=spec.lanes??[];if(!lanes.length)throw new Error('sequence needs lanes');
    const width=Math.max(640,lanes.length*205);let y=112;let body='';const activeIds=[];
    const position=new Map(lanes.map((lane,i)=>[lane.id,95+i*(width-190)/Math.max(1,lanes.length-1)]));
    const rows=[];
    (spec.steps??[]).forEach((frame,i)=>{for(const [j,message]of (frame.messages??[]).entries()){
      if(!position.has(message.from)||!position.has(message.to))throw new Error('Unknown sequence lane');
      const id=`${spec.id??'seq'}-${i}-${j}`;if(i<=step)activeIds.push(id);
      rows.push(`<g data-step="${i}" tabindex="0" role="button" aria-label="${esc(frame.title)}"><circle cx="25" cy="${y}" r="14" style="fill:var(${i===step?'--soft':'--paper'});stroke:var(--accent)"/>${text(25,y+5,i+1)}${arrow(position.get(message.from),y,position.get(message.to),y,message.label,message.kind,i<=step?1:.25)}</g>`);y+=60;
    }});
    const height=Math.max(210,y+30);
    for(const lane of lanes){const x=position.get(lane.id);body+=box(x-82,12,164,60,lane.label,lane.role??'');body+=`<path d="M${x} 72 V${height-20}" style="stroke:var(--line);stroke-dasharray:5 5;fill:none"/>`;}
    for(const note of spec.notes??[]){const x=position.get(note.lane);if(x===undefined)throw new Error('Unknown note lane');body+=box(Math.max(10,Math.min(width-190,x+12)),94+num(note.step)*60,180,62,note.text,'',false);}
    return {body:body+rows.join(''),width,height,activeIds};
  }
  function state(spec,step){
    const nodes=spec.states??[];const count=stepCount('state',spec);
    for(const edge of spec.transitions??[])if(edge.step<1||edge.step>count)throw new Error('State transition step outside range');
    const width=Math.max(640,nodes.length*185);const position=new Map(nodes.map((n,i)=>[n.id,20+i*185]));const active=spec.steps?.[step]?.active??spec.active;let body='';
    for(const [i,edge]of (spec.transitions??[]).entries()){
      if(!position.has(edge.from)||!position.has(edge.to))throw new Error('Unknown state endpoint');
      const a=position.get(edge.from)+75,b=position.get(edge.to)+75,y=165+i%3*38;
      body+=`<path d="M${a} 110 V${y} H${b} V110" style="stroke:var(${edge.step===step+1?'--accent':'--line'});fill:none;stroke-width:2"/>${text((a+b)/2,y-7,edge.label)}`;
    }
    nodes.forEach((n,i)=>{body+=box(position.get(n.id),35,150,76,n.label,n.id,n.id===active,`data-step="${Math.min(i,count-1)}" tabindex="0" role="button" aria-label="${esc(n.label)}"`);});
    return {body,width,height:300,activeIds:active?[active]:[]};
  }
  function ring(spec){
    const capacity=Math.max(1,Math.trunc(num(spec.capacity,8)));const read=((num(spec.read)%capacity)+capacity)%capacity;
    const write=((num(spec.write)%capacity)+capacity)%capacity;const used=Math.max(0,Math.min(capacity,num(spec.used)));const posted=Math.max(0,num(spec.posted));
    const cells=Math.min(capacity,64),stride=capacity/cells,activeIds=[];let body='';const cx=165,cy=177,r=100;
    for(let i=0;i<cells;i++){
      const index=Math.floor(i*stride);const occupied=(index-read+capacity)%capacity<used;
      const reserved=(index-write+capacity)%capacity<posted;const a=i*2*Math.PI/cells-Math.PI/2,b=(i+1)*2*Math.PI/cells-Math.PI/2;
      const point=(radius,angle)=>[cx+radius*Math.cos(angle),cy+radius*Math.sin(angle)];
      const [x1,y1]=point(r,a),[x2,y2]=point(r,b),[x3,y3]=point(r-25,b),[x4,y4]=point(r-25,a);
      body+=`<path data-ring-index="${index}" class="${occupied?'occupied':reserved?'posted':'empty'}" d="M${x1} ${y1} A${r} ${r} 0 ${b-a>Math.PI?1:0} 1 ${x2} ${y2} L${x3} ${y3} A${r-25} ${r-25} 0 ${b-a>Math.PI?1:0} 0 ${x4} ${y4} Z" style="fill:var(${occupied?'--soft':reserved?'--bg':'--paper'});stroke:var(${reserved?'--gold':'--accent'});stroke-width:${reserved?3:1}"/>`;
      const [tx,ty]=point(r+18,(a+b)/2);if(cells<=16)body+=text(tx,ty+5,index);
      if(occupied)activeIds.push('ring-'+index);
    }
    for(const [label,value,color,inner]of [['read',read,'--accent',56],['write',write,'--gold',77]]){
      const angle=value/capacity*2*Math.PI-Math.PI/2;const x=cx+inner*Math.cos(angle),y=cy+inner*Math.sin(angle);
      body+=`<path d="M${cx} ${cy} L${x} ${y}" style="stroke:var(${color});stroke-width:3;fill:none"/>${text(cx,cy+(label==='read'?25:45),label+'='+value)}`;
    }
    const status=used===0?'비어 있음':used===capacity?'가득 참':'미소비 '+used+' / '+capacity;
    body+=text(cx,32,status);body+=text(615,32,'같은 메모리를 두 번 매핑한 주소 뷰');
    const count=Math.min(capacity,32),cell=540/(count*2);
    for(let i=0;i<count*2;i++){
      const index=Math.floor((i%count)*capacity/count);const occupied=(index-read+capacity)%capacity<used;const x=350+i*cell;
      body+=`<rect data-view="${i<count?1:2}" data-index="${index}" x="${x}" y="110" width="${cell-1}" height="62" style="fill:var(${occupied?'--soft':'--paper'});stroke:var(--line)"/>`;
      if(count<=16)body+=text(x+cell/2,145,index);
    }
    body+=text(485,94,'view 1');body+=text(755,94,'view 2');
    const start=350+read/capacity*270,end=start+used/capacity*270;
    body+=`<path d="M${start} 193 H${end}" style="stroke:var(--accent);stroke-width:8;fill:none"/>`;
    body+=text(620,225,`연속 구간 ${read}부터 ${used}바이트 · 게시 ${posted}`);
    body+=text(620,258,used===capacity?'read와 write가 같아도 used가 capacity면 가득 차 있다.':'읽기 구간과 커널 게시 구간은 서로 겹치지 않는다.');
    if(spec.inFlight!==undefined)body+=text(620,290,'송신 중 '+spec.inFlight+'바이트 · 나머지는 게시 대기');
    return {body,width:920,height:330,activeIds};
  }
  function slots(spec){
    const workers=Math.max(1,num(spec.workers,2));const all=spec.slots??[];const rows=Math.max(1,...Array.from({length:workers},(_,w)=>all.filter(s=>num(s.owner,s.index%workers)===w).length));
    const width=Math.max(640,workers*260);let body='';const activeIds=[];
    for(let worker=0;worker<workers;worker++){
      body+=text(130+worker*260,28,'워커 '+worker);let row=0;
      for(const slot of all.filter(s=>num(s.owner,s.index%workers)===worker)){
        const free=(spec.free??[]).indexOf(slot.index);const active=!['free','kFree','비어 있음'].includes(slot.state);
        const label=`슬롯 ${slot.index} · 세대 ${slot.generation??0}`;const sub=free>=0?`free 순서 ${free+1}`:slot.state;
        body+=box(20+worker*260,55+row*95,220,78,label,sub,active);if(active)activeIds.push('slot-'+slot.index);row++;
      }
    }
    if(spec.note)body+=text(width/2,65+rows*95,spec.note);
    return {body,width,height:Math.max(190,100+rows*95),activeIds};
  }
  function queue(spec){
    const shards=spec.shards??[];const width=860;let body='';const activeIds=[];
    for(const [index,shard]of shards.entries()){
      const cells=(shard.slots??[]).slice(0,16);const total=Math.max(1,shard.capacity??cells.length);const y=60+index*135;
      const head=num(shard.head),count=num(shard.count);body+=text(20,y-15,`샤드 ${index} · ${count}/${total}${count>=total?' · 가득 참':''}`,'start');
      for(let i=0;i<cells.length;i++){
        const occupied=(i-head+total)%total<count;const x=35+i*48;
        body+=box(x,y,44,60,String(cells[i]??'·'),'',occupied);if(occupied)activeIds.push(`shard-${index}-${i}`);
        if(i===head)body+=text(x+22,y+83,'↑head');
      }
    }
    const height=Math.max(190,60+shards.length*135);let y=height-18;
    for(const gate of spec.gates??[]){body+=text(25,y,`${gate.parked?'⏸ 종료 보류':'✓ 게이트 열림'} · ${gate.session}`,'start');y+=24;}
    return {body,width,height:y+30,activeIds};
  }
  function bytes(spec){
    const values=(spec.bytes??[]).slice(0,64),columns=16;let body='';const activeIds=[];
    values.forEach((value,i)=>{
      const x=20+(i%columns)*48,y=55+Math.floor(i/columns)*100;const arrived=i<num(spec.arrived,values.length),consumed=i<num(spec.consumed);
      body+=box(x,y,44,65,Number(value).toString(16).padStart(2,'0').toUpperCase(),String(i),arrived&&!consumed);
      if(i<num(spec.headerBytes))body+=`<path d="M${x} ${y} h44" style="stroke:var(--gold);stroke-width:5;fill:none"/>`;
      if(consumed)body+=`<path d="M${x+3} ${y+12} l38 40" style="stroke:var(--muted);fill:none"/>`;if(arrived&&!consumed)activeIds.push('byte-'+i);
    });
    body+=text(20,27,`헤더 ${spec.headerBytes??0} · 도착 ${spec.arrived??values.length} · 소비 ${spec.consumed??0}`,'start');
    const height=90+Math.ceil(values.length/columns)*100;
    if(spec.marks)body+=text(20,height-20,spec.marks.map(m=>typeof m==='string'?m:m.label).join(' · '),'start');
    return {body,width:820,height,activeIds};
  }
  function graph(spec){
    const nodes=spec.nodes??[],edges=spec.edges??[];const ids=new Set(nodes.map(n=>n.id));const levels=new Map(),pending=new Set(ids);
    while(pending.size){let progress=false;for(const id of [...pending]){
      const incoming=edges.filter(e=>e.to===id);if(incoming.some(e=>!ids.has(e.from)))throw new Error('Unknown graph endpoint');
      if(incoming.every(e=>levels.has(e.from))){levels.set(id,Math.max(0,...incoming.map(e=>levels.get(e.from)+1)));pending.delete(id);progress=true;}
    }if(!progress)throw new Error('Dependency graph contains a cycle');}
    if(edges.some(e=>!ids.has(e.to)))throw new Error('Unknown graph endpoint');
    const max=Math.max(0,...levels.values()),groups=Array.from({length:max+1},(_,level)=>nodes.filter(n=>levels.get(n.id)===level));
    const width=Math.max(700,...groups.map(g=>g.length*240+40)),positions=new Map();
    groups.forEach((group,level)=>group.forEach((n,i)=>positions.set(n.id,{x:20+i*240,y:25+level*120})));
    let body='';for(const edge of edges){const a=positions.get(edge.from),b=positions.get(edge.to);body+=`<path d="M${a.x+105} ${a.y+75} V${b.y-15} H${b.x+105} V${b.y}" style="stroke:var(--accent);fill:none;stroke-width:2"/><polyline points="${b.x+100},${b.y-8} ${b.x+105},${b.y} ${b.x+110},${b.y-8}" style="stroke:var(--accent);fill:none"/>${text(b.x+120,b.y-8,edge.label??'','start')}`;}
    const activeIds=[];for(const n of nodes){const p=positions.get(n.id),active=n.state==='done'||n.state==='active';body+=box(p.x,p.y,210,75,n.label,n.state??'대기',active);if(n.state==='blocked')body+=text(p.x+190,p.y+55,'✕','middle','style="fill:var(--danger)"');if(active)activeIds.push(n.id);}
    return {body,width,height:(max+1)*120+20,activeIds};
  }
  function layers(spec){
    const layers=spec.layers??[];const max=Math.max(1,...layers.map(l=>l.nodes.length));const width=Math.max(760,max*180+120),positions=new Map();let body='';
    layers.forEach((layer,row)=>{body+=text(15,35+row*145,layer.label,'start');layer.nodes.forEach((n,i)=>positions.set(n.id,{x:20+i*180,y:55+row*145,n}));});
    for(const link of spec.links??[]){const a=positions.get(link.from),b=positions.get(link.to);if(!a||!b)throw new Error('Unknown layer endpoint');body+=arrow(a.x+75,a.y+78,b.x+75,b.y-5,link.label,link.kind,.6);}
    const activeIds=spec.focus??[];
    for(const [id,{x,y,n}]of positions)body+=box(x,y,160,78,n.label,n.thread??n.sub??'',activeIds.includes(id),`data-map-node="${esc(id)}" tabindex="0" role="button" aria-label="${esc(n.label)}"`);
    return {body,width,height:layers.length*145+20,activeIds};
  }

  const renderers={sequence,state,ring,slots,queue,bytes,graph,layers};
  function render(kind,spec,step=0){
    if(!kinds.includes(kind)||!renderers[kind])throw new Error('Unknown figure kind: '+kind);
    step=Math.max(0,Math.min(stepCount(kind,spec)-1,Math.trunc(num(step))));
    const frame=spec.steps?.[step]?.spec?{...spec,...spec.steps[step].spec}:spec;
    const result=renderers[kind](frame,step);
    const svg=`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${result.width} ${result.height}" role="group" aria-label="${esc(spec.title??'상태와 데이터 흐름')}" class="book-figure">${result.body}</svg>`;
    return {svg,width:result.width,height:result.height,activeIds:result.activeIds};
  }
  const api={kinds,render,stepCount};root.BookFigures=api;
  if(typeof module!=='undefined'&&module.exports)module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
