
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
    const zeroPosition = isVertical ? 66.67 : 50;
    
    // Cache de touch points específico para este controle
    let touchCache = [];
    
    // Função para calcular o valor baseado na posição
    function calculateValue(positionPercent) {
        if (isVertical) {
            const relativePosition = (zeroPosition - positionPercent) / zeroPosition;
            if (positionPercent < zeroPosition) {
                return Math.round(relativePosition * 100);
            } else {
                const belowZeroRange = 100 - zeroPosition;
                const belowZeroPosition = (positionPercent - zeroPosition) / belowZeroRange;
                return Math.round(-belowZeroPosition * 100);
            }
        } else {
            return Math.round((positionPercent - 50) * 2);
        }
    }
    
    // Função para atualizar a posição e valor
    function updateControl(positionPercent) {
        positionPercent = Math.max(0, Math.min(100, positionPercent));
        
        if (isVertical) {
            controlIndicator.style.top = positionPercent + '%';
        } else {
            controlIndicator.style.left = positionPercent + '%';
        }
        
        const value = calculateValue(positionPercent);
        sendCommandFunc(value);
        
        if (DEBUG) console.log(`${controlType}: Posição ${positionPercent.toFixed(1)}%, Valor: ${value}`);
    }
    
    // Função para forçar reset ao zero (útil para reset manual)
    function forceResetToZero() {
        if (DEBUG) console.log(`${controlType}: Reset forçado ao zero`);
        
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
    
    // Função para retornar ao zero (modificada para speed control)
    function returnToZero() {
        if (DEBUG) console.log(`${controlType}: Verificando zona de reset`);
        
        // Para o controle speed, só resetar se estiver próximo ao zero
        if (isVertical) { // Speed control
            const currentPosition = parseFloat(controlIndicator.style.top) || zeroPosition;
            const distanceFromZero = Math.abs(currentPosition - zeroPosition);
            const resetThreshold = 10; // 10% de zona de reset
            
            if (distanceFromZero <= resetThreshold) {
                // Dentro da zona de reset - retornar ao zero
                if (DEBUG) console.log(`${controlType}: Dentro da zona de reset (${distanceFromZero.toFixed(1)}%), retornando ao zero`);
                
                controlIndicator.style.transition = 'top 0.3s ease-out';
                updateControl(zeroPosition);
                
                setTimeout(() => {
                    controlIndicator.style.transition = 'top 0.1s ease-out';
                }, 300);
            } else {
                // Fora da zona de reset - manter posição atual
                if (DEBUG) console.log(`${controlType}: Fora da zona de reset (${distanceFromZero.toFixed(1)}%), mantendo posição atual`);
                
                // Só ajustar a transição para suavizar movimento futuro
                controlIndicator.style.transition = 'top 0.1s ease-out';
            }
        } else { // Wheels control - comportamento original (sempre volta ao centro)
            if (DEBUG) console.log(`${controlType}: Retornando ao centro (comportamento wheels)`);
            
            controlIndicator.style.transition = 'left 0.3s ease-out';
            updateControl(zeroPosition);
            
            setTimeout(() => {
                controlIndicator.style.transition = 'left 0.1s ease-out';
            }, 300);
        }
    }
    
    // Função para encontrar touch no cache por identifier
    function findTouchInCache(identifier) {
        for (let i = 0; i < touchCache.length; i++) {
            if (touchCache[i].identifier === identifier) {
                return { touch: touchCache[i], index: i };
            }
        }
        return null;
    }
    
    // Função para calcular posição do touch
    function getTouchPosition(touch) {
        const rect = controlTrack.getBoundingClientRect();
        let touchPercent;
        
        if (isVertical) {
            const touchY = touch.clientY - rect.top;
            const trackHeight = rect.height;
            touchPercent = (touchY / trackHeight) * 100;
        } else {
            const touchX = touch.clientX - rect.left;
            const trackWidth = rect.width;
            touchPercent = (touchX / trackWidth) * 100;
        }
        
        return Math.max(0, Math.min(100, touchPercent));
    }
    
    // Variáveis para detectar duplo toque para reset manual (apenas speed)
    let lastTapTime = 0;
    let tapCount = 0;
    
    // Touch Start Handler
    function handleTouchStart(ev) {
        ev.preventDefault();
        
        if (DEBUG) console.log(`${controlType}: touchstart - targetTouches: ${ev.targetTouches.length}`);
        
        // Detectar duplo toque para reset manual (apenas para speed)
        if (isVertical && ev.targetTouches.length === 1) {
            const now = Date.now();
            const timeDiff = now - lastTapTime;
            
            if (timeDiff < 300) { // 300ms para considerar duplo toque
                tapCount++;
                if (tapCount === 2) {
                    const touchPos = getTouchPosition(ev.targetTouches[0]);
                    const distanceFromZero = Math.abs(touchPos - zeroPosition);
                    
                    if (distanceFromZero <= 15) { // Zona um pouco maior para duplo toque
                        if (DEBUG) console.log(`${controlType}: Duplo toque detectado na zona zero, forçando reset`);
                        forceResetToZero();
                        tapCount = 0;
                        return; // Não processar como toque normal
                    }
                }
            } else {
                tapCount = 1;
            }
            lastTapTime = now;
        }
        
        // Usar targetTouches para toques específicos neste elemento
        for (let i = 0; i < ev.targetTouches.length; i++) {
            const touch = ev.targetTouches[i];
            
            // Verificar se já temos este touch no cache
            if (!findTouchInCache(touch.identifier)) {
                // Adicionar ao cache com informações de posição inicial
                const touchPercent = getTouchPosition(touch);
                const touchData = {
                    identifier: touch.identifier,
                    startX: touch.clientX,
                    startY: touch.clientY,
                    currentPercent: touchPercent,
                    initialPercent: touchPercent
                };
                
                touchCache.push(touchData);
                
                // Se é o primeiro toque, inicializar o controle
                if (touchCache.length === 1) {
                    controlIndicator.style.transition = 'none';
                    updateControl(touchPercent);
                    if (DEBUG) console.log(`${controlType}: Primeiro toque iniciado (ID: ${touch.identifier})`);
                }
            }
        }
    }
    
    // Touch Move Handler
    function handleTouchMove(ev) {
        ev.preventDefault();
        
        // Usar targetTouches para movimento específico neste elemento
        for (let i = 0; i < ev.targetTouches.length; i++) {
            const touch = ev.targetTouches[i];
            const cacheEntry = findTouchInCache(touch.identifier);
            
            if (cacheEntry) {
                // Calcular nova posição baseada no movimento
                const deltaX = touch.clientX - cacheEntry.touch.startX;
                const deltaY = touch.clientY - cacheEntry.touch.startY;
                
                const trackSize = isVertical ? controlTrack.clientHeight : controlTrack.clientWidth;
                const deltaPercent = (isVertical ? deltaY : deltaX) / trackSize * 100;
                const newPercent = cacheEntry.touch.initialPercent + deltaPercent;
                
                // Atualizar cache
                cacheEntry.touch.currentPercent = newPercent;
                
                // Se é o primeiro (principal) toque, atualizar controle
                if (cacheEntry.index === 0) {
                    updateControl(newPercent);
                }
            }
        }
    }
    
    // Touch End Handler
    function handleTouchEnd(ev) {
        ev.preventDefault();
        
        if (DEBUG) console.log(`${controlType}: touchend - changedTouches: ${ev.changedTouches.length}`);
        
        // Usar changedTouches para detectar quais toques terminaram
        for (let i = 0; i < ev.changedTouches.length; i++) {
            const touch = ev.changedTouches[i];
            const cacheEntry = findTouchInCache(touch.identifier);
            
            if (cacheEntry) {
                if (DEBUG) console.log(`${controlType}: Removendo toque (ID: ${touch.identifier})`);
                
                // Remover do cache
                touchCache.splice(cacheEntry.index, 1);
                
                // Se não há mais toques, retornar ao zero
                if (touchCache.length === 0) {
                    if (DEBUG) console.log(`${controlType}: Último toque removido, retornando ao zero`);
                    returnToZero();
                }
            }
        }
    }
    
    // Touch Cancel Handler
    function handleTouchCancel(ev) {
        if (DEBUG) console.log(`${controlType}: touchcancel`);
        handleTouchEnd(ev); // Tratar cancelamento como fim de toque
    }
    
    // Mouse handlers para compatibilidade desktop
    let mouseActive = false;
    let mouseStartPos = { x: 0, y: 0 };
    let mouseInitialPercent = 0;
    
    function handleMouseDown(ev) {
        if (touchCache.length > 0) return; // Ignorar mouse se já há toques ativos
        
        ev.preventDefault();
        mouseActive = true;
        
        const rect = controlTrack.getBoundingClientRect();
        let clickPercent;
        
        if (isVertical) {
            const clickY = ev.clientY - rect.top;
            clickPercent = (clickY / rect.height) * 100;
        } else {
            const clickX = ev.clientX - rect.left;
            clickPercent = (clickX / rect.width) * 100;
        }
        
        mouseStartPos = { x: ev.clientX, y: ev.clientY };
        mouseInitialPercent = clickPercent;
        
        controlIndicator.style.transition = 'none';
        updateControl(clickPercent);
        
        if (DEBUG) console.log(`${controlType}: Mouse down (${clickPercent.toFixed(1)}%)`);
    }
    
    function handleMouseMove(ev) {
        if (!mouseActive) return;
        
        ev.preventDefault();
        
        const deltaX = ev.clientX - mouseStartPos.x;
        const deltaY = ev.clientY - mouseStartPos.y;
        
        const trackSize = isVertical ? controlTrack.clientHeight : controlTrack.clientWidth;
        const deltaPercent = (isVertical ? deltaY : deltaX) / trackSize * 100;
        const newPercent = mouseInitialPercent + deltaPercent;
        
        updateControl(newPercent);
    }
    
    function handleMouseUp(ev) {
        if (!mouseActive) return;
        
        mouseActive = false;
        returnToZero();
        
        if (DEBUG) console.log(`${controlType}: Mouse up`);
    }
    
    // Registrar event listeners nos elementos específicos (conforme MDN best practices)
    controlIndicator.addEventListener('touchstart', handleTouchStart, { passive: false });
    controlIndicator.addEventListener('touchmove', handleTouchMove, { passive: false });
    controlIndicator.addEventListener('touchend', handleTouchEnd, { passive: false });
    controlIndicator.addEventListener('touchcancel', handleTouchCancel, { passive: false });
    
    controlTrack.addEventListener('touchstart', handleTouchStart, { passive: false });
    controlTrack.addEventListener('touchmove', handleTouchMove, { passive: false });
    controlTrack.addEventListener('touchend', handleTouchEnd, { passive: false });
    controlTrack.addEventListener('touchcancel', handleTouchCancel, { passive: false });
    
    // Mouse events para desktop
    controlIndicator.addEventListener('mousedown', handleMouseDown);
    controlTrack.addEventListener('mousedown', handleMouseDown);
    controlIndicator.addEventListener('mousemove', handleMouseMove);
    controlTrack.addEventListener('mousemove', handleMouseMove);
    controlIndicator.addEventListener('mouseup', handleMouseUp);
    controlTrack.addEventListener('mouseup', handleMouseUp);
    controlIndicator.addEventListener('mouseleave', handleMouseUp);
    controlTrack.addEventListener('mouseleave', handleMouseUp);
    
    // Inicializar na posição zero
    updateControl(zeroPosition);
    
    if (DEBUG) console.log(`${controlType}: Controle inicializado com cache local`);
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
        let hornPressed = false;
        
        hornBtn.addEventListener('mousedown', (e) => {
            if (!hornPressed) {
                hornPressed = true;
                hornBtn.classList.add('active');
                sendHornCommand(true);
            }
            e.preventDefault();
        });
        
        hornBtn.addEventListener('mouseup', (e) => {
            if (hornPressed) {
                hornPressed = false;
                hornBtn.classList.remove('active');
                sendHornCommand(false);
            }
            e.preventDefault();
        });
        
        hornBtn.addEventListener('mouseleave', (e) => {
            if (hornPressed) {
                hornPressed = false;
                hornBtn.classList.remove('active');
                sendHornCommand(false);
            }
        });
        
        // Touch events for mobile with better multi-touch support
        hornBtn.addEventListener('touchstart', (e) => {
            if (!hornPressed) {
                hornPressed = true;
                hornBtn.classList.add('active');
                sendHornCommand(true);
            }
            e.preventDefault();
            e.stopPropagation();
        });
        
        hornBtn.addEventListener('touchend', (e) => {
            if (hornPressed) {
                hornPressed = false;
                hornBtn.classList.remove('active');
                sendHornCommand(false);
            }
            e.preventDefault();
            e.stopPropagation();
        });
        
        hornBtn.addEventListener('touchcancel', (e) => {
            if (hornPressed) {
                hornPressed = false;
                hornBtn.classList.remove('active');
                sendHornCommand(false);
            }
            e.preventDefault();
            e.stopPropagation();
        });
    }

    // Initialize light button (toggle behavior)
    const lightBtn = view.html.querySelector('#btnLight');
    let lightState = false;
    if (lightBtn) {
        lightBtn.addEventListener('click', (e) => {
            lightState = !lightState;
            
            if (lightState) {
                lightBtn.classList.add('active');
            } else {
                lightBtn.classList.remove('active');
            }
            sendLightCommand(lightState);
            e.preventDefault();
        });
        
        // Better touch support for light button
        lightBtn.addEventListener('touchend', (e) => {
            // Evitar duplo evento (click + touchend)
            e.preventDefault();
            e.stopPropagation();
            
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

function preventDefaultBehaviors() {
    // Prevenir comportamentos padrão que podem interferir com os controles
    document.addEventListener('gesturestart', (e) => {
        e.preventDefault();
    });
    
    document.addEventListener('gesturechange', (e) => {
        e.preventDefault();
    });
    
    document.addEventListener('gestureend', (e) => {
        e.preventDefault();
    });
    
    // Prevenir zoom com duplo toque, mas permitir toques nos controles
    let lastTouchEnd = 0;
    document.addEventListener('touchend', (e) => {
        const now = (new Date()).getTime();
        if (now - lastTouchEnd <= 300) {
            // Verificar se o duplo toque não foi em um controle
            const target = e.target;
            const isControlElement = target.closest('.control') || 
                                   target.closest('.btn') || 
                                   target.id === 'btnHorn' || 
                                   target.id === 'btnLight' ||
                                   target.id === 'btnConfiguration';
            
            if (!isControlElement) {
                e.preventDefault();
            }
        }
        lastTouchEnd = now;
    }, false);
    
    // Prevenir menu de contexto em toque longo nos controles
    document.addEventListener('contextmenu', (e) => {
        const target = e.target;
        const isControlElement = target.closest('.control') || 
                               target.closest('.btn') || 
                               target.id === 'btnHorn' || 
                               target.id === 'btnLight' ||
                               target.id === 'btnConfiguration';
        
        if (isControlElement) {
            e.preventDefault();
        }
    });
}

const views = {};

function main() {

    // Inicializar prevenção de comportamentos padrão
    preventDefaultBehaviors();

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