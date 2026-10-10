'use strict';
const el = id => document.getElementById(id);
const message = text => { el('message').textContent = text; };
async function request(path, options) {
  const response = await fetch(path, {cache:'no-store', ...options});
  if (!response.ok) throw new Error(`${response.status}: ${await response.text()}`);
  return response.json();
}
function addRow(position = {}) {
  if (el('rows').children.length >= 8) return message('Maximum 8 tickers.');
  const row = document.createElement('tr');
  for (const key of ['symbol', 'name', 'quantity', 'cost']) {
    const cell = row.insertCell(), input = document.createElement('input');
    input.dataset.key = key;
    if (key === 'symbol') { input.maxLength = 23; input.pattern = '[A-Za-z0-9.\\^=_\\-]+'; input.required = true; }
    else if (key === 'name') { input.maxLength = 23; input.placeholder = 'Symbol if blank'; }
    else { input.type = 'number'; input.min = '0'; input.max = '1000000000'; input.step = 'any'; input.required = true; }
    input.value = position[key] ?? (key === 'symbol' || key === 'name' ? '' : 0);
    input.setAttribute('aria-label', key);
    cell.append(input);
  }
  const remove = document.createElement('button'); remove.type = 'button'; remove.textContent = 'Remove';
  remove.onclick = () => row.remove(); row.insertCell().append(remove);
  el('rows').append(row);
}
function showQuotes(data) {
  el('quotes').replaceChildren();
  for (const quote of data.quotes) {
    const row = document.createElement('tr');
    const status = quote.valid ? (quote.error ? 'STALE' : `${quote.ageSeconds}s ago`) : 'Waiting / retrying';
    for (const value of [quote.symbol, quote.valid ? `${quote.price} ${quote.currency}` : '--', quote.valid && quote.hasChange !== false ? quote.percent.toFixed(2) : '--', `${status} (HTTP ${quote.httpCode})`]) row.insertCell().textContent = value;
    el('quotes').append(row);
  }
  el('heap').textContent = `Free heap: ${data.freeHeapBytes} bytes; largest block: ${data.maximumFreeBlockBytes} bytes`;
}
async function load() {
  try {
    const data = await request('/api/v1/tickers');
    el('rows').replaceChildren(); data.positions.forEach(addRow);
    el('rotate').value = data.rotateSeconds; el('refresh').value = data.refreshSeconds;
    el('range').value = data.graphRange ?? '1d';
    showQuotes(data); message('Settings loaded.');
  } catch (error) { message(error.message); }
}
el('add').onclick = () => addRow(); el('reload').onclick = load;
el('form').onsubmit = async event => {
  event.preventDefault();
  const positions = [...el('rows').children].map(row => {
    const inputs = row.querySelectorAll('input');
    return {symbol:inputs[0].value.trim().toUpperCase(), name:inputs[1].value.trim(), quantity:Number(inputs[2].value), cost:Number(inputs[3].value)};
  });
  const data = {schemaVersion:1, rotateSeconds:Number(el('rotate').value), refreshSeconds:Number(el('refresh').value), graphRange:el('range').value, positions};
  try {
    await request('/api/v1/tickers', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(data)});
    message('Saved. Settings survive firmware OTA and reboot.');
  } catch (error) { message(error.message); }
};
el('fetch').onclick = async () => {
  try { await request('/api/v1/tickers/refresh', {method:'POST'}); message('Refresh queued.'); }
  catch (error) { message(error.message); }
};
load();
setInterval(async () => {
  if (document.hidden) return;
  try { showQuotes(await request('/api/v1/tickers')); } catch (error) { message(error.message); }
}, 10000);
