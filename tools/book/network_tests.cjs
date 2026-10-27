'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const models=require('../../docs/interactive_book/models/network_models.js');
const configuration=require('../../docs/interactive_book/models/configuration.js');
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
function run(model,config,actions){let state=model.createInitialState(config);for(const action of actions){const before=JSON.stringify(state);const result=model.transition(state,action);assert.equal(JSON.stringify(state),before,'transition mutated the previous state');state=result.state;assert.deepEqual(model.validateState(state),[]);}return state;}
test('L01 preserves a pending receive after the response send completes',()=>{
 const m=models.echoRoundtrip;const actions=Array.from({length:6},()=>({type:'next'}));const state=run(m,{},actions);
 assert.equal(state.step,6);assert.equal(state.receiveOutstanding,1);assert.equal(state.sendOutstanding,0);assert.equal(state.sent,7);
 assert.deepEqual(m.parseScenario(m.serializeScenario({},actions)).state,state);
});
test('L02 posting failure rolls back and cannot create a completion',()=>{
 const m=models.rioRequests;let s=m.transition(m.createInitialState(),{type:'post',succeeds:false}).state;
 assert.equal(s.outstanding,0);assert.equal(m.transition(s,{type:'complete'}).blocked,true);
 s=run(m,{},[{type:'post'},{type:'complete'}]);assert.equal(s.completions,1);
});
test('L03 interleaves sessions but blocks data before connect and after close',()=>{
 const m=models.callbackOrder;assert.equal(m.transition(m.createInitialState(),{type:'data',session:0}).blocked,true);
 const s=run(m,{},[{type:'connect',session:0},{type:'connect',session:1},{type:'data',session:1},{type:'data',session:0},{type:'close',session:0}]);
 assert.equal(m.transition(s,{type:'data',session:0}).blocked,true);assert.equal(s.sessions[1].closed,false);
});
test('L04 send zero alone does not permit deregistration, movement, or freeing',()=>{
 const m=models.registeredMemory;let s=run(m,{},[{type:'complete-send'}]);
 for(const type of ['free','move','unregister'])assert.equal(m.transition(s,{type}).blocked,true);
 s=run(m,{},[{type:'block'},{type:'complete-send'},{type:'complete-receive'},{type:'unregister'},{type:'move'},{type:'free'}]);assert.equal(s.freed,true);
});
test('L05 wraps virtual spans without doubling backing capacity or overwriting pending bytes',()=>{
 const m=models.ringBuffer;let s=run(m,{capacity:8},[{type:'write',bytes:6},{type:'consume',bytes:5},{type:'write',bytes:6}]);
 assert.deepEqual([s.read,s.write,s.used],[5,4,7]);assert.equal(m.transition(s,{type:'write',bytes:2}).blocked,true);
 s=m.transition(s,{type:'post',bytes:7}).state;assert.equal(m.transition(s,{type:'consume',bytes:1}).blocked,true);
 s=m.transition(s,{type:'complete',bytes:7}).state;assert.equal(s.used,0);assert.equal(s.read,s.write);
 s=run(m,{capacity:8},[{type:'write',bytes:8}]);assert.equal(s.read,s.write);assert.equal(s.used,8);
});
test('L06 partitions uneven counts, leases each slot once, and rolls back RQ failure',()=>{
 const m=models.acceptSession;let s=m.createInitialState({slots:5,workers:2});assert.deepEqual(s.free,[0,3,1,4,2]);
 const failed=m.transition(s,{type:'accept',rqSucceeds:false}).state;assert.equal(failed.sockets,0);assert.deepEqual(failed.free,[...s.free.slice(1),s.free[0]]);
 for(let i=0;i<6;i++)s=m.transition(s,{type:'accept'}).state;assert.equal(s.sockets,5);assert.equal(s.rejected,1);
});
test('L07 forbids early reuse and distinguishes generation checking from active state',()=>{
 const m=models.sessionIdentity;let s=m.createInitialState({generation:'18446744073709551615'});assert.equal(m.transition(s,{type:'reuse'}).blocked,true);
 s=run(m,{generation:'18446744073709551615'},[{type:'close'},{type:'complete'},{type:'consume-event'},{type:'notify'},{type:'quarantine'},{type:'recycle'},{type:'reuse'}]);
 assert.equal(s.generation,'1');assert.equal(m.transition(s,{type:'deliver'}).state.decision,'세대 불일치 거절');assert.equal(m.transition(s,{type:'deliver',checkGeneration:false}).state.decision,'위험한 통과');
});
test('L08 receive posting stays outside callback read bytes and closes on over-consumption',()=>{
 const m=models.receivePath;let s=run(m,{capacity:8,chunk:4},[{type:'complete',bytes:3}]);assert.equal(s.used,3);assert.equal(s.posted,4);
 s=m.transition(s,{type:'consume',bytes:4}).state;assert.equal(s.error,'receive_callback_consumed_too_much');
 s=m.transition(s,{type:'complete',bytes:2}).state;assert.equal(s.callbacks,1);assert.equal(s.ignored,1);
});
test('L10 coalesces queued sends, has one outstanding request, and rejects whole writes on saturation',()=>{
 const m=models.sendPath;let s=run(m,{capacity:8,maxMessage:4},[{type:'enqueue',bytes:2},{type:'enqueue',bytes:3}]);assert.equal(s.posts,1);assert.equal(s.posted,2);
 s=m.transition(s,{type:'complete',bytes:2}).state;assert.equal(s.posted,3);assert.equal(s.posts,2);
 s=m.transition(s,{type:'enqueue',bytes:4}).state;const rejected=m.transition(s,{type:'enqueue',bytes:2}).state;assert.equal(rejected.used,7);assert.equal(rejected.rejected,1);assert.equal(rejected.outstanding,1);
});
test('L12 peer drain timeout, one close notice, and outstanding reclamation are separate',()=>{
 const m=models.connectionClose;let s=run(m,{timeoutMs:5},[{type:'request',reason:'peer-fin'}]);assert.equal(m.transition(s,{type:'notify'}).blocked,true);
 s=run(m,{timeoutMs:5},[{type:'request',reason:'peer-fin'},{type:'tick',ms:5},{type:'notify'}]);assert.equal(s.notices,1);assert.equal(s.aborted,true);assert.equal(m.transition(s,{type:'recycle'}).blocked,true);
 s=m.transition(s,{type:'complete-send'}).state;s=m.transition(s,{type:'recycle'}).state;assert.equal(s.phase,'Free');
 const override=run(m,{},[{type:'request',reason:'peer-fin'},{type:'request',reason:'application'}]);assert.equal(override.reason,'application');assert.equal(override.peerWaiting,false);
});
test('L13 shutdown enforces dependencies and retains kernel-referenced resources after timeout',()=>{
 const m=models.serverShutdown;const initial=m.createInitialState();assert.equal(m.transition(initial,{type:'free-buffers'}).blocked,true);
 const normal=run(m,{},[{type:'stop-accept'},{type:'block-posts'},{type:'complete-send'},{type:'close-sockets'},{type:'complete-receive'},{type:'join-workers'},{type:'free-buffers'},{type:'free-cq'}]);assert.equal(normal.finished,true);
 const failed=run(m,{},[{type:'timeout'},{type:'force-drain',completionsArrive:false}]);assert.equal(failed.preserved,true);assert.equal(failed.buffers,true);assert.equal(failed.cq,true);assert.equal(m.transition(failed,{type:'free-buffers'}).blocked,true);
});
test('L14 full ring preserves close and blocks a new generation until parked close is delivered',()=>{
 const m=models.logicQueue;let s=run(m,{},[{type:'close'}]);assert.equal(s.queue.length+Number(s.parked),3);
 s=m.transition(s,{type:'new-connect'}).state;assert.equal(s.generation,'41');
 for(let i=0;i<3;i++)s=m.transition(s,{type:'pop'}).state;assert.deepEqual(s.delivered,['41:connected','41:data','41:closed']);
 s=m.transition(s,{type:'new-connect'}).state;assert.equal(s.generation,'42');
 s=m.transition(s,{type:'data'}).state;s=m.transition(s,{type:'close'}).state;assert.equal(s.parkedGeneration,'42');
 for(let i=0;i<3;i++)s=m.transition(s,{type:'pop'}).state;assert.equal(s.delivered.at(-1),'42:closed');
 s=m.transition(s,{type:'new-connect'}).state;assert.equal(s.generation,'43');
});
test('L15 matches every shared C++/C# v1 golden wire fixture',()=>{
 const rows=fs.readFileSync(path.resolve(__dirname,'../../docs/protocol/chat-fixtures.tsv'),'utf8').split(/\r?\n/).filter(l=>l&&!l.startsWith('#'));
 for(const row of rows){const [name,type,payload,wire]=row.split('\t');const bytes=payload==='-'?[]:[...Buffer.from(payload,'hex')];assert.equal(Buffer.from(models.chatServer.encodePacket(Number(type),bytes)).toString('hex').toUpperCase(),wire,name);}
});
function playersReady(shards=2){const m=models.chatServer;return run(m,{shards,capacity:2},Array.from({length:3},(_,player)=>[{type:'login',player,text:'ABC'[player]},{type:'enter',player,room:1}]).flat());}
test('L15 sends to the same room across shards and includes the sender',()=>{
 const m=models.chatServer;let s=playersReady(2);s=m.transition(s,{type:'chat',player:0,text:'안녕'}).state;assert.deepEqual(s.inboxes.map(b=>b.length),[1,1]);
 s=m.transition(s,{type:'drain'}).state;assert.deepEqual(s.players.map(p=>p.receipts),[1,1,1]);assert.equal(s.delivered,3);
});
test('L15 rejects invalid phase, version, UTF-8, and payload lengths',()=>{
 const m=models.chatServer;assert.equal(run(m,{},[{type:'enter',player:0,room:1}]).players[0].phase,'Closed');
 assert.equal(run(m,{},[{type:'login',player:0,text:'A',version:2}]).error,'protocol-error');
 assert.equal(run(m,{},[{type:'wire',player:0,hex:'0000000B000100010001FF'}]).error,'protocol-error');
 assert.equal(run(m,{},[{type:'login',player:0,text:'가'.repeat(11)}]).players[0].phase,'Closed');
 const partial=run(m,{},[{type:'wire',player:0,hex:'0000000B00010001'}]);assert.equal(partial.players[0].phase,'Login');assert.equal(partial.players[0].input.length,8);
});
test('L15 saturation cannot partially fan out to only some shards',()=>{
 const m=models.chatServer;let s=playersReady(2);for(let i=0;i<3;i++)s=m.transition(s,{type:'chat',player:0,text:'x'}).state;
 assert.equal(s.error,'overloaded');assert.deepEqual(s.inboxes.map(b=>b.length),[2,2]);assert.equal(s.broadcasts,2);
});
test('L16 consumes existing work before admission and capacity cannot solve sustained overload',()=>{
 const m=models.applicationQueue;let s=run(m,{},Array.from({length:5},()=>({type:'tick'})));assert.deepEqual([s.used,s.rejected,s.processed],[8,3,4]);
 s=m.transition(s,{type:'tick',incoming:5,outgoing:5}).state;assert.equal(s.used,8);
 s=m.transition(s,{type:'tick',incoming:0,outgoing:8}).state;assert.equal(s.used,0);
 assert.throws(()=>m.createInitialState({capacity:0}));
});
test('L17 agrees with executed C++ validation fixture and exact uint64 budgets',()=>{
 const root=path.resolve(__dirname,'../..');const fixture=JSON.parse(fs.readFileSync(path.join(__dirname,'fixtures/configuration.json'),'utf8'));
 assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'src/config.cpp'))).digest('hex'),fixture.sourceSha256,'C++ validation changed; re-execute fixture');
 assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'include/rionetlite/config.h'))).digest('hex'),fixture.headerSha256);
 assert.equal(fixture.cases.length,16);
 for(const item of fixture.cases){const s=configuration.createInitialState(item.config);assert.equal(s.valid,item.valid,item.id);if(item.valid)assert.deepEqual(s.budgets,item.budgets,item.id);else assert.equal(s.field,item.field,item.id);}
});
test('L18 offers additional evidence rather than declaring a cause from one symptom',()=>{
 const m=models.diagnostics;let s=m.transition(m.createInitialState(),{type:'choose',choice:'inspect-port'}).state;assert.equal(s.decision,'추가 확인 필요');
 s=run(m,{},[{type:'inspect'},{type:'choose',choice:'inspect-port'}]);assert.equal(s.decision,'근거에 따라 다음 검사 선택');
 s=m.transition(s,{type:'choose',choice:'kill-process'}).state;assert.equal(s.danger,true);
});
test('L19 fails each registration index and releases only the acquired prefix in reverse order',()=>{
 const m=models.failureRollback;const resources=['session0.send','session0.recv','session1.send','session1.recv'];
 for(let failAt=1;failAt<=4;failAt++){let s=m.createInitialState({failAt});for(let n=0;n<failAt;n++)s=m.transition(s,{type:'next'}).state;assert.equal(s.failed,true);assert.deepEqual(s.acquired,[]);assert.deepEqual(s.released,resources.slice(0,failAt-1).reverse());}
 const s=run(m,{failAt:0},[...Array.from({length:4},()=>({type:'next'})),{type:'dispose'}]);assert.deepEqual(s.released,[...resources].reverse());
});
test('L20 excludes error and foreign-environment runs and computes labeled synthetic statistics',()=>{
 const m=models.benchmarking;let s=run(m,{},[0,2,3,1,4].map(run=>({type:'include',run})));assert.deepEqual(s.selected,[0,2,3]);assert.equal(s.excluded,1);
 const view=m.view(s);assert.equal(view.values.find(v=>v.label==='반복 처리량 중앙값').value,'1000 requests/s');assert.equal(view.values.find(v=>v.label==='합친 표본 P99 (nearest rank)').value,'12 ms');
 s=run(m,{unit:'us'},[0,2,3].map(run=>({type:'include',run})));assert.equal(m.view(s).values.find(v=>v.label==='합친 표본 P99 (nearest rank)').value,'12000 us');
 assert.equal(m.transition(s,{type:'include',run:0}).blocked,true);
});
