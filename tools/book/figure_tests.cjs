'use strict';
const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const figures=require('../../docs/interactive_book/models/figure_engine.js');
test('sequence lights only messages through the selected step; builder kind inventory agrees',()=>{
  const python=fs.readFileSync(path.join(__dirname,'../../docs/interactive_book/build_book.py'),'utf8');
  const expected=[...python.match(/FIGURE_KINDS = \(([^\n]+)\)/)[1].matchAll(/'([^']+)'/g)].map(m=>m[1]);
  assert.deepEqual(figures.kinds,expected);
  const spec={id:'F-test',lanes:[{id:'a',label:'<unsafe>'},{id:'b',label:'B'}],steps:[0,1,2].map(i=>({messages:[{from:'a',to:'b',label:String(i)}]}))};
  assert.deepEqual(figures.render('sequence',spec,1).activeIds,['F-test-0-0','F-test-1-0']);
  assert.ok(!figures.render('sequence',spec,1).svg.includes('<unsafe>'));
});
test('state rejects transitions outside the declared step range',()=>{
  assert.throws(()=>figures.render('state',{states:[{id:'a'},{id:'b'}],steps:[{active:'a'}],transitions:[{from:'a',to:'b',step:2}]},0),/outside range/);
});
test('ring distinguishes empty/full with equal pointers and lights wrap cells in both views',()=>{
  const spec={capacity:8,read:6,write:2,used:4,posted:0};
  const wrapped=figures.render('ring',spec);
  assert.deepEqual(wrapped.activeIds,['ring-0','ring-1','ring-6','ring-7']);
  assert.equal((wrapped.svg.match(/data-view="1"/g)??[]).length,8);
  assert.equal((wrapped.svg.match(/data-view="2"/g)??[]).length,8);
  const empty=figures.render('ring',{...spec,read:2,write:2,used:0});
  const full=figures.render('ring',{...spec,read:2,write:2,used:8});
  assert.equal(empty.activeIds.length,0);assert.equal(full.activeIds.length,8);
  assert.notEqual(empty.svg,full.svg);
});
