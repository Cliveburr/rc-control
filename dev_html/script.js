
const DEBUG = false;

/**
 * RCP (RC Control Protocol) Client Library
 * Binary WebSocket protocol for efficient RC vehicle control
 */
class RCPClient {
    constructor(websocket) {
        this.ws = websocket;
        
        // RCP Protocol constants
        this.RCP_SYNC_BYTE = 0xAA;
        
        // Port definitions
        this.RCP_PORTS = {
            // Control Commands (0x01-0x0F)
            MOTOR: 0x01,        // Motor speed control
            SERVO: 0x02,        // Servo steering control
            HORN: 0x03,         // Horn on/off
            LIGHT: 0x04,        // Light on/off
            
            // System Commands (0x10-0x1F)
            SYSTEM: 0x10,       // System commands
            CONFIG: 0x11,       // Configuration
            STATUS: 0x12,       // Status requests
            
            // Response Commands (0x80-0xFF)
            BATTERY: 0x80,      // Battery status response
            TELEMETRY: 0x81,    // Telemetry data response
            ACK: 0xFF           // Acknowledgment
        };
        
        // System commands
        this.RCP_SYS_COMMANDS = {
            PING: 0x01,         // Ping request
            RESET: 0x02,        // Reset system
            STATUS: 0x03,       // Request status
            CONFIG: 0x04        // Configuration request
        };
        
        // Command statistics
        this.stats = {
            commandsSent: 0,
            responsesReceived: 0,
            errors: 0
        };
        
        console.log('RCP Client v1.0 initialized');
    }
    
    /**
     * Calculate CRC8 checksum for RCP message
     * @param {Uint8Array} data - Message data (excluding checksum byte)
     * @returns {number} CRC8 checksum
     */
    calculateChecksum(data) {
        // CRC8 lookup table for faster computation
        const crc8Table = new Uint8Array([
            0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
            0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
            0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
            0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
            0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
            0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
            0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
            0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
            0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
            0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
            0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
            0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
            0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
            0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
            0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
            0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
            0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
            0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
            0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
            0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
            0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
            0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
            0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
            0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
            0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
            0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
            0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
            0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
            0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
            0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
            0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
            0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
        ]);
        
        let crc = this.RCP_SYNC_BYTE; // Start with sync byte value
        
        // Calculate CRC8 for all bytes except the last one (checksum byte)
        for (let i = 0; i < data.length - 1; i++) {
            crc = crc8Table[crc ^ data[i]];
        }
        
        return crc;
    }
    
    /**
     * Validate RCP message checksum
     * @param {Uint8Array} data - Complete message data
     * @returns {boolean} true if checksum is valid
     */
    validateChecksum(data) {
        if (data.length < 3) return false;
        
        const receivedChecksum = data[data.length - 1];
        const calculatedChecksum = this.calculateChecksum(data);
        
        return receivedChecksum === calculatedChecksum;
    }
    
    /**
     * Validate RCP command parameters
     * @param {number} port - Command port
     * @param {Uint8Array} payload - Command payload
     * @returns {boolean} true if valid
     */
    validateCommand(port, payload) {
        // Validate port
        if (typeof port !== 'number' || port < 0 || port > 255) {
            console.error('RCP: Invalid port:', port);
            return false;
        }
        
        // Validate payload
        if (!(payload instanceof Uint8Array)) {
            console.error('RCP: Payload is not Uint8Array:', payload);
            return false;
        }
        
        // Validate payload size
        if (payload.length > 14) { // Max payload size for RCP
            console.error('RCP: Payload too large:', payload.length);
            return false;
        }
        
        // Validate specific command structures
        if (port === this.RCP_PORTS.SYSTEM && payload.length !== 2) {
            console.error('RCP: System command requires 2 bytes payload (command + param), got:', payload.length);
            return false;
        }
        
        if (port === this.RCP_PORTS.MOTOR && payload.length !== 1) {
            console.error('RCP: Motor command requires 1 byte payload, got:', payload.length);
            return false;
        }
        
        if (port === this.RCP_PORTS.SERVO && payload.length !== 1) {
            console.error('RCP: Servo command requires 1 byte payload, got:', payload.length);
            return false;
        }
        
        if ((port === this.RCP_PORTS.HORN || port === this.RCP_PORTS.LIGHT) && payload.length !== 1) {
            console.error('RCP: Horn/Light command requires 1 byte payload, got:', payload.length);
            return false;
        }
        
        return true;
    }

    /**
     * Send RCP command
     * @param {number} port - Command port
     * @param {Uint8Array} payload - Command payload (without header and checksum)
     */
    sendCommand(port, payload = new Uint8Array(0)) {
        // Validate command parameters first
        if (!this.validateCommand(port, payload)) {
            this.stats.errors++;
            return false;
        }
        
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            console.warn('RCP: WebSocket not connected');
            this.stats.errors++;
            return false;
        }
        
        // Create message buffer: [sync][port][payload][checksum]
        const messageSize = 2 + payload.length + 1; // header + payload + checksum
        const buffer = new ArrayBuffer(messageSize);
        const data = new Uint8Array(buffer);
        
        // Fill message
        data[0] = this.RCP_SYNC_BYTE;
        data[1] = port;
        
        // Copy payload
        if (payload.length > 0) {
            data.set(payload, 2);
        }
        
        // Calculate and set checksum
        data[messageSize - 1] = this.calculateChecksum(data);
        
        // Comprehensive WebSocket readiness check
        if (!this.ws) {
            console.warn('RCP: WebSocket instance is null');
            this.stats.errors++;
            return false;
        }
        
        if (this.ws.readyState !== WebSocket.OPEN) {
            console.warn('RCP: WebSocket not ready for sending (readyState:', this.ws.readyState, 'expected:', WebSocket.OPEN, ')');
            this.stats.errors++;
            return false;
        }
        
        if (this.ws.binaryType !== 'arraybuffer') {
            console.error('RCP: WebSocket binaryType is not arraybuffer:', this.ws.binaryType);
            this.stats.errors++;
            return false;
        }
        
        try {
            // Validate buffer before sending
            if (!(buffer instanceof ArrayBuffer)) {
                throw new Error('Buffer is not an ArrayBuffer instance');
            }
            
            if (buffer.byteLength === 0) {
                throw new Error('Buffer is empty');
            }
            
            // Send ArrayBuffer directly - browser will handle masking automatically
            this.ws.send(buffer);
            this.stats.commandsSent++;
            
            if (DEBUG) {
                console.log(`RCP: Sent command port=0x${port.toString(16).padStart(2, '0').toUpperCase()}, size=${messageSize} bytes`);
                // Log buffer contents for debugging
                const debugData = new Uint8Array(buffer);
                console.log('RCP: Buffer contents:', Array.from(debugData).map(b => '0x' + b.toString(16).padStart(2, '0')).join(' '));
                
                // Verify message structure
                const expectedChecksum = this.calculateChecksum(debugData);
                const actualChecksum = debugData[debugData.length - 1];
                console.log('RCP: Checksum validation - Expected:', expectedChecksum, 'Actual:', actualChecksum, 'Valid:', expectedChecksum === actualChecksum);
            }
            
            // Always verify critical aspects even in non-DEBUG mode
            const verifyData = new Uint8Array(buffer);
            if (verifyData[0] !== this.RCP_SYNC_BYTE) {
                console.error('RCP: CRITICAL - Sync byte incorrect!', verifyData[0], 'expected:', this.RCP_SYNC_BYTE);
            }
            if (verifyData[1] !== port) {
                console.error('RCP: CRITICAL - Port byte incorrect!', verifyData[1], 'expected:', port);
            }
            return true;
        } catch (error) {
            console.error('RCP: Failed to send command:', error);
            console.error('RCP: WebSocket state at error:', {
                readyState: this.ws.readyState,
                binaryType: this.ws.binaryType,
                url: this.ws.url
            });
            this.stats.errors++;
            return false;
        }
    }
    
    /**
     * Send motor speed command
     * @param {number} speed - Motor speed (-100 to +100)
     */
    sendMotorCommand(speed) {
        // Clamp speed to valid range
        speed = Math.max(-100, Math.min(100, Math.round(speed)));
        
        const payload = new Uint8Array(1);
        payload[0] = speed; // Signed byte will be interpreted correctly
        
        if (DEBUG) console.log(`RCP: Sending motor command: speed=${speed}`);
        return this.sendCommand(this.RCP_PORTS.MOTOR, payload);
    }
    
    /**
     * Send servo angle command  
     * @param {number} angle - Servo angle (-100 to +100)
     */
    sendServoCommand(angle) {
        // Clamp angle to valid range
        angle = Math.max(-100, Math.min(100, Math.round(angle)));
        
        const payload = new Uint8Array(1);
        payload[0] = angle; // Signed byte will be interpreted correctly
        
        if (DEBUG) console.log(`RCP: Sending servo command: angle=${angle}`);
        return this.sendCommand(this.RCP_PORTS.SERVO, payload);
    }
    
    /**
     * Send horn command
     * @param {boolean} state - Horn state (true=ON, false=OFF)
     */
    sendHornCommand(state) {
        const payload = new Uint8Array(1);
        payload[0] = state ? 1 : 0;
        
        if (DEBUG) console.log(`RCP: Sending horn command: ${state ? 'ON' : 'OFF'}`);
        return this.sendCommand(this.RCP_PORTS.HORN, payload);
    }
    
    /**
     * Send light command
     * @param {boolean} state - Light state (true=ON, false=OFF)
     */
    sendLightCommand(state) {
        const payload = new Uint8Array(1);
        payload[0] = state ? 1 : 0;
        
        if (DEBUG) console.log(`RCP: Sending light command: ${state ? 'ON' : 'OFF'}`);
        return this.sendCommand(this.RCP_PORTS.LIGHT, payload);
    }
    
    /**
     * Process incoming RCP response
     * @param {ArrayBuffer} data - Response data
     */
    processResponse(data) {
        const view = new DataView(data);
        const dataArray = new Uint8Array(data);
        
        if (data.byteLength < 3) {
            console.warn('RCP: Response too short');
            this.stats.errors++;
            return;
        }
        
        const sync = view.getUint8(0);
        const port = view.getUint8(1);
        
        // Validate sync byte
        if (sync !== this.RCP_SYNC_BYTE) {
            console.warn(`RCP: Invalid sync byte 0x${sync.toString(16)} (expected 0x${this.RCP_SYNC_BYTE.toString(16)})`);
            this.stats.errors++;
            return;
        }
        
        // Validate checksum
        if (!this.validateChecksum(dataArray)) {
            console.warn('RCP: Response checksum mismatch');
            this.stats.errors++;
            return;
        }
        
        this.stats.responsesReceived++;
        
        // Route response to appropriate handler
        switch (port) {
            case this.RCP_PORTS.BATTERY:
                this.processBatteryResponse(view, dataArray);
                break;
                
            case this.RCP_PORTS.TELEMETRY:
                this.processTelemetryResponse(view, dataArray);
                break;
                
            case this.RCP_PORTS.ACK:
                if (DEBUG) console.log('RCP: Acknowledgment received');
                break;
                
            default:
                console.warn(`RCP: Unknown response port 0x${port.toString(16)}`);
                this.stats.errors++;
        }
    }
    
    /**
     * Process battery status response
     * @param {DataView} view - Data view
     * @param {Uint8Array} data - Raw data array
     */
    processBatteryResponse(view, data) {
        if (data.length !== 7) {
            console.warn('RCP: Invalid battery response size');
            this.stats.errors++;
            return;
        }
        
        const voltage_mv = view.getUint16(2, true); // Little endian
        const level = view.getUint8(4);
        const type = view.getUint8(5);
        
        const voltage = voltage_mv / 1000.0;
        
        if (DEBUG) console.log(`RCP: Battery status - ${voltage.toFixed(2)}V, Level: ${level}/10, Type: ${type}S`);
        
        // Update battery level in UI
        updateBatteryLevel(level);
        
        // Update battery type for calculations
        batteryType = `${type}S`;
    }
    
    /**
     * Process telemetry response
     * @param {DataView} view - Data view
     * @param {Uint8Array} data - Raw data array
     */
    processTelemetryResponse(view, data) {
        if (data.length !== 8) {
            console.warn('RCP: Invalid telemetry response size');
            this.stats.errors++;
            return;
        }
        
        const speed = view.getInt8(2);        // Signed
        const angle = view.getInt8(3);        // Signed  
        const hornState = view.getUint8(4);
        const lightState = view.getUint8(5);
        const flags = view.getUint8(6);
        
        if (DEBUG) console.log(`RCP: Telemetry - Speed: ${speed}, Angle: ${angle}, Horn: ${hornState ? 'ON' : 'OFF'}, Light: ${lightState ? 'ON' : 'OFF'}, Flags: 0x${flags.toString(16)}`);
    }
    
    /**
     * Get protocol statistics
     * @returns {Object} Statistics object
     */
    getStats() {
        return { ...this.stats };
    }
    
    /**
     * Reset statistics
     */
    resetStats() {
        this.stats.commandsSent = 0;
        this.stats.responsesReceived = 0;
        this.stats.errors = 0;
    }
}

function makeView() {
    const img = document.getElementsByTagName('img')[0];
    img.src = '/video?' + new Date().getTime();
}

// WebSocket connection for sending commands
let ws = null;
let rcpClient = null; // RCP Client instance
let batteryType = '1S'; // Default battery type, will be updated from ESP32
let wsReconnectAttempts = 0;
let maxReconnectAttempts = 100;
let reconnectInterval = 3000; // Start with 3 seconds

// Command caching to avoid sending duplicate values
let lastSpeedValue = null;
let lastWheelsValue = null;
let lastHornValue = null;
let lastLightValue = null;

// Command throttling to prevent spam during multi-touch
let speedCommandTimeout = null;
let wheelsCommandTimeout = null;
const COMMAND_THROTTLE_MS = 50; // Minimum time between commands

// Multi-touch monitoring
let activeControls = new Set();

function setControlActive(controlType, active) {
    if (active) {
        activeControls.add(controlType);
    } else {
        activeControls.delete(controlType);
    }
    
    if (DEBUG && activeControls.size > 1) {
        // console.log(`Multi-touch detected: ${Array.from(activeControls).join(', ')} controls active`);
    }
}

// Function to reset command cache (useful after reconnection)
function resetCommandCache() {
    lastSpeedValue = null;
    lastWheelsValue = null;
    lastHornValue = null;
    lastLightValue = null;
    
    // Clear any pending throttled commands
    if (speedCommandTimeout) {
        clearTimeout(speedCommandTimeout);
        speedCommandTimeout = null;
    }
    if (wheelsCommandTimeout) {
        clearTimeout(wheelsCommandTimeout);
        wheelsCommandTimeout = null;
    }
    
    // Reset RCP statistics
    if (rcpClient) {
        rcpClient.resetStats();
    }
    
    console.log('Command cache reset - next commands will be sent regardless of previous values');
}

// Function to show RCP statistics
function showRCPStats() {
    if (rcpClient) {
        const stats = rcpClient.getStats();
        console.log('=== RCP Protocol Statistics ===');
        console.log(`Commands Sent: ${stats.commandsSent}`);
        console.log(`Responses Received: ${stats.responsesReceived}`);
        console.log(`Errors: ${stats.errors}`);
        console.log(`Success Rate: ${stats.commandsSent > 0 ? ((stats.commandsSent - stats.errors) / stats.commandsSent * 100).toFixed(1) : 0}%`);
        console.log('==============================');
        return stats;
    } else {
        console.log('RCP Client not initialized');
        return null;
    }
}

// Expose showRCPStats globally for console access
window.showRCPStats = showRCPStats;

/**
 * Check WebSocket health and log diagnostic information
 */
function checkWebSocketHealth() {
    if (!ws) {
        console.warn('WebSocket Health: No WebSocket instance');
        return false;
    }
    
    const health = {
        readyState: ws.readyState,
        readyStateText: ['CONNECTING', 'OPEN', 'CLOSING', 'CLOSED'][ws.readyState],
        binaryType: ws.binaryType,
        url: ws.url,
        protocol: ws.protocol,
        extensions: ws.extensions
    };
    
    console.log('WebSocket Health Check:', health);
    
    if (ws.readyState !== WebSocket.OPEN) {
        console.warn('WebSocket Health: Connection not OPEN');
        return false;
    }
    
    if (ws.binaryType !== 'arraybuffer') {
        console.error('WebSocket Health: Binary type is not arraybuffer!');
        return false;
    }
    
    return true;
}

/**
 * Test WebSocket connectivity with a simple binary frame
 */
function testWebSocketConnectivity() {
    if (!checkWebSocketHealth()) {
        console.error('WebSocket health check failed - skipping connectivity test');
        return false;
    }
    
    if (!rcpClient) {
        console.warn('RCP Client not initialized - skipping connectivity test');
        return false;
    }
    
    try {
        // Send a simple ping command to test connectivity
        console.log('Testing WebSocket connectivity with RCP ping...');
        // RCP system command requires: [command, param] = 2 bytes payload
        const success = rcpClient.sendCommand(rcpClient.RCP_PORTS.SYSTEM, new Uint8Array([rcpClient.RCP_SYS_COMMANDS.PING, 0x00]));
        
        if (success) {
            console.log('WebSocket connectivity test: PASSED');
        } else {
            console.error('WebSocket connectivity test: FAILED');
        }
        
        return success;
    } catch (error) {
        console.error('WebSocket connectivity test error:', error);
        return false;
    }
}

/**
 * Comprehensive WebSocket diagnostics
 */
function runWebSocketDiagnostics() {
    console.log('=== WebSocket Diagnostics ===');
    
    // 1. Health check
    const healthy = checkWebSocketHealth();
    console.log('Health Check:', healthy ? 'PASS' : 'FAIL');
    
    // 2. RCP Client check
    if (!rcpClient) {
        console.error('RCP Client: NOT INITIALIZED');
        return false;
    }
    console.log('RCP Client: OK');
    
    // 3. Test connectivity
    const connected = testWebSocketConnectivity();
    console.log('Connectivity Test:', connected ? 'PASS' : 'FAIL');
    
    // 4. Show RCP statistics
    showRCPStats();
    
    // 5. Test different command types
    console.log('Testing different RCP commands...');
    
    // Test motor command
    const motorTest = rcpClient.sendCommand(rcpClient.RCP_PORTS.MOTOR, new Uint8Array([0]));
    console.log('Motor Command Test:', motorTest ? 'PASS' : 'FAIL');
    
    // Test servo command  
    const servoTest = rcpClient.sendCommand(rcpClient.RCP_PORTS.SERVO, new Uint8Array([0]));
    console.log('Servo Command Test:', servoTest ? 'PASS' : 'FAIL');
    
    // Test horn command
    const hornTest = rcpClient.sendCommand(rcpClient.RCP_PORTS.HORN, new Uint8Array([0]));
    console.log('Horn Command Test:', hornTest ? 'PASS' : 'FAIL');
    
    // Test light command
    const lightTest = rcpClient.sendCommand(rcpClient.RCP_PORTS.LIGHT, new Uint8Array([0]));
    console.log('Light Command Test:', lightTest ? 'PASS' : 'FAIL');
    
    console.log('=== Diagnostics Complete ===');
    return healthy && connected;
}

// Expose diagnostic functions globally
window.checkWebSocketHealth = checkWebSocketHealth;
window.testWebSocketConnectivity = testWebSocketConnectivity;
window.runWebSocketDiagnostics = runWebSocketDiagnostics;



function initWebSocket() {
    if (DEBUG) {
        // console.log('DEBUG mode: WebSocket disabled for testing');
        return;
    }
    
    try {
        // Close existing connection if any
        if (ws && ws.readyState !== WebSocket.CLOSED) {
            ws.close();
        }
        
        // Create WebSocket with proper configuration
        const wsUrl = `ws://${window.location.host}/ws`;
        console.log('Connecting to WebSocket:', wsUrl);
        
        ws = new WebSocket(wsUrl);
        
        // Verify WebSocket was created successfully
        if (!ws) {
            throw new Error('Failed to create WebSocket instance');
        }
        
        // CRITICAL: Set binary type IMMEDIATELY after creation
        ws.binaryType = 'arraybuffer'; // Enable binary frame support for RCP
        
        // Force browser to respect our settings
        Object.defineProperty(ws, 'binaryType', {
            value: 'arraybuffer',
            writable: false,
            enumerable: true,
            configurable: false
        });
        
        // Verify WebSocket is properly configured
        console.log('WebSocket created - binaryType:', ws.binaryType, 'readyState:', ws.readyState);
        
        // Additional WebSocket validation
        if (ws.binaryType !== 'arraybuffer') {
            console.error('CRITICAL: WebSocket binaryType could not be set to arraybuffer!');
            throw new Error('WebSocket configuration failed');
        }
        
        // Set a timeout for connection
        let connectionTimeout = setTimeout(() => {
            if (ws.readyState === WebSocket.CONNECTING) {
                console.warn('WebSocket connection timeout, closing...');
                ws.close();
            }
        }, 5000); // 5 second timeout
        
        ws.onopen = () => {
            clearTimeout(connectionTimeout);
            console.log('WebSocket connected for control commands');
            wsReconnectAttempts = 0; // Reset reconnect attempts on successful connection
            reconnectInterval = 3000; // Reset reconnect interval
            
            // Verify WebSocket configuration after connection
            console.log('WebSocket post-connection check:', {
                readyState: ws.readyState,
                binaryType: ws.binaryType,
                url: ws.url
            });
            
            // Re-ensure binary type is set (some browsers might reset it)
            if (ws.binaryType !== 'arraybuffer') {
                console.warn('WebSocket binaryType was reset - fixing...');
                ws.binaryType = 'arraybuffer';
            }
            
            // Initialize RCP client
            rcpClient = new RCPClient(ws);
            console.log('RCP Client initialized and ready');
            
            // Reset command cache to ensure fresh state after reconnection
            resetCommandCache();
            
            // Run connectivity test after a short delay
            setTimeout(() => {
                testWebSocketConnectivity();
            }, 1000);
        };
        
        ws.onmessage = (event) => {
            try {
                // Handle binary frames (RCP responses)
                if (event.data instanceof ArrayBuffer) {
                    if (DEBUG) console.log(`Received binary frame (${event.data.byteLength} bytes)`);
                    if (rcpClient) {
                        rcpClient.processResponse(event.data);
                    }
                    return;
                }
                
                // Handle binary frames as Blob (ping/pong)
                if (event.data instanceof Blob) {
                    // Convert blob to arraybuffer to check frame type
                    event.data.arrayBuffer().then(buffer => {
                        // Check if this is an RCP response or ping/pong
                        if (buffer.byteLength >= 3) {
                            const view = new Uint8Array(buffer);
                            if (view[0] === 0xAA) { // RCP sync byte
                                if (rcpClient) {
                                    rcpClient.processResponse(buffer);
                                }
                                return;
                            }
                        }
                        
                        // This is likely a ping frame, respond with pong
                        if (ws && ws.readyState === WebSocket.OPEN) {
                            ws.pong && ws.pong();
                        }
                    });
                    return;
                }
            } catch (error) {
                console.error('Error handling WebSocket message:', error);
                console.error('Message type:', typeof event.data, 'Data:', event.data);
            }
        };
        
        ws.onerror = (error) => {
            console.error('WebSocket error occurred:', error);
            console.error('WebSocket state:', {
                readyState: ws.readyState,
                binaryType: ws.binaryType,
                url: ws.url,
                protocol: ws.protocol,
                extensions: ws.extensions
            });
            
            // Log additional context for debugging
            console.error('Error context:', {
                wsReconnectAttempts: wsReconnectAttempts,
                maxReconnectAttempts: maxReconnectAttempts,
                reconnectInterval: reconnectInterval,
                activeControls: Array.from(activeControls),
                rcpClientExists: !!rcpClient
            });
            
            // Clear RCP client on error
            rcpClient = null;
        };
        
        ws.onclose = (event) => {
            clearTimeout(connectionTimeout);
            console.log('WebSocket connection closed:', {
                code: event.code,
                reason: event.reason,
                wasClean: event.wasClean,
                readyState: ws.readyState
            });
            
            // Clear RCP client on disconnect
            rcpClient = null;
            
            // Only attempt reconnection if not manually closed and under attempt limit
            if (event.code !== 1000 && wsReconnectAttempts < maxReconnectAttempts) {
                wsReconnectAttempts++;
                // Exponential backoff with jitter
                const jitter = Math.random() * 1000;
                const delay = Math.min(reconnectInterval * Math.pow(1.5, wsReconnectAttempts - 1) + jitter, 30000);
                
                console.warn(`WebSocket disconnected (attempt ${wsReconnectAttempts}/${maxReconnectAttempts}), reconnecting in ${Math.round(delay/1000)}s...`);
                setTimeout(initWebSocket, delay);
            } else if (wsReconnectAttempts >= maxReconnectAttempts) {
                console.error('WebSocket max reconnection attempts reached. Please refresh the page.');
            }
        };
        
        ws.onerror = (error) => {
            clearTimeout(connectionTimeout);
            console.error('WebSocket error:', error);
        };
        
    } catch (error) {
        console.error('Failed to initialize WebSocket:', error);
        // Retry connection after delay if initialization fails
        if (wsReconnectAttempts < maxReconnectAttempts) {
            wsReconnectAttempts++;
            setTimeout(initWebSocket, reconnectInterval);
        }
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
    // Clear any pending speed command
    if (speedCommandTimeout) {
        clearTimeout(speedCommandTimeout);
    }
    
    // Throttle speed commands to prevent spam during multi-touch
    speedCommandTimeout = setTimeout(() => {
        // Only send if value has changed
        if (lastSpeedValue !== value) {
            sendControlCommand('speed', value);
            lastSpeedValue = value;
        } else {
            // if (DEBUG) console.log(`Speed command skipped - same value as last: ${value}`);
        }
        speedCommandTimeout = null;
    }, COMMAND_THROTTLE_MS);
}

function sendWheelsCommand(value) {
    // Clear any pending wheels command
    if (wheelsCommandTimeout) {
        clearTimeout(wheelsCommandTimeout);
    }
    
    // Throttle wheels commands to prevent spam during multi-touch
    wheelsCommandTimeout = setTimeout(() => {
        // Only send if value has changed
        if (lastWheelsValue !== value) {
            sendControlCommand('wheels', value);
            lastWheelsValue = value;
        } else {
            // if (DEBUG) console.log(`Wheels command skipped - same value as last: ${value}`);
        }
        wheelsCommandTimeout = null;
    }, COMMAND_THROTTLE_MS);
}

function sendControlCommand(type, value) {
    // Add validation to ensure value is a number
    if (typeof value !== 'number' || isNaN(value)) {
        console.error(`Invalid ${type} command value:`, value);
        return;
    }
    
    // Clamp value to valid range
    if (type === 'speed' || type === 'wheels') {
        value = Math.max(-100, Math.min(100, value));
    }
    
    if (DEBUG) {
        console.log(`DEBUG mode: ${type} command (visual test only):`, value);
        return;
    }
    
    if (ws && ws.readyState === WebSocket.OPEN && rcpClient) {
        // Use RCP client for all communication
        switch (type) {
            case 'speed':
                rcpClient.sendMotorCommand(value);
                break;
            case 'wheels':
                rcpClient.sendServoCommand(value);
                break;
            case 'horn':
                rcpClient.sendHornCommand(value);
                break;
            case 'light':
                rcpClient.sendLightCommand(value);
                break;
            default:
                console.warn(`Unknown RCP command type: ${type}`);
                return;
        }
    } else {
        console.warn(`WebSocket not connected or RCP client not available, cannot send ${type} command:`, value);
        // Attempt to reconnect if not already connecting
        if (!ws || ws.readyState === WebSocket.CLOSED) {
            initWebSocket();
        }
    }
}

function sendHornCommand(isPressed) {
    const hornValue = isPressed ? 1 : 0;
    
    // Only send if value has changed
    if (lastHornValue !== hornValue) {
        if (DEBUG) {
            console.log('DEBUG mode: Horn command (visual test only):', isPressed);
            lastHornValue = hornValue;
            return;
        }
        
        if (ws && ws.readyState === WebSocket.OPEN && rcpClient) {
            rcpClient.sendHornCommand(isPressed);
            lastHornValue = hornValue;
        } else {
            console.warn('WebSocket not connected or RCP client not available, cannot send horn command:', isPressed);
        }
    }
}

function sendLightCommand(isOn) {
    const lightValue = isOn ? 1 : 0;
    
    // Only send if value has changed
    if (lastLightValue !== lightValue) {
        if (DEBUG) {
            console.log('DEBUG mode: Light command (visual test only):', isOn);
            lastLightValue = lightValue;
            return;
        }
        
        if (ws && ws.readyState === WebSocket.OPEN && rcpClient) {
            rcpClient.sendLightCommand(isOn);
            lastLightValue = lightValue;
        } else {
            console.warn('WebSocket not connected or RCP client not available, cannot send light command:', isOn);
        }
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
        
        // if (DEBUG) console.log(`${controlType}: Posição ${positionPercent.toFixed(1)}%, Valor: ${value}`);
    }
    
    // Função para forçar reset ao zero (útil para reset manual)
    function forceResetToZero() {
        // if (DEBUG) console.log(`${controlType}: Reset forçado ao zero`);
        
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
        // if (DEBUG) console.log(`${controlType}: Verificando zona de reset`);
        
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
        
        // Mark this control as active for multi-touch monitoring
        setControlActive(controlType, true);
        
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
                
                // Se não há mais toques, retornar ao zero e marcar como inativo
                if (touchCache.length === 0) {
                    if (DEBUG) console.log(`${controlType}: Último toque removido, retornando ao zero`);
                    setControlActive(controlType, false);
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
        
        // Mark this control as active
        setControlActive(controlType, true);
        
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
        setControlActive(controlType, false);
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
    
    // if (DEBUG) console.log(`${controlType}: Controle inicializado com cache local`);
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
    // console.log('WiFi Config:', { ssid, password });
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
        
        let data;
        if (DEBUG) {
            // Mock data for debug mode
            data = {
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
                    }
                },
                ws_clients: 1
            };
        } else {
            // Fetch binary system info data
            const response = await fetch('/api/system-info');
            
            if (!response.ok) {
                throw new Error('Failed to fetch system info');
            }
            
            // Get binary data as ArrayBuffer
            const arrayBuffer = await response.arrayBuffer();
            data = parseBinarySystemInfo(arrayBuffer);
        }
        
        updateSystemInfoDisplay(data);
        
    } catch (error) {
        console.error('Erro ao carregar informações do sistema:', error);
        showSystemInfoError();
    }
}

/**
 * Parse binary system info data from ESP32
 * Binary structure (32 bytes):
 * [0] chip_model (1 byte): 0=Unknown, 1=ESP32, 2=ESP32-S2, 3=ESP32-S3, 4=ESP32-C3
 * [1] revision (1 byte)
 * [2] cores (1 byte)  
 * [3-4] cpu_freq (2 bytes, little endian)
 * [5] features (1 byte): bit 0=WiFi, bit 1=Bluetooth, bit 2=BLE
 * [6-9] flash_size_mb (4 bytes, little endian)
 * [10-13] heap_total_kb (4 bytes, little endian)
 * [14-17] heap_used_kb (4 bytes, little endian)
 * [18-21] heap_free_kb (4 bytes, little endian)
 * [22] ws_clients (1 byte)
 * [23] heap_usage_percent (1 byte)
 * [24-31] reserved (8 bytes)
 */
function parseBinarySystemInfo(arrayBuffer) {
    const view = new DataView(arrayBuffer);
    
    // Chip model
    const chipModelId = view.getUint8(0);
    const chipModels = ['Unknown', 'ESP32', 'ESP32-S2', 'ESP32-S3', 'ESP32-C3'];
    const chipModel = chipModels[chipModelId] || 'Unknown';
    
    // Basic info
    const revision = view.getUint8(1);
    const cores = view.getUint8(2);
    const cpuFreq = view.getUint16(3, true); // little endian
    
    // Features
    const features = view.getUint8(5);
    const hasWifi = (features & 0x01) !== 0;
    const hasBluetooth = (features & 0x02) !== 0;
    const hasBle = (features & 0x04) !== 0;
    
    // Memory info
    const flashSizeMb = view.getUint32(6, true); // little endian
    const heapTotalKb = view.getUint32(10, true);
    const heapUsedKb = view.getUint32(14, true);
    const heapFreeKb = view.getUint32(18, true);
    
    // Additional info
    const wsClients = view.getUint8(22);
    const heapUsagePercent = view.getUint8(23);
    
    console.log('RCP Binary System Info received:', {
        model: chipModel,
        revision: revision,
        cores: cores,
        freq: cpuFreq,
        features: `0x${features.toString(16)}`,
        flash: flashSizeMb,
        heap: `${heapUsedKb}/${heapTotalKb}KB (${heapUsagePercent}%)`,
        clients: wsClients
    });
    
    // Convert to expected format for compatibility with existing UI
    return {
        chip: {
            model: chipModel,
            cores: cores,
            revision: revision,
            cpu_freq_mhz: cpuFreq,
            has_wifi: hasWifi,
            has_bluetooth: hasBluetooth,
            has_ble: hasBle,
            flash_size_mb: flashSizeMb
        },
        memory: {
            heap: {
                total_bytes: heapTotalKb * 1024,
                used_bytes: heapUsedKb * 1024,
                free_bytes: heapFreeKb * 1024,
                usage_percent: heapUsagePercent
            }
        },
        ws_clients: wsClients
    };
}

function resetSystemInfoDisplay() {
    // Reset chip information to loading state
    const chipElements = [
        'chipModel', 'chipCores', 'chipRevision', 'cpuFreq',
        'hasWifi', 'hasBluetooth', 'flashSize'
    ];
    
    chipElements.forEach(id => {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = 'Carregando...';
        }
    });
    
    // Reset heap memory info to loading state
    const heapElements = ['heapTotal', 'heapUsed', 'heapFree', 'heapUsage'];
    heapElements.forEach(id => {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = 'Carregando...';
        }
    });
    
    // Reset other memory types to N/A (not available in binary format)
    const unavailableMemTypes = ['psram', 'dma', 'iram', 'dram'];
    unavailableMemTypes.forEach(type => {
        const elements = [type + 'Total', type + 'Used', type + 'Free', type + 'Usage'];
        elements.forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                element.textContent = 'N/A';
            }
        });
    });
    
    // Reset all progress bars
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
    
    // Update memory information (only heap data available in binary format)
    if (data.memory && data.memory.heap) {
        updateMemoryInfo('heap', data.memory.heap);
    }
    
    // For non-available memory types, show as unavailable
    const unavailableMemTypes = ['psram', 'dma', 'iram', 'dram'];
    unavailableMemTypes.forEach(type => {
        const elements = [type + 'Total', type + 'Used', type + 'Free', type + 'Usage'];
        elements.forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                element.textContent = 'N/A';
            }
        });
        
        // Set progress bar to 0
        const progressBar = document.getElementById(type + 'ProgressBar');
        if (progressBar) {
            progressBar.style.width = '0%';
        }
    });
    
    // Show WebSocket clients count if available
    if (data.ws_clients !== undefined) {
        console.log(`RCP Binary System Info: ${data.ws_clients} WebSocket client(s) connected`);
    }
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
        'heapTotal', 'heapUsed', 'heapFree', 'heapUsage'
    ];
    
    elements.forEach(id => {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = 'Erro';
        }
    });
    
    // Set other memory types to N/A on error
    const unavailableMemTypes = ['psram', 'dma', 'iram', 'dram'];
    unavailableMemTypes.forEach(type => {
        const elements = [type + 'Total', type + 'Used', type + 'Free', type + 'Usage'];
        elements.forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                element.textContent = 'N/A';
            }
        });
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
            // console.log('Battery level debug:', batteryLevel);
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
        // console.log('DEBUG mode enabled: Running in visual test mode without WebSocket');
        // Start battery debug loop
        startBatteryDebugLoop();
    }

    views.mainView = new View('mainView', mainCtr);
    views.configurationView = new View('configurationView', netCtr);

    views.mainView.show();
}

window.onload = main;