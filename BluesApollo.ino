#define NDEBUG

#include <Notecard.h>
#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
#endif

#ifndef NDEBUG
#define serialDebug Serial
#endif

#define productUID "com.blues.zfields:showcase"

static const size_t BUTTON_PIN = D6;
static const size_t SWITCH_PIN = D5;

Notecard notecard;

int awaitDoorClose(size_t timeout_ms_ = 35000) {
  int result;

  // Wait for mail carrier to deposit mail
  const size_t begin_door_close_wait_ms = ::millis();
  bool door_ajar = ::digitalRead(SWITCH_PIN);
  for ( ; door_ajar && (timeout_ms_ > (::millis() - begin_door_close_wait_ms)) ; door_ajar = ::digitalRead(SWITCH_PIN)) {
    ; // Wait for door to close for up to 35s.
  }
  const size_t door_ajar_duration_ms = (::millis() - begin_door_close_wait_ms);
  if (door_ajar) {
    result = -1;

    // Send Motion Detection Alert
    {
      J * req = notecard.newRequest("note.add");
      JAddBoolToObject(req, "sync", true);
      JAddStringToObject(req, "file", "mailbox.qo");
      J * body = JAddObjectToObject(req, "body");
      JAddBoolToObject(body, "alert", true);
      JAddStringToObject(body, "msg", "DOOR AJAR");
      notecard.sendRequest(req);
    }

#ifndef NDEBUG
    notecard.logDebug("WARNING: Door close timeout occurred!\r\n");
#endif
  } else {
    result = door_ajar_duration_ms;

#ifndef NDEBUG
    notecard.logDebugf("Door closed after %lu milliseconds.\r\n", door_ajar_duration_ms);
#endif
  }

  return result;
}

void setup() {
  // Provide visual signal when the Host MCU is powered
  ::pinMode(LED_BUILTIN, OUTPUT);
  ::digitalWrite(LED_BUILTIN, HIGH);

  // Configure buttons and switches
  ::pinMode(BUTTON_PIN, INPUT_PULLUP);
  ::pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Debounce mailbox door
  ::delay(250);

  // Check override button
  const bool override = !(::digitalRead(BUTTON_PIN)); // pull-up grounded via button press

  // Check alert latch
  const bool door_ajar = ::digitalRead(SWITCH_PIN); // pull-up grounded via door switch

#ifdef ARDUINO_ARCH_ESP32
  // Disable radios to improve power profile
  WiFi.mode(WIFI_OFF);
  ::btStop();
#endif

#ifndef NDEBUG
  // Initialize Debug Output
  serialDebug.begin(115200);
  static const size_t MAX_SERIAL_WAIT_MS = 5000;
  size_t begin_serial_wait_ms = ::millis();
  while (!serialDebug && (MAX_SERIAL_WAIT_MS > (::millis() - begin_serial_wait_ms))) {
    ; // wait for serial port to connect. Needed for native USB
  }
  notecard.setDebugOutputStream(serialDebug);
#endif

  // Initialize Notecard
  notecard.begin();

  // Send mailbox alert only when door is ajar
  if (override && door_ajar) {
#ifndef NDEBUG
    notecard.logDebug("Mail collection in progress...\r\n");
#endif

    // Configure Notecard to synchronize with Notehub periodically,
    // as well as adjust the frequency based on the battery level.
    {
      J * req = notecard.newRequest("hub.set");
      JAddStringToObject(req, "product", productUID);
      JAddStringToObject(req, "sn", "Apollo");
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
      JAddNumberToObject(req, "sensitivity", 3); // 1.952G
      notecard.sendRequest(req);
    }

    awaitDoorClose();
  } else if (door_ajar) {
#ifndef NDEBUG
    notecard.logDebug("Reporting mail deposit...\r\n");
#endif

    const int door_open_ms = awaitDoorClose();

    if (0 <= door_open_ms) {
      // Send Motion Detection Alert
      {
        J * req = notecard.newRequest("note.add");
        JAddBoolToObject(req, "sync", true);
        JAddStringToObject(req, "file", "mailbox.qo");
        J * body = JAddObjectToObject(req, "body");
        JAddNumberToObject(body, "door_open_ms", door_open_ms);
        notecard.sendRequest(req);
      }
    }
#ifndef NDEBUG
  } else {
    notecard.logDebug("Spurious motion event occurred.\r\n");
#endif
  }
}

void loop() {
  // Request sleep from loop to safeguard against tranmission failure, and
  // ensure sleep request is honored so power usage is minimized.
  {
    // Create a "command" instead of a "request", because the host
    // MCU is going to power down and cannot receive a response.
    J * cmd = NoteNewCommand("card.attn");
    JAddStringToObject(cmd, "mode", "arm,motion");
    notecard.sendRequest(cmd);

    // Pause for power down between retries
    ::delay(1000);
  }
}
