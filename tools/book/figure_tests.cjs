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
