#include <Arduino.h>
#include <M5Unified.h>
#include <IRremote.h>
#include <RF24.h>
#include <RCSwitch.h>  // Library for 433 MHz RF
#include <SD.h>
#include <SPI.h>

#define IR_RECEIVE_PIN  35
#define IR_SEND_PIN     9
#define RF_CE_PIN       15
#define RF_CS_PIN       5
#define RF_433_RECEIVE_PIN 2  // Pin for receiving 433 MHz RF signals
#define RF_433_SEND_PIN  3    // Pin for sending 433 MHz RF signals
#define REMOTE_FILE_DIR "/remote_names/"
#define LIGHTCYAN 0xE0FFFF

IRsend irsend(IR_SEND_PIN);
RF24 radio(RF_CE_PIN, RF_CS_PIN);
RCSwitch rf433Switch = RCSwitch();

String currentRemoteName = "";
String remoteData = "";

// Function prototypes
void displayIntro();
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

void setup() {
    auto cfg = M5.config();
    cfg.output_power = true;
    M5.begin(cfg);
    M5.Lcd.clear();

    if (!SD.begin()) {
        M5.Lcd.print("SD Card Mount Failed");
        return;
    }
    if (!SD.exists(REMOTE_FILE_DIR)) {
        SD.mkdir(REMOTE_FILE_DIR);
    }

    displayIntro();

    IrReceiver.begin(IR_RECEIVE_PIN);
    irsend.begin();

    if (!radio.begin()) {
        M5.Lcd.println("2.4 GHz RF init failed!");
    } else {
        radio.setPALevel(RF24_PA_LOW);
    }

    rf433Switch.enableReceive(digitalPinToInterrupt(RF_433_RECEIVE_PIN));  // Setup 433 MHz receive pin
    rf433Switch.enableTransmit(RF_433_SEND_PIN);  // Setup 433 MHz send pin
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) {
        scanRF24();
    } else if (M5.BtnB.wasPressed()) {
        scanRF433();
    } else if (M5.BtnC.wasPressed()) {
        scanIR();
    }

    delay(100);
}

void displayIntro() {
    M5.Lcd.clear();
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(LIGHTCYAN);
    M5.Lcd.setTextSize(2);

    M5.Lcd.setCursor(40, 50);
    M5.Lcd.print("Remote Possibility");
    M5.Lcd.setCursor(60, 80);
    M5.Lcd.print("by salvadorData");
    delay(2000);
    M5.Lcd.clear();
}

// Scan for 2.4 GHz RF signals
void scanRF24() {
    M5.Lcd.clear();
    M5.Lcd.print("Scanning 2.4 GHz RF...");
    radio.startListening();

    if (radio.available()) {
        char receivedMessage[32] = "";
        radio.read(&receivedMessage, sizeof(receivedMessage));
        remoteData = String(receivedMessage);
        M5.Lcd.print("RF24 Button Detected: ");
        M5.Lcd.print(remoteData);
        saveRemoteButton("RF24Button");
        sendRF24Signal(remoteData);
    } else {
        M5.Lcd.print("No 2.4 GHz RF signal found.");
    }
}

// Scan for 433 MHz RF signals
void scanRF433() {
    M5.Lcd.clear();
    M5.Lcd.print("Scanning 433 MHz RF...");
    
    if (rf433Switch.available()) {
        unsigned long receivedValue = rf433Switch.getReceivedValue();
        if (receivedValue == 0) {
            M5.Lcd.print("Unknown RF signal.");
        } else {
            remoteData = String(receivedValue, HEX);
            M5.Lcd.print("RF433 Button Detected: ");
            M5.Lcd.print(remoteData);
            saveRemoteButton("RF433Button");
            sendRF433Signal(receivedValue);
        }
        rf433Switch.resetAvailable();
    } else {
        M5.Lcd.print("No 433 MHz RF signal detected.");
    }
}

// IR Scanning
void scanIR() {
    M5.Lcd.clear();
    M5.Lcd.print("Scanning IR...");

    if (IrReceiver.decode()) {
        remoteData = String(IrReceiver.decodedIRData.decodedRawData, HEX);
        M5.Lcd.print("IR Button Detected: ");
        M5.Lcd.print(remoteData);
        IrReceiver.resume();
        saveRemoteButton("IRButton");
        sendIRSignal(IrReceiver.decodedIRData.decodedRawData, IrReceiver.decodedIRData.numberOfBits);
    } else {
        M5.Lcd.print("No IR signal detected.");
    }
}

// Send 2.4 GHz RF signal
void sendRF24Signal(String data) {
    M5.Lcd.clear();
    M5.Lcd.print("Sending 2.4 GHz RF signal...");
    
    if (radio.write(&data[0], data.length())) {
        M5.Lcd.print("RF24 Signal Sent!");
    } else {
        M5.Lcd.print("RF24 Signal Send Failed!");
    }
}

// Send 433 MHz RF signal
void sendRF433Signal(unsigned long data) {
    M5.Lcd.clear();
    M5.Lcd.print("Sending 433 MHz RF signal...");
    rf433Switch.send(data, 24);  // Send 433 MHz RF signal
    M5.Lcd.print("RF433 Signal Sent!");
}

// Send IR signal
void sendIRSignal(uint32_t data, uint16_t nbits) {
    M5.Lcd.clear();
    M5.Lcd.print("Sending IR signal...");
    irsend.sendNECMSB(data, nbits);
}

// Save button under the current remote name
void saveRemoteButton(String buttonName) {
    if (currentRemoteName == "") {
        M5.Lcd.print("No remote name set!");
        return;
    }
    saveRemoteData(buttonName, remoteData);  // Save the detected IR/RF data
}

// Save the button name and its data into the remote file
void saveRemoteData(String buttonName, String data) {
    String filePath = String(REMOTE_FILE_DIR) + currentRemoteName + ".txt";

    M5.Lcd.clear();
    M5.Lcd.print("Saving button to ");
    M5.Lcd.print(filePath);

    File remoteFile = SD.open(filePath, FILE_APPEND);
    if (!remoteFile) {
        M5.Lcd.print("Error opening file.");
        return;
    }

    remoteFile.println(buttonName + "," + data);
    remoteFile.close();
    M5.Lcd.print("Button Saved!");
}

// Load a saved button
void loadRemoteButton(String buttonName) {
    if (currentRemoteName == "") {
        M5.Lcd.print("No remote name set!");
        return;
    }

    String filePath = String(REMOTE_FILE_DIR) + currentRemoteName + ".txt";
    M5.Lcd.clear();
    M5.Lcd.print("Loading button from ");
    M5.Lcd.print(filePath);

    File remoteFile = SD.open(filePath);
    if (!remoteFile) {
        M5.Lcd.print("Error opening file.");
        return;
    }

    while (remoteFile.available()) {
        String line = remoteFile.readStringUntil('\n');
        int separator = line.indexOf(',');
        String name = line.substring(0, separator);
        String data = line.substring(separator + 1);

        if (name == buttonName) {
            remoteData = data;
            M5.Lcd.print("Button Loaded!");
            break;
        }
    }

    remoteFile.close();
}

// Playback a saved button
void playbackSavedButton(String buttonName) {
    loadRemoteButton(buttonName);

    if (!remoteData.isEmpty()) {
        M5.Lcd.clear();
        M5.Lcd.print("Playing back button...");
        M5.Lcd.setCursor(0, 50);
        M5.Lcd.print("Data: ");
        M5.Lcd.print(remoteData);

        uint32_t dataToSend = strtoul(remoteData.c_str(), nullptr, 16);
        sendIRSignal(dataToSend, 32);
        sendRF24Signal(remoteData);
        sendRF433Signal(dataToSend);
    } else {
        M5.Lcd.clear();
        M5.Lcd.print("Button not found.");
        delay(2000);
    }
}
