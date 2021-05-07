#include <Notecard.h>
#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
#endif

#define productUID "com.blues.zfields:showcase"
#define serialDebug Serial

static const size_t SWITCH_PIN = 14;

Notecard notecard;

void setup() {
#ifdef ARDUINO_ARCH_ESP32
  // Disable radios to improve power profile
  WiFi.mode(WIFI_OFF);
  ::btStop();
#endif

  // Provide visual signal when the Host MCU is powered
  ::pinMode(LED_BUILTIN, OUTPUT);
  ::digitalWrite(LED_BUILTIN, HIGH);

  // Initialize Debug Output
  serialDebug.begin(115200);
  static const size_t MAX_SERIAL_WAIT_MS = 5000;
  size_t begin_serial_wait_ms = ::millis();
  while (!serialDebug && (MAX_SERIAL_WAIT_MS > (::millis() - begin_serial_wait_ms))) {
    ; // wait for serial port to connect. Needed for native USB
  }
  notecard.setDebugOutputStream(serialDebug);

  // Initialize Notecard
  notecard.begin();

  // Check alert latch
  ::pinMode(SWITCH_PIN, INPUT);
  if (::digitalRead(SWITCH_PIN)) {
    // Alert already sent
  } else {
    // Send mailbox alert

    // Activate alert latch
    ::pinMode(SWITCH_PIN, OUTPUT);
    ::digitalWrite(SWITCH_PIN, HIGH);
  
    // Configure Notecard to synchronize with Notehub periodically,
    // as well as adjust the frequency based on the battery level.
    {
      J * req = notecard.newRequest("hub.set");
      JAddStringToObject(req, "product", productUID);
      JAddStringToObject(req, "mode", "periodic");
      JAddStringToObject(req, "vinbound", "usb:10;high:360;normal:720;low:1440;dead:0");
      JAddStringToObject(req, "voutbound", "usb:10;high:180;normal:180;low:360;dead:0");
      notecard.sendRequest(req);
    }
  
    // Optimize voltage variable behaviors for LiPo battery
    {
      J * req = notecard.newRequest("card.voltage");
      JAddStringToObject(req, "mode", "lipo");
      notecard.sendRequest(req);
    }
  
    // Configure IMU sensitivity
    {
      J * req = notecard.newRequest("card.motion.mode");
      JAddBoolToObject(req, "start", true);
      JAddNumberToObject(req, "sensitivity", 2); // 3.904G
      notecard.sendRequest(req);
    }
  
    // Send Motion Detection Alert
    {
      J * req = notecard.newRequest("note.add");
      JAddBoolToObject(req, "sync", true);
      JAddStringToObject(req, "file", "mailbox.qo");
      JAddObjectToObject(req, "body");
      notecard.sendRequest(req);
    }
  }
}

void loop() {
  // Request sleep from loop to safeguard against tranmission failure, and
  // ensure sleep request is honored so power usage is minimized.
  {
    // Create a "command" instead of a "request", because the host
    // MCU is going to power down and cannot receive a response.
    J * req = NoteNewCommand("card.attn");
    JAddStringToObject(req, "mode", "arm,motion");
    notecard.sendRequest(req);
  }

  ::delay(3000);
}
