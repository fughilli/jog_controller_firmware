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

// ESP32Encoder encoder;
WiFiClient client;
ArduinoStreamAdapter client_stream(&client);
util::Base64EncodeStream b64_encode_stream;

Control control = Control_init_default;
pb_ostream_t pb_stream;

Adafruit_ST7735 tft = Adafruit_ST7735(25, 27, 26);

Keypad keypad{&Wire, 0x20, 2};

// 21, 19, 18, 5, 17, 16,    4,   0,  2
//  4,  Z,  Y, X,  5,  6, X100, X10, X1
// constexpr int kSwitch4 = 21;
constexpr int kSwitchZ = 19;
constexpr int kSwitchY = 18;
constexpr int kSwitchX = 5;
constexpr int kSwitch5 = 17;
constexpr int kSwitch6 = 16;
constexpr int kSwitchX100 = 4;
constexpr int kSwitchX10 = 0;
// constexpr int kSwitchX1 = 2;

void InitSwitches() {
  // pinMode(kSwitch4, INPUT_PULLUP);
  pinMode(kSwitchZ, INPUT_PULLUP);
  pinMode(kSwitchY, INPUT_PULLUP);
  pinMode(kSwitchX, INPUT_PULLUP);
  pinMode(kSwitch5, INPUT_PULLUP);
  pinMode(kSwitch6, INPUT_PULLUP);
  pinMode(kSwitchX100, INPUT_PULLUP);
  pinMode(kSwitchX10, INPUT_PULLUP);
  // pinMode(kSwitchX1, INPUT_PULLUP);
}

void ReadSwitches(Control* control) {
  control->has_axis = false;
  control->has_multiplier = false;

  if (digitalRead(kSwitchX) == LOW) {
    control->has_axis = true;
    control->axis = Control_Axis_AXIS_X;
  } else if (digitalRead(kSwitchY) == LOW) {
    control->has_axis = true;
    control->axis = Control_Axis_AXIS_Y;
  } else if (digitalRead(kSwitchZ) == LOW) {
    control->has_axis = true;
    control->axis = Control_Axis_AXIS_Z;
    //} else if (digitalRead(kSwitch4) == LOW) {
    //  control->has_axis = true;
    //  control->axis = Control_Axis_AXIS_4;
  } else if (digitalRead(kSwitch5) == LOW) {
    control->has_axis = true;
    control->axis = Control_Axis_AXIS_5;
  } else if (digitalRead(kSwitch6) == LOW) {
    control->has_axis = true;
    control->axis = Control_Axis_AXIS_6;
  }

  // if (digitalRead(kSwitchX1) == LOW) {
  //  control->has_multiplier = true;
  //  control->multiplier = Control_Multiplier_MULT_X1;
  //} else
  if (digitalRead(kSwitchX10) == LOW) {
    control->has_multiplier = true;
    control->multiplier = Control_Multiplier_MULT_X10;
  } else if (digitalRead(kSwitchX100) == LOW) {
    control->has_multiplier = true;
    control->multiplier = Control_Multiplier_MULT_X100;
  }
}

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

void ExtMain() {
  ESP32Encoder::useInternalWeakPullResistors = NONE;
  Serial.begin(115200);
  // encoder.attachFullQuad(22, 23);
  SPI.setFrequency(20000000);
  SPI.begin(14, 12, 13, 15);
  tft.initR(INITR_GREENTAB);
  tft.setColRowStart(0, 0);
  InitSwitches();

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextWrap(true);
  tft.setRotation(1);
  tft.fillRect(0, 0, 160, 128, ST77XX_BLACK);

  Wire.begin();
  keypad.RegisterKeyHandler(&KeyHandler);
  keypad.Begin();

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

const char* kAxisNames[] = {"X", "Y", "Z", "4", "5", "6"};
const int kMultiplierValues[] = {1, 10, 100};

void UpdateDisplay(const Control& new_control) {
  static Control current_control = Control_init_default;
  if (memcmp(&new_control, &current_control, sizeof(Control)) == 0) {
    return;
  }

  current_control = new_control;

  tft.setFont(&FreeSans9pt7b);

  tft.fillRect(0, 0, 160, 24, ST77XX_BLACK);
  tft.setCursor(8, 16);
  if (current_control.has_axis) {
    tft.print("Jog ");
    tft.print(kAxisNames[clamp(static_cast<int>(current_control.axis), 0, 5)]);
    tft.print(": ");
    tft.print(current_control.value);
  } else {
    tft.print("<NAV>");
  }

  tft.fillRect(0, 24, 160, 48, ST77XX_BLACK);
  tft.setCursor(8, 40);
  if (current_control.has_multiplier) {
    tft.print("X");
    tft.print(kMultiplierValues[clamp(
        static_cast<int>(current_control.multiplier), 0, 2)]);
  }
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

    // control.has_value = true;
    // control.value = static_cast<int32_t>(encoder.getCount());

    ReadSwitches(&control);

    keypad.Poll();

    client.write('^');
    pb_encode(&pb_stream, Control_fields, &control);
    b64_encode_stream.Flush();
    client.write("$\r\n");
    pb_stream.bytes_written = 0;

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
