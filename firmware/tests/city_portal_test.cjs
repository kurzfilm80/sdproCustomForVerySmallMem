const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

async function check(script, status, fails = false) {
  const elements = {
    city: {value: 'SEOUL', addEventListener: (_, handler) => { elements.city.change = handler; }},
    cityName: {}, latitude: {}, longitude: {}
  };
  const inputs = [elements.cityName, elements.latitude, elements.longitude];
  const custom = {hidden: false, querySelectorAll: () => inputs};
  const button = {}, state = {};
  const form = {elements, querySelector: () => button};
  const displayElements = {};
  for (const name of ['autoRotate', 'weatherPageSeconds', 'airPageSeconds', 'fixedPage',
      'brightness', 'nightModeEnabled', 'nightStart', 'nightEnd', 'nightBrightness'])
    displayElements[name] = {value: '', addEventListener: (_, handler) => { displayElements[name].change = handler; }};
  displayElements.autoRotate.value = 'true';
  const legacyOptions = [];
  displayElements.brightness.add = option => legacyOptions.push(option);
  const displayButton = {}, displayState = {};
  const displayForm = {elements: displayElements, querySelector: () => displayButton};
  status.displaySettings = {
    autoRotate: status.city !== 'CUSTOM', weatherPageSeconds: 15, airPageSeconds: 10,
    fixedPage: 'AIR QUALITY', brightness: status.city === 'CUSTOM' ? 75 : 80,
    nightModeEnabled: true, nightStart: '22:00', nightEnd: '07:00', nightBrightness: 30
  };
  status.brightness = 30;
  vm.runInNewContext(script, {
    document: {getElementById: id => ({'city-form': form, 'custom-city': custom, 'city-state': state, 'display-form': displayForm, 'display-state': displayState}[id]), createElement: () => ({})},
    fetch: async (url, options) => {
      assert.equal(url, '/api/v1/status');
      assert.equal(options.cache, 'no-store');
      return {ok: !fails, json: async () => status};
    }
  });
  assert.equal(button.disabled, true);
  assert.equal(displayButton.disabled, true);
  assert.equal(displayElements.fixedPage.disabled, true);
  assert.equal(custom.hidden, true);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(button.disabled, false);
  assert.equal(displayButton.disabled, false);
  if (!fails) {
    assert.equal(elements.city.value, status.city);
    assert.equal(elements.cityName.value, status.cityName);
    assert.equal(elements.latitude.value, status.latitude);
    assert.equal(elements.longitude.value, status.longitude);
    assert.equal(custom.hidden, status.city !== 'CUSTOM');
    for (const [name, value] of Object.entries(status.displaySettings))
      assert.equal(displayElements[name].value, String(value));
    assert.equal(displayElements.fixedPage.disabled, status.displaySettings.autoRotate);
    assert.equal(legacyOptions.length, status.displaySettings.brightness === 75 ? 1 : 0);
    assert.match(displayState.textContent, /30%/);
  } else assert.match(state.textContent, /Could not load/);
  displayElements.autoRotate.value = 'false';
  displayElements.autoRotate.change();
  assert.equal(displayElements.fixedPage.disabled, false);
  elements.city.value = 'CUSTOM';
  elements.city.change();
  assert.equal(custom.hidden, false);
  for (const input of inputs) {
    assert.equal(input.disabled, false);
    assert.equal(input.required, true);
  }
  elements.city.value = 'BUSAN';
  elements.city.change();
  assert.equal(custom.hidden, true);
  for (const input of inputs) assert.equal(input.disabled, true);
}
(async () => {
  for (const path of ['web/city.js', 'web/city.min.js']) {
    const script = fs.readFileSync(path, 'utf8');
    await check(script, {city: 'BUSAN', cityName: 'BUSAN', latitude: '35.1796', longitude: '129.0756'});
    await check(script, {city: 'CUSTOM', cityName: 'MY CITY', latitude: '-33.123456', longitude: '151.000001'});
    await check(script, {}, true);
  }
  console.log('PASS: saved preset/custom form, display controls, legacy brightness, conditional inputs, failed status and minified script');
})().catch(error => {console.error(error); process.exitCode = 1;});
