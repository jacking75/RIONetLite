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
  const renderers={sequence,state};
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
