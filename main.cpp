#include <Arduino.h>
#include <M5Unified.h>
#include <IRremote.h>
#include <RF24.h>
#include <RCSwitch.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#define FEEDBACK_LED_PIN 2
#define IR_RECEIVE_PIN 35
#define IR_SEND_PIN 9
#define RF_CE_PIN 15
#define RF_CS_PIN 5
#define RF_433_RECEIVE_PIN 2
#define RF_433_SEND_PIN 3
#define REMOTE_FILE_DIR "/remote_names/"
#define LIGHTCYAN 0xE0FFFF

void displayIntro();
void initializeUI();
void updatePowerMeter();
void scanRF24();
void scanRF433();
void scanIR();
void saveRemoteButton(String buttonName);
void saveRemoteData(String buttonName, String data);
void loadRemoteButton(String buttonName);
void playbackSavedButton(String buttonName);
void sendIRSignal(uint32_t data, uint16_t nbits);
void sendRF24Signal(String data);
void sendRF433Signal(unsigned long data);
void handleKeyPress(char key);

class M5_KB {
public:
    M5_KB();
    void begin();
    bool available();
    char read();
};

#define CARDKB_ADDR 0x5F

M5_KB::M5_KB() {}

void M5_KB::begin() {
    Wire.begin();
}

bool M5_KB::available() {
    Wire.requestFrom(CARDKB_ADDR, 1);
    return Wire.available();
}

char M5_KB::read() {
    if (available()) {
        return Wire.read();
    }
    return 0;
}

IRsend IrSender;
IRrecv IrReceiver(IR_RECEIVE_PIN);

RF24 radio(RF_CE_PIN, RF_CS_PIN);
RCSwitch rf433Switch = RCSwitch();
M5_KB CardKB;

String currentRemoteName = "";
String remoteData = "";

void setup() {
    Serial.begin(115200);
    Serial.println("Starting setup...");

    auto cfg = M5.config();
    cfg.output_power = true;
    M5.begin(cfg);
    M5.Display.clear();
    Serial.println("M5 Display initialized.");

    displayIntro();
    Serial.println("Display intro complete.");

    // Initialize SPI communication
    SPI.begin(40, 39, 14, SD_CS_PIN);

    // SD Card initialization with detection check
    if (!SD.begin(SD_CS_PIN)) {
        M5.Display.setCursor(0, 100);
        M5.Display.setTextSize(2);
        M5.Display.setTextColor(RED);
        M5.Display.println("SD Card Mount Failed");
        Serial.println("SD Card initialization failed.");
        while (1);
    } else {
        Serial.println("SD Card initialized successfully.");
    }

    // Initialize IR receiver
    Serial.println("Initializing IR Receiver...");
    IrReceiver.enableIRIn();
    Serial.println("IR Receiver initialized.");

    // Initialize RF24 module with check
    Serial.println("Initializing RF24...");
    if (!radio.begin()) {
        M5.Display.println("2.4 GHz RF init failed!");
        Serial.println("RF24 failed to initialize!");
    } else {
        radio.setPALevel(RF24_PA_LOW);
        Serial.println("RF24 initialized.");
    }

    // Initialize 433 MHz RF module
    Serial.println("Initializing RCSwitch...");
    rf433Switch.enableReceive(digitalPinToInterrupt(RF_433_RECEIVE_PIN));
    rf433Switch.enableTransmit(RF_433_SEND_PIN);
    Serial.println("RCSwitch initialized.");

    // Initialize I2C and CardKB keyboard
    Serial.println("Initializing I2C and CardKB...");
    Wire.begin();
    CardKB.begin();
    pinMode(FEEDBACK_LED_PIN, OUTPUT);
    Serial.println("I2C and CardKB initialized.");

    initializeUI();
    Serial.println("UI initialized.");
}

void loop() {
    M5.update();
    updatePowerMeter();

    if (CardKB.available()) {
        char key = CardKB.read();
        M5.Display.print(key);
        handleKeyPress(key);
    }

    if (M5.BtnA.wasPressed()) {
        scanRF24();
    } else if (M5.BtnB.wasPressed()) {
        scanRF433();
    } else if (M5.BtnC.wasPressed()) {
        scanIR();
    }

    delay(100);
}

void handleKeyPress(char key) {
    if (key == 's') {
        saveRemoteButton("CustomButton");
    } else if (key == 'p') {
        playbackSavedButton("CustomButton");
    } else if (key == 'c') {
        currentRemoteName = "";
        M5.Display.clear();
        M5.Display.print("Cleared Remote Name");
    }
}

void displayIntro() {
    M5.Display.clear();
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(LIGHTCYAN);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(40, 50);
    M5.Display.print("Remote Possibility");
    M5.Display.setCursor(60, 80);
    M5.Display.print("by salvadorData");

    for (int i = 0; i < 100; i += 10) {
        M5.Display.setCursor(50 + i, 100);
        M5.Display.print("1010101 ");
        delay(200);
    }

    delay(2000);
    M5.Display.clear();
}

void initializeUI() {
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 20);
    M5.Display.print("Remote Possibility Interface");

    M5.Display.setCursor(0, 40);
    M5.Display.print("1. Scan Remote Buttons");

    M5.Display.setCursor(0, 60);
    M5.Display.print("2. Save Remote Buttons");

    M5.Display.setCursor(0, 80);
    M5.Display.print("3. Load Remote Files");

    M5.Display.setCursor(0, 100);
    M5.Display.print("4. Settings");
}

void updatePowerMeter() {
    int batteryLevel = M5.Power.getBatteryLevel();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(WHITE);
    M5.Display.print("Power: ");
    M5.Display.print(batteryLevel);
    M5.Display.print("%");
}

void scanRF24() {
    M5.Display.clear();
    M5.Display.print("Scanning 2.4 GHz RF...");
    radio.startListening();

    if (radio.available()) {
        char receivedMessage[32] = "";
        radio.read(&receivedMessage, sizeof(receivedMessage));
        remoteData = String(receivedMessage);
        M5.Display.print("RF24 Button Detected: ");
        M5.Display.print(remoteData);
        saveRemoteButton("RF24Button");
        sendRF24Signal(remoteData);
    } else {
        M5.Display.print("No 2.4 GHz RF signal found.");
    }
}

void scanRF433() {
    M5.Display.clear();
    M5.Display.print("Scanning 433 MHz RF...");

    if (rf433Switch.available()) {
        unsigned long receivedValue = rf433Switch.getReceivedValue();
        if (receivedValue == 0) {
            M5.Display.print("Unknown RF signal.");
        } else {
            remoteData = String(receivedValue, HEX);
            M5.Display.print("RF433 Button Detected: ");
            M5.Display.print(remoteData);
            saveRemoteButton("RF433Button");
            sendRF433Signal(receivedValue);
        }
        rf433Switch.resetAvailable();
    } else {
        M5.Display.print("No 433 MHz RF signal detected.");
    }
}

void scanIR() {
    decode_results results;
    M5.Display.clear();
    M5.Display.print("Scanning IR...");

    if (IrReceiver.decode(&results)) {
        remoteData = String(results.value, HEX);
        M5.Display.print("IR Button Detected: ");
        M5.Display.print(remoteData);
        saveRemoteButton("IRButton");
        sendIRSignal(results.value, results.bits);
        IrReceiver.resume();
    } else {
        M5.Display.print("No IR signal detected.");
    }
}

void sendRF24Signal(String data) {
    M5.Display.clear();
    M5.Display.print("Sending 2.4 GHz RF signal...");

    if (radio.write(&data[0], data.length())) {
        M5.Display.print("RF24 Signal Sent!");
    } else {
        M5.Display.print("RF24 Signal Send Failed!");
    }
}

void sendRF433Signal(unsigned long data) {
    M5.Display.clear();
    M5.Display.print("Sending 433 MHz RF signal...");
    rf433Switch.send(data, 24);
    M5.Display.print("RF433 Signal Sent!");
}

void sendIRSignal(uint32_t data, uint16_t nbits) {
    M5.Display.clear();
    M5.Display.print("Sending IR signal...");
    IrSender.sendNEC(data, nbits);
}

void saveRemoteButton(String buttonName) {
    if (currentRemoteName.isEmpty()) {
        M5.Display.print("No remote name set!");
        return;
    }
    saveRemoteData(buttonName, remoteData);
}

void saveRemoteData(String buttonName, String data) {
    String filePath = String(REMOTE_FILE_DIR) + currentRemoteName + ".txt";

    M5.Display.clear();
    M5.Display.print("Saving button to ");
    M5.Display.print(filePath);

    File remoteFile = SD.open(filePath, FILE_APPEND);
    if (!remoteFile) {
        M5.Display.print("Error opening file.");
        return;
    }

    remoteFile.println(buttonName + "," + data);
    remoteFile.close();
    M5.Display.print("Button Saved!");
}

void loadRemoteButton(String buttonName) {
    if (currentRemoteName.isEmpty()) {
        M5.Display.print("No remote name set!");
        return;
    }

    String filePath = String(REMOTE_FILE_DIR) + currentRemoteName + ".txt";
    M5.Display.clear();
    M5.Display.print("Loading button from ");
    M5.Display.print(filePath);

    File remoteFile = SD.open(filePath);
    if (!remoteFile) {
        M5.Display.print("Error opening file.");
        return;
    }

    while (remoteFile.available()) {
        String line = remoteFile.readStringUntil('\n');
        int separator = line.indexOf(',');
        String name = line.substring(0, separator);
        String data = line.substring(separator + 1);

        if (name == buttonName) {
            remoteData = data;
            M5.Display.print("Button Loaded!");
            break;
        }
    }

    remoteFile.close();
}

void playbackSavedButton(String buttonName) {
    loadRemoteButton(buttonName);

    if (!remoteData.isEmpty()) {
        M5.Display.clear();
        M5.Display.print("Playing back button...");
        M5.Display.setCursor(0, 50);
        M5.Display.print("Data: ");
        M5.Display.print(remoteData);

        uint32_t dataToSend = strtoul(remoteData.c_str(), nullptr, 16);
        sendIRSignal(dataToSend, 32);
        sendRF24Signal(remoteData);
        sendRF433Signal(dataToSend);
    } else {
        M5.Display.clear();
        M5.Display.print("Button not found.");
        delay(2000);
    }
}
