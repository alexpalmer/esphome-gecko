#include <Arduino.h>
#include <Wire.h>

#define SPA_ADDRESS 0x17
#define SERIAL_BAUD 115200

// Buffers
volatile uint8_t i2cBuffer[128];
volatile uint8_t i2cBufferLen = 0;
volatile bool newI2CMessage = false;

char uartBuffer[64];
uint8_t uartBufferPos = 0;

// State tracking
bool lightState = false;
bool pumpState = false;
bool circState = false;
bool heatingState = false;
bool standbyState = false;
uint8_t programId = 0xFF;
float targetTemp = 0;
float actualTemp = 0;
bool spaConnected = false;
unsigned long lastGoReceived = 0;

// Forward declarations
void receiveEvent(int numBytes);
void requestEvent();
void processUartCommand(const char* cmd);
void sendI2CMessage(uint8_t* msg, uint8_t len);
void parseStatusMessage();
void sendStatusUpdate();

// ============== GO Response Messages ==============
// Response 1: type=0x0 sub=0x5
uint8_t goResponse1[78] = {
    0x17, 0x09, 0x00, 0x00, 0x00, 0x17, 0x0A, 0x01,
    0x00, 0x01, 0x00, 0x00, 0x40, 0xC7, 0x52, 0x51,
    0x00, 0x00, 0x05, 0x02, 0xA3, 0x02, 0x08, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x0C,
    0x00, 0x01, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x02, 0x1E, 0x00, 0x00,
    0x00, 0x0A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0C, 0x01, 0x01, 0x30,
    0x14, 0x78, 0x78, 0x00, 0x28, 0x2A
};

// Response 2: type=0x3B sub=0x2
uint8_t goResponse2[78] = {
    0x17, 0x09, 0x00, 0x00, 0x00, 0x17, 0x0A, 0x01,
    0x00, 0x01, 0x00, 0x00, 0x40, 0xC7, 0x52, 0x51,
    0x00, 0x3B, 0x02, 0x04, 0x04, 0x00, 0x28, 0x00,
    0x73, 0x00, 0x90, 0x02, 0xE4, 0x00, 0x01, 0x24,
    0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x0F, 0x02,
    0xE5, 0x00, 0x02, 0x02, 0x02, 0x01, 0x01, 0x05,
    0x03, 0x03, 0x05, 0x05, 0x04, 0x04, 0x01, 0x03,
    0x00, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x07,
    0x06, 0x08, 0x06, 0x07, 0x07, 0xF4, 0x00, 0x02,
    0x00, 0x03, 0x00, 0x00, 0x00, 0xA1
};

// Response 3: type=0x76 sub=0x3C
uint8_t goResponse3[78] = {
    0x17, 0x09, 0x00, 0x00, 0x00, 0x17, 0x0A, 0x01,
    0x00, 0x01, 0x00, 0x00, 0x40, 0xC7, 0x52, 0x51,
    0x00, 0x76, 0x3C, 0x04, 0x1E, 0x01, 0x0A, 0x30,
    0x40, 0x80, 0x00, 0x83, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x30, 0x14, 0x10, 0x30, 0x30, 0x10, 0x0A, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x86
};

// Response 4: type=0xB1 sub=0x0 (23 bytes)
uint8_t goResponse4[23] = {
    0x17, 0x09, 0x00, 0x00, 0x00, 0x17, 0x0A, 0x01,
    0x00, 0x01, 0x00, 0x00, 0x09, 0xC7, 0x52, 0x51,
    0x00, 0xB1, 0x00, 0x00, 0x00, 0x01, 0x7E
};

// Response 5: 2-byte ACK
uint8_t goResponse5[2] = { 0x17, 0x0A };

// ============== Command Templates ==============
// 20-byte on/off command
uint8_t onOffCmd[20] = {
    0x17, 0x0A, 0x00, 0x00, 0x00, 0x17, 0x09, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x46, 0x52, 0x51,
    0x01, 0x00, 0x00, 0x00  // [17]=FUNC, [18]=STATE, [19]=CHECKSUM
};

// 18-byte program command
uint8_t progCmd[18] = {
    0x17, 0x0B, 0x00, 0x00, 0x00, 0x17, 0x09, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x4E, 0x03, 0xD0,
    0x00, 0x00  // [16]=PROG_ID, [17]=CHECKSUM
};

uint8_t calcChecksum(uint8_t* data, uint8_t len) {
    uint8_t xorVal = 0;
    for (uint8_t i = 0; i < len - 1; i++) {
        xorVal ^= data[i];
    }
    return xorVal;
}

void sendI2CMessage(uint8_t* msg, uint8_t len) {
    Wire.end();
    Wire.begin();

    Wire.beginTransmission(SPA_ADDRESS);
    Wire.write(msg, len);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)SPA_ADDRESS, (uint8_t)2);
    Wire.read();
    Wire.read();

    Wire.end();
    Wire.begin(SPA_ADDRESS);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);
}

void sendGoResponse() {
    delay(5);
    sendI2CMessage(goResponse1, 78);
    delay(5);
    sendI2CMessage(goResponse2, 78);
    delay(5);
    sendI2CMessage(goResponse3, 78);
    delay(5);
    sendI2CMessage(goResponse4, 23);
    for (int i = 0; i < 5; i++) {
        delay(5);
        sendI2CMessage(goResponse5, 2);
    }
}

void sendLightCommand(bool on) {
    onOffCmd[17] = 0x33;  // Light function
    onOffCmd[18] = on ? 0x01 : 0x00;
    onOffCmd[19] = calcChecksum(onOffCmd, 20);
    sendI2CMessage(onOffCmd, 20);
    Serial.println("STATUS:CMD_SENT:LIGHT");
}

void sendPumpCommand(bool on) {
    onOffCmd[17] = 0x03;  // Pump function
    onOffCmd[18] = on ? 0x02 : 0x00;  // Note: pump uses 0x02 for ON
    onOffCmd[19] = calcChecksum(onOffCmd, 20);
    sendI2CMessage(onOffCmd, 20);
    Serial.println("STATUS:CMD_SENT:PUMP");
}

void sendCircCommand(bool on) {
    onOffCmd[17] = 0x6B;  // Circulation function
    onOffCmd[18] = on ? 0x01 : 0x00;
    onOffCmd[19] = calcChecksum(onOffCmd, 20);
    sendI2CMessage(onOffCmd, 20);
    Serial.println("STATUS:CMD_SENT:CIRC");
}

void sendProgramCommand(uint8_t prog) {
    if (prog > 4) return;
    progCmd[16] = prog;
    progCmd[17] = calcChecksum(progCmd, 18);
    sendI2CMessage(progCmd, 18);
    Serial.print("STATUS:CMD_SENT:PROG:");
    Serial.println(prog);
}

bool isGoMessage() {
    return (i2cBufferLen == 15 && i2cBuffer[13] == 0x47 && i2cBuffer[14] == 0x4F);
}

bool isProgramStatus() {
    return (i2cBufferLen == 18 && i2cBuffer[1] == 0x0B);
}

bool isStatusMessage() {
    if (i2cBufferLen != 78) return false;
    if (i2cBuffer[17] != 0x00) return false;
    uint8_t sub = i2cBuffer[18];
    return (sub == 0x08 || sub == 0x09 || sub == 0x0A);
}

void parseStatusMessage() {
    bool prevLight = lightState;
    bool prevPump = pumpState;
    bool prevCirc = circState;
    bool prevHeating = heatingState;
    bool prevStandby = standbyState;
    float prevTarget = targetTemp;
    float prevActual = actualTemp;

    // Parse status bytes
    standbyState = (i2cBuffer[19] == 0x03);
    pumpState = (i2cBuffer[21] == 0x02) || (i2cBuffer[23] == 0x01);
    heatingState = (i2cBuffer[22] != 0x00) || (i2cBuffer[42] & 0x04);
    circState = (i2cBuffer[65] == 0x14);
    lightState = (i2cBuffer[69] == 0x01);

    // Temperature (bytes 37-40)
    if (i2cBuffer[37] != 0 || i2cBuffer[38] != 0) {
        uint16_t targetRaw = (i2cBuffer[37] << 8) | i2cBuffer[38];
        targetTemp = targetRaw / 18.0;
        uint16_t actualRaw = (i2cBuffer[39] << 8) | i2cBuffer[40];
        actualTemp = actualRaw / 18.0;
    }

    // Send updates only if changed
    if (lightState != prevLight) {
        Serial.print("STATUS:LIGHT:");
        Serial.println(lightState ? "ON" : "OFF");
    }
    if (pumpState != prevPump) {
        Serial.print("STATUS:PUMP:");
        Serial.println(pumpState ? "ON" : "OFF");
    }
    if (circState != prevCirc) {
        Serial.print("STATUS:CIRC:");
        Serial.println(circState ? "ON" : "OFF");
    }
    if (heatingState != prevHeating) {
        Serial.print("STATUS:HEATING:");
        Serial.println(heatingState ? "ON" : "OFF");
    }
    if (standbyState != prevStandby) {
        Serial.print("STATUS:STANDBY:");
        Serial.println(standbyState ? "ON" : "OFF");
    }
    if (abs(targetTemp - prevTarget) > 0.1 || abs(actualTemp - prevActual) > 0.1) {
        Serial.print("STATUS:TEMP:");
        Serial.print(targetTemp, 1);
        Serial.print(":");
        Serial.println(actualTemp, 1);
    }
}

void parseProgramStatus() {
    uint8_t newProg = i2cBuffer[16];
    if (newProg != programId && newProg <= 4) {
        programId = newProg;
        Serial.print("STATUS:PROG:");
        Serial.println(programId);
    }
}

void processUartCommand(const char* cmd) {
    if (strncmp(cmd, "CMD:LIGHT:ON", 12) == 0) {
        sendLightCommand(true);
    } else if (strncmp(cmd, "CMD:LIGHT:OFF", 13) == 0) {
        sendLightCommand(false);
    } else if (strncmp(cmd, "CMD:PUMP:ON", 11) == 0) {
        sendPumpCommand(true);
    } else if (strncmp(cmd, "CMD:PUMP:OFF", 12) == 0) {
        sendPumpCommand(false);
    } else if (strncmp(cmd, "CMD:CIRC:ON", 11) == 0) {
        sendCircCommand(true);
    } else if (strncmp(cmd, "CMD:CIRC:OFF", 12) == 0) {
        sendCircCommand(false);
    } else if (strncmp(cmd, "CMD:PROG:", 9) == 0) {
        uint8_t prog = cmd[9] - '0';
        sendProgramCommand(prog);
    } else if (strncmp(cmd, "CMD:STATUS", 10) == 0) {
        Serial.print("STATUS:LIGHT:");
        Serial.println(lightState ? "ON" : "OFF");
        Serial.print("STATUS:PUMP:");
        Serial.println(pumpState ? "ON" : "OFF");
        Serial.print("STATUS:CIRC:");
        Serial.println(circState ? "ON" : "OFF");
        Serial.print("STATUS:HEATING:");
        Serial.println(heatingState ? "ON" : "OFF");
        Serial.print("STATUS:STANDBY:");
        Serial.println(standbyState ? "ON" : "OFF");
        Serial.print("STATUS:PROG:");
        Serial.println(programId);
        Serial.print("STATUS:TEMP:");
        Serial.print(targetTemp, 1);
        Serial.print(":");
        Serial.println(actualTemp, 1);
        Serial.print("STATUS:CONNECTED:");
        Serial.println(spaConnected ? "YES" : "NO");
    }
}

void receiveEvent(int numBytes) {
    i2cBufferLen = 0;
    while (Wire.available() && i2cBufferLen < 128) {
        i2cBuffer[i2cBufferLen++] = Wire.read();
    }
    newI2CMessage = true;
}

void requestEvent() {
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)0x00);
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(100);

    Serial.println("STATUS:BOOT:SPA_BRIDGE_V1");

    Wire.begin(SPA_ADDRESS);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);

    Serial.println("STATUS:READY");
}

void loop() {
    // Process I2C messages
    if (newI2CMessage) {
        newI2CMessage = false;

        if (isGoMessage()) {
            lastGoReceived = millis();
            if (!spaConnected) {
                spaConnected = true;
                Serial.println("STATUS:CONNECTED:YES");
            }
            sendGoResponse();
        } else if (isProgramStatus()) {
            parseProgramStatus();
        } else if (isStatusMessage()) {
            parseStatusMessage();
        }
    }

    // Check connection timeout (no GO for 90 seconds = disconnected)
    if (spaConnected && (millis() - lastGoReceived > 90000)) {
        spaConnected = false;
        Serial.println("STATUS:CONNECTED:NO");
    }

    // Process UART commands
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (uartBufferPos > 0) {
                uartBuffer[uartBufferPos] = '\0';
                processUartCommand(uartBuffer);
                uartBufferPos = 0;
            }
        } else if (uartBufferPos < sizeof(uartBuffer) - 1) {
            uartBuffer[uartBufferPos++] = c;
        }
    }
}
