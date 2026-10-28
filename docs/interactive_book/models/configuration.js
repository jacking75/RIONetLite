'use strict';
(function(models){
  const {check,integer}=models.core;
  const defaults={port:32452,io_worker_count:2,max_sessions:1024,max_receive_chunk_bytes:4096,
    receive_buffer_bytes:65536,max_message_bytes:16384,send_buffer_bytes:262144,completion_batch_size:64,
    worker_wait_mode:1,adaptive_spin_count:128,enable_thread_affinity:false,enable_metrics:true,
    enable_callback_timing:true,graceful_shutdown_timeout_ms:5000,peer_close_drain_timeout_ms:5000,
    idle_timeout_ms:0,keepalive_time_ms:0,keepalive_interval_ms:1000,pageSize:4096};
  function calculate(config){
    check(config&&typeof config==='object'&&!Array.isArray(config),'설정은 객체여야 한다.');
    const c={...defaults,...config};
    for(const [key,value]of Object.entries(c)){
      check(Object.hasOwn(defaults,key),'알 수 없는 설정 필드다: '+key);
      if(typeof defaults[key]==='boolean')check(typeof value==='boolean',key+'는 참 또는 거짓이어야 한다.');
      else check(integer(value,0,key==='port'?65535:key==='worker_wait_mode'?255:4294967295),key+'의 C++ 정수 범위를 확인한다.');
    }
    check(integer(c.pageSize,1,1048576),'설명에 사용할 Windows page 크기가 필요하다.');
    const invalid=(field,reason)=>({config:c,valid:false,field,reason,budgets:null,error:'invalid-configuration'});
    if(c.port===0)return invalid('port','포트는 1~65535다.');
    if(c.io_worker_count===0||c.io_worker_count>256)return invalid('io_worker_count','워커는 1~256개다.');
    if(c.max_sessions===0||c.max_sessions>2147483647)return invalid('max_sessions','세션은 1~INT32_MAX개다.');
    if(c.io_worker_count>c.max_sessions)return invalid('io_worker_count','워커 수가 세션 수를 넘을 수 없다.');
    if(!c.receive_buffer_bytes||c.receive_buffer_bytes%c.pageSize)return invalid('receive_buffer_bytes','수신 버퍼는 Windows page 크기의 양의 배수다.');
    if(c.receive_buffer_bytes>1073741824||c.send_buffer_bytes>1073741824)return invalid('buffer_bytes','방향별 등록 버퍼는 1GiB 이하다.');
    if(!c.send_buffer_bytes||c.send_buffer_bytes%c.pageSize)return invalid('send_buffer_bytes','송신 버퍼는 Windows page 크기의 양의 배수다.');
    if(!c.max_receive_chunk_bytes||c.max_receive_chunk_bytes>c.receive_buffer_bytes)return invalid('max_receive_chunk_bytes','수신 조각은 수신 버퍼 안에 들어가야 한다.');
    if(!c.max_message_bytes||c.max_message_bytes>c.receive_buffer_bytes||c.max_message_bytes>c.send_buffer_bytes)return invalid('max_message_bytes','메시지는 양쪽 버퍼에 들어가야 한다.');
    if(!c.completion_batch_size||c.completion_batch_size>4096)return invalid('completion_batch_size','batch는 1~4096이다.');
    if(c.adaptive_spin_count>1000000)return invalid('adaptive_spin_count','spin은 0~1000000이다.');
    if(!c.graceful_shutdown_timeout_ms||c.graceful_shutdown_timeout_ms>300000)return invalid('graceful_shutdown_timeout_ms','서버 종료 예산은 1~300000ms다.');
    if(!c.peer_close_drain_timeout_ms||c.peer_close_drain_timeout_ms>60000)return invalid('peer_close_drain_timeout_ms','peer drain은 1~60000ms다.');
    if(c.idle_timeout_ms!==0&&c.idle_timeout_ms<1000)return invalid('idle_timeout_ms','idle은 0 또는 1000ms 이상이다.');
    if(c.keepalive_time_ms!==0&&c.keepalive_interval_ms===0)return invalid('keepalive_interval_ms','keepalive를 쓰면 interval이 양수여야 한다.');
    const sessionsPerWorker=Math.ceil(c.max_sessions/c.io_worker_count);
    const cq=sessionsPerWorker*2;if(cq>0x8000000)return invalid('max_sessions','계산한 CQ 용량이 RIO_MAX_CQ_SIZE를 초과한다.');
    const backing=BigInt(c.receive_buffer_bytes)+BigInt(c.send_buffer_bytes),sessions=BigInt(c.max_sessions);
    const registered=backing*2n*sessions,control=640n*sessions+BigInt(c.io_worker_count)*(BigInt(c.completion_batch_size)*32n+256n);
    return {config:c,valid:true,field:'',reason:'자원 예산의 범위 검사를 통과했다. 실제 주소 매핑·메모리 확보·provider 성공은 별도다.',error:null,
      budgets:{sessionsPerWorker,cq,registered:registered.toString(),reserved:registered.toString(),committed:(backing*sessions).toString(),control:control.toString()}};
  }
  function transition(state,action){
    check(action&&['calculate','set'].includes(action.type),'calculate 또는 set 동작만 허용한다.');
    const config={...state.config};if(action.type==='set'){check(Object.hasOwn(defaults,action.field),'알 수 없는 설정 필드다.');config[action.field]=action.value;}
    const next=calculate(config);return {state:next,events:[{type:next.valid?'validated':'invalid',field:next.field}],explanationId:next.valid?'config.valid':'config.invalid',message:next.reason};
  }
  const model={createInitialState:calculate,transition,validateState:s=>s.valid!==Boolean(s.budgets)?['검증 결과와 예산 불일치']:[],
    view(s){return {budget:s.budgets,values:[{label:'설정 판정',value:s.valid?'설명용 계산: 범위 통과':'설명용 계산: 거절'},{label:'오류 필드',value:s.field||'없음'},
      {label:'page 크기 가정',value:s.config.pageSize},...Object.entries(s.budgets??{}).map(([label,value])=>({label,value}))],
      nodes:[{label:'설정 범위',value:s.valid?'통과':'거절',active:!s.valid},{label:'등록·예약',value:s.budgets?.registered??'계산 안 함'},{label:'backing commit',value:s.budgets?.committed??'계산 안 함'},{label:'실제 자원 확보',value:'별도 실행'}],message:s.reason};},
    serializeScenario(config,actions){return models.core.serialize(model,'configuration',config,actions);},parseScenario(text){return models.core.parse(text,model,'configuration');},defaults};
  models.configuration=model;
})(typeof module!=='undefined'&&module.exports?require('./core.js'):BookModels);
if(typeof module!=='undefined'&&module.exports)module.exports=require('./core.js').configuration;
