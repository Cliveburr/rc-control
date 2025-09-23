
const DEBUG = true;

function makeView() {
    const img = document.getElementsByTagName('img')[0];
    img.src = '/video?' + new Date().getTime();
}

// const ws = new WebSocket('/ws');
// ws.binaryType = 'arraybuffer';

// const canvas = document.getElementsByTagName('canvas')[0];
// const context = canvas.getContext('2d');
// let latest = performance.now();

// ws.onmessage = (ev) => {
    
//     const blob = new Blob([event.data], { type: 'image/jpeg' });
//     const img = new Image();

//     img.onload = function() {
//         canvas.width = img.width;
//         canvas.height = img.height;
//         context.drawImage(img, 0, 0);
//     };

//     img.src = URL.createObjectURL(blob);
   
//     const now = performance.now();
//     console.log(`Take: ${now - latest}`);
//     latest = performance.now();
// }

// ws.onopen = () => {
//     console.log('WebSocket connected');
// };

// ws.onclose = () => {
//     console.log('WebSocket closed');
// };

// window.wsSend = (data) => {
//     ws.send(data);
// }

// WebSocket connection for sending commands
let ws = null;
let batteryType = '1S'; // Default battery type, will be updated from ESP32

function initWebSocket() {
    if (DEBUG) {
        console.log('DEBUG mode: WebSocket disabled for testing');
        return;
    }
    
    try {
        ws = new WebSocket(`ws://${window.location.host}/ws`);
        
        ws.onopen = () => {
            console.log('WebSocket connected for control commands');
        };
        
        ws.onmessage = (event) => {
            handleWebSocketMessage(event.data);
        };
        
        ws.onclose = () => {
            console.log('WebSocket closed');
            // Tentar reconectar após 3 segundos
            setTimeout(initWebSocket, 3000);
        };
        
        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };
    } catch (error) {
        console.error('Failed to initialize WebSocket:', error);
    }
}

function handleWebSocketMessage(data) {
    try {
        const message = JSON.parse(data);
        console.log('Received WebSocket message:', message);
        
        switch (message.type) {
            case 'init':
                handleInitMessage(message);
                break;
            case 'battery':
                handleBatteryMessage(message);
                break;
            default:
                console.log('Unknown message type:', message.type);
        }
    } catch (error) {
        console.error('Failed to parse WebSocket message:', error, data);
    }
}

function handleInitMessage(message) {
    if (message.battery_type) {
        batteryType = message.battery_type;
        console.log('Battery type configured:', batteryType);
    }
}

function handleBatteryMessage(message) {
    if (typeof message.voltage === 'number') {
        const voltage = message.voltage;
        const batteryLevel = calculateBatteryLevel(voltage, batteryType);
        updateBatteryLevel(batteryLevel);
        console.log(`Battery: ${voltage.toFixed(2)}V (${batteryLevel}/10 levels, ${batteryType})`);
    }
}

function calculateBatteryLevel(voltage, type) {
    let minVoltage, maxVoltage;
    
    if (type === '1S') {
        minVoltage = 3.0;  // 1S LiPo empty (3.0V)
        maxVoltage = 4.2;  // 1S LiPo full (4.2V)
    } else if (type === '2S') {
        minVoltage = 6.0;  // 2S LiPo empty (6.0V)
        maxVoltage = 8.4;  // 2S LiPo full (8.4V)
    } else {
        console.error('Unknown battery type:', type);
        return 0;
    }
    
    // Calculate percentage and convert to level (0-10)
    const percentage = Math.max(0, Math.min(100, (voltage - minVoltage) / (maxVoltage - minVoltage) * 100));
    const level = Math.floor(percentage / 10);
    
    return Math.max(0, Math.min(10, level));
}

function sendSpeedCommand(value) {
    sendControlCommand('speed', value);
}

function sendWheelsCommand(value) {
    sendControlCommand('wheels', value);
}

function sendControlCommand(type, value) {
    if (DEBUG) {
        console.log(`DEBUG mode: ${type} command (visual test only):`, value);
        return;
    }
    
    if (ws && ws.readyState === WebSocket.OPEN) {
        const command = {
            type: type,
            value: value
        };
        ws.send(JSON.stringify(command));
        console.log(`${type} command sent:`, value);
    } else {
        console.warn(`WebSocket not connected, cannot send ${type} command:`, value);
    }
}

function sendHornCommand(isPressed) {
    if (DEBUG) {
        console.log('DEBUG mode: Horn command (visual test only):', isPressed);
        return;
    }
    
    if (ws && ws.readyState === WebSocket.OPEN) {
        const command = {
            type: 'horn',
            value: isPressed ? 1 : 0
        };
        ws.send(JSON.stringify(command));
        console.log('Horn command sent:', isPressed);
    } else {
        console.warn('WebSocket not connected, cannot send horn command:', isPressed);
    }
}

function sendLightCommand(isOn) {
    console.log('sendLightCommand called with:', isOn);
    if (DEBUG) {
        console.log('DEBUG mode: Light command (visual test only):', isOn);
        return;
    }
    
    if (ws && ws.readyState === WebSocket.OPEN) {
        const command = {
            type: 'light',
            value: isOn ? 1 : 0
        };
        ws.send(JSON.stringify(command));
        console.log('Light command sent:', isOn);
    } else {
        console.warn('WebSocket not connected, cannot send light command:', isOn);
    }
}

function initGenericControl(controlType, sendCommandFunc) {
    // Aguardar o DOM estar pronto
    const controlIndicator = document.getElementById(`${controlType}Indicator`);
    
    if (!controlIndicator) {
        console.error(`${controlType} control elements not found. Retrying in 100ms...`);
        setTimeout(() => initGenericControl(controlType, sendCommandFunc), 100);
        return;
    }
    
    const controlTrack = controlIndicator.parentElement;
    const isVertical = controlType === 'speed';
    
    let isDragging = false;
    let startPos = 0;
    let initialPosition = 0;
    const zeroPosition = isVertical ? 66.67 : 50; // 1/3 de baixo para cima para speed, centro para wheels
    
    // Função para calcular o valor baseado na posição
    function calculateValue(positionPercent) {
        if (isVertical) {
            // Speed control (vertical) - mapear posição para velocidade (-100 a +100)
            const relativePosition = (zeroPosition - positionPercent) / zeroPosition;
            
            if (positionPercent < zeroPosition) {
                // Acima do zero = velocidade positiva
                return Math.round(relativePosition * 100);
            } else {
                // Abaixo do zero = velocidade negativa
                const belowZeroRange = 100 - zeroPosition;
                const belowZeroPosition = (positionPercent - zeroPosition) / belowZeroRange;
                return Math.round(-belowZeroPosition * 100);
            }
        } else {
            // Wheels control (horizontal) - mapear posição para direção (-100 a +100)
            // 0% = -100 (esquerda), 50% = 0 (centro), 100% = +100 (direita)
            return Math.round((positionPercent - 50) * 2);
        }
    }
    
    // Função para atualizar a posição e valor
    function updateControl(positionPercent) {
        // Limitar entre 0% e 100%
        positionPercent = Math.max(0, Math.min(100, positionPercent));
        
        if (isVertical) {
            controlIndicator.style.top = positionPercent + '%';
        } else {
            controlIndicator.style.left = positionPercent + '%';
        }
        
        const value = calculateValue(positionPercent);
        
        // Enviar comando via WebSocket
        sendCommandFunc(value);
    }
    
    // Função para retornar ao zero
    function returnToZero() {
        if (isVertical) {
            controlIndicator.style.transition = 'top 0.3s ease-out';
        } else {
            controlIndicator.style.transition = 'left 0.3s ease-out';
        }
        updateControl(zeroPosition);
        
        setTimeout(() => {
            if (isVertical) {
                controlIndicator.style.transition = 'top 0.1s ease-out';
            } else {
                controlIndicator.style.transition = 'left 0.1s ease-out';
            }
        }, 300);
    }
    
    // Event listeners para mouse
    controlIndicator.addEventListener('mousedown', (e) => {
        isDragging = true;
        startPos = isVertical ? e.clientY : e.clientX;
        initialPosition = parseFloat(isVertical ? controlIndicator.style.top : controlIndicator.style.left) || zeroPosition;
        controlIndicator.style.transition = 'none';
        e.preventDefault();
    });

    // Event listener para clique direto no track
    controlTrack.addEventListener('mousedown', (e) => {
        // Se o clique foi no indicador, deixar o handler dele cuidar
        if (e.target === controlIndicator || controlIndicator.contains(e.target)) {
            return;
        }
        
        const rect = controlTrack.getBoundingClientRect();
        let clickPercent;
        
        if (isVertical) {
            const clickY = e.clientY - rect.top;
            const trackHeight = rect.height;
            clickPercent = (clickY / trackHeight) * 100;
        } else {
            const clickX = e.clientX - rect.left;
            const trackWidth = rect.width;
            clickPercent = (clickX / trackWidth) * 100;
        }
        
        isDragging = true;
        startPos = isVertical ? e.clientY : e.clientX;
        initialPosition = clickPercent;
        controlIndicator.style.transition = 'none';
        
        // Mover imediatamente para a posição clicada
        updateControl(clickPercent);
        
        e.preventDefault();
    });
    
    document.addEventListener('mousemove', (e) => {
        if (!isDragging) return;
        
        const deltaPos = (isVertical ? e.clientY : e.clientX) - startPos;
        const trackSize = isVertical ? controlTrack.clientHeight : controlTrack.clientWidth;
        const deltaPercent = (deltaPos / trackSize) * 100;
        const newPosition = initialPosition + deltaPercent;
        
        updateControl(newPosition);
        e.preventDefault();
    });
    
    document.addEventListener('mouseup', () => {
        if (isDragging) {
            isDragging = false;
            returnToZero();
        }
    });
    
    // Event listeners para touch (dispositivos móveis)
    controlIndicator.addEventListener('touchstart', (e) => {
        isDragging = true;
        startPos = isVertical ? e.touches[0].clientY : e.touches[0].clientX;
        initialPosition = parseFloat(isVertical ? controlIndicator.style.top : controlIndicator.style.left) || zeroPosition;
        controlIndicator.style.transition = 'none';
        e.preventDefault();
    });

    // Event listener para touch direto no track
    controlTrack.addEventListener('touchstart', (e) => {
        // Se o toque foi no indicador, deixar o handler dele cuidar
        if (e.target === controlIndicator || controlIndicator.contains(e.target)) {
            return;
        }
        
        const rect = controlTrack.getBoundingClientRect();
        let touchPercent;
        
        if (isVertical) {
            const touchY = e.touches[0].clientY - rect.top;
            const trackHeight = rect.height;
            touchPercent = (touchY / trackHeight) * 100;
        } else {
            const touchX = e.touches[0].clientX - rect.left;
            const trackWidth = rect.width;
            touchPercent = (touchX / trackWidth) * 100;
        }
        
        isDragging = true;
        startPos = isVertical ? e.touches[0].clientY : e.touches[0].clientX;
        initialPosition = touchPercent;
        controlIndicator.style.transition = 'none';
        
        // Mover imediatamente para a posição tocada
        updateControl(touchPercent);
        
        e.preventDefault();
    });
    
    document.addEventListener('touchmove', (e) => {
        if (!isDragging) return;
        
        const deltaPos = (isVertical ? e.touches[0].clientY : e.touches[0].clientX) - startPos;
        const trackSize = isVertical ? controlTrack.clientHeight : controlTrack.clientWidth;
        const deltaPercent = (deltaPos / trackSize) * 100;
        const newPosition = initialPosition + deltaPercent;
        
        updateControl(newPosition);
        e.preventDefault();
    });
    
    document.addEventListener('touchend', () => {
        if (isDragging) {
            isDragging = false;
            returnToZero();
        }
    });
    
    // Inicializar na posição zero
    updateControl(zeroPosition);
}

function initSpeedControl() {
    initGenericControl('speed', sendSpeedCommand);
}

function initWheelsControl() {
    initGenericControl('wheels', sendWheelsCommand);
}

class ViewInst {

    constructor(template, parentCtx) {
        this.parentCtx = parentCtx;
        this.html = template.cloneNode(true);
        this.ctx = {};
        
        // Adiciona funcionalidade genérica para botões de fechar
        this.setupCloseButtons();
    }

    setupCloseButtons() {
        const closeButtons = this.html.querySelectorAll('[data-close-view="true"]');
        closeButtons.forEach(button => {
            button.addEventListener('click', () => {
                this.close();
            });
        });
    }

    setOnclick(id, callback) {
        const el = this.html.querySelector(`#${id}`);
        el.onclick = callback;
    }

    close() {
        document.body.removeChild(this.html);
    }
}

class View {

    constructor(id, ctr) {
        this.ctr = ctr;
        this.template = document.getElementById(id);
        document.body.removeChild(this.template);
    }

    show(parentCtx) {

        const inst = new ViewInst(this.template, parentCtx);

        if (this.ctr) {
            this.ctr(inst);
        }

        document.body.appendChild(inst.html);

        return inst;
    }
}


function mainCtr(view) {

    // Inicializar controles após o DOM estar pronto
    setTimeout(() => {
        initSpeedControl();
        initWheelsControl();
    }, 0);

    view.setOnclick('btnConfiguration', () => {

        //console.log('hit');
        const temp = views.configurationView.show();

        // setTimeout(() => {
        //     temp.close();
        // }, 50000);

    });

    // Initialize horn button
    const hornBtn = view.html.querySelector('#btnHorn');
    if (hornBtn) {
        hornBtn.addEventListener('mousedown', () => {
            hornBtn.classList.add('active');
            sendHornCommand(true);
        });
        
        hornBtn.addEventListener('mouseup', () => {
            hornBtn.classList.remove('active');
            sendHornCommand(false);
        });
        
        hornBtn.addEventListener('mouseleave', () => {
            hornBtn.classList.remove('active');
            sendHornCommand(false);
        });
        
        // Touch events for mobile
        hornBtn.addEventListener('touchstart', (e) => {
            e.preventDefault();
            hornBtn.classList.add('active');
            sendHornCommand(true);
        });
        
        hornBtn.addEventListener('touchend', (e) => {
            e.preventDefault();
            hornBtn.classList.remove('active');
            sendHornCommand(false);
        });
    }

    // Initialize light button (toggle behavior)
    const lightBtn = view.html.querySelector('#btnLight');
    let lightState = false;
    if (lightBtn) {
        lightBtn.addEventListener('click', () => {
            lightState = !lightState;
            
            if (lightState) {
                lightBtn.classList.add('active');
            } else {
                lightBtn.classList.remove('active');
            }
            sendLightCommand(lightState);
        });
    } else {
        console.error('Light button not found!');
    }

}

function netCtr(view) {
    // Tab switching functionality
    const tabItems = view.html.querySelectorAll('.tab-item');
    const tabPanels = view.html.querySelectorAll('.tab-panel');
    
    tabItems.forEach(tab => {
        tab.addEventListener('click', () => {
            // Remove active class from all tabs and panels
            tabItems.forEach(t => t.classList.remove('active'));
            tabPanels.forEach(p => p.classList.remove('active'));
            
            // Add active class to clicked tab
            tab.classList.add('active');
            
            // Show corresponding panel
            const targetPanel = tab.id.replace('tab', 'tab') + 'Content';
            document.getElementById(targetPanel).classList.add('active');
        });
    });
    
    // Load OTA status when OTA tab is clicked
    view.html.querySelector('#tabOTA').addEventListener('click', loadOTAStatus);
    
    // Load system info when Info tab is clicked
    view.html.querySelector('#tabInfo').addEventListener('click', loadSystemInfo);
    
    // WiFi configuration save
    view.html.querySelector('#saveWifiConfig').addEventListener('click', saveWifiConfig);
    
    // OTA upload functionality
    view.html.querySelector('#uploadOTA').addEventListener('click', uploadOTAFirmware);
    
    // System info refresh button
    view.html.querySelector('#refreshSystemInfo').addEventListener('click', loadSystemInfo);
    
    // Close button (if added later)
    // view.setOnclick('conf_close', () => {
    //     view.close();
    // });
}

async function loadOTAStatus() {
    try {
        const response = await fetch('/ota/status');
        const data = await response.json();
        
        const statusDiv = document.getElementById('otaStatus');
        statusDiv.innerHTML = `
            <strong>Partição em execução:</strong> ${data.running_partition}<br>
            <strong>Partição de boot:</strong> ${data.boot_partition}<br>
            <strong>OTA em progresso:</strong> ${data.ota_in_progress ? 'Sim' : 'Não'}
        `;
    } catch (error) {
        console.error('Erro ao carregar status OTA:', error);
        document.getElementById('otaStatus').innerHTML = 'Erro ao carregar informações do sistema.';
    }
}

function saveWifiConfig() {
    const ssid = document.getElementById('wifiSsid').value;
    const password = document.getElementById('wifiPassword').value;
    
    if (!ssid) {
        alert('Por favor, insira o nome da rede WiFi');
        return;
    }
    
    // Here you would typically send the WiFi config to the ESP32
    // For now, just show a success message
    alert('Configuração WiFi salva com sucesso!');
    console.log('WiFi Config:', { ssid, password });
}

async function uploadOTAFirmware() {
    const fileInput = document.getElementById('otaFile');
    const file = fileInput.files[0];
    
    if (!file) {
        alert('Por favor, selecione um arquivo .bin');
        return;
    }
    
    if (!file.name.endsWith('.bin')) {
        alert('Por favor, selecione um arquivo .bin válido');
        return;
    }
    
    const progressContainer = document.getElementById('otaProgress');
    const progressBar = document.getElementById('progressBar');
    const progressText = document.getElementById('progressText');
    const uploadButton = document.getElementById('uploadOTA');
    
    // Show progress bar and disable upload button
    progressContainer.style.display = 'block';
    uploadButton.disabled = true;
    uploadButton.textContent = 'Enviando...';
    
    try {
        const formData = new FormData();
        formData.append('firmware', file);
        
        const xhr = new XMLHttpRequest();
        
        xhr.upload.addEventListener('progress', (e) => {
            if (e.lengthComputable) {
                const percentComplete = (e.loaded / e.total) * 100;
                progressBar.style.width = percentComplete + '%';
                progressText.textContent = Math.round(percentComplete) + '%';
            }
        });
        
        xhr.onload = function() {
            if (xhr.status === 200) {
                try {
                    const response = JSON.parse(xhr.responseText);
                    alert('Firmware enviado com sucesso! O dispositivo será reiniciado.');
                    // Reset form
                    fileInput.value = '';
                    progressContainer.style.display = 'none';
                } catch (e) {
                    alert('Firmware enviado com sucesso! O dispositivo será reiniciado.');
                }
            } else {
                alert('Erro no upload: ' + xhr.responseText);
            }
            
            uploadButton.disabled = false;
            uploadButton.textContent = 'Atualizar Firmware';
            progressContainer.style.display = 'none';
        };
        
        xhr.onerror = function() {
            alert('Erro na conexão durante o upload');
            uploadButton.disabled = false;
            uploadButton.textContent = 'Atualizar Firmware';
            progressContainer.style.display = 'none';
        };
        
        xhr.open('POST', '/ota/upload');
        xhr.send(file);
        
    } catch (error) {
        console.error('Erro no upload OTA:', error);
        alert('Erro ao enviar firmware');
        uploadButton.disabled = false;
        uploadButton.textContent = 'Atualizar Firmware';
        progressContainer.style.display = 'none';
    }
}

async function loadSystemInfo() {
    try {
        // Show loading state
        resetSystemInfoDisplay();
        
        let response;
        if (DEBUG) {
            // Mock data for debug mode
            response = {
                ok: true,
                json: async () => ({
                    chip: {
                        model: "ESP32-S3",
                        cores: 2,
                        revision: 3,
                        cpu_freq_mhz: 240,
                        has_wifi: true,
                        has_bluetooth: true,
                        has_ble: true,
                        flash_size_mb: 16
                    },
                    memory: {
                        heap: {
                            total_bytes: 327680,
                            used_bytes: 98304,
                            free_bytes: 229376,
                            usage_percent: 30
                        },
                        psram: {
                            total_bytes: 8388608,
                            used_bytes: 1048576,
                            free_bytes: 7340032,
                            usage_percent: 12
                        },
                        dma: {
                            total_bytes: 262144,
                            used_bytes: 32768,
                            free_bytes: 229376,
                            usage_percent: 12
                        },
                        iram: {
                            total_bytes: 131072,
                            used_bytes: 65536,
                            free_bytes: 65536,
                            usage_percent: 50
                        },
                        dram: {
                            total_bytes: 524288,
                            used_bytes: 157286,
                            free_bytes: 367002,
                            usage_percent: 30
                        }
                    }
                })
            };
        } else {
            response = await fetch('/api/system-info');
        }
        
        if (!response.ok) {
            throw new Error('Failed to fetch system info');
        }
        
        const data = await response.json();
        updateSystemInfoDisplay(data);
        
    } catch (error) {
        console.error('Erro ao carregar informações do sistema:', error);
        showSystemInfoError();
    }
}

function resetSystemInfoDisplay() {
    // Reset all display values to loading state
    const elements = [
        'chipModel', 'chipCores', 'chipRevision', 'cpuFreq',
        'hasWifi', 'hasBluetooth', 'flashSize',
        'heapTotal', 'heapUsed', 'heapFree', 'heapUsage',
        'psramTotal', 'psramUsed', 'psramFree', 'psramUsage',
        'dmaTotal', 'dmaUsed', 'dmaFree', 'dmaUsage',
        'iramTotal', 'iramUsed', 'iramFree', 'iramUsage',
        'dramTotal', 'dramUsed', 'dramFree', 'dramUsage'
    ];
    
    elements.forEach(id => {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = 'Carregando...';
        }
    });
    
    // Reset progress bars
    ['heapProgressBar', 'psramProgressBar', 'dmaProgressBar', 'iramProgressBar', 'dramProgressBar'].forEach(id => {
        const element = document.getElementById(id);
        if (element) {
            element.style.width = '0%';
        }
    });
}

function updateSystemInfoDisplay(data) {
    // Update chip information
    document.getElementById('chipModel').textContent = data.chip.model || '--';
    document.getElementById('chipCores').textContent = data.chip.cores || '--';
    document.getElementById('chipRevision').textContent = data.chip.revision || '--';
    document.getElementById('cpuFreq').textContent = (data.chip.cpu_freq_mhz || '--') + ' MHz';
    document.getElementById('hasWifi').textContent = data.chip.has_wifi ? 'Sim' : 'Não';
    document.getElementById('hasBluetooth').textContent = data.chip.has_bluetooth ? 'Sim' : 'Não';
    document.getElementById('flashSize').textContent = (data.chip.flash_size_mb || '--') + ' MB';
    
    // Update memory information
    updateMemoryInfo('heap', data.memory.heap);
    updateMemoryInfo('psram', data.memory.psram);
    updateMemoryInfo('dma', data.memory.dma);
    updateMemoryInfo('iram', data.memory.iram);
    updateMemoryInfo('dram', data.memory.dram);
}

function updateMemoryInfo(type, memInfo) {
    const totalKB = Math.round(memInfo.total_bytes / 1024);
    const usedKB = Math.round(memInfo.used_bytes / 1024);
    const freeKB = Math.round(memInfo.free_bytes / 1024);
    const usagePercent = memInfo.usage_percent || 0;
    
    // Update individual values
    document.getElementById(type + 'Total').textContent = totalKB + ' KB';
    document.getElementById(type + 'Used').textContent = usedKB + ' KB';
    document.getElementById(type + 'Free').textContent = freeKB + ' KB';
    document.getElementById(type + 'Usage').textContent = `${usedKB} / ${totalKB} KB (${usagePercent}%)`;
    
    // Update progress bar
    const progressBar = document.getElementById(type + 'ProgressBar');
    if (progressBar) {
        progressBar.style.width = usagePercent + '%';
    }
}

function showSystemInfoError() {
    const elements = [
        'chipModel', 'chipCores', 'chipRevision', 'cpuFreq',
        'hasWifi', 'hasBluetooth', 'flashSize',
        'heapTotal', 'heapUsed', 'heapFree', 'heapUsage',
        'psramTotal', 'psramUsed', 'psramFree', 'psramUsage',
        'dmaTotal', 'dmaUsed', 'dmaFree', 'dmaUsage',
        'iramTotal', 'iramUsed', 'iramFree', 'iramUsage',
        'dramTotal', 'dramUsed', 'dramFree', 'dramUsage'
    ];
    
    elements.forEach(id => {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = 'Erro';
        }
    });
}

// Battery level debug simulation
let batteryLevel = 0;
let batteryDirection = 1; // 1 for increasing, -1 for decreasing

function updateBatteryLevel(level) {
    const batteryLevels = document.querySelectorAll('.battery-level');
    
    // Remove active class from all levels
    batteryLevels.forEach(levelElement => {
        levelElement.classList.remove('active');
    });
    
    // Add active class to levels up to the current level
    for (let i = 0; i < Math.min(level, 10); i++) {
        batteryLevels[i].classList.add('active');
    }
}

function startBatteryDebugLoop() {
    if (DEBUG) {
        setInterval(() => {
            batteryLevel += batteryDirection;
            
            // Change direction when reaching limits
            if (batteryLevel >= 10) {
                batteryDirection = -1;
            } else if (batteryLevel <= 0) {
                batteryDirection = 1;
            }
            
            updateBatteryLevel(batteryLevel);
            console.log('Battery level debug:', batteryLevel);
        }, 1000); // Update every 1 second
    }
}

const views = {};

function main() {

    // Inicializar WebSocket para comandos (apenas se não estiver em modo DEBUG)
    if (!DEBUG) {
        initWebSocket();
    } else {
        console.log('DEBUG mode enabled: Running in visual test mode without WebSocket');
        // Start battery debug loop
        startBatteryDebugLoop();
    }

    views.mainView = new View('mainView', mainCtr);
    views.configurationView = new View('configurationView', netCtr);

    views.mainView.show();
}

window.onload = main;