'use strict';
const {chromium}=require('playwright');
const {pathToFileURL}=require('node:url');
const path=require('node:path');
const fs=require('node:fs');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const book=path.join(root,'docs/interactive_book');
const output=path.join(root,'.artifacts/book/reader');
const manifest=JSON.parse(fs.readFileSync(path.join(book,'manifest.json'),'utf8'));
fs.mkdirSync(output,{recursive:true});
const standalone=path.join(output,'standalone.html');
fs.copyFileSync(path.join(book,'index.html'),standalone);
const base=pathToFileURL(standalone).href;
const value=async(lab,label)=>lab.locator('dt').filter({hasText:new RegExp('^'+label+'$')}).locator('..').locator('dd').innerText();
(async()=>{
 const browser=await chromium.launch({channel:'msedge',headless:true});
 const errors=[],requests=[];
 try{
  const context=await browser.newContext({offline:true,viewport:{width:1440,height:900}});
  context.on('request',r=>requests.push(r.url()));await context.route(/^https?:/,r=>r.abort());
  const page=await context.newPage();page.on('pageerror',e=>errors.push(String(e)));
  const go=async(slug,section='')=>{await page.goto(base+'#'+new URLSearchParams({chapter:slug,...(section?{section}:{})}));await page.waitForSelector(`article#${slug}.active`);};
  await go('reading-guide');
  const firstRender=await page.evaluate(()=>performance.now());
  assert.ok(firstRender<2000,`first render ${firstRender} ms exceeds 2 s target`);
  assert.equal(await page.locator('html').getAttribute('data-reader-ready'),'true');
  await go('receive-path');
  assert.equal(await page.locator('#navigation a[href*="prior-notes"]').count(),0);
  assert.equal(await page.locator('#receive-path [data-archive]').isVisible(),false);
  await page.fill('#search','원문 SHA-256');assert.equal(await page.locator('#searchResults button').count(),0);
  await page.click('#archiveToggle');assert.ok(await page.locator('#searchResults button').count()>0);
  await page.reload();assert.equal(await page.locator('#archiveToggle').getAttribute('aria-pressed'),'true');
  await page.click('#archiveToggle');await page.fill('#search','');
  await page.locator('#receive-path .archive-toggle').click();assert.equal(await page.locator('#receive-path [data-archive]').isVisible(),true);
  await page.locator('#receive-path .archive-toggle').click();
  await go('server-basics');const term=page.locator('#server-basics dfn[data-term]').first();
  await term.focus();await page.keyboard.press('Enter');assert.equal(await page.locator('#termPopover').isVisible(),true);
  await page.keyboard.press('Escape');assert.equal(await term.evaluate(n=>n===document.activeElement),true);
  await go('echo-overview');const figure=page.locator('[data-figure="F-P03-1"]');
  const figureStepMs=await figure.getByRole('button',{name:'다음',exact:true}).evaluate(b=>{const start=performance.now();b.click();b.click();b.click();return (performance.now()-start)/3;});
  assert.ok(figureStepMs<50);assert.match(await figure.locator('[role="status"]').innerText(),/다음 수신을 게시하고 콜백을 부른다/);
  await figure.getByRole('button',{name:'재생',exact:true}).click();assert.equal(await figure.getByRole('button',{name:'정지',exact:true}).count(),1);
  await go('reading-guide');assert.equal(await page.evaluate(()=>playbackSessions.size),0);
  await go('echo-walkthrough');const walk=page.locator('[data-walkthrough="W02"]');
  await walk.getByRole('button',{name:'다음',exact:true}).click();await walk.getByRole('button',{name:'다음',exact:true}).click();
  assert.equal(await walk.locator('.focusline').first().getAttribute('data-line'),String(await page.evaluate(()=>BOOK.walkthroughs.find(w=>w.id==='W02').steps[2].startLine)));
  await go('echo-overview');const caller=page.locator('[data-walkthrough="W01"]');
  await caller.getByRole('button',{name:'Start 뒤 콜백으로 넘어간다',exact:true}).click();await caller.getByRole('button',{name:'들어가 보기',exact:true}).click();
  await page.waitForSelector('#echo-walkthrough.active');await walk.getByRole('button',{name:'돌아가기',exact:true}).click();await page.waitForSelector('#echo-overview.active');
  assert.match(await caller.locator('[role="status"]').innerText(),/5\/7/);
  await go('library-overview','overview-inside');const systemMap=page.locator('#library-overview [data-map]:not([data-mini])');
  await systemMap.locator('[data-map-node="io-worker"]').focus();await page.keyboard.press('Enter');
  assert.ok(await systemMap.locator('.map-panel a').filter({hasText:'C12'}).count()>0);
  await go('receive-path');await page.locator('#receive-path [data-mini] button').click();await page.waitForSelector('#library-overview.active');
  assert.match(await systemMap.locator('.map-panel h3').innerText(),/I\/O 워커/);
  await go('reading-guide');await page.locator('[data-route="first"]').click();await page.reload();assert.equal(await page.locator('#routeSelect').inputValue(),'first');
  await go('packet-framing');const routeCheck=page.locator('#packet-framing [data-check="C13-understood"]');await routeCheck.uncheck();
  const routeBefore=await page.evaluate(()=>routeProgress(selectedRoute()));await routeCheck.check();
  assert.equal(await page.evaluate(()=>routeProgress(selectedRoute())),routeBefore+1);assert.match(await page.locator('#routeNext').innerText(),/C03/);
  const quiz=page.locator('[data-quiz="Q-C13-1"]');await quiz.locator('input').first().check();await quiz.getByRole('button',{name:'확인',exact:true}).click();
  assert.match(await quiz.locator('[role="status"]').innerText(),/정답|다시 생각/);await page.reload();
  assert.equal(await quiz.locator('input').first().isChecked(),true);assert.equal(await quiz.locator('.quiz-feedback').isVisible(),true);
  await quiz.scrollIntoViewIfNeeded();await page.screenshot({path:path.join(output,'quiz-result.png')});
  await page.goto(base+'#p4');await page.waitForSelector('#packet-framing.active');assert.match(page.url(),/chapter=packet-framing/);
  let lab=page.locator('[data-lab="L09"]');
  for(const expected of ['0','0','7']){await lab.getByRole('button',{name:'다음 수신 전달',exact:true}).click();assert.equal(await value(lab,'이번 소비 바이트'),expected);}
  await lab.getByRole('button',{name:'이전 단계',exact:true}).click();assert.equal(await value(lab,'누적 소비 바이트'),'0');
  await lab.getByRole('button',{name:'다시 다음 단계',exact:true}).click();assert.equal(await value(lab,'누적 소비 바이트'),'7');
  await lab.locator('details').first().locator('summary').click();
  for(const [mode,count,error]of [['coalesced','2',null],['empty','1',null],['invalid','0','kInvalidLength'],['large','0','kMessageTooLarge']]){
   await lab.locator('select').selectOption(mode);await lab.getByRole('button',{name:'이 조건으로 처음부터',exact:true}).click();await lab.getByRole('button',{name:'다음 수신 전달',exact:true}).click();assert.equal(await value(lab,'완성된 패킷'),count);if(error)assert.match(await lab.locator('.lab-output').innerText(),new RegExp(error));
  }
  await lab.getByRole('button',{name:/실제 코드/}).first().click();
  assert.equal(await page.locator('#sourceDialog').evaluate(d=>d.open),true);assert.equal(await page.locator('#sourceCode .focusline').count(),1);
  await page.locator('#sourceScope').selectOption('function');assert.match(await page.locator('#sourceCode').innerText(),/DecodeResult Decode/);
  await page.locator('#sourceScope').selectOption('file');assert.match(await page.locator('#sourceMeta').innerText(),/SHA-256/);await page.keyboard.press('Escape');
  assert.match(await page.evaluate(()=>document.activeElement.textContent),/실제 코드/);
  await lab.getByRole('button',{name:/실제 코드/}).first().click();
  const originalSource=await page.locator('#sourceTitle').innerText();
  if(await page.locator('#sourceRelated button').count()){
   await page.locator('#sourceRelated button').first().click();assert.equal(await page.locator('#sourceBack').isDisabled(),false);
   await page.locator('#sourceBack').click();assert.equal(await page.locator('#sourceTitle').innerText(),originalSource);
  }
  await page.keyboard.press('Escape');
  await go('partial-send');lab=page.locator('[data-lab="L11"]');
  await lab.getByRole('button',{name:/실제 코드/}).first().click();
  const partialSource=await page.locator('#sourceTitle').innerText();assert.ok(await page.locator('#sourceRelated button').count()>0);
  await page.locator('#sourceRelated button').first().click();assert.equal(await page.locator('#sourceBack').isDisabled(),false);
  await page.locator('#sourceBack').click();assert.equal(await page.locator('#sourceTitle').innerText(),partialSource);await page.keyboard.press('Escape');
  await lab.getByRole('button',{name:'2바이트 완료',exact:true}).click();assert.equal(await value(lab,'링 읽기 위치'),'2');assert.equal(await value(lab,'게시된 길이'),'2');assert.equal(await value(lab,'미완료 송신'),'1');
  await lab.getByRole('button',{name:'2바이트 완료',exact:true}).click();assert.equal(await value(lab,'미완료 송신'),'0');
  await lab.getByRole('button',{name:/실험 JSON 내보내기/}).click();const exported=await page.locator('#scenarioText').inputValue();
  const parsed=JSON.parse(exported);assert.equal(parsed.actions.length,2);assert.equal(parsed.schemaVersion,1);
  await page.fill('#scenarioText',JSON.stringify({...parsed,schemaVersion:99}));await page.click('#scenarioImport');assert.match(await page.locator('#scenarioStatus').innerText(),/가져오지 못했다/);
  await page.fill('#scenarioText',JSON.stringify({...parsed,config:{generation:'<img src=x onerror="window.injected=true">'}}));await page.click('#scenarioImport');assert.match(await page.locator('#scenarioStatus').innerText(),/가져오지 못했다/);assert.equal(await page.evaluate(()=>window.injected),undefined);
  await page.fill('#scenarioText',exported);await page.click('#scenarioImport');assert.match(await page.locator('#scenarioStatus').innerText(),/재실행했다/);await page.keyboard.press('Escape');
  await lab.getByRole('button',{name:'이 실험 초기화',exact:true}).click();assert.equal(await value(lab,'미완료 송신'),'1');
  await lab.getByRole('button',{name:'부분 완료 뒤 재게시 실패',exact:true}).click();assert.equal(await value(lab,'미완료 송신'),'0');assert.match(await lab.locator('.lab-output').innerText(),/kSEND_IO_FAIL/);
  await go('packet-framing');assert.equal(await value(page.locator('[data-lab="L09"]'),'길이 오류'),'kMessageTooLarge');
  await page.goBack();await page.waitForSelector('#partial-send.active');assert.equal(await value(page.locator('[data-lab="L11"]'),'오류'),'kSEND_IO_FAIL');
  await page.keyboard.press('/');assert.equal(await page.locator('#search').evaluate(n=>n===document.activeElement),true);
  await page.fill('#search','generation');assert.ok(await page.locator('#searchResults button').count()>0);await page.locator('#searchResults button').first().click();
  await page.fill('#search','<img src=x onerror=alert(1)>');assert.match(await page.locator('#searchResults').innerText(),/검색 결과 0개/);assert.equal(await page.locator('#searchResults img').count(),0);await page.fill('#search','');
  await go('packet-framing');const check=page.locator('#packet-framing [data-check]').first();if(await check.count()){await check.check();await page.reload();assert.equal(await page.locator('#packet-framing [data-check]').first().isChecked(),true);}
  await page.goto(base+'#chapter=unknown&section=bad');await page.waitForSelector('#reading-guide.active');assert.match(await page.locator('#notice').innerText(),/찾을 수 없어/);
  await page.goto(base+'#chapter=packet-framing&section=bad&lab=L09&scenario=%7Bbad');await page.waitForSelector('#packet-framing.active');assert.match(await page.locator('#notice').innerText(),/자료를 읽지 못했다/);
  const rendered=manifest.chapters.filter(c=>c.file);
  const labReceipts=[];
  const walkReceipts=[];
  const walks=await page.evaluate(()=>BOOK.walkthroughs);
  for(const definition of walks){
   const chapter=manifest.chapters.find(c=>c.id===(definition.chapter??(definition.id==='W01'?'P03':'C04')));await go(chapter.slug);
   const host=page.locator(`[data-walkthrough="${definition.id}"]`);
   for(let i=0;i<definition.steps.length;i++){
    await host.locator('button[data-walk-step]').nth(i).click();const step=definition.steps[i];
    assert.equal(await host.locator('.focusline').first().getAttribute('data-line'),String(step.startLine));
    assert.equal(await host.locator('.focusline').count(),step.endLine-step.startLine+1);
    assert.ok((await host.locator('[role="status"]').innerText()).includes(step.title));
   }
   walkReceipts.push({id:definition.id,steps:definition.steps.length,focusLinesChecked:true});
   if(definition.id==='W05'){await host.scrollIntoViewIfNeeded();await page.screenshot({path:path.join(output,'c12-walkthrough.png')});}
  }
  for(const definition of manifest.labs){
   const chapter=manifest.chapters.find(c=>c.id===definition.chapter);await go(chapter.slug);
   const host=page.locator(`[data-lab="${definition.id}"]`);
   await host.getByRole('button',{name:'이 실험 초기화',exact:true}).click();
   const step=await host.locator('button[data-action]').first().evaluate(button=>{const start=performance.now();button.click();return performance.now()-start;});
   assert.ok(step<100,`${definition.id}: step ${step}ms exceeds 100ms`);
   await host.getByRole('button',{name:'이 실험 초기화',exact:true}).click();
   await host.getByRole('button',{name:/실험 JSON 내보내기/}).click();
   const scenario=JSON.parse(await page.locator('#scenarioText').inputValue());scenario.actions=definition.stepSequence??definition.defaultActions;
   await page.fill('#scenarioText',JSON.stringify(scenario));await page.click('#scenarioImport');
   assert.match(await page.locator('#scenarioStatus').innerText(),/재실행했다/,definition.id);await page.keyboard.press('Escape');
   assert.equal(await page.evaluate(id=>{const s=labSessions.get(id);return JSON.stringify(s.state)===JSON.stringify(BookModels.core.replay(s.model,s.config,s.actions));},definition.id),true,definition.id);
   await host.getByRole('button',{name:'이전 단계',exact:true}).click();await host.getByRole('button',{name:'다시 다음 단계',exact:true}).click();
   assert.equal(await page.evaluate(id=>{const s=labSessions.get(id);return s.cursor===s.actions.length&&s.model.validateState(s.state).length===0;},definition.id),true);
   assert.ok(await host.locator('svg[role="group"]').count()>0,definition.id+' state diagram');
   await host.getByRole('button',{name:'단계별 재생',exact:true}).click();assert.equal(await host.getByRole('button',{name:'재생 멈추기',exact:true}).count(),1);
   await host.getByRole('button',{name:'재생 멈추기',exact:true}).click();
   await host.getByRole('button',{name:'이 실험 초기화',exact:true}).click();
   labReceipts.push({id:definition.id,stepMs:step,actions:scenario.actions.length,jsonReplay:true,undoRedo:true,playStop:true});
  }
  await go('packet-framing');
  const searchMs=await page.locator('#search').evaluate(input=>{input.value='generation';const start=performance.now();input.dispatchEvent(new Event('input',{bubbles:true}));return performance.now()-start;});
  assert.ok(searchMs<100,`search ${searchMs}ms exceeds 100ms`);await page.fill('#search','');
  for(const width of [1440,1920,390]){await page.setViewportSize({width,height:width===390?844:900});for(const c of rendered){await go(c.slug);assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,`overflow ${c.slug} at ${width}`);}}
  await page.setViewportSize({width:390,height:844});await page.click('#menu');assert.equal(await page.locator('#menu').getAttribute('aria-expanded'),'true');await page.locator('#navigation a').first().focus();await page.keyboard.press('Enter');await page.waitForFunction(()=>document.getElementById('menu').getAttribute('aria-expanded')==='false');
  await page.emulateMedia({reducedMotion:'reduce'});await go('packet-framing');
  await page.setViewportSize({width:1440,height:900});
  for(const [slug,name]of [['reading-guide','cover'],['server-basics','p01'],['library-overview','p02'],['echo-overview','p03'],['build-and-run','c02'],['echo-walkthrough','c04'],['packet-framing','c13'],['echo-walkthrough','echo'],['packet-framing','framing'],['partial-send','partial'],['server-shutdown','shutdown'],['ring-buffer','ring'],['accept-and-session','slots']]){await go(slug);await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>{scrollTo(0,0);requestAnimationFrame(resolve);})));await page.screenshot({path:path.join(output,name+'.png'),fullPage:true});const labs=page.locator('article.active .lab');if(await labs.count())await labs.first().scrollIntoViewIfNeeded();await page.screenshot({path:path.join(output,name+'-viewport.png')});}
  await go('packet-framing');await page.evaluate(()=>scrollTo(0,0));await page.click('#theme');await page.screenshot({path:path.join(output,'dark.png'),fullPage:true});await page.screenshot({path:path.join(output,'dark-viewport.png')});await page.click('#theme');await page.setViewportSize({width:390,height:844});await page.evaluate(()=>scrollTo(0,0));await page.screenshot({path:path.join(output,'mobile.png'),fullPage:true});await page.screenshot({path:path.join(output,'mobile-viewport.png')});
  await page.evaluate(()=>window.dispatchEvent(new Event('beforeprint')));await page.emulateMedia({media:'print'});
  assert.equal(await page.locator('#printBook article:visible').count(),rendered.length);assert.equal(await page.locator('#printBook .lab .controls:visible').count(),0);
  const printColor=await page.locator('#printBook').evaluate(n=>getComputedStyle(n).getPropertyValue('--ink').trim());assert.equal(printColor,'#172c32');
  assert.equal(await page.locator('#printBook').evaluate(root=>[...root.querySelectorAll('[aria-labelledby]')].every(n=>n.getAttribute('aria-labelledby').split(' ').every(id=>document.getElementById(id)))),true,'print SVG labels remain linked');
  await page.pdf({path:path.join(output,'book.pdf'),preferCSSPageSize:true,printBackground:true});
  assert.ok(fs.statSync(path.join(output,'book.pdf')).size>10000);
  const printBytes=fs.statSync(path.join(output,'book.pdf')).size;
  assert.equal(await page.locator('#printBook [data-archive]').count(),0);
  assert.ok(await page.locator('#printBook [data-figure] ol li').count()>0);
  await page.emulateMedia({media:'screen'});
  await page.click('#archiveToggle');await page.evaluate(()=>window.dispatchEvent(new Event('beforeprint')));await page.emulateMedia({media:'print'});
  await page.pdf({path:path.join(output,'book-with-archives.pdf'),preferCSSPageSize:true,printBackground:true});
  const archivePrintBytes=fs.statSync(path.join(output,'book-with-archives.pdf')).size;assert.ok(archivePrintBytes>printBytes);
  await page.emulateMedia({media:'screen'});await page.click('#archiveToggle');
  const noStorage=await browser.newContext({offline:true});await noStorage.addInitScript(()=>Object.defineProperty(window,'localStorage',{get(){throw new Error('blocked');}}));const blocked=await noStorage.newPage();blocked.on('pageerror',e=>errors.push(String(e)));await blocked.goto(base+'#chapter=packet-framing');assert.match(await blocked.locator('#storageStatus').innerText(),/저장할 수 없다/);await blocked.locator('[data-lab="L09"]').getByRole('button',{name:'다음 수신 전달',exact:true}).click();await noStorage.close();
  assert.deepEqual(errors,[]);assert.equal(requests.filter(u=>!u.startsWith('file:')).length,0);
  const receipt={scope:manifest.chapters.every(c=>c.status==='complete')?'complete book browser acceptance':'reader acceptance; manuscript acceptance pending',browser:browser.version(),offline:true,chapters:rendered.length,labs:labReceipts,viewports:[1440,1920,390],firstRenderMs:firstRender,searchMs,errors,externalRequests:requests.filter(u=>!u.startsWith('file:')),checks:['canonical/legacy URL','back/forward','search and aliases','source scopes, history and focus','all 20 models: JSON replay, undo/redo, play/stop','bounded scenario JSON','persistence','storage blocked','keyboard menu','reduced motion','all-chapter print visibility and PDF','representative screenshots']};
  receipt.printBytes=printBytes;receipt.archivePrintBytes=archivePrintBytes;receipt.checks.push('archive toggle, search, persistence and smaller default PDF');
  receipt.figureStepMs=figureStepMs;receipt.checks.push('term keyboard focus return','figure step, playback cleanup and print steps');
  receipt.walkthroughs=walkReceipts;receipt.checks.push('all walkthrough steps and highlighted source lines');
  receipt.checks.push('system map keyboard selection and mini-map route');
  receipt.checks.push('reading route selection, persistence and progress');
  receipt.checks.push('quiz submission, explanation, persistence and print answers');
  fs.writeFileSync(path.join(output,'receipt.json'),JSON.stringify(receipt,null,2));console.log(JSON.stringify(receipt,null,2));
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
