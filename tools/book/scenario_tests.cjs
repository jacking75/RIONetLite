'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const root=path.resolve(__dirname,'../../docs/interactive_book');
const manifest=JSON.parse(fs.readFileSync(path.join(root,'manifest.json'),'utf8'));
const context=vm.createContext({structuredClone,TextEncoder,TextDecoder});
for(const file of manifest.modelFiles)vm.runInContext(fs.readFileSync(path.join(root,file),'utf8'),context,{filename:file});
for(const definition of manifest.labs){
 test(`${definition.id}: published scenario, every prefix, views, bounded import and reset`,()=>{
  const model=context.BookModels[definition.modelKey];
  const config={...model.defaults,...definition.defaultConfig};
  const actions=definition.stepSequence??definition.defaultActions;
  const initial=model.createInitialState(config);let state=initial;
  const snapshot=JSON.stringify(initial);
  for(let i=0;i<actions.length;i++){
   const before=JSON.stringify(state);const transition=model.transition(state,actions[i]);
   assert.equal(JSON.stringify(state),before,'previous state is immutable');state=transition.state;
   assert.equal(model.validateState(state).length,0);
   assert.equal(JSON.stringify(context.BookModels.core.replay(model,config,actions.slice(0,i+1))),JSON.stringify(state));
   const view=model.view(state);assert.ok(Array.isArray(view.values)&&view.values.length);assert.ok(typeof view.message==='string');
   if(view.visual){assert.ok(context.BookFigures.kinds.includes(view.visual.kind));for(const visual of [view.visual,...(view.visual.extra??[])])assert.ok(context.BookFigures.render(visual.kind,visual,visual.step??0).svg);}
  }
  const serialized=model.serializeScenario(config,actions);const parsed=model.parseScenario(serialized);
  assert.equal(JSON.stringify(parsed.state),JSON.stringify(state));
  assert.equal(JSON.stringify(model.createInitialState(config)),snapshot,'reset restores initial state');
  assert.throws(()=>model.parseScenario(JSON.stringify({schemaVersion:999,labId:definition.id,config,actions})));
  assert.throws(()=>model.parseScenario(JSON.stringify({schemaVersion:1,labId:definition.id,config,actions:Array(129).fill(actions[0])})));
  assert.throws(()=>model.parseScenario(' '.repeat(32769)));
  assert.throws(()=>model.transition(initial,{type:'not-a-published-action'}));
 });
}
