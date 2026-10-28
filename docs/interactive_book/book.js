'use strict';
const $ = id => document.getElementById(id);
const all = (selector, root = document) => [...root.querySelectorAll(selector)];
const escapeHtml = value => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const chapters = BOOK.manifest.chapters;
const articles = all('.chapter');
const available = chapters.filter(c => $(c.slug));
const labSessions = new Map();
const figureSessions = new Map();
const playbackSessions = new Set();
const expandedArchives = new Set();
const components = {figure:initFigure,walkthrough:initWalkthrough,compare:initCompare,quiz:initQuiz,map:initMap};
const scrollPositions = new Map();
const storageKey = 'rionetlite.book.reader.v2';
let current = available[0];
let currentSection = '';
let lastHighlight = null;
let sourceFrame = null;
let sourceFrames = [];
let sourceOpener = null;
let scenarioLab = null;
let storageWorks = true;
let firstRoute = true;
let persisted = {schemaVersion:3, edition:BOOK.manifest.edition, checks:{}, position:null, theme:null,quiz:{},route:null,showArchive:false};
function notice(text) { $('notice').textContent=text; $('notice').hidden=!text; }
try {
  const raw=localStorage.getItem(storageKey);
  if(raw){
    const value=JSON.parse(raw);
    if(value && [2,3].includes(value.schemaVersion) && value.edition===BOOK.manifest.edition &&
      value.checks && typeof value.checks==='object' && !Array.isArray(value.checks)) persisted={...persisted,...value,schemaVersion:3};
    else notice('저장된 읽기 기록이 이번 판본과 달라 새로 시작한다.');
  }
  localStorage.setItem(storageKey,JSON.stringify(persisted));
}catch{storageWorks=false;$('storageStatus').textContent='이 브라우저에서는 읽기 기록을 저장할 수 없다. 본문과 실험은 계속 사용할 수 있다.';}
function save(){if(storageWorks)try{localStorage.setItem(storageKey,JSON.stringify(persisted));}catch{storageWorks=false;$('storageStatus').textContent='읽기 기록 저장이 중단됐다. 현재 창의 실험은 계속 사용할 수 있다.';}}
function chapterUrl(chapter,section=''){return '#'+new URLSearchParams({chapter:chapter.slug,...(section?{section}:{})});}
function setTheme(value){document.documentElement.dataset.theme=value;$('theme').textContent=value==='dark'?'밝은 화면':'어두운 화면';}
setTheme(['dark','light'].includes(persisted.theme)?persisted.theme:matchMedia('(prefers-color-scheme: dark)').matches?'dark':'light');
$('theme').onclick=()=>{persisted.theme=document.documentElement.dataset.theme==='dark'?'light':'dark';setTheme(persisted.theme);save();};
function stopPlayback(){for(const session of [...labSessions.values(),...figureSessions.values()])if(session.timer){clearInterval(session.timer);session.timer=null;session.playButton.textContent=session.definition?'단계별 재생':'재생';}playbackSessions.clear();}
function initComponents(article){for(const [kind,init]of Object.entries(components))for(const host of all(`[data-${kind}]`,article))if(!host.dataset.initialized){init(host);host.dataset.initialized='true';}}
function componentStub(host){host.replaceChildren();const p=document.createElement('p');p.className='small';p.textContent='집필 중인 학습 컴포넌트다.';host.append(p);}
function initFigure(host){componentStub(host);}
function initWalkthrough(host){componentStub(host);}
function initCompare(host){componentStub(host);}
function initQuiz(host){componentStub(host);}
function initMap(host){componentStub(host);}
function printComponents(clone){for(const kind of Object.keys(components))for(const host of all(`[data-${kind}]`,clone)){host.replaceChildren();host.textContent='인쇄판에서는 생략한다.';}}
function makeButton(text,fn,className=''){const b=document.createElement('button');b.type='button';b.textContent=text;b.className=className;b.onclick=fn;return b;}
function makeToc(){
  $('navigation').replaceChildren();
  BOOK.manifest.parts.forEach((part,index)=>{
    const group=document.createElement('details');group.open=true;
    const heading=document.createElement('summary');heading.textContent=index===BOOK.manifest.parts.length-1?'부록':`${index}부 · ${part}`;group.append(heading);
    chapters.filter(c=>c.part===index+1).forEach(c=>{
      const a=document.createElement($(c.slug)?'a':'span');a.textContent=`${c.id} ${c.title}`;
      if(c.level&&c.level!=='참고'){const level=document.createElement('span');level.className='level level-'+({입문:'basic',중급:'middle',심화:'deep'}[c.level]);level.textContent=c.level;a.append(' ',level);}
      if($(c.slug)){a.href=chapterUrl(c);a.dataset.chapterLink=c.slug;}else{a.className='unwritten';a.textContent+=' · 집필 예정';}
      group.append(a);
      if(c.slug===current.slug){const sections=document.createElement('div');sections.className='sections';
        c.sections.filter(s=>!s.archive||persisted.showArchive||expandedArchives.has(c.slug)).forEach(s=>{const link=document.createElement('a');link.href=chapterUrl(c,s.id);link.textContent=s.title;sections.append(link);});group.append(sections);}
    });$('navigation').append(group);
  });
  all('[data-chapter-link]').forEach(a=>{if(a.dataset.chapterLink===current.slug)a.setAttribute('aria-current','page');});
}
function navigate(chapter,section='',replace=false){const target=chapterUrl(chapter,section);if(location.hash===target){renderRoute();return;}if(replace)location.replace(target);else location.hash=target;}
function renderRoute(){
  if(!available.length)return;
  if(!firstRoute)notice('');firstRoute=false;
  stopPlayback();
  if(current)scrollPositions.set(current.slug,{y:scrollY,section:currentSection});
  let hash=location.hash.slice(1);
  if(hash.length>40000){notice('주소의 실험 자료가 너무 길다. 기본 읽기 위치로 이동한다.');hash='';}
  const alias=BOOK.manifest.aliases?.[hash];
  if(alias){navigate(chapters.find(c=>c.id===alias), '',true);return;}
  const params=new URLSearchParams(hash);
  const requested=params.get('chapter');
  let chapter=available.find(c=>c.slug===requested);
  if(!chapter){
    if(requested)notice('주소의 장을 찾을 수 없어 첫 장으로 이동했다.');
    const savedChapter=available.find(c=>c.slug===persisted.position?.chapter);
    chapter=requested?available[0]:(savedChapter??available[0]);
  }
  let section=params.get('section')??'';
  if(section && (!$(section)||!$(chapter.slug).contains($(section)))){notice('주소의 절을 찾을 수 없어 장의 처음으로 이동했다.');section='';}
  if(section&&$(section).closest('[data-archive]')){expandedArchives.add(chapter.slug);for(let n=$(section);n;n=n.parentElement)if(n.tagName==='DETAILS')n.open=true;}
  current=chapter;currentSection=section;
  articles.forEach(a=>{a.classList.toggle('active',a.id===chapter.slug);a.setAttribute('aria-hidden',String(a.id!==chapter.slug));});
  const article=$(chapter.slug);
  all('[data-lab]',article).forEach(initLab);
  initComponents(article);
  applyArchives();
  if(params.has('lab')){
    const session=labSessions.get(params.get('lab'));
    if(!session || session.definition.chapter!==chapter.id)notice('이 장에서 사용할 수 없는 실험 주소다. 기본 실험 상태를 유지한다.');
    else if(params.has('scenario'))try{loadScenario(session,params.get('scenario'));}catch(error){notice(`실험 자료를 읽지 못했다. ${error.message}`);}
  }
  makeToc();
  const index=available.indexOf(chapter);$('previous').disabled=index===0;$('next').disabled=index===available.length-1;
  $('position').textContent=`${chapter.id} · ${chapter.title} · ${index+1}/${available.length}`;
  persisted.position={chapter:chapter.slug,section};save();
  $('sidebar').classList.remove('open');$('menu').setAttribute('aria-expanded','false');
  requestAnimationFrame(()=>{
    if(section){$(section)?.scrollIntoView({block:'start'});}
    else window.scrollTo({top:scrollPositions.get(chapter.slug)?.y??0,behavior:'instant'});
    article.querySelector('h1')?.setAttribute('tabindex','-1');
  });
}
window.addEventListener('hashchange',renderRoute);
$('previous').onclick=()=>navigate(available[Math.max(0,available.indexOf(current)-1)]);
$('next').onclick=()=>navigate(available[Math.min(available.length-1,available.indexOf(current)+1)]);
$('resume').onclick=()=>{const c=available.find(c=>c.slug===persisted.position?.chapter)??available[0];navigate(c,persisted.position?.section??'');};
$('menu').onclick=()=>{const open=$('sidebar').classList.toggle('open');$('menu').setAttribute('aria-expanded',String(open));};
function applyArchives(){
  for(const chapter of available){
    const article=$(chapter.slug);const visible=persisted.showArchive||expandedArchives.has(chapter.slug);
    for(const node of all('[data-archive]',article))node.hidden=!visible;
    let button=article.querySelector('.archive-toggle');
    if(chapter.archiveCount&&!button){button=makeButton('',()=>{if(expandedArchives.has(chapter.slug))expandedArchives.delete(chapter.slug);else expandedArchives.add(chapter.slug);applyArchives();makeToc();},'archive-toggle');article.append(button);}
    if(button){button.hidden=persisted.showArchive;button.textContent=visible?'이 장의 보관 자료 숨기기':`이전 문서 보관 자료 ${chapter.archiveCount}건 보기`;button.setAttribute('aria-expanded',String(visible));}
  }
  $('archiveToggle').textContent=persisted.showArchive?'보관 자료 숨김':'보관 자료 표시';$('archiveToggle').setAttribute('aria-pressed',String(persisted.showArchive));
}
$('archiveToggle').onclick=()=>{persisted.showArchive=!persisted.showArchive;expandedArchives.clear();save();applyArchives();makeToc();search();};
document.addEventListener('click',event=>{
  const source=event.target.closest('[data-evidence]');if(source){event.preventDefault();openEvidence(source.dataset.evidence);return;}
  const direct=event.target.closest('[data-file]');if(direct){event.preventDefault();openSource(direct.dataset.file,direct.dataset.find??'');}
});
const searchIndex=[];
let architectureMode='echo';
let architectureComponent='application';
function renderArchitecture(){
  const panel=$('architectureFocus');if(!panel)return;
  const content={application:architectureMode==='echo'?['EchoHandler가 I/O 콜백에서 Decode와 Send를 실행한다. span은 콜백 동안 유효하며 송신은 내부 복사다. 소유 객체는 애플리케이션이 유지한다.','echo.on_data']:['QueuedEventAdapter가 raw chunk를 복사하고 게임 shard가 TryPop한다. Player와 룸 상태는 로직 워커가 소유하고 Send로 응답한다.','architecture.chat_path'],
    server:['Server::Impl은 공개 상태·설정·핸들러 브리지와 워커별 지표를 소유한다. 애플리케이션의 핸들러·로거는 빌려 쓰므로 더 오래 살아야 한다.','architecture.bridge'],
    transport:['내부 transport는 세션 풀·링·accept·I/O 워커를 조립한다. 완료를 소유 워커에서 검사하고 수신 콜백으로 전달한다.','worker.dispatch'],
    rio:['운영체제 RIO 함수 표가 RQ/CQ와 등록 버퍼를 다룬다. 성공한 게시의 완료가 돌아올 때까지 참조 자원을 유지한다.','rio.api']}[architectureComponent];
  panel.textContent=content[0];$('architectureSource').dataset.evidence=content[1];
  for(const button of all('[data-component]'))button.setAttribute('aria-pressed',String(button.dataset.component===architectureComponent));
  for(const button of all('[data-architecture-mode]'))button.setAttribute('aria-pressed',String(button.dataset.architectureMode===architectureMode));
}
for(const button of all('[data-component]'))button.onclick=()=>{architectureComponent=button.dataset.component;renderArchitecture();};
for(const button of all('[data-architecture-mode]'))button.onclick=()=>{architectureMode=button.dataset.architectureMode;renderArchitecture();};
renderArchitecture();
for(const chapter of available){
  const article=$(chapter.slug);
  for(const node of all('h1,h2,h3,p,td,li',article)){
    if(node.closest('.lab'))continue;
    const text=node.textContent.trim();if(text.length<3)continue;
    const section=chapter.sections.find(s=>$(s.id)?.contains(node) || $(s.id)===node || node.closest('section')?.id===s.id);
    const archive=!!node.closest('[data-archive]');
    searchIndex.push({type:archive?'보관':'본문',archive,title:chapter.title,text,chapter,section:section?.id??'',node});
  }
}
for(const term of BOOK.glossary){
  const chapter=available.find(c=>term.chapters.includes(c.id))??available[0];
  searchIndex.push({type:'용어',title:term.term,text:term.aliases.join(' ')+' '+term.definition,chapter,section:'',node:$(chapter.slug).querySelector('h1')});
}
for(const symbol of BOOK.api?.symbols??[]){
  const chapter=available.find(c=>c.id==='A01')??available.find(c=>c.id==='C03')??available[0];
  searchIndex.push({type:'API',title:symbol.symbol,text:symbol.description+' '+symbol.header,chapter,section:'',source:'include/rionetlite/'+symbol.header,find:symbol.symbol});
}
function search(){
  const query=$('search').value.trim().toLocaleLowerCase();const results=$('searchResults');results.replaceChildren();results.hidden=!query;if(!query)return;
  const terms=new Set([query]);
  for(const term of BOOK.glossary)if([term.term,...term.aliases].some(a=>a.toLocaleLowerCase()===query))for(const alias of [term.term,...term.aliases])terms.add(alias.toLocaleLowerCase());
  const matches=searchIndex.filter(item=>(!item.archive||persisted.showArchive)&&[...terms].some(t=>(item.title+' '+item.text).toLocaleLowerCase().includes(t)));
  const count=document.createElement('p');count.className='small';count.textContent=`검색 결과 ${matches.length}개${matches.length>60?' · 앞의 60개 표시':''}`;results.append(count);
  for(const item of matches.slice(0,60)){
    const button=makeButton('',()=>{
      if(lastHighlight)lastHighlight.classList.remove('search-hit');
      navigate(item.chapter,item.section);
      requestAnimationFrame(()=>{if(item.source)openSource(item.source,item.find);else if(item.node){for(let parent=item.node.parentElement;parent;parent=parent.parentElement)if(parent.tagName==='DETAILS')parent.open=true;item.node.classList.add('search-hit');lastHighlight=item.node;item.node.tabIndex=-1;item.node.focus({preventScroll:true});item.node.scrollIntoView({block:'center'});} });
      results.hidden=true;$('search').value='';
    });
    const title=document.createElement('strong');title.textContent=`${item.type} · ${item.title}`;
    const snippet=document.createElement('small');const offset=Math.max(0,item.text.toLocaleLowerCase().indexOf(query)-30);snippet.textContent=item.text.slice(offset,offset+135);
    const place=document.createElement('small');place.textContent=`${item.chapter.id}${item.section?' / '+item.section:''}`;
    button.append(title,snippet,place);results.append(button);
  }
}
$('search').addEventListener('input',search);
document.addEventListener('keydown',event=>{
  if(event.key==='/' && !/INPUT|TEXTAREA|SELECT/.test(document.activeElement.tagName)){event.preventDefault();$('search').focus();}
  if(event.key==='Escape'&&!$('sourceDialog').open&&!$('scenarioDialog').open){$('searchResults').hidden=true;$('sidebar').classList.remove('open');$('menu').setAttribute('aria-expanded','false');}
});
for(const input of all('[data-check]')){input.checked=persisted.checks[input.dataset.check]===true;input.onchange=()=>{persisted.checks[input.dataset.check]=input.checked;save();};}
function openEvidence(id,append=false){const evidence=BOOK.evidence.find(e=>e.id===id);if(!evidence){notice('코드 출처를 찾지 못했다. 책을 재생성해 확인해야 한다.');return;}openSource(evidence.source,evidence.selector.start,append,evidence);}
function openSource(file,find='',append=false,evidence=null){
  stopPlayback();const source=SOURCES[file];if(!source){notice('포함된 소스에서 파일을 찾을 수 없다.');return;}
  if(!$('sourceDialog').open){sourceOpener=document.activeElement;sourceFrames=[];}
  else if(append&&sourceFrame)sourceFrames.push(sourceFrame);
  const lines=source.text.split('\n');let line=lines.findIndex(l=>l.includes(find));if(!find||line<0)line=0;
  const selected=evidence??BOOK.evidence.find(e=>e.source===file&&e.startLine<=line+1&&e.endLine>=line+1);
  sourceFrame={file,find,line,evidence:selected,scope:'snippet'};$('sourceScope').value='snippet';
  $('sourceScope').querySelector('[value="function"]').disabled=!selected;
  renderSource();if(!$('sourceDialog').open)$('sourceDialog').showModal();
}
function renderSource(){
  const frame=sourceFrame;const source=SOURCES[frame.file];const lines=source.text.split('\n');const scope=$('sourceScope').value;
  frame.scope=scope;
  const start=scope==='file'?0:scope==='function'?frame.evidence.startLine-1:Math.max(0,frame.line-5);
  const end=scope==='file'?lines.length:scope==='function'?frame.evidence.endLine:Math.min(lines.length,frame.line+18);
  $('sourceTitle').textContent=frame.file;
  $('sourceMeta').textContent=`실제 원문 L${start+1}–L${end} · 검토 기준 ${source.commit.slice(0,12)} · SHA-256 ${source.sha256}`;
  $('sourceRelation').textContent=frame.evidence?`${frame.evidence.kind==='contract'?'공개 계약':'구현'} · ${frame.evidence.testName} · ${frame.evidence.testScope??'연결된 회귀 검사이며 이 창에서 실행하지 않는다.'}`:'자동 선택 구간 · 함수 경계는 등록되지 않았다.';
  $('sourceCode').innerHTML=lines.slice(start,end).map((line,i)=>`<span${i+start===frame.line?' class="focusline"':''}><em>${i+start+1}</em>${escapeHtml(line)}</span>`).join('');
  $('sourceCode').scrollTop=0;$('sourceBack').disabled=sourceFrames.length===0;
  $('sourceRelated').replaceChildren();
  BOOK.evidence.filter(e=>e.chapter===current.id&&e.id!==frame.evidence?.id).slice(0,8).forEach(e=>$('sourceRelated').append(makeButton(e.label??e.id,()=>openEvidence(e.id,true))));
}
$('sourceScope').onchange=renderSource;
$('sourceBack').onclick=()=>{sourceFrame=sourceFrames.pop();$('sourceScope').value=sourceFrame.scope??'snippet';$('sourceScope').querySelector('[value="function"]').disabled=!sourceFrame.evidence;renderSource();};
$('sourceClose').onclick=()=>$('sourceDialog').close();
$('sourceDialog').addEventListener('close',()=>sourceOpener?.focus());
let figureOpener=null;
$('figureClose').onclick=()=>$('figureDialog').close();
$('figureDialog').addEventListener('close',()=>figureOpener?.focus());
let termOpener=null;
function closeTerm(){if($('termPopover').hidden)return;$('termPopover').hidden=true;termOpener?.focus();}
document.addEventListener('keydown',event=>{if(event.key==='Escape')closeTerm();});
function getModel(definition){return BookModels[definition.modelKey];}
function labAction(session,action){
  try{
    if(session.actions.length>=128)throw new Error('이 실험의 동작은 128개까지 기록한다. 초기화하거나 이전 단계로 돌아간다.');
    const result=session.model.transition(session.state,action);
    session.actions=session.actions.slice(0,session.cursor);session.actions.push(action);session.cursor++;
    session.state=result.state;session.last=result;renderLab(session);
  }catch(error){session.output.textContent=error.message;session.output.classList.add('bad');}
}
function restoreLab(session,cursor){session.cursor=cursor;session.state=BookModels.core.replay(session.model,session.config,session.actions.slice(0,cursor));session.last=null;renderLab(session);}
function resetLab(session,config=session.config){const state=session.model.createInitialState(config);session.config=config;session.state=state;session.actions=[];session.cursor=0;session.last=null;renderLab(session);}
function initialView(model,state){return model.view?model.view(state):{values:Object.entries(state).filter(([,v])=>!v||typeof v!=='object').map(([label,value])=>({label,value})),message:'다음 동작을 선택해 상태 변화를 관찰한다.'};}
function visualState(host,session,state){
  const view=initialView(session.model,state);const diagram=document.createElement('div');diagram.className='diagram-scroll';
  const nodes=view.nodes??view.values.slice(0,5).map(item=>({label:item.label,value:item.value}));
  const width=Math.max(620,nodes.length*155);const row=nodes.map((node,i)=>{
    const x=12+i*(width/nodes.length);const w=width/nodes.length-22;
    return `<g class="node ${node.active?'active':''}" tabindex="0" role="button" aria-label="${escapeHtml(node.label)}" data-node="${i}"><rect x="${x}" y="30" width="${w}" height="70" rx="4"/><text x="${x+w/2}" y="57" text-anchor="middle">${escapeHtml(node.label)}</text><text x="${x+w/2}" y="83" text-anchor="middle">${escapeHtml(String(node.value??''))}</text></g>${i<nodes.length-1?`<path d="M${x+w} 65 h18 l-5 -4 m5 4 l-5 4"/>`:''}`;
  }).join('');
  const active=Math.max(0,nodes.findIndex(n=>n.active));
  diagram.innerHTML=`<svg viewBox="0 0 ${width} 125" role="group" aria-label="실험 상태 그림">${row}<circle cx="${12+active*(width/nodes.length)+(width/nodes.length-22)/2}" cy="18" r="5"/></svg>`;
  all('[data-node]',diagram).forEach(node=>{
    const select=()=>{const i=Number(node.dataset.node);if(session.definition.stepSequence&&i<session.definition.stepSequence.length){session.actions=session.definition.stepSequence.slice(0,i+1);restoreLab(session,i+1);}else{session.output.textContent=`${nodes[i].label}: ${nodes[i].value??''}. ${view.message}`;}};
    node.onclick=select;node.onkeydown=event=>{if(event.key==='Enter'||event.key===' '){event.preventDefault();select();}};
  });
  host.append(diagram);
  if(session.definition.id==='L09'){
    const bytes=document.createElement('div');bytes.className='bytes';bytes.innerHTML=state.bytes.map((b,i)=>`<span class="byte ${i<4?'header':''} ${i<state.arrived?'arrived':''} ${i<state.consumed?'done':''}">${b.toString(16).padStart(2,'0').toUpperCase()}<small>${i}</small></span>`).join('');host.append(bytes);
  }
  if(session.definition.id==='L11'){
    const memory=document.createElement('div');memory.className='memory';memory.innerHTML=Array.from({length:state.messageBytes},(_,i)=>`<span class="${i<state.offset?'done':'pending'}">${i}</span>`).join('');host.append(memory);
  }
  const details=document.createElement('div');details.className='lab-state';const dl=document.createElement('dl');
  for(const item of view.values){const pair=document.createElement('div');const dt=document.createElement('dt');dt.textContent=item.label;const dd=document.createElement('dd');dd.textContent=String(item.value??'—');pair.append(dt,dd);dl.append(pair);}details.append(dl);host.append(details);
  return view;
}
function renderLab(session){
  session.visual.replaceChildren();const view=visualState(session.visual,session,session.state);
  session.output.textContent=session.last?.message??view.message;
  if(!session.model.view&&session.last)session.output.textContent=`${session.last.explanationId} · ${JSON.stringify(session.state)}`;
  session.output.classList.toggle('bad',!!(session.state.error||session.last?.blocked));
  session.backButton.disabled=session.cursor===0;session.forwardButton.disabled=session.cursor===session.actions.length;
  session.counter.textContent=`기록 ${session.cursor}/${session.actions.length} · 설명용 단계이며 실제 지연 시간이 아니다.`;
  session.sourceButtons.replaceChildren();
  const evidenceIds=session.definition.evidenceIds??[];
  const selected=session.definition.evidenceByStep?.[session.state.step??0];
  for(const id of selected?[selected]:evidenceIds)session.sourceButtons.append(makeButton('실제 코드 · '+(BOOK.evidence.find(e=>e.id===id)?.label??id),()=>openEvidence(id),'source'));
}
function initLab(host){
  const id=host.dataset.lab;if(labSessions.has(id))return;
  const definition=BOOK.manifest.labs.find(l=>l.id===id);const model=definition&&getModel(definition);
  if(!model){host.textContent='이 실험은 집필 중이다.';return;}
  const config=structuredClone({...model.defaults,...definition.defaultConfig});let state;
  try{state=model.createInitialState(config);}catch(error){host.textContent=error.message;return;}
  const session={definition,model,config,state,actions:[],cursor:0,last:null,timer:null,host};labSessions.set(id,session);
  host.classList.add('lab');host.replaceChildren();
  const head=document.createElement('div');head.className='lab-head';const title=document.createElement('h3');title.textContent=`${id} · ${definition.title}`;const tag=document.createElement('span');tag.className='tag';tag.textContent='설명용 모델';head.append(title,tag);host.append(head);
  const purpose=document.createElement('p');purpose.className='lab-description';purpose.textContent=definition.purpose;host.append(purpose);
  const configDetails=document.createElement('details');const summary=document.createElement('summary');summary.textContent='초기 조건 바꾸기';configDetails.append(summary);
  const fields=document.createElement('div');fields.className='lab-config';const inputs={};
  for(const field of definition.configFields??[]){
    const label=document.createElement('label');label.textContent=field.label;
    const input=document.createElement(field.options?'select':'input');input.id=`${id}-config-${field.key}`;
    if(field.options){for(const [value,title] of field.options){const o=document.createElement('option');o.value=value;o.textContent=title;input.append(o);}}
    else{input.type=field.type??'number';if(input.type==='text')input.maxLength=512;if(field.min!==undefined)input.min=field.min;if(field.max!==undefined)input.max=field.max;}
    input.value=String(config[field.key]??field.default??'');label.append(input);fields.append(label);inputs[field.key]={input,field};
  }
  configDetails.append(fields,makeButton('이 조건으로 처음부터',()=>{try{const next={...session.config};for(const [key,{input,field}]of Object.entries(inputs))next[key]=(field.type==='text'||field.options)?input.value:Number(input.value);stopPlayback();resetLab(session,next);}catch(error){session.output.textContent=error.message;session.output.classList.add('bad');}}));host.append(configDetails);
  session.visual=document.createElement('div');host.append(session.visual);
  const controls=document.createElement('div');controls.className='controls';
  for(const command of definition.actions??[]){
    const params={};
    for(const field of command.fields??[]){const label=document.createElement('label');label.className='action-field';label.textContent=field.label;const input=document.createElement('input');input.type=field.type??'number';if(input.type==='text')input.maxLength=512;input.value=String(field.default);if(field.min!==undefined)input.min=field.min;if(field.max!==undefined)input.max=field.max;label.append(input);controls.append(label);params[field.key]={input,field};}
    const button=makeButton(command.label,()=>{stopPlayback();const action=structuredClone(command.action);for(const [key,{input,field}]of Object.entries(params))action[key]=field.type==='text'?input.value:Number(input.value);labAction(session,action);},command.primary?'primary':'');button.dataset.action=command.action.type;controls.append(button);
  }
  session.configInputs=inputs;
  host.append(controls);const playback=document.createElement('div');playback.className='controls';
  session.backButton=makeButton('이전 단계',()=>{stopPlayback();restoreLab(session,Math.max(0,session.cursor-1));});
  session.forwardButton=makeButton('다시 다음 단계',()=>{stopPlayback();restoreLab(session,Math.min(session.actions.length,session.cursor+1));});
  session.playButton=makeButton('단계별 재생',()=>{
    if(session.timer){stopPlayback();return;}
    const sequence=definition.stepSequence??definition.defaultActions??[];if(!sequence.length)return;
    resetLab(session);session.playButton.textContent='재생 멈추기';
    session.timer=setInterval(()=>{if(session.cursor>=sequence.length){stopPlayback();return;}labAction(session,sequence[session.cursor]);if(session.cursor>=sequence.length)stopPlayback();},matchMedia('(prefers-reduced-motion: reduce)').matches?1400:900);
    playbackSessions.add(session);
  });
  playback.append(session.backButton,session.forwardButton,session.playButton,makeButton('이 실험 초기화',()=>{stopPlayback();resetLab(session);}),makeButton('실험 JSON 내보내기 / 가져오기',()=>openScenario(session)));
  if(!(definition.stepSequence??definition.defaultActions)?.length)session.playButton.disabled=true;
  host.append(playback);session.output=document.createElement('p');session.output.className='lab-output';session.output.setAttribute('role','status');host.append(session.output);
  session.counter=document.createElement('p');session.counter.className='small';host.append(session.counter);session.sourceButtons=document.createElement('div');host.append(session.sourceButtons);
  const omissions=document.createElement('p');omissions.className='small';omissions.textContent='모델에서 생략한 동작: '+definition.omissions;host.append(omissions);renderLab(session);
}
function openScenario(session){stopPlayback();scenarioLab=session;sourceOpener=document.activeElement;$('scenarioTitle').textContent=`${session.definition.id} 실험 자료`;$('scenarioText').value=session.model.serializeScenario(session.config,session.actions.slice(0,session.cursor));$('scenarioStatus').textContent='최대 동작 수는 128개다. 가져온 HTML이나 코드는 실행하지 않는다.';$('scenarioDialog').showModal();}
function loadScenario(session,text){const value=session.model.parseScenario(text);session.config={...session.model.defaults,...value.config};session.actions=value.actions;session.cursor=value.actions.length;session.state=value.state;session.last=null;for(const [key,{input,field}]of Object.entries(session.configInputs??{}))input.value=String(session.config[key]??session.definition.defaultConfig?.[key]??field.default??'');renderLab(session);}
$('scenarioClose').onclick=()=>$('scenarioDialog').close();$('scenarioDialog').addEventListener('close',()=>sourceOpener?.focus());
$('scenarioImport').onclick=()=>{try{loadScenario(scenarioLab,$('scenarioText').value);$('scenarioStatus').textContent='자료를 검사하고 같은 초기값부터 재실행했다.';}catch(error){$('scenarioStatus').textContent='가져오지 못했다. '+error.message;}};
$('scenarioCopy').onclick=async()=>{try{await navigator.clipboard.writeText($('scenarioText').value);$('scenarioStatus').textContent='JSON을 복사했다.';}catch{$('scenarioText').focus();$('scenarioText').select();$('scenarioStatus').textContent='자동 복사를 사용할 수 없다. 선택된 JSON을 Ctrl+C로 복사한다.';}};
function preparePrint(){
  stopPlayback();const output=$('printBook');output.replaceChildren();
  for(const chapter of available){const clone=$(chapter.slug).cloneNode(true);clone.className='print-chapter';clone.removeAttribute('aria-hidden');
    for(const node of all('[data-archive]',clone)){if(!persisted.showArchive)node.remove();else node.hidden=false;}
    for(const node of all('.archive-toggle',clone))node.remove();
    printComponents(clone);
    for(const host of all('[data-lab]',clone)){
      const definition=BOOK.manifest.labs.find(l=>l.id===host.dataset.lab);const model=definition&&getModel(definition);host.replaceChildren();if(!model)continue;
      const h=document.createElement('h3');h.textContent=`${definition.id} · ${definition.title}`;const p=document.createElement('p');p.textContent='인쇄 대표 상태: '+definition.printSummary;host.append(h,p);visualState(host,{definition,model,output:document.createElement('p')},model.createInitialState(definition.defaultConfig??{}));
      const limits=document.createElement('p');limits.className='small';limits.textContent='생략한 동작: '+definition.omissions;host.append(limits);
    }
    for(const details of all('details',clone))details.open=true;
    for(const button of all('[data-evidence],[data-file]',clone)){const text=document.createElement('p');text.className='source-ref';const e=BOOK.evidence.find(e=>e.id===button.dataset.evidence);text.textContent=e?`실제 코드: ${e.source} · ${e.testName}`:`실제 코드: ${button.dataset.file}`;button.replaceWith(text);}
    const idMap=new Map([clone,...all('[id]',clone)].filter(n=>n.id).map(n=>[n.id,'print-'+n.id]));
    for(const node of [clone,...all('*',clone)]){
      if(node.id)node.id=idMap.get(node.id);
      for(const attribute of [...node.attributes]){
        if(attribute.name==='aria-labelledby')node.setAttribute(attribute.name,attribute.value.split(' ').map(id=>idMap.get(id)??id).join(' '));
        else if(attribute.value.includes('url(#'))node.setAttribute(attribute.name,attribute.value.replace(/url\(#([^)]+)\)/g,(_,id)=>`url(#${idMap.get(id)??id})`));
      }
    }
    for(const node of all('[tabindex]',clone))node.removeAttribute('tabindex');output.append(clone);
  }
}
window.addEventListener('beforeprint',preparePrint);
$('print').onclick=()=>{preparePrint();window.print();};
document.addEventListener('visibilitychange',()=>{if(document.hidden)stopPlayback();});
renderRoute();
document.documentElement.dataset.readerReady='true';
