// Usage: node verify_book.cjs <path-to-playwright-core> <screenshot-directory>
const { chromium } = require(process.argv[2] || 'playwright');
const { pathToFileURL } = require('node:url');
const path = require('node:path');
const fs = require('node:fs');
const assert = require('node:assert/strict');
(async () => {
  const browser = await chromium.launch({channel:'msedge',headless:true});
  try {
    const page = await browser.newPage({viewport:{width:1440,height:1000}});
    const errors=[];
    page.on('pageerror',e=>errors.push(String(e)));
    const url=pathToFileURL(path.join(__dirname,'index.html')).href;
    await page.goto(url);
    const go=async n=>{await page.evaluate(n=>location.hash='p'+n,n);await page.waitForSelector('#p'+n+'.active');};
    const text=id=>page.locator('#'+id).innerText();
    const click=id=>page.locator('#'+id).click();
    await go(4);
    await click('packetNext');assert.equal(await text('packetConsumed'),'0');
    await click('packetNext');assert.equal(await text('packetConsumed'),'0');
    await click('packetNext');assert.equal(await text('packetConsumed'),'7');assert.equal(await text('packetCount'),'1');
    await page.selectOption('#packetMode','coalesced');await click('packetNext');assert.equal(await text('packetCount'),'2');assert.equal(await text('packetConsumed'),'12');
    await page.selectOption('#packetMode','invalid');await click('packetNext');assert.match(await text('packetStatus'),/kInvalidLength/);
    await page.selectOption('#packetMode','large');await click('packetNext');assert.match(await text('packetStatus'),/kMessageTooLarge/);
    await go(5);await click('sendFree');assert.match(await text('sendStatus'),/차단/);await click('sendPartial');assert.equal(await text('sendOffset'),'2');assert.equal(await text('sendLength'),'2');assert.equal(await text('sendOutstanding'),'1');await click('sendAll');assert.equal(await text('sendOutstanding'),'0');await click('sendFree');assert.match(await text('sendStatus'),/해제 가능/);
    await go(6);await click('sessionReuse');await click('sessionDeliver');assert.equal(await text('sessionDecision'),'거절');await page.uncheck('#sessionGuard');await click('sessionDeliver');assert.match(await text('sessionStatus'),/잘못된 통과/);
    await go(8);for(let i=0;i<5;i++)await click('queueTick');assert.equal(await text('queueUsed'),'8 / 8');assert.equal(await text('queueRejected'),'3');assert.equal(await text('queueProcessed'),'4');
    await go(7);await page.locator('[data-answer="correct"]').click();assert.match(await text('shutdownAnswer'),/맞다/);
    await go(3);await click('traceNext');assert.match(await text('traceLabel'),/02/);await click('traceSource');assert.equal(await page.locator('#sourceDialog').evaluate(d=>d.open),true);assert.equal(await page.locator('#sourceCode .focusline').count(),1);await page.keyboard.press('Escape');
    await page.evaluate(()=>document.querySelector('#traceSpeed').value='1100');await click('tracePlay');await page.waitForTimeout(1250);assert.match(await text('traceLabel'),/03/);await click('tracePlay');
    const sourceLinks=await page.locator('[data-file]').evaluateAll(buttons=>buttons.map(b=>({file:b.dataset.file,find:b.dataset.find})));
    for(const link of sourceLinks){assert.equal(await page.evaluate(({file,find})=>!!SOURCES[file]&&SOURCES[file].text.includes(find),link),true,JSON.stringify(link));}
    await go(10);await page.check('[data-check="0"]');await page.reload();assert.equal(await page.locator('[data-check="0"]').isChecked(),true);
    await page.fill('#search','部分不存在');assert.equal(await page.locator('.toc a:visible').count(),0);await page.fill('#search','generation');assert.ok(await page.locator('.toc a:visible').count()>0);await page.fill('#search','');
    for(const width of [1440,390]){await page.setViewportSize({width,height:1000});for(let n=1;n<=10;n++){await go(n);assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,`horizontal overflow page ${n} width ${width}`);}}
    if(process.argv[3]){fs.mkdirSync(process.argv[3],{recursive:true});await page.setViewportSize({width:1440,height:1000});await go(1);await page.screenshot({path:path.join(process.argv[3],'cover.png'),fullPage:true});await go(3);await page.screenshot({path:path.join(process.argv[3],'trace.png'),fullPage:true});await go(4);await page.selectOption('#packetMode','split');await click('packetNext');await click('packetNext');await page.screenshot({path:path.join(process.argv[3],'packet.png'),fullPage:true});await click('theme');await page.screenshot({path:path.join(process.argv[3],'dark.png'),fullPage:true});await click('theme');await page.setViewportSize({width:390,height:844});await page.screenshot({path:path.join(process.argv[3],'mobile.png'),fullPage:true});}
    await page.emulateMedia({media:'print'});assert.equal(await page.locator('.page:visible').count(),10);
    assert.deepEqual(errors,[]);
    console.log('PASS: 5 experiments, quiz, playback, source references, persistence, search, 20 viewport/page checks, print visibility, no browser errors.');
  } finally { await browser.close(); }
})().catch(e=>{console.error(e);process.exitCode=1;});
