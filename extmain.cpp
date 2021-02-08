#include "extmain.h"

#include <Adafruit_ST7735.h>
#include <EEPROM.h>
#include <ESP32Encoder.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <WString.h>
#include <WiFi.h>
#include <stdint.h>

#include "base64_stream.h"
#include "control_message.pb.h"
#include "credentials.h"
#include "pb_stream.h"
#include "stream.h"

namespace jog_controller {

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

void ExtMain() {
  ESP32Encoder::useInternalWeakPullResistors = NONE;
  Serial.begin(115200);
  encoder.attachFullQuad(22, 23);
  SPI.setFrequency(20000000);
  SPI.begin(14, 12, 13, 15);
  tft.initR(INITR_GREENTAB);

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

void UpdateDisplay(const Control& new_control) {
  static Control current_control = Control_init_default;
  if (memcmp(&new_control, &current_control, sizeof(Control)) == 0) {
    return;
  }

  current_control = new_control;

  tft.fillRect(0, 0, 160, 16, ST77XX_BLACK);
  tft.setCursor(8, 4);
  tft.print(String() + "X: " + current_control.value);
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

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextWrap(true);
  tft.setRotation(1);
  tft.fillRect(0, 0, 160, 128, ST77XX_BLACK);

  while (true) {
    // This will send the request to the server
    unsigned long timeout = millis();

    control.has_value = true;
    control.value = static_cast<int32_t>(encoder.getCount());

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
