#include <SoftwareSerial.h>
#include <EEPROM.h>

// ======================================================================
//  Quectel EC200U on pins 10(RX) and 11(TX) with Arduino UNO
//  Using 115200 in the code, but SoftwareSerial can be unstable at this speed.
//
//  If you see garbled responses, set EC200U to 9600 baud with:
//      AT+IPR=9600
//      AT&W
//  then change EC200U_BAUD to 9600 in this code.
//
// ======================================================================
SoftwareSerial ec200u(10, 11);
#define EC200U_BAUD 115200

// ======================================================================
//  Relay Configuration
//  4 relays on pins 2,3,4,5
//  This code: HIGH = ON, LOW = OFF
// ======================================================================
const int NUM_RELAYS = 4;
const int relayPins[NUM_RELAYS] = {2, 3, 4, 5}; 

// Relay states in software: false=OFF, true=ON
bool relayState[NUM_RELAYS] = {false, false, false, false};

// ======================================================================
//  EEPROM Layout
//  For each relay: we store two 16-bit integers => ON count + OFF count
//  That is 4 bytes per relay -> total 16 bytes for 4 relays
// ======================================================================
int getEEPROMAddressOnCount(int relayIndex)  { return relayIndex*4;     }  // 0,4,8,12
int getEEPROMAddressOffCount(int relayIndex) { return relayIndex*4 + 2; }  // 2,6,10,14

// ======================================================================
//  Global Variables
// ======================================================================

// Main phone number to which we always send the summary
String ownerNumber = "+917052722734";  // <-- change to your real number

// Track call activity
bool isCallActive = false;
String callerNumber = "";

// For summarizing changes
String changesBuffer = "";   // accumulates e.g. "R1 => ON\nR2 => OFF\n..."
bool pendingSummary   = false; 
unsigned long lastChangeMillis = 0;   // when was the last relay toggle or SMS command

// ---------------------------------------------------------------------
//  Function Prototypes
// ---------------------------------------------------------------------
void initRelays();
void loadRelayUsageFromEEPROM();
void saveRelayOnCountToEEPROM(int relayIndex, unsigned int value);
void saveRelayOffCountToEEPROM(int relayIndex, unsigned int value);
unsigned int readRelayOnCountFromEEPROM(int relayIndex);
unsigned int readRelayOffCountFromEEPROM(int relayIndex);

void handleIncomingResponse(String &response);
void sendATCommand(String command, String expectedResponse, unsigned long timeout);
void parseSMS(String smsText);
void sendSMS(String number, String text);

// Relay control
void toggleRelay(int relayIndex);
void setRelay(int relayIndex, bool on);
void setAllRelays(bool on);

// Summaries
void markChange(String changeLine);
void sendSummaryToOwner();
void sendImmediateCallEndSummary();

// ---------------------------------------------------------------------
//  setup()
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);         // PC Serial
  ec200u.begin(EC200U_BAUD);    // EC200U

  Serial.println();
  Serial.println("=== Quectel EC200U - 4 Relay Control (DTMF+SMS) ===");
  Serial.println("   (HIGH = Relay ON, LOW = Relay OFF)");
  
  // Initialize Relay Pins => default OFF
  initRelays();

  // Load usage counters from EEPROM (optional, for debug)
  loadRelayUsageFromEEPROM();

  // Setup the module
  sendATCommand("AT+CMGF=1",     "OK", 2000);   // SMS text mode
  sendATCommand("AT+CLIP=1",     "OK", 2000);   // Caller ID
  sendATCommand("AT+QTONEDET=1", "OK", 2000);   // DTMF detection
  sendATCommand("AT+CNMI=2,2,0,0,0", "OK", 2000);// auto +CMT

  Serial.println("Setup complete. Waiting for calls/SMS...\n");
}

// ---------------------------------------------------------------------
//  loop()
//   - Check EC200U responses
//   - Check if 1 minute has passed since last change => send summary
// ---------------------------------------------------------------------
void loop() {
  // Read from EC200U
  while (ec200u.available() > 0) {
    static String responseBuffer = "";
    char c = ec200u.read();
    responseBuffer += c;

    if (c == '\n') {
      handleIncomingResponse(responseBuffer);
      responseBuffer = "";
    }
  }

  // If we have pending changes, and 60s have passed since lastChangeMillis
  // => send summary now
  if (pendingSummary && (millis() - lastChangeMillis >= 60000)) {
    sendSummaryToOwner();
  }

  // Optionally handle user commands from Serial Monitor (for debugging)
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("s")) {
      // test SMS
      sendSMS(ownerNumber, "Relay status now:\n R1 is ON \n R2 is OFF \n R3 is OFF \n R4 is OFF");
    }
    else if (cmd.equalsIgnoreCase("call")) {
      sendATCommand("ATD" + ownerNumber + ";", "OK", 10000);
    }
    else if (cmd.equalsIgnoreCase("h")) {
      sendATCommand("ATH", "OK", 2000);
    }
    else {
      Serial.println("Unknown command. Use: s=SMS, call=Dial, h=HangUp");
    }
  }
}

// ---------------------------------------------------------------------
//  handleIncomingResponse: parse URCs (+CMT, RING, +QTONEDET, etc.)
// ---------------------------------------------------------------------
void handleIncomingResponse(String &response) {
  response.trim();
  if (response.length() == 0) return;
  
  Serial.println("[EC200U] " + response);

  // 1) RING => auto-answer
  if (response.indexOf("RING") != -1) {
    Serial.println("Incoming call => Answering...");
    sendATCommand("ATA", "OK", 5000);
    return;
  }

  // 2) +CLIP => Caller ID
  if (response.indexOf("+CLIP:") != -1) {
    int sq = response.indexOf('"');
    int eq = response.indexOf('"', sq+1);
    if (sq != -1 && eq != -1) {
      callerNumber = response.substring(sq+1, eq);
      Serial.println("Caller Number: " + callerNumber);
    }
    return;
  }

  // 3) OK after ATA => call active
  if (!isCallActive && response.equals("OK") && callerNumber != "") {
    isCallActive = true;
    Serial.println("Call is now ACTIVE.");
    return;
  }

  // 4) Call end detection
  if ( response.indexOf("NO CARRIER") != -1 ||
       response.indexOf("BUSY") != -1      ||
       response.indexOf("NO ANSWER") != -1 ||
       (response.indexOf("ERROR") != -1 && isCallActive) )
  {
    Serial.println("Call ended => sending immediate summary...");
    if (isCallActive) {
      isCallActive = false;
      // Immediately send whatever changes we have so far
      sendImmediateCallEndSummary();
      callerNumber = "";
    }
    return;
  }

  // 5) +QTONEDET => DTMF digits
  //    ASCII '1'=49..'4'=52 => toggle
  //          '5'=53 => ALL ON
  //          '6'=54 => ALL OFF
  if (response.indexOf("+QTONEDET:") != -1) {
    int idx = response.indexOf(':');
    if (idx != -1) {
      String dtmfStr = response.substring(idx+1);
      dtmfStr.trim();
      int asciiVal = dtmfStr.toInt();
      
      if (asciiVal >= 49 && asciiVal <= 52) {
        int r = asciiVal - 49;
        toggleRelay(r);
      } else if (asciiVal == 53) {
        setAllRelays(true);
      } else if (asciiVal == 54) {
        setAllRelays(false);
      }
      // small delay
      delay(150);
    }
    return;
  }

  // 6) +CMT => next line is the SMS text
  static bool waitingForSMSContent = false;

  if (response.startsWith("+CMT:")) {
    // We skip phone number parsing because we always send summary to ownerNumber
    waitingForSMSContent = true;
    return;
  }

  if (waitingForSMSContent) {
    waitingForSMSContent = false;
    parseSMS(response);
    delay(200);
    return;
  }
}

// ---------------------------------------------------------------------
//  parseSMS: checks commands like "R1 ON", "ALL OFF", etc.
//  Instead of sending immediate replies, we just log changes and rely
//  on the 1-minute summary or call-end summary to send final statuses.
// ---------------------------------------------------------------------
void parseSMS(String smsText) {
  smsText.toUpperCase();
  smsText.trim();

  bool recognized = false;

  // 1) ALL ON / ALL OFF
  if (smsText.indexOf("ALL ON") != -1) {
    setAllRelays(true);
    recognized = true;
  }
  else if (smsText.indexOf("ALL OFF") != -1) {
    setAllRelays(false);
    recognized = true;
  }
  else {
    // 2) R1..R4 ON/OFF
    for (int i=0; i<NUM_RELAYS; i++) {
      String onCmd  = "R" + String(i+1) + " ON";
      String offCmd = "R" + String(i+1) + " OFF";
      if (smsText.indexOf(onCmd) != -1) {
        setRelay(i, true);
        recognized = true;
      }
      else if (smsText.indexOf(offCmd) != -1) {
        setRelay(i, false);
        recognized = true;
      }
    }
  }

  // It's normal to do nothing if unrecognized. 
  // We'll still do a summary eventually if recognized toggles occurred.
  if (!recognized) {
    Serial.println("Unrecognized SMS command: " + smsText);
    // If you want, you can log it or do partial help.
  }
}

// ---------------------------------------------------------------------
//  toggleRelay
// ---------------------------------------------------------------------
void toggleRelay(int relayIndex) {
  if (relayIndex < 0 || relayIndex >= NUM_RELAYS) return;
  bool newState = !relayState[relayIndex];
  setRelay(relayIndex, newState);
}

// ---------------------------------------------------------------------
//  setRelay (HIGH=ON, LOW=OFF), also track usage in EEPROM
// ---------------------------------------------------------------------
void setRelay(int relayIndex, bool on) {
  if (relayIndex < 0 || relayIndex >= NUM_RELAYS) return;
  
  bool current = relayState[relayIndex];
  if (current == on) return; // no change

  relayState[relayIndex] = on;
  digitalWrite(relayPins[relayIndex], on ? HIGH : LOW);

  // Update usage counters in EEPROM
  if (on) {
    unsigned int oldVal = readRelayOnCountFromEEPROM(relayIndex);
    saveRelayOnCountToEEPROM(relayIndex, oldVal+1);
  } else {
    unsigned int oldVal = readRelayOffCountFromEEPROM(relayIndex);
    saveRelayOffCountToEEPROM(relayIndex, oldVal+1);
  }

  // Print to Serial
  Serial.print("Relay ");
  Serial.print(relayIndex+1);
  Serial.println(on ? " => ON" : " => OFF");

  // Mark change in changesBuffer
  String line = "R" + String(relayIndex+1) + " => " + (on ? "ON" : "OFF");
  markChange(line);

  // A short delay
  delay(100);
}

// ---------------------------------------------------------------------
//  setAllRelays
// ---------------------------------------------------------------------
void setAllRelays(bool on) {
  for (int i=0; i<NUM_RELAYS; i++) {
    setRelay(i, on);
  }
  // Additional log line
  markChange(String("ALL RELAYS => ") + (on?"ON":"OFF"));
  Serial.println(on ? "ALL RELAYS => ON" : "ALL RELAYS => OFF");
}

// ---------------------------------------------------------------------
//  markChange: store the action in changesBuffer & reset the 1-min timer
// ---------------------------------------------------------------------
void markChange(String changeLine) {
  changesBuffer += changeLine + "\n";
  pendingSummary   = true;
  lastChangeMillis = millis();
}

// ---------------------------------------------------------------------
//  sendSummaryToOwner: after 1 minute of inactivity
// ---------------------------------------------------------------------
void sendSummaryToOwner() {
  if (!pendingSummary) return;  // no changes to send?

  // Compile final states
  String finalStates = "Final Relay States:\n";
  for (int i=0; i<NUM_RELAYS; i++) {
    finalStates += "R" + String(i+1) + " => " + (relayState[i]?"ON":"OFF") + "\n";
  }

  // Put them together
  String message = "Actions:\n" + changesBuffer + "\n" + finalStates;
  
  sendSMS(ownerNumber, message);
  
  // Clear buffer
  changesBuffer   = "";
  pendingSummary  = false;
}

// ---------------------------------------------------------------------
//  sendImmediateCallEndSummary
//   - If a call just ended, we skip the 1-minute wait and send the summary now.
//   - Then we reset the buffer so it won't get sent again later.
// ---------------------------------------------------------------------
void sendImmediateCallEndSummary() {
  if (!pendingSummary) {
    // No changes => we can send a minimal note or skip entirely
    sendSMS(ownerNumber, "Call ended. No relay changes happened.");
    return;
  }

  // We do the same logic as sendSummaryToOwner()
  String finalStates = "Final Relay States:\n";
  for (int i=0; i<NUM_RELAYS; i++) {
    finalStates += "R" + String(i+1) + " => " + (relayState[i]?"ON":"OFF") + "\n";
  }

  String message = "Call ended.\nChanges:\n" + changesBuffer + "\n" + finalStates;
  sendSMS(ownerNumber, message);

  // Clear
  changesBuffer   = "";
  pendingSummary  = false;
}

// ---------------------------------------------------------------------
//  sendSMS
// ---------------------------------------------------------------------
void sendSMS(String number, String text) {
  if (number.length() == 0) return;

  // Make sure in text mode
  sendATCommand("AT+CMGF=1", "OK", 2000);

  // AT+CMGS
  String cmd = "AT+CMGS=\"" + number + "\"";
  sendATCommand(cmd, ">", 3000);

  // Write the text
  ec200u.print(text);
  delay(200);

  // Ctrl+Z
  ec200u.write(26);
  Serial.println("SMS Sent to " + number);

  // Let the modem handle +CMGS URC
  delay(1000);
}

// ---------------------------------------------------------------------
//  sendATCommand
// ---------------------------------------------------------------------
void sendATCommand(String command, String expectedResponse, unsigned long timeout) {
  ec200u.println(command);
  unsigned long start = millis();
  String receivedData;

  while (millis() - start < timeout) {
    while (ec200u.available() > 0) {
      char c = ec200u.read();
      receivedData += c;
    }
  }

  Serial.println("-------------------------------------------------");
  Serial.println("Command: " + command);
  Serial.println("Response: " + receivedData);
  if (receivedData.indexOf(expectedResponse) != -1) {
    Serial.println("Got expected response.");
  } else {
    Serial.println("Expected response NOT found or Timeout!");
  }
  Serial.println("-------------------------------------------------");
}

// ---------------------------------------------------------------------
//  initRelays
//    => "HIGH = ON, LOW = OFF"
//    => so default them to OFF => digitalWrite(pin, LOW)
// ---------------------------------------------------------------------
void initRelays() {
  for (int i=0; i<NUM_RELAYS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);   // OFF
    relayState[i] = false;
  }
}

// ---------------------------------------------------------------------
//  EEPROM usage read/write
// ---------------------------------------------------------------------
void loadRelayUsageFromEEPROM() {
  Serial.println("Loading relay usage from EEPROM...");
  for (int i=0; i<NUM_RELAYS; i++) {
    unsigned int onC  = readRelayOnCountFromEEPROM(i);
    unsigned int offC = readRelayOffCountFromEEPROM(i);
    Serial.print("Relay ");
    Serial.print(i+1);
    Serial.print(": ON count=");
    Serial.print(onC);
    Serial.print(", OFF count=");
    Serial.println(offC);
  }
  Serial.println();
}

void saveRelayOnCountToEEPROM(int relayIndex, unsigned int value) {
  int addr = getEEPROMAddressOnCount(relayIndex);
  byte lowByte  = value & 0xFF;
  byte highByte = (value >> 8) & 0xFF;
  EEPROM.update(addr,   lowByte);
  EEPROM.update(addr+1, highByte);
}

void saveRelayOffCountToEEPROM(int relayIndex, unsigned int value) {
  int addr = getEEPROMAddressOffCount(relayIndex);
  byte lowByte  = value & 0xFF;
  byte highByte = (value >> 8) & 0xFF;
  EEPROM.update(addr,   lowByte);
  EEPROM.update(addr+1, highByte);
}

unsigned int readRelayOnCountFromEEPROM(int relayIndex) {
  int addr = getEEPROMAddressOnCount(relayIndex);
  byte lowByte  = EEPROM.read(addr);
  byte highByte = EEPROM.read(addr+1);
  unsigned int val = ((unsigned int)highByte << 8) | lowByte;
  return val;
}

unsigned int readRelayOffCountFromEEPROM(int relayIndex) {
  int addr = getEEPROMAddressOffCount(relayIndex);
  byte lowByte  = EEPROM.read(addr);
  byte highByte = EEPROM.read(addr+1);
  unsigned int val = ((unsigned int)highByte << 8) | lowByte;
  return val;
}
