let gateway = `ws://${window.location.hostname}:81/`;
let websocket;
const MAX_CURVE_POINTS_UI = 8;
const MIN_CURVE_POINTS_UI = 2;
let initialDataLoaded = false; 
let fanCurveDataCache = []; // Cache for the fan curve data

// --- Modal Elements ---
let fanCurveModal;
let closeModalButton;
let openModalButton;

// --- Chart.js instance ---
let fanCurveChart = null;

window.addEventListener('load', onLoad);

function onLoad(event) { 
  initWebSocket();
  initModal();
  
  const mqttEnableCheckbox = document.getElementById('mqttEnable');
  if (mqttEnableCheckbox) {
    mqttEnableCheckbox.addEventListener('change', toggleMqttFields);
  }
}

function initWebSocket() {
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen    = onOpen;
  websocket.onclose   = onClose;
  websocket.onmessage = onMessage;
}

function initModal() {
    fanCurveModal = document.getElementById('fanCurveModal');
    closeModalButton = document.querySelector('#fanCurveModal .close-button');
    openModalButton = document.getElementById('openCurveEditorBtn');

    if (openModalButton) openModalButton.addEventListener('click', openCurveModal);
    if (closeModalButton) closeModalButton.addEventListener('click', closeCurveModal);
    
    window.addEventListener('click', (event) => {
        if (event.target == fanCurveModal) {
            closeCurveModal();
        }
    });
}

function openCurveModal() {
    if (fanCurveModal) {
        createOrUpdateFanChart(fanCurveDataCache);
        fanCurveModal.style.display = 'block';
    }
}

function closeCurveModal() {
    if (fanCurveModal) {
        fanCurveModal.style.display = 'none';
        if(fanCurveChart) {
            const canvas = document.getElementById('fanCurveChart');
            if (canvas) {
                // Remove event listener to prevent memory leaks
                canvas.removeEventListener('contextmenu', handleChartRightClick);
            }
            fanCurveChart.destroy();
            fanCurveChart = null;
        }
    }
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
      document.getElementById('loadingOverlay').style.display = 'none';
      document.querySelector('.main-content-wrapper').style.display = 'block';
      initialDataLoaded = true;
  }

  // --- Update Main Page Data ---
  if (data.serialDebugEnabled !== undefined) { 
      document.getElementById('debugModeNotice').classList.toggle('hidden', !data.serialDebugEnabled);
  }

  document.getElementById('temp').innerText = (data.tempSensorFound && data.temperature > -990) ? data.temperature.toFixed(1) : 'N/A';
  document.getElementById('pcTemp').innerText = (data.pcTempDataReceived && data.pcTemperature > -990) ? data.pcTemperature.toFixed(1) : 'N/A';
  
  const curveEditorContainer = document.querySelector('.curve-editor-container');
  if(curveEditorContainer) {
    curveEditorContainer.classList.toggle('hidden', !data.tempSensorFound && !data.pcTempDataReceived);
  }

  if (data.firmwareVersion !== undefined) { 
    document.getElementById('firmwareVersion').innerText = data.firmwareVersion;
  }

  if(data.fanSpeed !== undefined) document.getElementById('fanSpeed').innerText = data.fanSpeed;
  if(data.fanRpm !== undefined) document.getElementById('rpm').innerText = data.fanRpm;
  
  if(data.isAutoMode !== undefined) {
    document.getElementById('mode').innerText = data.isAutoMode ? "AUTO" : "MANUAL";
    document.getElementById('manualControl').style.display = data.isAutoMode ? 'none' : 'block';
    const autoModeNotice = document.getElementById('autoModeNotice');
    if (autoModeNotice) { 
        autoModeNotice.innerText = (data.isAutoMode && !data.tempSensorFound && !data.pcTempDataReceived) ? '(Sensor N/A - Fixed Speed)' : '';
    }
  }
  
  if(data.manualFanSpeed !== undefined && !data.isAutoMode) {
    document.getElementById('speedSlider').value = data.manualFanSpeed;
    document.getElementById('manualSpeedValue').innerText = data.manualFanSpeed;
  }

  if(data.fanCurve && Array.isArray(data.fanCurve)) { 
    fanCurveDataCache = data.fanCurve;
  }

  // --- MQTT & OTA updates ---
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
  if (data.mqttDiscoveryPrefix !== undefined) document.getElementById('mqttDiscoveryPrefix').value = data.mqttDiscoveryPrefix;

  if (data.otaStatusMessage !== undefined) {
    document.getElementById('otaStatusMessage').innerText = data.otaStatusMessage;
  }
  if (data.otaInProgress !== undefined) {
    const otaButton = document.getElementById('otaUpdateButton');
    if (otaButton) {
        otaButton.disabled = data.otaInProgress;
        otaButton.innerText = data.otaInProgress ? "Update in Progress..." : "Check for Updates & Install";
    }
  }
}

function sendCommand(commandPayload) {
  if (websocket && websocket.readyState === WebSocket.OPEN) {
    websocket.send(JSON.stringify(commandPayload));
  } else { console.log("WebSocket not open."); }
}

function updateSliderValueDisplay(value) { document.getElementById('manualSpeedValue').innerText = value; }
function setManualSpeed(value) {
  updateSliderValueDisplay(value); 
  sendCommand({ action: 'setManualSpeed', value: parseInt(value) });
}

// --- Fan Curve Chart Functions ---

function handleChartRightClick(e) {
    e.preventDefault();
    if (!fanCurveChart) return;

    const points = fanCurveChart.getElementsAtEventForMode(e, 'point', { intersect: true }, true);

    if (points.length > 0) {
        const firstPoint = points[0];
        const datasetIndex = firstPoint.datasetIndex;
        const index = firstPoint.index;
        
        let data = fanCurveChart.data.datasets[datasetIndex].data;
        if (data.length > MIN_CURVE_POINTS_UI) {
            data.splice(index, 1);
            fanCurveChart.update();
        } else {
            alert(`A minimum of ${MIN_CURVE_POINTS_UI} points is required.`);
        }
    }
}

function createOrUpdateFanChart(curvePoints) {
    const canvas = document.getElementById('fanCurveChart');
    const ctx = canvas.getContext('2d');
    
    if (fanCurveChart) {
        fanCurveChart.destroy();
    }

    const chartData = curvePoints.map(p => ({ x: p.temp, y: p.pwmPercent }));

    const config = {
        type: 'scatter',
        data: {
            datasets: [{
                label: 'Fan Curve',
                data: chartData,
                showLine: true,
                borderColor: '#3498db',
                backgroundColor: '#3498db',
                pointRadius: 6,
                pointHoverRadius: 8,
                pointBorderColor: '#fff',
                pointBorderWidth: 2
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: true,
            onClick: (e) => {
                if (!fanCurveChart) return;
                
                // Prevent adding a new point if an existing point was clicked.
                const pointsAtEvent = fanCurveChart.getElementsAtEventForMode(e, 'point', { intersect: true }, true);
                if (pointsAtEvent.length > 0) {
                    return;
                }

                const data = fanCurveChart.data.datasets[0].data;
                if (data.length >= MAX_CURVE_POINTS_UI) {
                    alert(`Maximum of ${MAX_CURVE_POINTS_UI} points reached.`);
                    return;
                }

                const canvasPosition = Chart.helpers.getRelativePosition(e, fanCurveChart);
                const newX = fanCurveChart.scales.x.getValueForPixel(canvasPosition.x);
                const newY = fanCurveChart.scales.y.getValueForPixel(canvasPosition.y);
                
                // Add the new point, rounding to the nearest integer for consistency.
                data.push({x: Math.round(newX), y: Math.round(newY)});
                
                // Sort the data array by x-value to maintain order
                data.sort((a,b) => a.x - b.x);
                
                // Update the chart
                fanCurveChart.update();
            },
            scales: {
                x: {
                    type: 'linear',
                    position: 'bottom',
                    title: { display: true, text: 'Temperature (°C)', font: { size: 14 } },
                    min: 0,
                    max: 120,
                    ticks: { stepSize: 10 }
                },
                y: {
                    title: { display: true, text: 'Fan Speed (%)', font: { size: 14 } },
                    min: 0,
                    max: 100,
                    ticks: { stepSize: 10 }
                }
            },
            plugins: {
                legend: { display: false },
                dragData: {
                    round: 0,
                    dragX: true, // This enables dragging on the X axis
                    onDrag: (e, datasetIndex, index, value) => {
                        value.x = Math.max(0, Math.min(120, value.x));
                        value.y = Math.max(0, Math.min(100, value.y));
                        e.target.style.cursor = 'grabbing';
                    },
                    onDragEnd: (e, datasetIndex, index, value) => {
                        e.target.style.cursor = 'grab';
                        let data = fanCurveChart.data.datasets[datasetIndex].data;
                        data.sort((a, b) => a.x - b.x);
                        fanCurveChart.update('none');
                    },
                },
                tooltip: {
                    callbacks: {
                        label: function(context) {
                            return `Temp: ${context.parsed.x.toFixed(0)}°C, Speed: ${context.parsed.y.toFixed(0)}%`;
                        }
                    }
                }
            }
        }
    };
    fanCurveChart = new Chart(ctx, config);
    // Add context menu listener for deleting points
    canvas.addEventListener('contextmenu', handleChartRightClick);
}

function saveFanCurveFromChart() {
    if (!fanCurveChart) {
        console.error("Chart not initialized.");
        return;
    }

    const chartData = fanCurveChart.data.datasets[0].data;
    const pointsToSend = chartData.map(p => ({
        temp: Math.round(p.x),
        pwmPercent: Math.round(p.y)
    }));

    if (pointsToSend.length < MIN_CURVE_POINTS_UI) { 
        alert(`A minimum of ${MIN_CURVE_POINTS_UI} points is required.`); 
        return; 
    }
    
    for (let i = 0; i < pointsToSend.length - 1; i++) {
        if (pointsToSend[i].temp >= pointsToSend[i+1].temp) {
            alert("Invalid curve: Temperatures must be unique and in increasing order. Please adjust the points.");
            return;
        }
    }

    sendCommand({ action: 'setCurve', curve: pointsToSend });
    
    const saveButton = document.querySelector('.modal-content .chart-controls button.secondary');
    if(saveButton) {
        const originalText = saveButton.innerText;
        saveButton.innerText = "Saved!";
        saveButton.disabled = true;
        setTimeout(() => {
            saveButton.innerText = originalText;
            saveButton.disabled = false;
        }, 2000);
    }
}


// --- Other Config Functions ---
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
