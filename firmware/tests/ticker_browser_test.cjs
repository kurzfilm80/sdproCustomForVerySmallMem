// Test the production minified ticker portal against a local fake device.
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const fs = require('node:fs');
const http = require('node:http');
const assert = require('node:assert/strict');
(async()=>{
  let config={schemaVersion:1,rotateSeconds:10,refreshSeconds:300,positions:[{symbol:'AAPL',quantity:2,cost:150}]};
  let failSave=false, saves=0, refreshes=0;
  const html=fs.readFileSync('firmware/web/ticker.html','utf8').replace('<script src="ticker.min.js"></script>','<script>'+fs.readFileSync('firmware/web/ticker.min.js','utf8')+'</script>');
  const server=http.createServer((req,res)=>{
    if(req.url==='/api/v1/tickers' && req.method==='GET'){
      res.setHeader('Content-Type','application/json');
      return res.end(JSON.stringify({...config,freeHeapBytes:35000,maximumFreeBlockBytes:30000,quotes:[{symbol:'<script>',valid:true,error:true,httpCode:429,price:200,currency:'USD',percent:5,ageSeconds:100}]}));
    }
    if(req.url==='/api/v1/tickers/refresh' && req.method==='POST'){
      ++refreshes; res.setHeader('Content-Type','application/json'); return res.end('{"queued":true}');
    }
    if(req.url==='/api/v1/tickers' && req.method==='POST'){
      let body=''; req.on('data',part=>body+=part);req.on('end',()=>{
        if(failSave){res.writeHead(503);return res.end('Storage unavailable');}
        config=JSON.parse(body);++saves;res.setHeader('Content-Type','application/json');res.end('{"saved":true}');
      });return;
    }
    res.setHeader('Content-Type','text/html');res.end(html);
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  let browser;
  try{
    browser=await chromium.launch({headless:true, ...(process.env.CHROMIUM_EXECUTABLE ? {executablePath:process.env.CHROMIUM_EXECUTABLE} : {})});
    const page=await browser.newPage({viewport:{width:900,height:1000}});
    const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.getByText('Settings loaded.',{exact:true}).waitFor();
    assert.equal(await page.locator('#rows tr').count(),1);
    assert.match(await page.locator('#quotes').textContent(),/STALE/);
    assert.equal(await page.locator('#quotes script').count(),0);
    await page.getByRole('button',{name:'Add ticker',exact:true}).click();
    let row=page.locator('#rows tr').nth(1);
    await row.getByLabel('symbol',{exact:true}).fill('btc-usd');
    await row.getByLabel('quantity',{exact:true}).fill('0');
    await row.getByLabel('cost',{exact:true}).fill('0');
    await page.getByRole('button',{name:'Save settings',exact:true}).click();
    await page.getByText('Saved. Settings survive firmware OTA and reboot.',{exact:true}).waitFor();
    assert.equal(config.positions[1].symbol,'BTC-USD');assert.equal(saves,1);
    await row.getByLabel('symbol',{exact:true}).fill('AAPL?bad');
    assert.equal(await row.getByLabel('symbol',{exact:true}).evaluate(input=>input.checkValidity()),false);
    await page.getByRole('button',{name:'Save settings',exact:true}).click();assert.equal(saves,1);
    await row.getByLabel('symbol',{exact:true}).fill('^GSPC');
    assert.equal(await row.getByLabel('symbol',{exact:true}).evaluate(input=>input.checkValidity()),true);
    failSave=true;await page.getByRole('button',{name:'Save settings',exact:true}).click();
    await page.getByText('503: Storage unavailable',{exact:true}).waitFor();assert.equal(saves,1);
    failSave=false;await page.reload();await page.getByText('Settings loaded.',{exact:true}).waitFor();
    assert.equal(await page.locator('#rows tr').nth(1).getByLabel('symbol',{exact:true}).inputValue(),'BTC-USD');
    for(let i=0;i<8;++i)await page.getByRole('button',{name:'Add ticker',exact:true}).click();
    assert.equal(await page.locator('#rows tr').count(),8);
    await page.locator('#rows tr').last().getByRole('button',{name:'Remove',exact:true}).click();
    assert.equal(await page.locator('#rows tr').count(),7);
    await page.getByRole('button',{name:'Refresh quotes',exact:true}).click();
    await page.getByText('Refresh queued.',{exact:true}).waitFor();assert.equal(refreshes,1);
    await page.setViewportSize({width:375,height:850});
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'Mobile page overflows');
    assert.deepEqual(errors,[]);
    console.log('PASS: real Chromium ticker registration, persistence reload, browser validation, errors, cap/removal/refresh, escaping and mobile layout');
  }finally{if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(e=>{console.error(e);process.exitCode=1;});
