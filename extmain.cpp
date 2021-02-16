#include "extmain.h"

#include <Adafruit_MCP23017.h>
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

ESP32Encoder encoder;
WiFiClient client;
ArduinoStreamAdapter client_stream(&client);
util::Base64EncodeStream b64_encode_stream;

Control control = Control_init_default;
pb_ostream_t pb_stream;

Adafruit_ST7735 tft = Adafruit_ST7735(25, 27, 26);

Keypad keypad{&Wire, 0x24, 19};

Adafruit_MCP23017 io_expander;

bool port_a_flag = false;
bool port_b_flag = false;

// 18, 5 for MCP interrupts
//
void Mcp23017PortAIinterrupt() { port_a_flag = true; }
void Mcp23017PortBIinterrupt() { port_b_flag = true; }

void InitSwitches() {
  io_expander.begin(0, &Wire);

  pinMode(18, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  attachInterrupt(18, &Mcp23017PortAIinterrupt, FALLING);
  attachInterrupt(5, &Mcp23017PortBIinterrupt, FALLING);

  io_expander.pinMode(0, OUTPUT);
  io_expander.digitalWrite(0, LOW);

  for (int i = 6; i <= 15; ++i) {
    io_expander.pullUp(i, 1);
    io_expander.setupInterruptPin(i, CHANGE);
  }

  io_expander.setupInterrupts(false, true, LOW);
}

void ReadSwitches(Control* control) {
  control->has_axis = false;
  control->has_multiplier = false;

  if (port_a_flag) {
    port_a_flag = false;
    control->has_port_a_flag = true;
    control->port_a_flag = true;
  }

  if (port_b_flag) {
    port_b_flag = false;
    control->has_port_b_flag = true;
    control->port_b_flag = true;
  }

  uint16_t mask = io_expander.readGPIOAB();

  uint8_t axis_mask = util::GetField<uint16_t>(mask, 6, 8);

  control->has_axis = true;
  switch (util::CountLeadingZeros<uint8_t>(~(axis_mask | 0xc0))) {
    case 2:
      control->axis = Control_Axis_AXIS_6;
      break;
    case 3:
      control->axis = Control_Axis_AXIS_5;
      break;
    case 4:
      control->axis = Control_Axis_AXIS_4;
      break;
    case 5:
      control->axis = Control_Axis_AXIS_Z;
      break;
    case 6:
      control->axis = Control_Axis_AXIS_Y;
      break;
    case 7:
      control->axis = Control_Axis_AXIS_X;
      break;
    default:
      control->has_axis = false;
  }

  uint8_t multiplier_mask = util::GetField<uint16_t>(mask, 2, 14);

  control->has_multiplier = true;
  switch (util::CountLeadingZeros<uint8_t>(~(multiplier_mask | 0xfc))) {
    case 6:
      control->multiplier = Control_Multiplier_MULT_X100;
      break;
    case 7:
      control->multiplier = Control_Multiplier_MULT_X10;
      break;
    default:
      control->multiplier = Control_Multiplier_MULT_X1;
      break;
  }

  bool estop = util::GetField<uint16_t>(mask, 1, 7);
  if (estop) {
    control->has_estop = true;
    control->estop = estop;
  }

  bool feedhold = util::GetField<uint16_t>(mask, 1, 6);
  if (feedhold) {
    control->has_feedhold = true;
    control->feedhold = feedhold;
  }

  control->has_port_mask = true;
  control->port_mask = mask;
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
  encoder.attachFullQuad(34, 35);
  SPI.setFrequency(20000000);
  SPI.begin(14, 12, 13, 15);
  tft.initR(INITR_GREENTAB);
  tft.setColRowStart(0, 0);
  InitSwitches();

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextWrap(true);
  tft.setRotation(3);
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

    ReadSwitches(&control);

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
