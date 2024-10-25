#include <Arduino.h>
#include <M5Unified.h>        // Updated for M5 Cardputer compatibility
#include <IRremote.h>         // Use IRremote for handling IR signals
#include <RF24.h>             // For RF transmission
#include <SD.h>
#include <SPI.h>

#define IR_RECEIVE_PIN  35   // Pin to receive IR signals
#define IR_SEND_PIN     9    // Pin to send IR signals (connect your IR LED)
#define RF_CE_PIN       15   // RF Chip Enable
#define RF_CS_PIN       5    // RF Chip Select
#define REMOTE_FILE_DIR "/remote_names/"  // Directory to save remote names
#define LIGHTCYAN 0xE0FFFF  // Define light cyan color

// Initialize the necessary objects
IRsend irsend(IR_SEND_PIN);     // Initialize IR sender with the correct pin
RF24 radio(RF_CE_PIN, RF_CS_PIN);  // Initialize RF module

String currentRemoteName = ""; // Store the current remote's name
String remoteData = "";        // Store received IR/RF data

// Expanded keyboard layout
char keyboardLayout[4][10] = {
    {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'},
    {'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T'},
    {'U', 'V', 'W', 'X', 'Y', 'Z', '-', '_', '<', '>'},
    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'}
};

int cursorX = 0;
int cursorY = 0;
String enteredText = "";

// Function prototypes
void displayIntro();
void scanRF();
void scanIR();
void saveRemoteButton(String buttonName);
void saveRemoteData(String buttonName, String data);
void loadRemoteButton(String buttonName);
void displayKeyboard();
void playbackSavedButton(String buttonName);
void showSavedButtons();
void sendIRSignal(uint32_t data, uint16_t nbits);  // New function for IR transmission
void sendRFSignal(String data);                    // New function for RF transmission
void handleKeyboardInput();                        // New function for handling keyboard input

void setup() {
    auto cfg = M5.config();       // Configuration for M5 Cardputer
    cfg.output_power = true;      // Set output power
    M5.begin(cfg);                // M5 initialization for Cardputer
    M5.Lcd.clear();

    // Initialize SD card
    if (!SD.begin()) {
        M5.Lcd.print("SD Card Mount Failed");
        return;
    }

    // Ensure directory exists for remote names
    if (!SD.exists(REMOTE_FILE_DIR)) {
        SD.mkdir(REMOTE_FILE_DIR);
    }

    displayIntro();

    IrReceiver.begin(IR_RECEIVE_PIN);  // Start IR receiver using the default IrReceiver instance
    irsend.begin();                     // Start IR sender (pass the pin explicitly)

    if (!radio.begin()) {
        M5.Lcd.println("RF init failed!");
        return;
    }
    radio.setPALevel(RF24_PA_LOW); // Set RF power level
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) {
        scanRF();
    } else if (M5.BtnB.wasPressed()) {
        scanIR();
    } else if (M5.BtnC.wasPressed()) {
        displayKeyboard();  // Display keyboard to enter remote name
        handleKeyboardInput(); // Handle keyboard input
    }

    delay(100);
}

// New cool intro animation with light baby blue text and black background
void displayIntro() {
    M5.Lcd.clear();
    M5.Lcd.fillScreen(BLACK);        // Set background to black
    M5.Lcd.setTextColor(LIGHTCYAN);  // Set text color to light baby blue
    M5.Lcd.setTextSize(2);

    // Draw the title "Remote Possibility"
    M5.Lcd.setCursor(40, 50);
    M5.Lcd.print("Remote Possibility");
    M5.Lcd.setCursor(60, 80);
    M5.Lcd.print("by salvadorData");
    delay(2000);  // Pause to show the title

    // Animation of a remote sending binary code in a triangle pattern
    for (int i = 1; i <= 10; i++) {
        int startX = 160; // Start from the center of the screen for the transmission origin
        int startY = 120; // Middle of the screen
        int spacing = i * 10;  // Increasing spacing for each row in the triangle

        // Create outward-facing triangle rows of binary code
        for (int j = 0; j < i; j++) {
            // Left side of the triangle
            M5.Lcd.setCursor(startX - spacing - (j * 10), startY + (i * 15));
            M5.Lcd.print("01");

            // Right side of the triangle
            M5.Lcd.setCursor(startX + spacing + (j * 10), startY + (i * 15));
            M5.Lcd.print("10");
        }

        delay(200);  // Delay to animate the rows being drawn
    }

    // Final transmission effect
    M5.Lcd.setCursor(80, 200);
    M5.Lcd.print("Transmission Complete");
    delay(2000);  // Pause before continuing
    M5.Lcd.clear();  // Clear screen after intro
}

// Complete IR Transmission
void sendIRSignal(uint32_t data, uint16_t nbits) {
    M5.Lcd.clear();
    M5.Lcd.print("Sending IR signal...");
    irsend.sendNECMSB(data, nbits);  // Use the newer sendNECMSB() method for IR transmission
    delay(200);  // Delay for user feedback
}

// Complete RF Transmission
void sendRFSignal(String data) {
    M5.Lcd.clear();
    M5.Lcd.print("Sending RF signal...");

    if (radio.write(&data[0], data.length())) {
        M5.Lcd.print("RF Signal Sent!");
    } else {
        M5.Lcd.print("RF Signal Send Failed!");
    }
    delay(200);  // Delay for user feedback
}

// Keyboard display function
void displayKeyboard() {
    M5.Lcd.clear();
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print("Enter Remote Name:");

    // Display keyboard layout
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 10; col++) {
            int x = 10 + col * 30;  // Adjust the spacing between characters
            int y = 60 + row * 40;
            if (row == cursorY && col == cursorX) {
                M5.Lcd.fillRect(x - 5, y - 5, 25, 30, LIGHTCYAN);  // Highlight the selected key
                M5.Lcd.setTextColor(BLACK);
            } else {
                M5.Lcd.setTextColor(WHITE);
            }
            M5.Lcd.setCursor(x, y);
            M5.Lcd.print(keyboardLayout[row][col]);
        }
    }
}

// Function to handle keyboard input
void handleKeyboardInput() {
    bool inputDone = false;
    while (!inputDone) {
        M5.update();
        
        if (M5.BtnA.wasPressed()) {
            // Select character at current cursor position
            enteredText += keyboardLayout[cursorY][cursorX];
            M5.Lcd.setCursor(10, 220);
            M5.Lcd.setTextColor(WHITE);
            M5.Lcd.setTextSize(2);
            M5.Lcd.print("Name: " + enteredText);
        } else if (M5.BtnB.wasPressed()) {
            // Finish input
            inputDone = true;
            currentRemoteName = enteredText;
            M5.Lcd.clear();
            M5.Lcd.print("Remote name saved: " + currentRemoteName);
            delay(2000);
        } else if (M5.BtnC.wasPressed()) {
            // Move cursor
            cursorX = (cursorX + 1) % 10;
            if (cursorX == 0) {
                cursorY = (cursorY + 1) % 4;
            }
            displayKeyboard();  // Refresh the keyboard display with the new cursor position
        }

        delay(100);  // Small delay to prevent too fast input
    }
}

// Add the definitions for scanRF and scanIR functions
void scanRF() {
    M5.Lcd.clear();
    M5.Lcd.print("Scanning RF...");
    radio.startListening();

    if (radio.available()) {
        char receivedMessage[32] = "";
        radio.read(&receivedMessage, sizeof(receivedMessage));
        remoteData = String(receivedMessage);
        M5.Lcd.clear();
        M5.Lcd.print("RF Button Detected:");
        M5.Lcd.print(remoteData);
        saveRemoteButton("RFButton");

        // After saving, play back the RF signal (optional)
        sendRFSignal(remoteData);
    } else {
        M5.Lcd.print("No RF signal found.");
    }
}

void scanIR() {
    M5.Lcd.clear();
    M5.Lcd.print("Scanning IR...");

    if (irrecv.decode(&results)) {
        remoteData = String(results.value, HEX);  // Convert decoded IR signal to HEX string
        M5.Lcd.clear();
        M5.Lcd.print("IR Button Detected:");
        M5.Lcd.print(remoteData);
        irrecv.resume();  // Prepare for the next signal
        saveRemoteButton("IRButton");

        // After saving, play back the IR signal (optional)
        sendIRSignal(results.value, results.bits);  // Send the received IR signal
    } else {
        M5.Lcd.print("No IR signal detected.");
    }
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

        // If it's an IR signal, send it via IR transmission
        uint32_t dataToSend = strtoul(remoteData.c_str(), nullptr, 16);
        sendIRSignal(dataToSend, 32);  // Send IR signal assuming it's a 32-bit NEC code

        // If it's an RF signal, send it via RF transmission
        sendRFSignal(remoteData);  // Send RF signal using the stored remoteData
    } else {
        M5.Lcd.clear();
        M5.Lcd.print("Button not found.");
        delay(2000);  // Delay for user to see the message
    }
}

// Show saved buttons
void showSavedButtons() {
    M5.Lcd.clear();
    M5.Lcd.setTextSize(2);
    M5.Lcd.print("Saved Buttons:");

    String filePath = String(REMOTE_FILE_DIR) + currentRemoteName + ".txt";

    File remoteFile = SD.open(filePath);
    if (!remoteFile) {
        M5.Lcd.print("Error opening file.");
        return;
    }

    int y = 30;  // Starting Y position for the first button
    int buttonIndex = 0;  // Track the index of the button

    // Display all saved buttons on the screen
    while (remoteFile.available()) {
        String line = remoteFile.readStringUntil('\n');
        int separator = line.indexOf(',');
        String buttonName = line.substring(0, separator);  // Extract the button name

        M5.Lcd.setCursor(10, y);
        M5.Lcd.print(buttonIndex + 1);  // Display button index (starting from 1)
        M5.Lcd.print(". ");
        M5.Lcd.print(buttonName);  // Display button name

        y += 20;  // Move to the next line for the next button
        buttonIndex++;

        if (y > 200) {
            M5.Lcd.setCursor(10, y);
            M5.Lcd.print("More buttons available...");
            break;  // Stop if we run out of display space
        }
    }

    remoteFile.close();

    // If no buttons were found in the file, display a message
    if (buttonIndex == 0) {
        M5.Lcd.clear();
        M5.Lcd.print("No buttons saved.");
    }

    // Instructions for selecting a button
    M5.Lcd.setCursor(10, y + 30);
    M5.Lcd.print("Press B to select");

    bool selecting = true;
    int selectedIndex = 0;

    // Allow the user to scroll through saved buttons and select one
    while (selecting) {
        M5.update();

        if (M5.BtnA.wasPressed()) {
            // Move selection up
            selectedIndex = (selectedIndex == 0) ? buttonIndex - 1 : selectedIndex - 1;
            M5.Lcd.clear();
            M5.Lcd.setCursor(10, y + 50);
            M5.Lcd.print("Selected: Button ");
            M5.Lcd.print(selectedIndex + 1);
        } else if (M5.BtnC.wasPressed()) {
            // Move selection down
            selectedIndex = (selectedIndex + 1) % buttonIndex;
            M5.Lcd.clear();
            M5.Lcd.setCursor(10, y + 50);
            M5.Lcd.print("Selected: Button ");
            M5.Lcd.print(selectedIndex + 1);
        } else if (M5.BtnB.wasPressed()) {
            // Play selected button
            playbackSavedButton("Button" + String(selectedIndex + 1));  // Button indices are 1-based in the display
            selecting = false;
        }

        delay(200);  // Small delay to avoid excessive input reading
    }
}
