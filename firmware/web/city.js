(() => {
  const form = document.getElementById('city-form');
  const selector = form.elements.city;
  const custom = document.getElementById('custom-city');
  const button = form.querySelector('button');
  const state = document.getElementById('city-state');
  const toggle = () => {
    custom.hidden = selector.value !== 'CUSTOM';
    for (const input of custom.querySelectorAll('input')) {
      input.disabled = custom.hidden;
      input.required = !custom.hidden;
    }
  };
  selector.addEventListener('change', toggle);
  toggle();
  const displayForm = document.getElementById('display-form');
  const displayState = document.getElementById('display-state');
  const displayButton = displayForm && displayForm.querySelector('button');
  const toggleFixed = () => {
    if (displayForm) displayForm.elements.fixedPage.disabled = displayForm.elements.autoRotate.value === 'true';
  };
  if (displayForm) {
    displayForm.elements.autoRotate.addEventListener('change', toggleFixed);
    toggleFixed();
    displayButton.disabled = true;
  }
  button.disabled = true;
  fetch('/api/v1/status', {cache: 'no-store'})
    .then(response => {
      if (!response.ok) throw Error('Status unavailable');
      return response.json();
    })
    .then(status => {
      selector.value = status.city;
      form.elements.cityName.value = status.cityName;
      form.elements.latitude.value = status.latitude;
      form.elements.longitude.value = status.longitude;
      state.textContent = `Current: ${status.cityName} (${status.latitude}, ${status.longitude})`;
      toggle();
      if (displayForm) {
        const settings = status.displaySettings;
        const brightness = displayForm.elements.brightness;
        if (![20, 40, 60, 80, 100].includes(settings.brightness)) {
          const option = document.createElement('option');
          option.value = String(settings.brightness);
          option.textContent = `${settings.brightness}% (saved)`;
          brightness.add(option);
        }
        for (const name of ['autoRotate', 'weatherPageSeconds', 'airPageSeconds', 'fixedPage',
                            'brightness', 'nightModeEnabled', 'nightStart', 'nightEnd', 'nightBrightness']) {
          displayForm.elements[name].value = String(settings[name]);
        }
        toggleFixed();
        displayState.textContent = `Applied brightness: ${status.brightness}%`;
      }
    })
    .catch(() => {
      state.textContent = 'Could not load saved city. Select a city before saving.';
      if (displayState) displayState.textContent = 'Could not load saved display settings. Check all values before saving.';
    })
    .finally(() => { button.disabled = false; if (displayButton) displayButton.disabled = false; });
})();
