var gateway = `ws://${window.location.hostname}:81/`;
var websocket;
const MAX_CURVE_POINTS_UI = 8;
let initialDataLoaded = false; // Flag to track first data load

window.addEventListener('load', onLoad);

function onLoad(event) { 
  // Main content is hidden by CSS initially via .hidden-initially
  initWebSocket(); 
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
    // If you want to send an initial message to ESP32 upon connection:
    // websocket.send("Client Hello");
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
      if (mainContentWrapper) mainContentWrapper.style.display = 'block'; // Or 'flex' etc. depending on your layout
      initialDataLoaded = true;
  }

  if (data.serialDebugEnabled !== undefined) { 
      document.getElementById('debugModeNotice').classList.toggle('hidden', !data.serialDebugEnabled);
  }

  if (data.isWiFiEnabled === false) {
      console.log("ESP32 reports WiFi is disabled. Web UI may not function fully.");
      // You might want to disable UI elements or show a more prominent notice if WiFi is off
  }

  if (data.tempSensorFound !== undefined) {
    const curveEditor = document.getElementById('curveEditorContainer');
    const autoModeNotice = document.getElementById('autoModeNotice');
    const tempDisplay = document.getElementById('temp');

    if (data.tempSensorFound) {
      tempDisplay.innerText = data.temperature !== undefined && data.temperature > -990 ? data.temperature.toFixed(1) : 'N/A';
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
     document.getElementById('temp').innerText = data.temperature > -990 ? data.temperature.toFixed(1) : 'N/A';
  }


  if(data.fanSpeed !== undefined) document.getElementById('fanSpeed').innerText = data.fanSpeed;
  if(data.fanRpm !== undefined) document.getElementById('rpm').innerText = data.fanRpm;
  
  if(data.isAutoMode !== undefined) {
    const modeStr = data.isAutoMode ? "AUTO" : "MANUAL";
    document.getElementById('mode').innerText = modeStr;
    document.getElementById('manualControl').style.display = data.isAutoMode ? 'none' : 'block';
    const autoModeNotice = document.getElementById('autoModeNotice');
    if (autoModeNotice) { // Check if element exists
        if (data.isAutoMode && data.tempSensorFound === false) {
            autoModeNotice.innerText = '(Sensor N/A - Fixed Speed)';
        } else if (data.isAutoMode && data.tempSensorFound === true) {
             autoModeNotice.innerText = ''; 
        } else if (!data.isAutoMode) {
             autoModeNotice.innerText = ''; // Clear if manual mode
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
    if (curvePointsContainer) { // Check if element exists
        curvePointsContainer.innerHTML = '<p class="placeholder">Fan curve editor disabled: Temperature sensor not detected.</p>';
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
  if (!container) return; // Guard if element not found
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
}
function saveFanCurve() {
  const points = [];
  const pElements = document.querySelectorAll('#fanCurvePointsContainer .curve-point');
  let isValid = true, lastTemp = -100; 
  pElements.forEach((el, index) => {
    const tempIn = el.querySelector('input[type="number"][id^="temp_p_"]');
    const pwmIn = el.querySelector('input[type="number"][id^="pwm_p_"]');
    if (!tempIn || !pwmIn) { isValid = false; return; }
    const temp = parseInt(tempIn.value), pwmPercent = parseInt(pwmIn.value);
    if (isNaN(temp) || isNaN(pwmPercent) || temp < 0 || temp > 120 || pwmPercent < 0 || pwmPercent > 100 || (index > 0 && temp <= lastTemp)) {
      alert(`Invalid data at point ${index + 1}. Temps must increase.`); isValid = false; return;
    }
    lastTemp = temp; points.push({ temp: temp, pwmPercent: pwmPercent });
  });
  if (!isValid) return;
  if (points.length < 2) { alert("Min 2 points."); return; }
  if (points.length > MAX_CURVE_POINTS_UI) { alert(`Max ${MAX_CURVE_POINTS_UI} points.`); return; }
  sendCommand({ action: 'setCurve', curve: points });
  alert("Fan curve sent and saved.");
}
