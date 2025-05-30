let gateway = `ws://${window.location.hostname}:81/`;
let websocket;
const MAX_CURVE_POINTS_UI = 8;
let initialDataLoaded = false; 

window.addEventListener('load', onLoad);

function onLoad(event) { 
  initWebSocket(); 
  const mqttEnableCheckbox = document.getElementById('mqttEnable');
  if (mqttEnableCheckbox) {
    mqttEnableCheckbox.addEventListener('change', toggleMqttFields);
  }
  // No specific listener for discovery needed here if controlled with mqttFields
}

function initWebSocket() {
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen    = onOpen;
  websocket.onclose   = onClose;
  websocket.onmessage = onMessage;
}

function onOpen(event) { 
    console.log('Connection opened'); 
}
function onClose(event) { 
    console.log('Connection closed'); 
    if (initialDataLoaded) {
        document.getElementById('loadingOverlay').style.display = 'flex';
        const mainContent = document.querySelector('.main-content-wrapper');
        if(mainContent) mainContent.style.display = 'none';
        initialDataLoaded = false; 
    }
    setTimeout(initWebSocket, 5000); 
}

function onMessage(event) {
  let data;
  try {
    data = JSON.parse(event.data);
  } catch (e) { console.error("Error parsing JSON:", e, event.data); return; }

  if (!initialDataLoaded) {
      const loadingOverlay = document.getElementById('loadingOverlay');
      const mainContentWrapper = document.querySelector('.main-content-wrapper');
      if (loadingOverlay) loadingOverlay.style.display = 'none';
      if (mainContentWrapper) mainContentWrapper.style.display = 'block';
      initialDataLoaded = true;
  }

  if (data.serialDebugEnabled !== undefined) { 
      document.getElementById('debugModeNotice').classList.toggle('hidden', !data.serialDebugEnabled);
  }

  if (data.isWiFiEnabled === false) {
      console.log("ESP32 reports WiFi is disabled. Web UI may not function fully.");
      const mqttContainer = document.getElementById('mqttConfigContainer');
      if (mqttContainer) mqttContainer.classList.add('hidden');
      // Also hide OTA update section if WiFi is off
      const otaContainer = document.querySelector('.ota-update-container');
      if (otaContainer) otaContainer.classList.add('hidden');
  } else {
      const mqttContainer = document.getElementById('mqttConfigContainer');
      if (mqttContainer) mqttContainer.classList.remove('hidden');
      const otaContainer = document.querySelector('.ota-update-container');
      if (otaContainer) otaContainer.classList.remove('hidden');
  }


  if (data.tempSensorFound !== undefined) {
    const curveEditor = document.getElementById('curveEditorContainer');
    const autoModeNotice = document.getElementById('autoModeNotice');
    const tempDisplay = document.getElementById('temp');

    if (data.tempSensorFound) {
      tempDisplay.innerText = data.temperature !== undefined && data.temperature !== null && data.temperature > -990 ? data.temperature.toFixed(1) : 'N/A';
      if (curveEditor) curveEditor.classList.remove('hidden');
      if (autoModeNotice) autoModeNotice.innerText = '';
    } else {
      tempDisplay.innerText = 'N/A';
      if (curveEditor) curveEditor.classList.add('hidden');
       if (data.isAutoMode && autoModeNotice) { 
        autoModeNotice.innerText = '(Sensor N/A - Fixed Speed)';
      } else if (autoModeNotice) {
        autoModeNotice.innerText = '';
      }
    }
  } else if (data.temperature !== undefined) { 
     document.getElementById('temp').innerText = data.temperature !== null && data.temperature > -990 ? data.temperature.toFixed(1) : 'N/A';
  }

  if (data.firmwareVersion !== undefined) { 
    const fwVersionEl = document.getElementById('firmwareVersion');
    if (fwVersionEl) fwVersionEl.innerText = data.firmwareVersion;
  }

  if(data.fanSpeed !== undefined) document.getElementById('fanSpeed').innerText = data.fanSpeed;
  if(data.fanRpm !== undefined) document.getElementById('rpm').innerText = data.fanRpm;
  
  if(data.isAutoMode !== undefined) {
    const modeStr = data.isAutoMode ? "AUTO" : "MANUAL";
    document.getElementById('mode').innerText = modeStr;
    document.getElementById('manualControl').style.display = data.isAutoMode ? 'none' : 'block';
    const autoModeNotice = document.getElementById('autoModeNotice');
    if (autoModeNotice) { 
        if (data.isAutoMode && data.tempSensorFound === false) {
            autoModeNotice.innerText = '(Sensor N/A - Fixed Speed)';
        } else if (data.isAutoMode && data.tempSensorFound === true) {
             autoModeNotice.innerText = ''; 
        } else if (!data.isAutoMode) {
             autoModeNotice.innerText = ''; 
        }
    }
  }
  
  if(data.manualFanSpeed !== undefined && !data.isAutoMode) {
    document.getElementById('speedSlider').value = data.manualFanSpeed;
    document.getElementById('manualSpeedValue').innerText = data.manualFanSpeed;
  }

  if(data.fanCurve && Array.isArray(data.fanCurve) && data.tempSensorFound) { 
    displayFanCurve(data.fanCurve); 
  } else if (!data.tempSensorFound) {
    const curvePointsContainer = document.getElementById('fanCurvePointsContainer');
    if (curvePointsContainer) { 
        curvePointsContainer.innerHTML = '<p class="placeholder">Fan curve editor disabled: Temperature sensor not detected.</p>';
    }
  }

  if (data.isMqttEnabled !== undefined) {
    document.getElementById('mqttEnable').checked = data.isMqttEnabled;
    toggleMqttFields(); 
  }
  if (data.mqttServer !== undefined) document.getElementById('mqttServer').value = data.mqttServer;
  if (data.mqttPort !== undefined) document.getElementById('mqttPort').value = data.mqttPort;
  if (data.mqttUser !== undefined) document.getElementById('mqttUser').value = data.mqttUser;
  if (data.mqttBaseTopic !== undefined) document.getElementById('mqttBaseTopic').value = data.mqttBaseTopic;

  if (data.isMqttDiscoveryEnabled !== undefined) {
    document.getElementById('mqttDiscoveryEnable').checked = data.isMqttDiscoveryEnabled;
  }
  if (data.mqttDiscoveryPrefix !== undefined) {
    document.getElementById('mqttDiscoveryPrefix').value = data.mqttDiscoveryPrefix;
  }

  // Handle OTA Status Updates
  if (data.otaStatusMessage !== undefined) {
    const otaStatusEl = document.getElementById('otaStatusMessage');
    if (otaStatusEl) otaStatusEl.innerText = data.otaStatusMessage;
  }
  if (data.otaInProgress !== undefined) {
    const otaButton = document.getElementById('otaUpdateButton');
    if (otaButton) otaButton.disabled = data.otaInProgress;
    if (data.otaInProgress) {
        if (otaButton) otaButton.innerText = "Update in Progress...";
    } else {
        if (otaButton) otaButton.innerText = "Check for Updates & Install";
    }
  }
}

function sendCommand(commandPayload) {
  if (websocket && websocket.readyState === WebSocket.OPEN) {
    websocket.send(JSON.stringify(commandPayload));
  } else { console.log("WebSocket not open or WiFi disabled on ESP32."); }
}
function updateSliderValueDisplay(value) { document.getElementById('manualSpeedValue').innerText = value; }
function setManualSpeed(value) {
  updateSliderValueDisplay(value); 
  sendCommand({ action: 'setManualSpeed', value: parseInt(value) });
}
function displayFanCurve(curvePoints) {
  const container = document.getElementById('fanCurvePointsContainer');
  if (!container) return; 
  container.innerHTML = ''; 
  if (!curvePoints || curvePoints.length === 0) {
    container.innerHTML = '<p class="placeholder">No curve points. Add points.</p>'; return;
  }
  curvePoints.forEach((point, index) => {
    const div = document.createElement('div');
    div.classList.add('curve-point');
    div.innerHTML = `
      <label>Temp (&deg;C):</label> <input type="number" value="${point.temp}" min="0" max="120" id="temp_p_${index}">
      <label>Fan (%):</label> <input type="number" value="${point.pwmPercent}" min="0" max="100" id="pwm_p_${index}">
      <button onclick="removeCurvePointUI(${index})" class="danger" style="padding:5px 10px; font-size:0.8em;">X</button>`;
    container.appendChild(div);
  });
}
function addCurvePointUI() {
  const container = document.getElementById('fanCurvePointsContainer');
  if (!container) return; 
  const placeholder = container.querySelector('.placeholder');
  if (placeholder) placeholder.remove();
  const currentPoints = container.querySelectorAll('.curve-point').length;
  if (currentPoints >= MAX_CURVE_POINTS_UI) {
    alert(`Max ${MAX_CURVE_POINTS_UI} points.`); return;
  }
  const index = currentPoints; 
  const div = document.createElement('div');
  div.classList.add('curve-point');
  let lastTemp = 0, lastPwm = 0;
  if (index > 0) {
      try { 
        lastTemp = parseInt(document.getElementById(`temp_p_${index-1}`).value) || 0;
        lastPwm = parseInt(document.getElementById(`pwm_p_${index-1}`).value) || 0;
      } catch (e) { /* ignore */ }
  }
  const defaultNewTemp = Math.min(120, lastTemp + 10); 
  const defaultNewPwm = Math.min(100, lastPwm + 10);  
  div.innerHTML = `
    <label>Temp (&deg;C):</label> <input type="number" value="${defaultNewTemp}" min="0" max="120" id="temp_p_${index}">
    <label>Fan (%):</label> <input type="number" value="${defaultNewPwm}" min="0" max="100" id="pwm_p_${index}">
    <button onclick="removeCurvePointUI(${index})" class="danger" style="padding:5px 10px; font-size:0.8em;">X</button>`;
  container.appendChild(div);
}
function removeCurvePointUI(idx) {
    const container = document.getElementById('fanCurvePointsContainer');
    if (!container) return;
    const points = container.querySelectorAll('.curve-point');
    if (points[idx]) points[idx].remove();
    if (container.querySelectorAll('.curve-point').length === 0) {
        container.innerHTML = '<p class="placeholder">No curve points. Add points.</p>';
    }
}
function saveFanCurve() {
  const points = [];
  const pElements = document.querySelectorAll('#fanCurvePointsContainer .curve-point');
  let isValid = true, lastTemp = -100; 
  pElements.forEach((el, index) => {
    const tempIn = el.querySelector('input[type="number"][id^="temp_p_"]');
    const pwmIn = el.querySelector('input[type="number"][id^="pwm_p_"]');
    if (!tempIn || !pwmIn) { isValid = false; return; } 
    const temp = parseInt(tempIn.value);
    const pwmPercent = parseInt(pwmIn.value);
    if (isNaN(temp) || isNaN(pwmPercent) || temp < 0 || temp > 120 || pwmPercent < 0 || pwmPercent > 100 || (index > 0 && temp <= lastTemp)) {
      alert(`Invalid data at point ${index + 1}. Temps must be increasing and within valid ranges (Temp: 0-120, PWM: 0-100).`); 
      isValid = false; 
      return; 
    }
    lastTemp = temp; 
    points.push({ temp: temp, pwmPercent: pwmPercent });
  });

  if (!isValid) return; 

  if (points.length < 2) { 
    alert("Minimum 2 points required for a fan curve."); 
    return; 
  }
  if (points.length > MAX_CURVE_POINTS_UI) { 
    alert(`Maximum ${MAX_CURVE_POINTS_UI} points allowed for a fan curve.`); 
    return; 
  }
  sendCommand({ action: 'setCurve', curve: points });
  alert("Fan curve sent to device. It will be validated and saved by the ESP32.");
}

function toggleMqttFields() {
  const mqttEnableCheckbox = document.getElementById('mqttEnable');
  const mqttFieldsContainer = document.getElementById('mqttFieldsContainer');
  const mqttDiscoveryFieldsContainer = document.getElementById('mqttDiscoveryFieldsContainer'); 
  
  if (mqttEnableCheckbox && mqttFieldsContainer && mqttDiscoveryFieldsContainer) {
    const isEnabled = mqttEnableCheckbox.checked;
    mqttFieldsContainer.classList.toggle('hidden', !isEnabled);
    mqttDiscoveryFieldsContainer.classList.toggle('hidden', !isEnabled); 
  }
}

function saveMqttConfig() {
  const mqttConfig = {
    action: 'setMqttConfig',
    isMqttEnabled: document.getElementById('mqttEnable').checked,
    mqttServer: document.getElementById('mqttServer').value.trim(),
    mqttPort: parseInt(document.getElementById('mqttPort').value),
    mqttUser: document.getElementById('mqttUser').value.trim(),
    mqttPassword: document.getElementById('mqttPassword').value, 
    mqttBaseTopic: document.getElementById('mqttBaseTopic').value.trim()
  };

  if (mqttConfig.isMqttEnabled) {
    if (!mqttConfig.mqttServer) { alert("MQTT Broker Server/IP is required when MQTT is enabled."); return; }
    if (isNaN(mqttConfig.mqttPort) || mqttConfig.mqttPort < 1 || mqttConfig.mqttPort > 65535) { alert("Invalid MQTT Broker Port. Must be between 1 and 65535."); return; }
    if (!mqttConfig.mqttBaseTopic) { alert("MQTT Base Topic is required when MQTT is enabled."); return; }
  }
  
  document.getElementById('mqttPassword').value = ''; 

  sendCommand(mqttConfig);
  alert("MQTT configuration sent to device. A reboot might be required for changes to take full effect.");
  document.getElementById('mqttRebootNotice').classList.remove('hidden');
  setTimeout(() => {
    document.getElementById('mqttRebootNotice').classList.add('hidden');
  }, 7000); 
}

function saveMqttDiscoveryConfig() {
  const discoveryConfig = {
    action: 'setMqttDiscoveryConfig',
    isMqttDiscoveryEnabled: document.getElementById('mqttDiscoveryEnable').checked,
    mqttDiscoveryPrefix: document.getElementById('mqttDiscoveryPrefix').value.trim()
  };

  if (discoveryConfig.isMqttDiscoveryEnabled && !discoveryConfig.mqttDiscoveryPrefix) {
    alert("MQTT Discovery Prefix is required when Discovery is enabled.");
    return;
  }

  sendCommand(discoveryConfig);
  alert("MQTT Discovery configuration sent to device. A reboot might be required for changes to take full effect.");
  document.getElementById('mqttDiscoveryRebootNotice').classList.remove('hidden');
  setTimeout(() => {
    document.getElementById('mqttDiscoveryRebootNotice').classList.add('hidden');
  }, 7000);
}

function triggerOtaUpdate() {
  const otaButton = document.getElementById('otaUpdateButton');
  if (confirm("Are you sure you want to check for firmware updates from GitHub and install if a new version is found? The device will reboot.")) {
    if (otaButton) {
        otaButton.disabled = true;
        otaButton.innerText = "Checking...";
    }
    const otaStatusEl = document.getElementById('otaStatusMessage');
    if (otaStatusEl) otaStatusEl.innerText = "Requesting update check...";
    
    sendCommand({ action: 'triggerOtaUpdate' });
  }
}
