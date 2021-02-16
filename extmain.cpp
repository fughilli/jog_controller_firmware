#include "extmain.h"

#include <Adafruit_ST7735.h>
#include <EEPROM.h>
#include <ESP32Encoder.h>
#include <Fonts/FreeSans9pt7b.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <WString.h>
#include <WiFi.h>
#include <stdint.h>

#include <algorithm>

#include "base64_stream.h"
#include "control_message.pb.h"
#include "credentials.h"
#include "keypad.h"
#include "pb_stream.h"
#include "stream.h"
#include "switches.h"

namespace jog_controller {

template <typename T>
T clamp(T value, T low, T high) {
  return std::min(high, std::max(low, value));
}

class ArduinoStreamAdapter : public util::Stream<uint8_t> {
 public:
  ArduinoStreamAdapter(::Stream* stream) : stream_(stream) {}

  bool Write(const uint8_t& token) final {
    stream_->write(token);
    return true;
  }

 private:
  ::Stream* stream_;
};

ESP32Encoder encoder;
WiFiClient client;
ArduinoStreamAdapter client_stream(&client);
util::Base64EncodeStream b64_encode_stream;

Control control = Control_init_default;
pb_ostream_t pb_stream;

Adafruit_ST7735 tft = Adafruit_ST7735(25, 27, 26);

Keypad keypad{&Wire, 0x24, 19};
Switches switches{&Wire, 0x20, 18, 5};

void KeyHandler(int key, KeyState state) {
  if (state == KeyState::kPressed) {
    control.has_key_pressed = true;
    control.key_pressed |= (1 << key);
  }

  if (state == KeyState::kReleased) {
    control.has_key_released = true;
    control.key_released |= (1 << key);
  }
}

void RotarySwitchHandler(RotarySwitch index, int position) {
  switch (index) {
    case RotarySwitch::kAxis:
      control.has_axis = true;
      control.axis = static_cast<Control_Axis>(position);
      break;

    case RotarySwitch::kMultiplier:
      control.has_multiplier = true;
      control.multiplier = static_cast<Control_Multiplier>(position);
      break;
  }
}

void ButtonHandler(int button, KeyState state) {
  switch (button) {
    case Switches::kEstopIndex:
      control.has_estop = true;
      control.estop = (state == KeyState::kPressed);
      break;

    case Switches::kFeedholdIndex:
      control.has_feedhold = true;
      control.feedhold = (state == KeyState::kPressed);
      break;
  }
}

void ExtMain() {
  ESP32Encoder::useInternalWeakPullResistors = NONE;
  Serial.begin(115200);
  encoder.attachFullQuad(35, 34);
  SPI.setFrequency(20000000);
  SPI.begin(14, 12, 13, 15);
  tft.initR(INITR_GREENTAB);
  tft.setColRowStart(0, 0);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextWrap(true);
  tft.setRotation(3);
  tft.fillRect(0, 0, 160, 128, ST77XX_BLACK);

  Wire.begin();
  keypad.RegisterKeyHandler(&KeyHandler);
  keypad.Begin();
  switches.RegisterRotarySwitchHandler(&RotarySwitchHandler);
  switches.RegisterKeyHandler(&ButtonHandler);
  switches.Begin();

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(kSsid);

  WiFi.begin(kSsid, kPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  b64_encode_stream.RegisterDownstream(&client_stream);
  pb_stream = util::WrapStream(&b64_encode_stream);
}

const char* kAxisNames[] = {"<NAV>", "X", "Y", "Z", "4", "5", "6"};
const int kMultiplierValues[] = {1, 10, 100};

void UpdateDisplay(const Control& new_control) {
  static Control current_control = Control_init_default;
  if (memcmp(&new_control, &current_control, sizeof(Control)) == 0) {
    return;
  }

  current_control = new_control;

  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(ST77XX_WHITE);

  if (current_control.has_axis) {
    tft.fillRect(0, 0, 160, 24, ST77XX_BLACK);
    tft.setCursor(8, 16);
    tft.print("Jog ");
    tft.print(kAxisNames[clamp(static_cast<int>(current_control.axis), 0, 6)]);
    tft.print(": ");
    tft.print(current_control.value);
  }

  if (current_control.has_multiplier) {
    tft.fillRect(0, 24, 160, 48, ST77XX_BLACK);
    tft.setCursor(8, 40);
    tft.print("X");
    tft.print(kMultiplierValues[clamp(
        static_cast<int>(current_control.multiplier), 0, 2)]);
  }

  if (current_control.has_estop) {
    if (current_control.estop) {
      tft.setCursor(8, 64);
      tft.setTextColor(ST77XX_RED);
      tft.print("!ESTOP!");
    } else {
      tft.fillRect(0, 48, 160, 72, ST77XX_BLACK);
    }
  }

  if (current_control.has_feedhold) {
    if (current_control.feedhold) {
      tft.setCursor(8, 88);
      tft.setTextColor(ST77XX_BLUE);
      tft.print("Feedhold");
    } else {
      tft.fillRect(0, 72, 160, 96, ST77XX_BLACK);
    }
  }
}

void WriteControl() {
  static Control last_control = Control_init_default;

  if (memcmp(&control, &last_control, sizeof(Control)) == 0) {
    return;
  }

  client.write('^');
  pb_encode(&pb_stream, Control_fields, &control);
  b64_encode_stream.Flush();
  client.write("$\r\n");
  pb_stream.bytes_written = 0;

  last_control = control;
}

void ExtLoop() {
  Serial.print("connecting to ");
  Serial.println(kHost);

  // Use WiFiClient class to create TCP connections
  if (!client.connect(kHost, kPort)) {
    Serial.println("connection failed");
    delay(2000);
    return;
  }

  while (true) {
    control = Control_init_default;
    // This will send the request to the server
    unsigned long timeout = millis();

    control.has_value = true;
    control.value = static_cast<int32_t>(encoder.getCount());

    switches.Poll();
    keypad.Poll();

    WriteControl();

    UpdateDisplay(control);

    // Read all the lines of the reply from server and print them to Serial
    while (client.available()) {
      client.read();
    }

    delay(100);
  }

  Serial.println();
  Serial.println("closing connection");
}

}  // namespace jog_controller
