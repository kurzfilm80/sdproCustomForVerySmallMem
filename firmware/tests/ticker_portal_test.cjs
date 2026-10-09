const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
class Element {
  constructor() { this.children = []; this.dataset = {}; this.value = ''; }
  append(child) { child.parent = this; this.children.push(child); }
  insertCell() { const cell = new Element(); this.append(cell); return cell; }
  replaceChildren() { this.children = []; }
  remove() { this.parent.children = this.parent.children.filter(x => x !== this); }
  setAttribute() {}
  querySelectorAll() { return this.children.flatMap(c => c.children).filter(x => x.tag === 'input'); }
}
async function check(script) {
  const elements = Object.fromEntries(['rows','quotes','heap','message','form','add','reload','fetch','rotate','refresh','range'].map(x => [x, new Element()]));
  const calls = []; let failSave = false;
  const data = {schemaVersion:1, rotateSeconds:10, refreshSeconds:300, positions:[{symbol:'AAPL',quantity:2,cost:150}], quotes:[{symbol:'<script>',valid:true,error:true,httpCode:429,price:200,currency:'USD',percent:5,ageSeconds:100}], freeHeapBytes:35000, maximumFreeBlockBytes:30000};
  vm.runInNewContext(script, {
    document:{getElementById:id => elements[id], createElement:tag => Object.assign(new Element(), {tag}), hidden:false},
    setInterval:() => {},
    fetch:async (path, options) => {
      calls.push({path, options});
      if (options.method === 'POST' && path === '/api/v1/tickers') return {ok:!failSave,status:503,text:async ()=>'Storage unavailable',json:async()=>({saved:true})};
      return {ok:true,json:async()=>data};
    }
  });
  await new Promise(setImmediate);
  assert.equal(elements.rows.children.length,1);
  assert.equal(elements.range.value,'1d');
  assert.match(elements.quotes.children[0].children[3].textContent,/STALE/);
  assert.equal(elements.quotes.children[0].children[0].textContent,'<script>');
  for (let i=0;i<10;i++) elements.add.onclick();
  assert.equal(elements.rows.children.length,8);
  while(elements.rows.children.length>1) elements.rows.children.at(-1).children[4].children[0].onclick();
  const inputs=elements.rows.children[0].querySelectorAll();
  const symbolPattern = new RegExp(`^(?:${inputs[0].pattern})$`, 'v');
  for (const symbol of ['AAPL', 'BTC-USD', '^GSPC', 'EURUSD=X', '005930.KS']) assert(symbolPattern.test(symbol));
  assert(!symbolPattern.test('AAPL?x'));
  inputs[0].value='btc-usd';
  inputs[1].value='Bitcoin';
  elements.range.value='1y';
  await elements.form.onsubmit({preventDefault(){}});
  const post=calls.find(x=>x.path==='/api/v1/tickers' && x.options.method==='POST');
  const saved=JSON.parse(post.options.body); assert.equal(saved.positions[0].symbol,'BTC-USD'); assert.equal(saved.positions[0].quantity,2);
  assert.equal(saved.positions[0].name,'Bitcoin'); assert.equal(saved.positions[0].cost,150); assert.equal(saved.graphRange,'1y');
  assert.equal(saved.schemaVersion,1); assert.match(elements.message.textContent,/Saved/);
  failSave=true; await elements.form.onsubmit({preventDefault(){}}); assert.match(elements.message.textContent,/503/);
  await elements.fetch.onclick(); assert.equal(calls.at(-1).path,'/api/v1/tickers/refresh');
  await elements.reload.onclick(); assert.equal(elements.rows.children.length,1);
  data.graphRange='3mo'; data.positions[0].name='Apple';
  await elements.reload.onclick(); assert.equal(elements.range.value,'3mo');
  assert.equal(elements.rows.children[0].querySelectorAll()[1].value,'Apple');
  data.quotes[0].hasChange=false;
  await elements.reload.onclick(); assert.equal(elements.quotes.children[0].children[2].textContent,'--');
}
(async()=>{for(const file of ['ticker.js','ticker.min.js']) await check(fs.readFileSync('web/'+file,'utf8')); console.log('PASS: ticker portal load/add/remove/cap/save/refresh, stale status, text escaping, storage failure, minified equivalence');})().catch(e=>{console.error(e);process.exit(1);});
