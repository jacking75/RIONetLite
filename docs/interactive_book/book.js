'use strict';
const $ = id => document.getElementById(id);
const all = selector => [...document.querySelectorAll(selector)];
const escapeHtml = value => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const saved = {get(key, fallback) { try { return JSON.parse(localStorage.getItem('rio-book-' + key)) ?? fallback; } catch { return fallback; } }, set(key,value) { try { localStorage.setItem('rio-book-' + key, JSON.stringify(value)); } catch { /* file origins may disable storage */ } }};
const pages = all('.page');
let currentPage = 0;
let traceTimer = null;
const toc = document.querySelector('.toc');
toc.innerHTML = pages.map((page,i) => `<a href="#${page.id}"><span class="num">${String(i+1).padStart(2,'0')}</span><span>${page.dataset.title}</span></a>`).join('');
function stopTrace() { clearInterval(traceTimer); traceTimer = null; $('tracePlay').textContent = '자동 재생'; }
function showPage(index) {
  currentPage = Math.max(0, Math.min(pages.length-1,index));
  stopTrace();
  pages.forEach((page,i) => page.classList.toggle('active',i===currentPage));
  [...toc.children].forEach((link,i) => {link.classList.toggle('active',i===currentPage); if(i===currentPage) link.setAttribute('aria-current','page'); else link.removeAttribute('aria-current');});
  $('prevPage').disabled = currentPage === 0;
  $('nextPage').disabled = currentPage === pages.length-1;
  $('pageLabel').textContent = `${String(currentPage+1).padStart(2,'0')} / 10 · ${pages[currentPage].dataset.title}`;
  $('progress').style.width = `${(currentPage+1)/pages.length*100}%`;
  $('readingProgress').textContent = `현재 읽는 위치 ${currentPage+1} / 10`;
  saved.set('page',currentPage);
  window.scrollTo({top:0,behavior:'instant'});
}
function go(index) { location.hash = pages[Math.max(0,Math.min(9,index))].id; }
function readHash() { const index = pages.findIndex(p=> '#'+p.id===location.hash); if(index>=0) showPage(index); }
window.addEventListener('hashchange',readHash);
$('prevPage').onclick = () => go(currentPage-1);
$('nextPage').onclick = () => go(currentPage+1);
$('search').oninput = event => { const term = event.target.value.toLowerCase().trim(); [...toc.children].forEach((link,i)=> {link.hidden=!pages[i].textContent.toLowerCase().includes(term);}); };
function theme(dark) {document.documentElement.dataset.theme=dark?'dark':'light'; $('theme').textContent=dark?'밝은 화면':'어두운 화면'; saved.set('dark',dark);}
theme(saved.get('dark',false));
$('theme').onclick=()=>theme(document.documentElement.dataset.theme!=='dark');
$('print').onclick=()=>{stopTrace();window.print();};
let printDetails=[];
window.addEventListener('beforeprint',()=>{printDetails=all('details').filter(d=>!d.open);printDetails.forEach(d=>d.open=true);});
window.addEventListener('afterprint',()=>printDetails.forEach(d=>d.open=false));
function status(id,text,bad=false) {$(id).textContent=text;$(id).classList.toggle('bad',bad);}

function openSource(file,find='') {
  stopTrace();
  const source=SOURCES[file];
  if(!source) return;
  $('sourceTitle').textContent=file;
  const lines=source.text.split('\n');
  const target=find?lines.findIndex(line=>line.includes(find)):-1;
  $('sourceMeta').textContent=`생성 시점 코드 스냅샷 · ${target>=0?'L'+(target+1)+' · ':''}SHA-256 ${source.sha256.slice(0,16)}…`;
  $('sourceMeta').title=source.sha256;
  $('sourceCode').innerHTML=lines.map((line,i)=>`<span${i===target?' class="focusline"':''}><em>${i+1}</em>${escapeHtml(line)}</span>`).join('');
  $('sourceDialog').showModal();
  requestAnimationFrame(()=>{ $('sourceCode').scrollTop=Math.max(0,(target-3)*23.4); });
}
all('[data-file]').forEach(button=>button.onclick=()=>openSource(button.dataset.file,button.dataset.find));
$('sourceClose').onclick=()=>$('sourceDialog').close();
$('sourceDialog').addEventListener('click',event=>{if(event.target===$('sourceDialog')){const r=event.target.getBoundingClientRect();if(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom)event.target.close();}});

const traceSteps=[
  {title:'01 · 수신할 구간을 게시한다',desc:'등록된 수신 버퍼의 빈 구간으로 요청을 게시한다. 게시 성공은 수신 완료를 의미하지 않는다. 요청과 컨텍스트의 수명을 유지한다.',state:'수신 요청: 진행 중 · 데이터: 아직 없음',code:'PostRecv()\n  → 빈 구간으로 수신 요청\n  → 완료를 기다린다',file:'RIONetLite/impl/server_net_io_worker.h',find:'bool PostRecv(',x:125,y:14},
  {title:'02 · 완료의 소유자를 확인한다',desc:'완료 컨텍스트의 UID와 현재 세션을 비교한다. 정상 상태와 길이를 확인한 뒤 수신 버퍼를 갱신한다. 여유가 있으면 다음 수신도 먼저 게시한다.',state:'현재 완료: 수신 7바이트 · 다음 수신: 별도 구간에 게시 가능',code:'UID 검증 → 상태 확인\nRecvCompletion(transferred)\nPostRecv() → 콜백 호출',file:'RIONetLite/impl/server_net_io_worker.h',find:'auto cq_session_uid',x:418,y:14},
  {title:'03 · 완전한 패킷을 찾는다',desc:'EchoHandler::OnData가 LengthPrefixedDecoder를 호출한다. 전체 길이 7의 헤더와 payload가 모두 있으므로 패킷 핸들러가 호출된다.',state:'콜백 데이터: 7바이트 · 완전한 패킷: 1개',code:'OnData(session, data)\n  → decoder_.Decode(data, handler)\n  → handler(packet)',file:'samples/echo_server/main.cpp',find:'std::size_t OnData',x:713,y:14},
  {title:'04 · 응답을 송신 경로에 넘긴다',desc:'EchoHandler는 받은 패킷 전체를 Server::Send에 전달한다. 데이터는 내부에 복사된다. kQueued 결과는 상대방의 처리 완료가 아니다.',state:'애플리케이션 span: 콜백 동안 유효 · 송신용 복사본: 내부 소유',code:'server_->Send(session, packet)\n  → 세션과 용량 검사\n  → 내부 송신 링에 복사',file:'samples/echo_server/main.cpp',find:'server_->Send(session, packet)',x:713,y:164},
  {title:'05 · 등록 버퍼로 RIO 송신을 게시한다',desc:'진행 중인 송신이 없다면 요청을 준비하고 RIOSend를 호출한다. 이미 요청이 있다면 새 데이터는 링에 남아 다음 게시를 기다린다.',state:'송신 미완료: 1 · 등록 버퍼: 유지 필요',code:'PreparePostLocked()\n  → outstanding 증가\nIssuePost() → RIOSend',file:'RIONetLite/impl/send_immediately.h',find:'bool PreparePostLocked',x:418,y:164},
  {title:'06 · 완료된 바이트만 소비한다',desc:'이 예에서는 7바이트 모두 완료됐다. 완료 요청을 정리하고 읽기 위치를 이동한다. 대기 데이터가 있으면 이어 게시한다. 부분 완료는 다음 실험에서 다룬다.',state:'이번 응답 송신: 완료 · 세션: 계속 활성 · 수신 요청은 별도',code:'Completed(context, transferred)\n  → 요청 retire\n  → 읽기 위치 += transferred',file:'RIONetLite/impl/send_immediately.h',find:'bool Completed',x:125,y:164}
];
let traceIndex=0;
function renderTrace(){const s=traceSteps[traceIndex];$('traceLabel').textContent=s.title;$('traceDescription').textContent=s.desc;$('traceState').textContent=s.state;$('traceCode').textContent=s.code;all('#traceSvg .node').forEach((node,i)=>{node.classList.toggle('active',i===traceIndex);node.setAttribute('aria-pressed',i===traceIndex?'true':'false');});$('traceToken').setAttribute('cx',s.x);$('traceToken').setAttribute('cy',s.y);$('traceBack').disabled=traceIndex===0;$('traceNext').disabled=traceIndex===5;}
$('traceBack').onclick=()=>{stopTrace();traceIndex=Math.max(0,traceIndex-1);renderTrace();};
$('traceNext').onclick=()=>{stopTrace();traceIndex=Math.min(5,traceIndex+1);renderTrace();};
$('traceReset').onclick=()=>{stopTrace();traceIndex=0;renderTrace();};
function playTrace(){if(traceIndex===5)traceIndex=0;renderTrace();$('tracePlay').textContent='일시 정지';traceTimer=setInterval(()=>{traceIndex++;renderTrace();if(traceIndex===5)stopTrace();},Number($('traceSpeed').value));}
$('tracePlay').onclick=()=>{if(traceTimer)stopTrace();else playTrace();};
$('traceSpeed').onchange=()=>{if(traceTimer){stopTrace();playTrace();}};
all('#traceSvg .node').forEach(node=>{const select=()=>{stopTrace();traceIndex=Number(node.dataset.step);renderTrace();};node.onclick=select;node.onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();select();}};});
$('traceSource').onclick=()=>openSource(traceSteps[traceIndex].file,traceSteps[traceIndex].find);

const packetScenarios={split:{bytes:[0,0,0,7,114,105,111],chunks:[2,3,2]},coalesced:{bytes:[0,0,0,7,114,105,111,0,0,0,5,33],chunks:[12]},invalid:{bytes:[0,0,0,3],chunks:[4]},large:{bytes:[0,0,0,17],chunks:[4]}};
let packet={arrived:0,consumed:0,count:0,chunk:0,error:false};
function renderPacket(){const mode=$('packetMode').value;const s=packetScenarios[mode];$('packetBytes').innerHTML=s.bytes.map((b,i)=>`<span class="byte ${i<4||(mode==='coalesced'&&i>=7&&i<11)?'header':''} ${i<packet.arrived?'arrived':''} ${i<packet.consumed?'done':''}">${b.toString(16).padStart(2,'0').toUpperCase()}<small>${i}</small></span>`).join('');$('packetArrived').textContent=packet.arrived;$('packetConsumed').textContent=packet.consumed;$('packetCount').textContent=packet.count;$('packetNext').disabled=packet.chunk>=s.chunks.length||packet.error;}
function resetPacket(){packet={arrived:0,consumed:0,count:0,chunk:0,error:false};status('packetStatus','아직 바이트가 도착하지 않았다. 다음 수신을 전달한다.');renderPacket();}
$('packetNext').onclick=()=>{const s=packetScenarios[$('packetMode').value];if(packet.chunk>=s.chunks.length||packet.error)return;packet.arrived+=s.chunks[packet.chunk++];const before=packet.consumed;while(packet.arrived-packet.consumed>=4){const i=packet.consumed;const len=s.bytes[i]*16777216+s.bytes[i+1]*65536+s.bytes[i+2]*256+s.bytes[i+3];if(len<4||len>16){packet.error=true;status('packetStatus',`${len<4?'kInvalidLength':'kMessageTooLarge'}: 헤더 길이 ${len}. EchoHandler는 프로토콜 오류로 종료를 요청한다.`,true);break;}if(len>packet.arrived-i)break;packet.consumed+=len;packet.count++;}if(!packet.error)status('packetStatus',packet.consumed>before?`이번 Decode에서 ${packet.consumed-before}바이트를 소비했다. 누적 ${packet.count}개 패킷이 완성됐다.`:`이번 소비량은 0이다. ${packet.arrived<4?'헤더 4바이트가 아직 모이지 않았다.':'헤더는 있지만 패킷 전체 7바이트가 아직 모이지 않았다.'}`);renderPacket();};
$('packetMode').onchange=resetPacket;$('packetReset').onclick=resetPacket;

let sendOffset=0,sendFreed=false;
function renderSend(){const remaining=4-sendOffset;$('sendMemory').innerHTML=[...'rio!'].map((c,i)=>`<span class="${i<sendOffset?'done':'pending'}">${c}</span>`).join('');$('sendOffset').textContent=sendOffset;$('sendLength').textContent=remaining;$('sendOutstanding').textContent=remaining?1:0;$('sendPartial').disabled=remaining===0||sendFreed;$('sendAll').disabled=remaining===0||sendFreed;$('sendFree').disabled=sendFreed;}
function completeSend(n){if(sendFreed)return;sendOffset=Math.min(4,sendOffset+n);status('sendStatus',sendOffset<4?`${n}바이트 완료. 남은 ${4-sendOffset}바이트를 새 요청으로 게시했다. 등록 메모리를 계속 유지한다.`:'4바이트 모두 완료됐다. 이 모델의 송신 미완료 요청은 0이다.');renderSend();}
$('sendPartial').onclick=()=>completeSend(Math.min(2,4-sendOffset));$('sendAll').onclick=()=>completeSend(4-sendOffset);
$('sendFree').onclick=()=>{if(sendOffset<4){status('sendStatus','차단: 미완료 요청이 등록 메모리를 참조한다. 조기 해제는 수명 조건을 위반한다.',true);}else{sendFreed=true;status('sendStatus','이 모델에서는 해제 가능하다. 실제 서버에서는 신규 게시 차단과 다른 자원의 의존 관계도 확인해야 한다.');renderSend();}};
$('sendReset').onclick=()=>{sendOffset=0;sendFreed=false;status('sendStatus','4바이트 송신이 게시된 상태다. 완료 전에 해제도 시도해 본다.');renderSend();};

let generation=41;
function resetSession(){generation=41;$('sessionCurrent').textContent='7 : 41';$('sessionCaption').textContent='아직 이전 연결이 활성 상태다.';$('sessionDecision').textContent='—';$('sessionGuard').checked=true;$('sessionReuse').disabled=false;status('sessionStatus','먼저 재사용하고, 검증 조건을 바꾸어 결과를 비교한다.');}
$('sessionReuse').onclick=()=>{generation=42;$('sessionCurrent').textContent='7 : 42';$('sessionCaption').textContent='새 연결이 같은 슬롯을 사용한다.';$('sessionDecision').textContent='—';$('sessionReuse').disabled=true;status('sessionStatus','이전 수명 정리가 끝난 뒤 재사용했다고 가정한다. 이제 옛 작업을 전달해 본다.');};
$('sessionGuard').onchange=()=>{$('sessionDecision').textContent='—';status('sessionStatus','검증 조건이 바뀌었다. 지연 작업을 다시 전달한다.');};
$('sessionDeliver').onclick=()=>{const stale=generation!==41;const guarded=$('sessionGuard').checked;const rejected=stale&&guarded;$('sessionDecision').textContent=rejected?'거절':'통과';status('sessionStatus',rejected?'세대 불일치: 41 ≠ 42. 오래된 작업을 새 연결에 적용하지 않는다.':stale?'잘못된 통과: 인덱스 7만 같아서 이전 작업을 새 연결에 적용하게 된다.':'현재 연결과 같은 세대다. 이 식별 검사는 통과한다. 실제 작업에는 연결 상태 검사도 필요하다.',stale&&!guarded);};
$('sessionReset').onclick=resetSession;
all('[data-quiz="shutdown"] button').forEach(button=>button.onclick=()=>{const ok=button.dataset.answer==='correct';status('shutdownAnswer',ok?'맞다. 완료를 drain할 주체와 자원 수명을 유지해야 한다. 현재 구현은 워커 종료의 drain 결과를 확인하고 실패 시 별도 경로로 진행한다.':'소켓 종료와 완료 처리 완료는 다르다. 완료 처리 주체를 없애고 완료만 기다리면 정리가 진행되지 않을 수 있다.',!ok);});
let queue={used:0,rejected:0,processed:0,tick:0};
function renderQueue(){$('queueSlots').innerHTML=Array.from({length:8},(_,i)=>`<i class="${i<queue.used?'filled':''}"></i>`).join('');$('queueSlots').setAttribute('aria-label',`8칸 중 ${queue.used}칸 사용`);$('queueUsed').textContent=`${queue.used} / 8`;$('queueRejected').textContent=queue.rejected;$('queueProcessed').textContent=queue.processed;}
$('queueTick').onclick=()=>{const consumed=Math.min(queue.used,Number($('queueOut').value));queue.used-=consumed;queue.processed+=consumed;const incoming=Number($('queueIn').value);const accepted=Math.min(8-queue.used,incoming);const rejected=incoming-accepted;queue.used+=accepted;queue.rejected+=rejected;queue.tick++;status('queueStatus',`${queue.tick}번째 틱: ${consumed}개 처리 → ${accepted}개 접수 → ${rejected}개 거절. ${rejected?'유입이 처리 용량을 초과해 데이터가 거절됐다.':'처리량과 대기량의 변화를 관찰한다.'}`,rejected>0);renderQueue();};
$('queueReset').onclick=()=>{queue={used:0,rejected:0,processed:0,tick:0};status('queueStatus','큐를 비웠다. 선택한 유입·처리 속도는 유지한다.');renderQueue();};
const checks=saved.get('checks',[]);
function renderChecks(){const checked=all('[data-check]').filter(c=>c.checked).length;$('checkProgress').textContent=`자기 점검 ${checked} / 5 · 읽기 진도와 별도로 기록한다.`;saved.set('checks',all('[data-check]').map(c=>c.checked));}
all('[data-check]').forEach((c,i)=>{c.checked=checks[i]===true;c.onchange=renderChecks;});
renderTrace();resetPacket();$('sendReset').click();resetSession();renderQueue();renderChecks();
const initial=pages.findIndex(p=>'#'+p.id===location.hash);
showPage(initial>=0?initial:Number(saved.get('page',0))||0);
document.addEventListener('visibilitychange',()=>{if(document.hidden)stopTrace();});
