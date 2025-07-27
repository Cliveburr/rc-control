
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

function sendSpeedCommand(value) {
    if (DEBUG) {
        console.log('DEBUG mode: Speed command (visual test only):', value);
        return;
    }
    
    if (ws && ws.readyState === WebSocket.OPEN) {
        const command = {
            type: 'speed',
            value: value
        };
        ws.send(JSON.stringify(command));
        console.log('Speed command sent:', value);
    } else {
        console.warn('WebSocket not connected, cannot send speed command:', value);
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

function initSpeedControl() {
    // Aguardar o DOM estar pronto
    const speedIndicator = document.getElementById('speedIndicator');
    
    if (!speedIndicator) {
        console.error('Speed control elements not found. Retrying in 100ms...');
        setTimeout(initSpeedControl, 100);
        return;
    }
    
    const speedTrack = speedIndicator.parentElement;
    
    let isDragging = false;
    let startY = 0;
    let initialTop = 0;
    const zeroPosition = 66.67; // 1/3 de baixo para cima em percentual
    
    // Função para calcular o valor da velocidade baseado na posição
    function calculateSpeed(topPercent) {
        // Mapear posição para velocidade (-100 a +100)
        // Zero position é 66.67%
        // Top (0%) = +100, Bottom (100%) = -100
        const relativePosition = (zeroPosition - topPercent) / zeroPosition;
        
        if (topPercent < zeroPosition) {
            // Acima do zero = velocidade positiva
            return Math.round(relativePosition * 100);
        } else {
            // Abaixo do zero = velocidade negativa
            const belowZeroRange = 100 - zeroPosition;
            const belowZeroPosition = (topPercent - zeroPosition) / belowZeroRange;
            return Math.round(-belowZeroPosition * 100);
        }
    }
    
    // Função para atualizar a posição e valor
    function updateSpeed(topPercent) {
        // Limitar entre 0% e 100%
        topPercent = Math.max(0, Math.min(100, topPercent));
        
        speedIndicator.style.top = topPercent + '%';
        const speed = calculateSpeed(topPercent);
        
        // Enviar comando via WebSocket
        sendSpeedCommand(speed);
    }
    
    // Função para retornar ao zero
    function returnToZero() {
        speedIndicator.style.transition = 'top 0.3s ease-out';
        updateSpeed(zeroPosition);
        
        setTimeout(() => {
            speedIndicator.style.transition = 'top 0.1s ease-out';
        }, 300);
    }
    
    // Event listeners para mouse
    speedIndicator.addEventListener('mousedown', (e) => {
        isDragging = true;
        startY = e.clientY;
        initialTop = parseFloat(speedIndicator.style.top) || zeroPosition;
        speedIndicator.style.transition = 'none';
        e.preventDefault();
    });

    // Event listener para clique direto no track
    speedTrack.addEventListener('mousedown', (e) => {
        // Se o clique foi no indicador, deixar o handler dele cuidar
        if (e.target === speedIndicator || speedIndicator.contains(e.target)) {
            return;
        }
        
        const rect = speedTrack.getBoundingClientRect();
        const clickY = e.clientY - rect.top;
        const trackHeight = rect.height;
        const clickPercent = (clickY / trackHeight) * 100;
        
        isDragging = true;
        startY = e.clientY;
        initialTop = clickPercent;
        speedIndicator.style.transition = 'none';
        
        // Mover imediatamente para a posição clicada
        updateSpeed(clickPercent);
        
        e.preventDefault();
    });
    
    document.addEventListener('mousemove', (e) => {
        if (!isDragging) return;
        
        const deltaY = e.clientY - startY;
        const trackHeight = speedTrack.clientHeight;
        const deltaPercent = (deltaY / trackHeight) * 100;
        const newTop = initialTop + deltaPercent;
        
        updateSpeed(newTop);
        e.preventDefault();
    });
    
    document.addEventListener('mouseup', () => {
        if (isDragging) {
            isDragging = false;
            returnToZero();
        }
    });
    
    // Event listeners para touch (dispositivos móveis)
    speedIndicator.addEventListener('touchstart', (e) => {
        isDragging = true;
        startY = e.touches[0].clientY;
        initialTop = parseFloat(speedIndicator.style.top) || zeroPosition;
        speedIndicator.style.transition = 'none';
        e.preventDefault();
    });

    // Event listener para touch direto no track
    speedTrack.addEventListener('touchstart', (e) => {
        // Se o toque foi no indicador, deixar o handler dele cuidar
        if (e.target === speedIndicator || speedIndicator.contains(e.target)) {
            return;
        }
        
        const rect = speedTrack.getBoundingClientRect();
        const touchY = e.touches[0].clientY - rect.top;
        const trackHeight = rect.height;
        const touchPercent = (touchY / trackHeight) * 100;
        
        isDragging = true;
        startY = e.touches[0].clientY;
        initialTop = touchPercent;
        speedIndicator.style.transition = 'none';
        
        // Mover imediatamente para a posição tocada
        updateSpeed(touchPercent);
        
        e.preventDefault();
    });
    
    document.addEventListener('touchmove', (e) => {
        if (!isDragging) return;
        
        const deltaY = e.touches[0].clientY - startY;
        const trackHeight = speedTrack.clientHeight;
        const deltaPercent = (deltaY / trackHeight) * 100;
        const newTop = initialTop + deltaPercent;
        
        updateSpeed(newTop);
        e.preventDefault();
    });
    
    document.addEventListener('touchend', () => {
        if (isDragging) {
            isDragging = false;
            returnToZero();
        }
    });
    
    // Inicializar na posição zero
    updateSpeed(zeroPosition);
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

    // Inicializar controle de velocidade após o DOM estar pronto
    setTimeout(() => {
        initSpeedControl();
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
    
    // WiFi configuration save
    view.html.querySelector('#saveWifiConfig').addEventListener('click', saveWifiConfig);
    
    // OTA upload functionality
    view.html.querySelector('#uploadOTA').addEventListener('click', uploadOTAFirmware);
    
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

const views = {};

function main() {

    // Inicializar WebSocket para comandos (apenas se não estiver em modo DEBUG)
    if (!DEBUG) {
        initWebSocket();
    } else {
        console.log('DEBUG mode enabled: Running in visual test mode without WebSocket');
    }

    views.mainView = new View('mainView', mainCtr);
    views.configurationView = new View('configurationView', netCtr);

    views.mainView.show();
}

window.onload = main;