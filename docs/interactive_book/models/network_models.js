'use strict';
(function(models){
  const {check,integer}=models.core;
  const copy=value=>structuredClone(value);
  const number=(config,key,fallback,low,high)=>{const value=config[key]??fallback;check(integer(value,low,high),`${key}는 ${low}~${high}의 정수여야 한다.`);return value;};
  const object=value=>check(value&&typeof value==='object'&&!Array.isArray(value),'설정은 객체여야 한다.');
  const values=entries=>entries.map(([label,value])=>({label,value}));
  const blocked=(state,message)=>({state,events:[],blocked:true,message,explanationId:'blocked'});
  const result=(state,message,events=[])=>({state,events,message,explanationId:'transition'});
  function define(name,id,create,step,validate,view){
    const model={createInitialState(config={}){object(config);const state=create(config);check(validate(state).length===0,'초기 모델 상태가 유효하지 않다.');return state;},
      transition(state,action){object(action);check(typeof action.type==='string','동작 종류가 필요하다.');const next=step(copy(state),copy(action));check(validate(next.state).length===0,'모델의 안전 조건을 위반했다.');return next;},validateState:validate,view,
      serializeScenario(config,actions){return models.core.serialize(model,id,config,actions);},parseScenario(text){return models.core.parse(text,model,id);}};
    models[name]=model;
  }
  define('echoRoundtrip','echo-roundtrip',c=>{check(c.nextReceive===undefined||typeof c.nextReceive==='boolean','다음 수신 게시 여부는 참 또는 거짓이다.');return {step:0,receiveOutstanding:0,sendOutstanding:0,decoded:0,queued:0,sent:0,nextReceive:c.nextReceive!==false};},
    (s,a)=>{check(a.type==='next','next 동작만 허용한다.');if(s.step===6)return blocked(s,'이 응답의 송신은 끝났다. 수신 요청은 별도로 남을 수 있다.');
      s.step++;if(s.step===1)s.receiveOutstanding=1;if(s.step===2)s.receiveOutstanding=s.nextReceive?1:0;if(s.step===3)s.decoded=1;if(s.step===4)s.queued=7;if(s.step===5)s.sendOutstanding=1;if(s.step===6){s.sendOutstanding=0;s.sent=7;}
      return result(s,['','빈 수신 구간을 게시했다. 아직 데이터는 도착하지 않았다.','세대·상태·완료 길이를 확인한 뒤 다음 수신을 게시했다. 이 설명에서는 다른 세션 실행을 생략한다.','EchoHandler::OnData가 헤더 포함 7바이트 패킷을 해석했다.','Send가 내부 링으로 복사하고 kQueued를 반환했다. 상대방의 처리 완료는 아니다.','등록된 링의 데이터로 RIO 송신 요청 1개를 게시했다.','7바이트 송신 완료를 반영했다. 이 응답과 다음 수신 요청의 수명은 별개다.'][s.step]);},
    s=>integer(s.step,0,6)&&[0,1].includes(s.receiveOutstanding)&&[0,1].includes(s.sendOutstanding)?[]:['요청 수 또는 단계 위반'],
    s=>({values:values([['단계',s.step],['미완료 수신',s.receiveOutstanding],['완성 패킷',s.decoded],['접수 바이트',s.queued],['미완료 송신',s.sendOutstanding],['완료 바이트',s.sent]]),nodes:['수신 게시','수신 완료','패킷 해석','송신 접수','송신 게시','송신 완료'].map((label,i)=>({label,value:i+1,active:s.step===i+1})),message:'한 연결의 Echo 응답을 순서대로 따라간다. 단계 간격은 실제 실행 시간이 아니다.'}));
  define('rioRequests','rio-requests',c=>{check(c.postSucceeds===undefined||typeof c.postSucceeds==='boolean','게시 성공 여부는 참 또는 거짓이다.');return {status:'idle',outstanding:0,postCalls:0,completions:0,postSucceeds:c.postSucceeds!==false};},
    (s,a)=>{check(['post','complete'].includes(a.type),'post 또는 complete만 허용한다.');
      if(a.type==='post'){if(s.outstanding)return blocked(s,'이미 요청이 진행 중이다. 같은 컨텍스트로 두 번째 요청을 만들지 않는다.');s.postCalls++;const ok=a.succeeds??s.postSucceeds;check(typeof ok==='boolean','게시 성공 여부는 참 또는 거짓이다.');if(!ok){s.status='failed';return result(s,'게시가 실패했다. 증가시켰던 outstanding을 롤백했으며 이 요청의 완료를 만들지 않는다.',[{type:'rollback'}]);}s.status='pending';s.outstanding=1;return result(s,'게시 함수가 성공했다. 아직 완료가 도착하지 않았으므로 outstanding은 1이다.');}
      if(!s.outstanding)return blocked(s,'진행 중인 요청이 없다. 실패한 게시에 가상의 완료를 만들지 않는다.');s.outstanding=0;s.completions++;s.status='completed';return result(s,'실제 완료를 받은 것으로 모델링해 요청 참조를 정리했다.');},
    s=>s.outstanding===Number(s.status==='pending')?[]:['게시 상태와 참조 수 불일치'],
    s=>({values:values([['게시 호출',s.postCalls],['미완료 요청',s.outstanding],['완료 수',s.completions],['상태',s.status]]),nodes:[{label:'게시 호출',value:s.postCalls},{label:'미완료 요청',value:s.outstanding,active:s.outstanding===1},{label:'처리한 완료',value:s.completions}],message:'성공 반환은 요청의 접수다. 완료가 지연되어도 등록 메모리는 유지한다.'}));
  define('callbackOrder','callback-order',()=>({sessions:[{connected:false,closed:false,data:0},{connected:false,closed:false,data:0}],order:[]}),
    (s,a)=>{check(['connect','data','close'].includes(a.type),'connect/data/close만 허용한다.');check(integer(a.session,0,1),'세션은 0 또는 1이다.');const p=s.sessions[a.session];
      if(a.type==='connect'){if(p.connected||p.closed)return blocked(s,'같은 연결의 OnConnected를 중복 실행하지 않는다.');p.connected=true;}
      if(a.type==='data'){if(!p.connected||p.closed)return blocked(s,'OnConnected 이전이나 OnClosed 이후의 OnData를 차단한다.');p.data++;}
      if(a.type==='close'){if(!p.connected||p.closed)return blocked(s,'연결 통지보다 먼저 종료하거나 종료를 중복 통지하지 않는다.');p.closed=true;}
      s.order.push(`${a.session===0?'A':'B'}:${a.type}`);if(s.order.length>32)s.order.shift();return result(s,'같은 세션의 순서를 지켰다. 다른 세션의 콜백은 이 사이에 실행될 수 있다.');},
    s=>s.sessions.every(p=>!p.closed||p.connected)?[]:['접속 전 종료 통지'],
    s=>({values:values([['A 연결',s.sessions[0].closed?'닫힘':s.sessions[0].connected?'활성':'미연결'],['A 데이터 횟수',s.sessions[0].data],['B 연결',s.sessions[1].closed?'닫힘':s.sessions[1].connected?'활성':'미연결'],['B 데이터 횟수',s.sessions[1].data],['호출 순서',s.order.join(' → ')||'없음']]),message:'한 동작은 콜백 하나의 시작과 반환을 묶는다. 세션 간 실제 병렬 시간은 재현하지 않는다.'}));
  define('registeredMemory','registered-memory',()=>({registered:true,send:1,receive:1,postsBlocked:false,moved:false,freed:false}),
    (s,a)=>{check(['block','complete-send','complete-receive','unregister','move','free'].includes(a.type),'알 수 없는 메모리 동작이다.');
      if(a.type==='block'){s.postsBlocked=true;return result(s,'새 요청 게시를 차단했다. 기존 요청은 완료될 때까지 남는다.');}
      if(a.type==='complete-send'||a.type==='complete-receive'){const k=a.type==='complete-send'?'send':'receive';if(!s[k])return blocked(s,'완료할 요청이 없다.');s[k]=0;return result(s,`${k} 요청 참조를 정리했다. 다른 방향의 요청도 확인한다.`);}
      if(a.type==='unregister'){if(s.send+s.receive||!s.postsBlocked)return blocked(s,'미완료 참조가 있거나 신규 게시가 열려 있어 등록 해제를 차단한다.');s.registered=false;return result(s,'신규 게시와 모든 요청 참조를 확인한 뒤 등록을 해제했다.');}
      if(s.registered||s.send+s.receive||!s.postsBlocked)return blocked(s,'등록 중인 주소 또는 미완료 참조를 옮기거나 해제할 수 없다.');
      if(a.type==='move'){if(s.freed)return blocked(s,'이미 해제한 저장소는 옮길 수 없다.');s.moved=true;return result(s,'등록 해제 뒤 저장소 주소를 바꾸는 동작만 허용했다. 새 주소를 사용하려면 다시 등록해야 한다. 주소가 유지되는 RAII 소유권 이동과는 별개다.');}
      s.freed=true;return result(s,'이 버퍼의 참조와 등록을 정리했다. CQ·RQ의 의존 관계는 종료 장에서 별도로 확인한다.');},
    s=>s.freed&&(s.registered||s.send+s.receive)?['사용 중인 버퍼 해제']:[],
    s=>({values:values([['RIO 등록',s.registered?'유지':'해제'],['미완료 송신',s.send],['미완료 수신',s.receive],['신규 게시',s.postsBlocked?'차단':'허용'],['메모리 이동',s.moved?'완료':'안 함'],['저장소',s.freed?'해제':'유지']]),message:'송신 0만으로 안전한 해제를 판정하지 않는다. 등록된 주소와 다른 요청 참조도 함께 확인한다.'}));
  define('ringBuffer','ring-buffer',c=>({capacity:number(c,'capacity',8,2,32),read:0,write:0,used:0,posted:0}),
    (s,a)=>{check(['write','consume','post','complete'].includes(a.type),'알 수 없는 링 동작이다.');check(integer(a.bytes,1,64),'바이트 수는 1~64다.');
      if(a.type==='write'){if(a.bytes>s.capacity-s.used)return blocked(s,'빈 공간이 부족하다. 부분 쓰기나 미소비 영역 덮어쓰기를 하지 않는다.');s.write=(s.write+a.bytes)%s.capacity;s.used+=a.bytes;return result(s,'쓰기 위치를 이동했다. 두 가상 view가 같은 실제 저장소를 가리킨다.');}
      if(a.type==='post'){if(s.posted||a.bytes>s.used)return blocked(s,'진행 요청이 있거나 읽을 바이트보다 길다.');s.posted=a.bytes;return result(s,'읽기 위치에서 연속 구간을 요청에 연결했다. 이 범위를 완료 전에 소비하거나 덮어쓰지 않는다.');}
      if(a.type==='consume'){if(s.posted)return blocked(s,'진행 요청이 참조하는 읽기 구간을 임의 소비할 수 없다.');if(a.bytes>s.used)return blocked(s,'미소비 바이트보다 많이 읽을 수 없다.');}
      if(a.type==='complete'){if(!s.posted||a.bytes>s.posted)return blocked(s,'게시된 구간을 넘는 완료다.');s.posted-=a.bytes;}
      s.read=(s.read+a.bytes)%s.capacity;s.used-=a.bytes;return result(s,'실제로 소비한 만큼만 읽기 위치를 이동했다. 남은 요청 구간은 계속 보호한다.');},
    s=>integer(s.used,0,s.capacity)&&s.posted<=s.used&&s.posted>=0&&(s.read+s.used)%s.capacity===s.write?[]:['링 범위 또는 위치 위반'],
    s=>({values:values([['실제 저장소 크기',s.capacity],['읽기 위치',s.read],['쓰기 위치',s.write],['미소비 바이트',s.used],['빈 공간',s.capacity-s.used],['게시 구간',s.posted],['가상 읽기 범위',`[${s.read}, ${s.read+s.used})`],['두 번째 view',`[${s.capacity}, ${s.capacity*2})`]]),nodes:[{label:'첫 가상 view',value:`0..${s.capacity-1}`},{label:'같은 실제 저장소',value:s.used+' 사용',active:s.used>0},{label:'두 번째 view',value:`${s.capacity}..${s.capacity*2-1}`}],message:s.used===0?'read와 write가 같고 used=0이므로 비어 있다.':s.used===s.capacity?'read와 write가 같아도 used=capacity이므로 가득 찼다.':'wrap은 주소 표현의 경계다. 실제 저장소의 용량이 두 배로 늘지 않는다.'}));
  define('acceptSession','accept-session',c=>{
    const count=number(c,'slots',6,1,16),workers=number(c,'workers',2,1,count);const slots=[];let index=0;
    for(let w=0;w<workers;w++){const size=Math.floor(count/workers)+(w<count%workers?1:0);for(let j=0;j<size;j++)slots.push({index:index++,owner:w,active:false,generation:'0'});}
    const order=[];for(let i=0;i<count;i++)for(let w=0;w<workers;w++){const row=slots.filter(s=>s.owner===w);if(row[i])order.push(row[i].index);}
    return {slots,free:order,workers,accepted:0,rejected:0,sockets:0};},
    (s,a)=>{check(['accept','close'].includes(a.type),'accept 또는 close만 허용한다.');
      if(a.type==='close'){check(integer(a.index,0,s.slots.length-1),'슬롯 번호 범위를 확인한다.');const slot=s.slots[a.index];if(!slot.active)return blocked(s,'활성 연결이 아니다.');slot.active=false;s.free.push(slot.index);s.sockets--;return result(s,'완료와 종료 통지까지 정리했다고 가정하고 슬롯을 한 번 반환했다.');}
      if(!s.free.length){s.rejected++;return result(s,'사용할 슬롯이 없어 연결을 거절했다. 새 세션 자원을 만들지 않는다.');}
      const index=s.free.shift();const slot=s.slots[index];slot.generation=(BigInt(slot.generation)+1n).toString();s.sockets++;
      if(a.rqSucceeds===false){s.sockets--;s.free.push(index);s.rejected++;return result(s,'RQ 생성 실패로 소켓을 닫고 슬롯을 free list 뒤에 반환했다. 실패한 연결을 활성 세션으로 남기지 않는다.');}
      slot.active=true;s.accepted++;return result(s,`슬롯 ${index}를 소유 워커 ${slot.owner}에 배정했다. 나머지 슬롯도 파티션에 포함된다.`);},
    s=>new Set(s.free).size===s.free.length&&s.slots.filter(x=>x.active).length===s.sockets&&s.free.length+s.sockets===s.slots.length?[]:['슬롯 중복 또는 누수'],
    s=>({values:values([['활성 소켓',s.sockets],['빈 슬롯',s.free.join(', ')||'없음'],['누적 수락',s.accepted],['누적 거절',s.rejected],['슬롯별 소유 워커',s.slots.map(x=>`${x.index}→${x.owner}`).join(' ')]]),nodes:s.slots.slice(0,6).map(x=>({label:`슬롯 ${x.index}`,value:`W${x.owner} ${x.active?'사용':'비어 있음'}`,active:x.active})),message:'처음 슬롯은 워커 파티션을 번갈아 꺼낸다. 완료를 처리할 워커는 세션 생성 때 결정된다.'}));
  define('sessionIdentity','session-identity',c=>{const generation=c.generation??'41';check(typeof generation==='string'&&/^[1-9][0-9]{0,19}$/.test(generation)&&BigInt(generation)<=18446744073709551615n,'세대는 uint64 양의 문자열이다.');return {index:7,generation,oldGeneration:generation,state:'Active',outstanding:1,pendingEvents:1,notified:false,decision:'미실행'};},
    (s,a)=>{check(['close','complete','consume-event','notify','quarantine','recycle','reuse','deliver'].includes(a.type),'알 수 없는 세션 동작이다.');
      if(a.type==='close'){if(s.state!=='Active')return blocked(s,'첫 종료 전이만 적용한다.');s.state='Draining';}
      if(a.type==='complete'){if(!s.outstanding)return blocked(s,'남은 I/O 참조가 없다.');s.outstanding=0;}
      if(a.type==='consume-event'){if(!s.pendingEvents)return blocked(s,'남은 이벤트가 없다.');s.pendingEvents=0;}
      if(a.type==='notify'){if(s.state!=='Draining'||s.notified)return blocked(s,'종료 통지는 닫는 연결에서 한 번만 발생한다.');s.notified=true;}
      if(a.type==='quarantine'){if(s.state!=='Draining'||!s.notified)return blocked(s,'닫힘 통지 뒤 Closing을 거쳐 격리한다.');s.state='Quarantined';}
      if(a.type==='recycle'){if(s.state!=='Quarantined'||!s.notified||s.outstanding||s.pendingEvents)return blocked(s,'미완료 I/O·이벤트·종료 통지가 남아 슬롯 재사용을 차단한다.');s.state='Free';}
      if(a.type==='reuse'){if(s.state!=='Free')return blocked(s,'이전 연결의 수명을 정리한 Free 슬롯만 예약한다.');let g=(BigInt(s.generation)+1n)&18446744073709551615n;if(g===0n)g=1n;s.generation=g.toString();s.state='Active';s.notified=false;}
      if(a.type==='deliver'){check(a.checkGeneration===undefined||typeof a.checkGeneration==='boolean','세대 검사 여부는 참 또는 거짓이다.');const stale=s.oldGeneration!==s.generation;s.decision=s.state!=='Active'?'활성 상태 아님':stale&&a.checkGeneration!==false?'세대 불일치 거절':stale?'위험한 통과':'현재 연결 통과';return result(s,`${s.decision}. 인덱스 일치·세대 일치·활성 상태는 서로 다른 조건이다.`);}
      return result(s,'세션 상태를 갱신했다. 세대와 별개로 미완료 참조와 통지 순서를 확인한다.');},
    s=>s.generation!=='0'&&!(s.state==='Free'&&(s.outstanding||s.pendingEvents||!s.notified))?[]:['재사용 조건 위반'],
    s=>({values:values([['현재 ID',`${s.index}:${s.generation}`],['지연 작업 ID',`${s.index}:${s.oldGeneration}`],['연결 상태',s.state],['미완료 I/O',s.outstanding],['대기 이벤트',s.pendingEvents],['종료 통지',s.notified],['작업 판정',s.decision]]),message:'큰 세대 값은 문자열과 BigInt로 처리한다. JavaScript Number 반올림으로 다른 연결을 같다고 보지 않는다.'}));
  define('receivePath','receive-path',c=>{const capacity=number(c,'capacity',8,4,32),chunk=number(c,'chunk',4,1,capacity);return {capacity,chunk,read:0,write:0,used:0,posted:chunk,callback:false,closed:false,callbacks:0,ignored:0,error:null};},
    (s,a)=>{check(['complete','consume','close'].includes(a.type),'complete/consume/close만 허용한다.');if(a.type==='close'){s.closed=true;s.callback=false;return result(s,'종료 통지 이후의 데이터 콜백을 막았다. 지연 완료는 참조만 정리한다.');}
      check(integer(a.bytes,0,64),'바이트는 0~64 정수다.');
      if(a.type==='complete'){
        if(s.closed){s.posted=0;s.ignored++;return result(s,'닫힌 연결의 지연 수신 완료다. OnData를 다시 호출하지 않았다.');}
        if(s.callback)return blocked(s,'이번 콜백이 반환되기 전에 같은 워커의 다음 콜백을 겹쳐 실행하지 않는다.');
        if(!s.posted)return blocked(s,'게시한 수신 요청이 없다.');
        if(a.bytes===0){s.posted=0;s.closed=true;return result(s,'0바이트 수신은 peer FIN 경로다. 본문 콜백을 호출하지 않았다.');}
        if(a.bytes>s.posted){s.error='receive_completion_length';s.posted=0;s.closed=true;return result(s,'게시 길이보다 큰 완료를 거절하고 연결 종료 경로로 진행한다.');}
        s.used+=a.bytes;s.write=(s.write+a.bytes)%s.capacity;s.posted=Math.min(s.chunk,s.capacity-s.used);s.callback=true;s.callbacks++;return result(s,'완료를 반영한 뒤 빈 구간으로 다음 수신을 게시했다. 콜백은 미소비 읽기 범위만 본다.');
      }
      if(!s.callback)return blocked(s,'현재 소비할 콜백 범위가 없다.');if(a.bytes>s.used){s.error='receive_callback_consumed_too_much';s.closed=true;s.callback=false;return result(s,'읽을 수 있는 범위보다 큰 소비량을 거절했다. 프로토콜 종료로 처리한다.');}
      s.used-=a.bytes;s.read=(s.read+a.bytes)%s.capacity;s.callback=false;if(!s.posted)s.posted=Math.min(s.chunk,s.capacity-s.used);return result(s,'반환한 소비량만큼 읽기 위치를 이동했다. 남은 꼬리는 다음 콜백에서 다시 보인다.');},
    s=>integer(s.used,0,s.capacity)&&s.posted>=0&&s.posted<=s.capacity-s.used?[]:['커널 쓰기와 콜백 읽기 범위 중첩'],
    s=>({values:values([['콜백 읽기 바이트',s.used],['다음 수신 길이',s.posted],['읽기 시작',s.read],['쓰기 시작',s.write],['OnData 횟수',s.callbacks],['지연 완료 정리',s.ignored],['상태',s.closed?'닫힘':s.callback?'콜백 중':'완료 대기'],['오류',s.error??'없음']]),message:'사용 중인 읽기 범위와 다음 커널 쓰기 범위를 분리한다. 콜백 반환 뒤 span을 보관하지 않는다.'}));
  define('sendPath','send-path',c=>{const capacity=number(c,'capacity',8,2,32);return {capacity,maxMessage:number(c,'maxMessage',Math.min(4,capacity),1,capacity),used:0,posted:0,outstanding:0,posts:0,queuedCalls:0,completed:0,rejected:0};},
    (s,a)=>{check(['enqueue','complete','force-post'].includes(a.type),'enqueue/complete/force-post만 허용한다.');
      if(a.type==='force-post')return blocked(s,s.outstanding?'한 세션의 송신 요청은 이미 1개다. 같은 데이터를 중복 게시하지 않는다.':'대기 데이터 없이 요청을 만들지 않는다.');
      check(integer(a.bytes,0,64),'바이트는 0~64 정수다.');
      if(a.type==='enqueue'){
        if(a.bytes===0||a.bytes>s.maxMessage)return blocked(s,'한 메시지 크기를 벗어나 kMessageTooLarge에 해당한다.');
        if(a.bytes>s.capacity-s.used){s.rejected++;return result(s,'kWouldBlock: 메시지 전체를 받지 않았다. 링의 기존 바이트는 바꾸지 않았다.');}
        s.used+=a.bytes;s.queuedCalls++;if(!s.outstanding){s.outstanding=1;s.posted=s.used;s.posts++;}return result(s,'내부 링에 복사했다. 진행 요청이 있으면 이번 메시지는 다음 완료까지 대기한다.');
      }
      if(!s.outstanding||a.bytes===0||a.bytes>s.posted)return blocked(s,'현재 게시한 길이 안의 양수 완료만 정상 소비한다. 오류 경로는 L11에서 다룬다.');
      s.used-=a.bytes;s.completed+=a.bytes;s.outstanding=0;s.posted=0;if(s.used){s.posted=s.used;s.outstanding=1;s.posts++;}return result(s,'완료된 바이트만 소비했다. 남은 구간과 새로 쌓인 데이터를 한 요청으로 다시 게시했다.');},
    s=>integer(s.used,0,s.capacity)&&s.posted<=s.used&&s.outstanding===Number(s.posted>0)?[]:['송신 용량 또는 요청 수 위반'],
    s=>({values:values([['송신 링 사용량',s.used],['빈 공간',s.capacity-s.used],['게시 구간',s.posted],['미완료 송신',s.outstanding],['Send 수락 횟수',s.queuedCalls],['RIO 게시 횟수',s.posts],['완료 바이트',s.completed],['거절',s.rejected]]),message:'Send 호출 수와 RIO 게시 수는 다르다. 두 span 오버로드도 반환 전에 내부 링으로 복사한다.'}));
  define('connectionClose','connection-close',c=>({phase:'Active',reason:null,receive:1,send:1,notices:0,peerWaiting:false,timeoutMs:number(c,'timeoutMs',5000,1,60000),elapsed:0,aborted:false}),
    (s,a)=>{check(['request','complete-send','complete-receive','notify','tick','recycle'].includes(a.type),'알 수 없는 종료 동작이다.');
      if(a.type==='request'){
        check(['application','peer-fin','io-error','idle'].includes(a.reason),'알 수 없는 종료 이유다.');
        if(s.peerWaiting&&a.reason!=='peer-fin'){s.reason=a.reason;s.peerWaiting=false;s.phase='Closing';s.aborted=true;return result(s,'보류 중인 peer FIN보다 명시적·오류 종료를 우선했다. 닫힘 통지를 늘리지는 않는다.');}
        if(s.reason!==null)return blocked(s,'이미 확정한 종료를 중복 통지하지 않는다.');
        s.reason=a.reason;s.peerWaiting=a.reason==='peer-fin';if(s.peerWaiting){s.receive=0;s.phase='Peer drain';}else{s.phase='Closing';s.aborted=true;}
        return result(s,s.peerWaiting?'peer FIN 수신 참조를 정리했다. 새 애플리케이션 송신은 막고 접수된 송신의 완료를 기다린다.':'종료 이유를 기록하고 소켓을 닫기 시작한다. 소켓 종료와 취소 완료의 회수는 별개다.');
      }
      if(a.type==='complete-send'){if(!s.send)return blocked(s,'남은 송신 요청이 없다.');s.send=0;if(s.peerWaiting){s.phase='Closing';s.peerWaiting=false;}return result(s,'송신 요청 참조를 정리했다. peer drain 조건을 다시 확인한다.');}
      if(a.type==='complete-receive'){if(!s.receive)return blocked(s,'남은 수신 요청이 없다.');s.receive=0;return result(s,'수신 또는 취소 완료를 정리했다. 닫힘 통지 뒤의 OnData는 실행하지 않는다.');}
      if(a.type==='tick'){check(integer(a.ms,1,60000),'경과 시간은 1~60000ms다.');s.elapsed+=a.ms;if(s.peerWaiting&&s.elapsed>=s.timeoutMs){s.peerWaiting=false;s.aborted=true;s.reason='send-error';s.phase='Closing';return result(s,'peer drain 기한을 넘겨 SendError와 abortive close로 전환했다. 남은 완료 참조는 계속 정리해야 한다.');}return result(s,'연결 단위의 대기 기한을 확인했다. 이 시간은 서버 전체 Stop 예산과 별개다.');}
      if(a.type==='notify'){if(s.phase!=='Closing'||s.notices)return blocked(s,'종료할 연결에서 한 번만 닫힘을 통지한다. peer 송신 대기 중에는 먼저 통지하지 않는다.');s.notices=1;s.phase='Quarantined';return result(s,'OnClosed를 한 번 통지했다. 격리된 슬롯의 미완료 참조가 0인지는 별도로 확인한다.');}
      if(s.phase!=='Quarantined'||s.receive+s.send||s.notices!==1)return blocked(s,'닫힘 통지와 모든 I/O 참조를 정리하기 전에는 슬롯을 반환하지 않는다.');s.phase='Free';return result(s,'이 연결의 슬롯을 반환할 수 있다. 애플리케이션 큐에 남은 이벤트는 별도 gate도 확인해야 한다.');},
    s=>[0,1].includes(s.notices)&&!(s.phase==='Free'&&(s.receive+s.send||s.notices!==1))?[]:['닫힘 통지 또는 재사용 위반'],
    s=>({values:values([['연결 단계',s.phase],['첫 종료 이유',s.reason??'없음'],['미완료 수신',s.receive],['미완료 송신',s.send],['닫힘 통지 수',s.notices],['peer 경과 시간',s.elapsed+' ms'],['강제 소켓 종료',s.aborted]]),message:'peer FIN은 수신 종료다. 이미 접수한 송신, 닫힘 통지, 슬롯 재사용의 시점을 나누어 본다.'}));
  define('serverShutdown','server-shutdown',c=>{check(c.mode===undefined||['graceful','immediate'].includes(c.mode),'종료 모드는 graceful/immediate다.');return {mode:c.mode??'graceful',accept:true,posts:true,send:1,receive:1,sockets:true,workers:true,buffers:true,cq:true,timedOut:false,preserved:false,finished:false};},
    (s,a)=>{check(['stop-accept','block-posts','complete-send','close-sockets','complete-receive','join-workers','free-buffers','free-cq','timeout','force-drain','callback-stop'].includes(a.type),'알 수 없는 서버 종료 동작이다.');
      if(a.type==='callback-stop')return blocked(s,'같은 서버의 콜백에서 Stop을 실행하면 kInvalidState다. 소유 스레드에 종료 요청을 전달한다.');
      if(s.preserved)return blocked(s,'커널 참조가 남은 실패 상태다. 보존한 등록 자원과 CQ를 성공적인 회수로 바꾸지 않는다.');
      if(a.type==='stop-accept'){s.accept=false;return result(s,'새 연결 수락을 중지하고 accept 스레드를 합류시켰다고 가정한다.');}
      if(a.type==='block-posts'){s.posts=false;return result(s,'새 애플리케이션 송신·종료 요청의 진입을 차단했다. 접수된 송신의 내부 재게시와 완료 처리는 계속한다.');}
      if(a.type==='complete-send'){if(!s.workers||!s.cq||!s.send)return blocked(s,'완료를 처리할 워커·CQ·요청이 필요하다.');s.send=0;return result(s,'접수된 송신 또는 취소 완료를 처리했다.');}
      if(a.type==='close-sockets'){if(s.accept)return blocked(s,'accept부터 중지해야 새 연결이 끼어들지 않는다.');if(s.mode==='graceful'&&s.send&&!s.timedOut)return blocked(s,'graceful 송신 대기가 남았다. 완료시키거나 기한 초과 경로를 선택한다.');s.sockets=false;s.posts=false;return result(s,'소켓을 닫고 취소 완료를 기다린다. immediate도 완료 회수는 생략하지 않는다.');}
      if(a.type==='complete-receive'){if(s.sockets||!s.workers||!s.cq)return blocked(s,'이 실험은 소켓 종료 뒤의 수신 취소 완료를 처리한다. 워커와 CQ가 필요하다.');s.receive=0;return result(s,'수신 취소 완료를 처리했다. 등록 메모리 참조가 줄었다.');}
      if(a.type==='join-workers'){if(s.accept||s.posts||s.receive+s.send)return blocked(s,'새 작업과 미완료 참조를 정리할 주체를 먼저 없애면 안 된다.');s.workers=false;return result(s,'완료 처리가 끝난 워커를 합류시켰다.');}
      if(a.type==='free-buffers'){if(s.workers||s.posts||s.receive+s.send)return blocked(s,'워커 또는 커널이 참조할 수 있는 버퍼의 조기 해제를 차단한다.');s.buffers=false;return result(s,'요청 참조가 없고 게시가 차단된 등록 버퍼와 매핑을 해제했다.');}
      if(a.type==='free-cq'){if(s.workers||s.buffers||s.receive+s.send)return blocked(s,'완료 처리와 버퍼 의존 관계를 정리한 뒤 CQ를 해제한다.');s.cq=false;s.finished=true;return result(s,'이 모델의 정상 자원 해제를 마쳤다. 실제 서버의 Winsock·notification·API 배출도 확인한다.');}
      if(a.type==='timeout'){s.timedOut=true;s.accept=false;s.posts=false;s.sockets=false;return result(s,'공유 종료 예산을 넘겼다. 소켓을 abort하고 제한된 추가 drain을 시도한다.');}
      if(!s.timedOut)return blocked(s,'강제 drain은 종료 기한 초과 뒤의 실패 경로다.');
      check(typeof a.completionsArrive==='boolean','추가 완료 도착 여부가 필요하다.');
      if(a.completionsArrive){s.receive=0;s.send=0;return result(s,'추가 drain에서 완료를 회수했다. 정상 기한 초과 사실은 사라지지 않는다.');}
      s.preserved=true;s.workers=false;return result(s,'추가 drain 뒤에도 커널 참조가 남았다. 버퍼·매핑·CQ를 의도적으로 보존하고 실패를 보고한다.');},
    s=>(!s.buffers&&(s.send+s.receive||s.workers||s.posts))||(!s.cq&&(s.send+s.receive||s.workers))?['사용 중 자원 해제']:[],
    s=>({values:values([['종료 모드',s.mode],['accept',s.accept?'허용':'중지'],['앱 진입',s.posts?'허용':'차단'],['미완료 송신',s.send],['미완료 수신',s.receive],['I/O 워커',s.workers?'실행':'합류'],['등록 버퍼',s.buffers?'유지':'해제'],['CQ',s.cq?'유지':'해제'],['자원 보존 실패',s.preserved]]),nodes:[{label:'accept',value:s.accept?'실행':'중지',active:s.accept},{label:'완료 처리',value:s.send+s.receive,active:s.send+s.receive>0},{label:'워커',value:s.workers?'실행':'합류'},{label:'버퍼',value:s.buffers?'유지':'해제'},{label:'CQ',value:s.cq?'유지':'해제'}],message:s.preserved?'기한 내 회수에 실패했다. 자원을 보존한 사실을 정상 해제로 기록하지 않는다.':'새 작업을 막고, 완료 처리 주체를 유지하며, 참조가 끝난 뒤 자원을 해제한다.'}));
  define('logicQueue','logic-queue',c=>{const capacity=number(c,'capacity',2,2,8);return {capacity,queue:[{type:'connected',generation:'41'},{type:'data',generation:'41'}],generation:'41',parked:false,parkedGeneration:null,closed:false,closeDelivered:false,rejected:0,delivered:[]};},
    (s,a)=>{check(['data','close','new-connect','pop'].includes(a.type),'알 수 없는 큐 동작이다.');
      if(a.type==='data'){if(s.closed||s.queue.length===s.capacity){s.rejected++;return result(s,'data chunk를 거절했다. framed TCP에서는 이를 손실 복구로 보지 않고 disconnect 정책을 사용한다.');}s.queue.push({type:'data',generation:s.generation});return result(s,'payload를 복사한 data 이벤트를 링에 넣었다.');}
      if(a.type==='close'){if(s.closed)return blocked(s,'같은 세대의 close를 중복 저장하지 않는다.');s.closed=true;if(s.queue.length===s.capacity){s.parked=true;s.parkedGeneration=s.generation;}else s.queue.push({type:'closed',generation:s.generation});return result(s,s.parked?'링이 가득 차 close를 세션 gate에 보류했다. depth는 링 용량보다 클 수 있다.':'close를 링에 넣었다. 선행 이벤트 뒤에서 소비된다.');}
      if(a.type==='new-connect'){if(!s.closed)return blocked(s,'이 실험에서는 이전 transport 종료 뒤의 새 연결만 추가한다. 같은 연결을 중복 접속시키지 않는다.');if(s.parked){s.rejected++;return result(s,'이전 세대의 parked close를 아직 전달하지 않아 새 generation의 connect를 거절했다.');}if(s.queue.length===s.capacity){s.rejected++;return result(s,'connect를 넣을 큐 공간이 없다.');}s.generation=(BigInt(s.generation)+1n).toString();s.closed=false;s.closeDelivered=false;s.queue.push({type:'connected',generation:s.generation});return result(s,'새 세대 connect를 링 뒤에 넣었다. 남은 이전 세대 이벤트의 순서를 유지한다.');}
      let event;if(s.parked&&s.queue.length===0){event={type:'closed',generation:s.parkedGeneration};s.parked=false;s.parkedGeneration=null;}else event=s.queue.shift();
      if(!event)return blocked(s,'링과 parked close가 모두 비었다. 무한 대기하지 않고 kEmpty를 반환한다.');
      if(event.type==='closed')s.closeDelivered=true;s.delivered.push(`${event.generation}:${event.type}`);return result(s,`${event.generation} 세대의 ${event.type}를 전달했다. TryPop은 선행 이벤트가 모두 빠진 세션의 parked close를 먼저 합성할 수 있다.`);},
    s=>s.queue.length<=s.capacity&&new Set(s.delivered.filter(v=>v.endsWith(':closed'))).size===s.delivered.filter(v=>v.endsWith(':closed')).length?[]:['큐 용량 또는 close 중복 위반'],
    s=>({values:values([['링 이벤트',s.queue.map(e=>`${e.generation}:${e.type}`).join(' → ')||'없음'],['보류한 close',s.parked],['전체 depth',s.queue.length+Number(s.parked)],['거절',s.rejected],['소비한 순서',s.delivered.join(' → ')||'없음']]),nodes:[{label:'링',value:s.queue.length,active:s.queue.length>0},{label:'parked close',value:Number(s.parked),active:s.parked},{label:'게임 워커',value:s.delivered.length}],message:'close는 data 포화와 다르게 보존한다. 같은 세션의 선행 data를 소비한 뒤 close를 전달한다.'}));
  const u16=(bytes,p)=>(bytes[p]*256+bytes[p+1]);
  const u32=(bytes,p)=>(bytes[p]*16777216+bytes[p+1]*65536+bytes[p+2]*256+bytes[p+3]);
  const write16=n=>[(n>>>8)&255,n&255];
  const write32=n=>[(n>>>24)&255,(n>>>16)&255,(n>>>8)&255,n&255];
  function encodeChat(type,payload,version=1){return [...write32(8+payload.length),...write16(version),...write16(type),...payload];}
  define('chatServer','chat-server',c=>{
    const shards=number(c,'shards',2,1,4),capacity=number(c,'capacity',2,1,8);
    return {shards,capacity,players:Array.from({length:3},(_,index)=>({index,shard:index%shards,phase:'Login',room:0,name:'',input:[],receipts:0})),inboxes:Array.from({length:shards},()=>[]),lastWire:[],broadcasts:0,delivered:0,error:null};},
    (s,a)=>{check(['login','enter','chat','ping','wire','disconnect','drain'].includes(a.type),'알 수 없는 Chat 동작이다.');
      if(a.type==='drain'){for(let shard=0;shard<s.shards;shard++)while(s.inboxes[shard].length){const packet=s.inboxes[shard].shift();for(const p of s.players)if(p.shard===shard&&p.phase==='Room'&&p.room===packet.room){p.receipts++;s.delivered++;}}return result(s,'각 shard가 자기 PlayerMap에서 같은 room의 활성 플레이어에게 전달했다. 발신자도 같은 room이면 포함된다.');}
      check(integer(a.player,0,2),'플레이어는 0~2다.');const p=s.players[a.player];
      if(a.type==='disconnect'){p.phase='Closed';p.input=[];return result(s,'연결 종료 이벤트를 처리한 것으로 가정하고 Player를 제거한 상태로 표시한다.');}
      if(p.phase==='Closed')return blocked(s,'닫힌 연결의 패킷을 처리하지 않는다. 새 연결은 새 세대로 구성한다.');
      let wire;
      if(a.type==='wire'){check(typeof a.hex==='string'&&a.hex.length<=4096&&/^(?:\s*[0-9a-fA-F]{2}\s*)+$/.test(a.hex),'wire 입력은 4,096자 이하의 16진 바이트다.');wire=a.hex.replace(/\s+/g,'').match(/../g).map(v=>parseInt(v,16));}
      else{
        let payload=[],type=4;
        if(a.type==='login'||a.type==='chat'){check(typeof a.text==='string'&&a.text.length<=512,'문자열 입력은 512자 이하다.');const bytes=[...new TextEncoder().encode(a.text)];payload=[...write16(bytes.length),...bytes];type=a.type==='login'?1:3;}
        if(a.type==='enter'){check(integer(a.room,0,4294967295),'room은 uint32다.');payload=write32(a.room);type=2;}
        const version=a.version??1;check(integer(version,0,65535),'version은 uint16이다.');wire=encodeChat(type,payload,version);
      }
      s.lastWire=wire.slice(0,80);p.input.push(...wire);
      if(p.input.length>2048){p.phase='Closed';p.input=[];s.error='input-limit';return result(s,'실험의 입력 축적 상한을 넘겨 연결을 닫았다. 무제한 임시 버퍼로 바꾸지 않는다.');}
      let processed=0;
      while(p.input.length>=8){
        const length=u32(p.input,0),version=u16(p.input,4),type=u16(p.input,6);
        const close=message=>{p.phase='Closed';p.input=[];s.error='protocol-error';return result(s,message);};
        if(length<8||length>1024||version!==1)return close('전체 길이 또는 protocol version을 검증해 종료했다.');
        if(p.input.length<length)break;
        const payload=p.input.slice(8,length);p.input.splice(0,length);processed++;
        if(type===1&&p.phase==='Login'){
          if(payload.length<2)return close('로그인 payload에 문자열 길이가 없다.');const bytes=u16(payload,0);if(!bytes||bytes>32||payload.length!==2+bytes)return close('이름의 UTF-8 바이트 길이는 1~32이고 payload 길이와 같아야 한다.');
          try{p.name=new TextDecoder('utf-8',{fatal:true}).decode(new Uint8Array(payload.slice(2)));}catch{return close('유효하지 않은 UTF-8 이름이다.');}p.phase='Lobby';continue;
        }
        if(type===2&&p.phase==='Lobby'&&payload.length===4){p.room=u32(payload,0);p.phase='Room';continue;}
        if(type===3&&p.phase==='Room'&&payload.length>=2){
          const bytes=u16(payload,0);if(bytes>256||payload.length!==2+bytes)return close('채팅의 UTF-8 길이 또는 payload 크기가 맞지 않는다.');
          let text;try{text=new TextDecoder('utf-8',{fatal:true}).decode(new Uint8Array(payload.slice(2)));}catch{return close('유효하지 않은 UTF-8 채팅이다.');}
          if(s.inboxes.some(box=>box.length===s.capacity)){p.phase='Closed';s.error='overloaded';return result(s,'어느 shard의 inbox라도 가득 차면 어떤 shard에도 추가하지 않고 발신자를 overload 종료한다.');}
          for(const box of s.inboxes)box.push({room:p.room,text});s.broadcasts++;continue;
        }
        if(type===4&&payload.length===0)continue;
        return close('패킷 타입, 현재 Login/Lobby/Room 상태와 payload 형식이 맞지 않는다.');
      }
      return result(s,processed?`${processed}개 프레임을 검증했다. 문자열 길이는 글자 수가 아닌 UTF-8 바이트 수다.`:'헤더 또는 본문이 덜 왔다. 입력 복사본을 남기고 다음 조각을 기다린다.');},
    s=>s.inboxes.every(box=>box.length<=s.capacity)&&s.players.every(p=>p.input.length<=2048)?[]:['Chat 입력 또는 방송 용량 위반'],
    s=>({values:values([...s.players.map(p=>[`플레이어 ${p.index}`,`W${p.shard} ${p.phase} room=${p.room} 수신=${p.receipts} 입력=${p.input.length}`]),['shard별 방송 대기',s.inboxes.map(b=>b.length).join(', ')],['방송 요청',s.broadcasts],['실제 대상 전달',s.delivered],['마지막 wire (앞 80바이트)',s.lastWire.map(n=>n.toString(16).padStart(2,'0')).join(' ')],['오류',s.error??'없음']]),nodes:s.players.map(p=>({label:`플레이어 ${p.index} / W${p.shard}`,value:`${p.phase} R${p.room}`,active:p.phase==='Room'})),message:'실제 v1 규칙을 사용하는 3명 모델이다. 큐 잠금·네트워크 전달 시간·서버 인증은 재현하지 않는다.'}));
  models.chatServer.encodePacket=encodeChat;
  define('applicationQueue','application-queue',c=>{const policy=c.policy??'reject';check(['reject','disconnect'].includes(policy),'포화 정책은 reject/disconnect다.');return {capacity:number(c,'capacity',8,1,64),incoming:number(c,'incoming',3,0,100),outgoing:number(c,'outgoing',1,0,100),policy,used:0,rejected:0,processed:0,disconnects:0,ticks:0};},
    (s,a)=>{check(a.type==='tick','tick만 허용한다.');const incoming=a.incoming??s.incoming,outgoing=a.outgoing??s.outgoing;check(integer(incoming,0,100)&&integer(outgoing,0,100),'유입과 처리 수는 0~100이다.');s.incoming=incoming;s.outgoing=outgoing;
      const processed=Math.min(s.used,outgoing);s.used-=processed;s.processed+=processed;const accepted=Math.min(s.capacity-s.used,incoming),rejected=incoming-accepted;s.used+=accepted;s.rejected+=rejected;if(s.policy==='disconnect')s.disconnects+=rejected;s.ticks++;
      return result(s,`${s.ticks}틱: 기존 ${processed}개 처리 → 새 ${accepted}개 접수 → ${rejected}개 거절. 용량을 늘려도 유입이 지속적으로 더 크면 다시 포화된다.`);},
    s=>integer(s.used,0,s.capacity)&&s.rejected>=0&&s.processed>=0?[]:['애플리케이션 큐 용량 위반'],
    s=>({values:values([['현재 대기량',s.used],['용량',s.capacity],['틱당 유입',s.incoming],['틱당 처리 상한',s.outgoing],['누적 처리',s.processed],['거절',s.rejected],['종료 정책 적용 수',s.disconnects]]),nodes:[{label:'새 작업',value:s.incoming},{label:'용량 있는 큐',value:`${s.used}/${s.capacity}`,active:s.used===s.capacity},{label:'처리 워커',value:s.outgoing}],message:'이 실험은 게임 작업 개수를 센다. raw TCP chunk를 손실시켜도 되는 정책이라고 해석하지 않는다.'}));
  const diagnosticCases={
    bind:{symptom:'bind_failed / native 10048',evidence:'같은 포트의 LISTEN PID를 확인하기 전이다.',safe:'inspect-port',danger:'kill-process',next:'Get-NetTCPConnection -LocalPort 32452 -State Listen으로 소유자를 확인하고 두 프로그램의 포트를 맞춘다.'},
    pressure:{symptom:'send_would_block 증가, 수락 바이트와 완료 바이트 차이',evidence:'상대 수신 속도와 callback 시간을 아직 확인하지 않았다.',safe:'inspect-consumer',danger:'infinite-retry',next:'소비 속도·대기량·callback 시간을 함께 확인하고 유계 재시도 또는 overload 종료를 고른다.'},
    stale:{symptom:'같은 index에서 stale_operations 증가',evidence:'작업이 기록한 generation과 현재 ID를 아직 비교하지 않았다.',safe:'inspect-generation',danger:'index-only',next:'요청에 저장한 index와 generation 전체를 비교한다. 현재 활성 상태 검사도 별도다.'},
    shutdown:{symptom:'Stop timeout / remaining_outstanding > 0',evidence:'막힌 callback과 완료 회수 상태를 확인하기 전이다.',safe:'inspect-outstanding',danger:'force-free',next:'작업 진입·callback·취소 완료와 잔여 참조를 조사한다. 참조 중인 자원을 무조건 해제하지 않는다.'}
  };
  define('diagnostics','diagnostics',c=>{const key=c.case??'bind';check(Object.hasOwn(diagnosticCases,key),'알 수 없는 진단 사례다.');return {case:key,inspected:false,decision:'미선택',danger:false};},
    (s,a)=>{check(['inspect','choose'].includes(a.type),'inspect/choose만 허용한다.');const d=diagnosticCases[s.case];if(a.type==='inspect'){s.inspected=true;return result(s,d.next);}
      check(typeof a.choice==='string'&&a.choice.length<=64,'조치 이름은 64자 이하 문자열이다.');
      if(a.choice===d.danger){s.decision='위험한 조치';s.danger=true;return result(s,'증거 없이 실행하면 다른 서비스 종료, 워커 정지 또는 use-after-free를 만들 수 있다. '+d.next);}
      if(a.choice!==d.safe)return blocked(s,'현재 증상 하나로 그 원인을 확정할 수 없다. 추가 확인 항목을 먼저 고른다.');
      s.decision=s.inspected?'근거에 따라 다음 검사 선택':'추가 확인 필요';s.danger=false;return result(s,d.next+' 원인을 확정한 실제 실행 결과와 가상의 진단 선택은 구분한다.');},
    s=>Object.hasOwn(diagnosticCases,s.case)?[]:['없는 진단 사례'],
    s=>({values:values([['관찰한 증상',diagnosticCases[s.case].symptom],['현재 근거',s.inspected?diagnosticCases[s.case].next:diagnosticCases[s.case].evidence],['조사 여부',s.inspected],['선택 결과',s.decision]]),message:'증상은 여러 원인에서 나올 수 있다. 안전한 추가 확인과 파괴적인 추측을 구분한다.'}));
  define('failureRollback','failure-rollback',c=>({failAt:number(c,'failAt',3,0,4),call:0,acquired:[],released:[],failed:false,disposed:false}),
    (s,a)=>{check(['next','dispose'].includes(a.type),'next/dispose만 허용한다.');
      if(a.type==='dispose'){s.released.push(...[...s.acquired].reverse());s.acquired=[];s.disposed=true;return result(s,'성공적으로 확보한 등록 버퍼만 역순으로 해제했다.');}
      if(s.failed||s.disposed||s.call===4)return blocked(s,'초기화 시퀀스가 끝났다. 초기화하거나 명시적으로 확보한 자원을 정리한다.');
      const resources=['session0.send','session0.recv','session1.send','session1.recv'];s.call++;
      if(s.call===s.failAt){s.failed=true;s.released.push(...[...s.acquired].reverse());s.acquired=[];return result(s,`${s.call}번째 등록 호출이 실패했다. 이전 성공분만 정리했으며 실패한 자원의 가짜 해제를 만들지 않는다.`);}
      s.acquired.push(resources[s.call-1]);return result(s,`${resources[s.call-1]}를 등록했다. 실패를 설정하지 않았다면 네 건까지 진행한다.`);},
    s=>new Set([...s.acquired,...s.released]).size===s.acquired.length+s.released.length?[]:['미확보 자원 해제 또는 이중 해제'],
    s=>({values:values([['등록 시도',s.call],['실패시킬 호출 (0=없음)',s.failAt],['유지한 등록',s.acquired.join(' → ')||'없음'],['해제 순서',s.released.join(' → ')||'없음'],['초기화 실패',s.failed],['명시적 정리',s.disposed]]),nodes:[{label:'등록 시도',value:s.call},{label:'확보 자원',value:s.acquired.length,active:s.acquired.length>0},{label:'역순 정리',value:s.released.length}],message:'두 세션의 recv/send 등록 네 건에 한정한다. 매핑·소켓·CQ의 실패 접점은 실제 fake 테스트에서 따로 확인한다.'}));
  const syntheticRuns=[
    {id:'A-1',environment:'A',errors:0,throughput:1000,latency:[1,1.1,1.2,1.3,1.4,1.5,1.7,2,3,10]},
    {id:'A-error',environment:'A',errors:2,throughput:1400,latency:[1,1,1,1,1,1,1,1,1,1]},
    {id:'A-2',environment:'A',errors:0,throughput:1100,latency:[1,1.1,1.2,1.3,1.4,1.5,1.7,2,3,11]},
    {id:'A-3',environment:'A',errors:0,throughput:900,latency:[1,1.1,1.2,1.3,1.4,1.5,1.7,2,3,12]},
    {id:'B-1',environment:'B',errors:0,throughput:1800,latency:[1,1,1,1,1,1,1,1,1,5]}
  ];
  define('benchmarking','benchmarking',c=>{const unit=c.unit??'ms';check(['ms','us'].includes(unit),'단위는 ms/us다.');return {unit,selected:[],excluded:0,warning:'가상 자료다. 제품 성능 수치가 아니다.'};},
    (s,a)=>{check(['include','compare'].includes(a.type),'include/compare만 허용한다.');check(integer(a.run,0,4),'자료 번호는 0~4다.');const run=syntheticRuns[a.run];
      if(run.errors){s.excluded++;s.warning='오류가 있는 실행은 정상 성능 비교에서 제외한다.';return result(s,s.warning);}
      if(run.environment!=='A'){s.warning='장비·환경 B의 수치를 A와 직접 비교해 개선으로 판정하지 않는다.';return result(s,s.warning);}
      if(a.type==='compare'){s.warning='같은 환경·조건·오류 0인 반복 자료를 먼저 선택한다.';return result(s,s.warning);}
      if(s.selected.includes(a.run))return blocked(s,'이미 선택한 실행을 중복으로 반복 횟수에 세지 않는다.');s.selected.push(a.run);s.warning='선택한 같은 환경의 오류 0 자료를 집계했다. 표본 수가 적어 꼬리 지연 해석에는 한계가 있다.';return result(s,s.warning);},
    s=>s.selected.every(i=>syntheticRuns[i].errors===0&&syntheticRuns[i].environment==='A')&&new Set(s.selected).size===s.selected.length?[]:['오류 실행 또는 다른 환경 혼합'],
    s=>{const runs=s.selected.map(i=>syntheticRuns[i]);const through=runs.map(r=>r.throughput).sort((a,b)=>a-b);const sorted=runs.flatMap(r=>r.latency).sort((a,b)=>a-b);const n=through.length;const median=n?(n%2?through[(n-1)/2]:(through[n/2-1]+through[n/2])/2):0;const p99=sorted.length?sorted[Math.ceil(sorted.length*.99)-1]*(s.unit==='us'?1000:1):0;return {values:values([['자료 종류','설명용 가상 자료'],['선택한 실행',runs.map(r=>r.id).join(', ')||'없음'],['오류 제외 수',s.excluded],['반복 처리량 중앙값',median+' requests/s'],['합친 지연 표본 수',sorted.length],['합친 표본 P99 (nearest rank)',p99+' '+s.unit]]),message:s.warning};});
})(typeof module!=='undefined'&&module.exports?require('./core.js'):BookModels);
if(typeof module!=='undefined'&&module.exports)module.exports=require('./core.js');
