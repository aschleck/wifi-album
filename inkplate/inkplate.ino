#include <Inkplate.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

#include "cat.h"

const uint16_t PORT = 9876;
Inkplate display(INKPLATE_1BIT);
const size_t DISPLAY_WIDTH = 1200;
const size_t DISPLAY_HEIGHT = 825;
const size_t DISPLAY_RESOLUTION = DISPLAY_WIDTH * DISPLAY_HEIGHT;
uint8_t *framebuffer;

// Draw an 8 bit BMP into 1 bit Inkplate colorspace
void draw_bitmap_1bit(int16_t x, int16_t y, const uint8_t *p, int16_t w, int16_t h) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      display.drawPixel(j + x, i + y, *(p + w * i + j) < 128 ? BLACK : WHITE);
    }
  }
}

// Draw an 8 bit BMP into the 3 bit Inkplate colorspace
void draw_bitmap_3bit(int16_t x, int16_t y, const uint8_t *p, int16_t w, int16_t h) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      display.drawPixel(j + x, i + y, *(p + w * i + j) >> 5);
    }
  }
}

void draw_packed_1bit(int16_t x, int16_t y, const uint8_t *p, int16_t w, int16_t h) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j += 8) {
      uint8_t pixel = p[(w * i + j) / 8];
      display.drawPixel(j + 0 + x, i + y, (pixel >> 7) & 0x01);
      display.drawPixel(j + 1 + x, i + y, (pixel >> 6) & 0x01);
      display.drawPixel(j + 2 + x, i + y, (pixel >> 5) & 0x01);
      display.drawPixel(j + 3 + x, i + y, (pixel >> 4) & 0x01);
      display.drawPixel(j + 4 + x, i + y, (pixel >> 3) & 0x01);
      display.drawPixel(j + 5 + x, i + y, (pixel >> 2) & 0x01);
      display.drawPixel(j + 6 + x, i + y, (pixel >> 1) & 0x01);
      display.drawPixel(j + 7 + x, i + y, (pixel >> 0) & 0x01);
    }
  }
}
void draw_cat() {
  // Note that this is undithered
  draw_bitmap_1bit(0, 0, cat_pgm, 1200, 825);
}

void setup_wifi() {
  const char *ssid = "your-ssid";
  const char *password = "your-password";
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to ");
  Serial.println(ssid);
}

void read_uint32(WiFiClient &from, uint32_t &v) {
  int got = 0;
  while (from.connected() && got < 4) {
    got += from.readBytes(reinterpret_cast<unsigned char *>(&v) + got, 4 - got);
  }
  v = ntohl(v);
}

int read_varbytes(WiFiClient &from, void *buffer, size_t limit) {
  uint32_t remaining;
  read_uint32(from, remaining);

  int actual = static_cast<int>(min(remaining, limit));
  int got = 0;
  while (from.connected() && got < actual) {
    got += from.readBytes(reinterpret_cast<uint8_t *>(buffer), actual - got);
  }
  return got;
}

void handle_message(WiFiClient &from) {
  char command = from.read();
  if (command == '1') {
    size_t got = read_varbytes(from, framebuffer, DISPLAY_RESOLUTION / 8);
    if (got == DISPLAY_RESOLUTION / 8) {
      Serial.print("Updating framebuffer, got pixel count ");
      Serial.println(got, DEC);
      draw_packed_1bit(0, 0, framebuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
      display.display(/* leave_on= */ true);
      from.print("y");
    } else {
      Serial.print("Bad pixel count ");
      Serial.println(got, DEC);
      from.print("n");
    }
  } else if (command == '3') {
    size_t got = read_varbytes(from, framebuffer, DISPLAY_RESOLUTION);
    if (got == DISPLAY_RESOLUTION) {
      Serial.print("Updating framebuffer, got pixel count ");
      Serial.println(got, DEC);
      draw_bitmap_3bit(0, 0, framebuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
      display.display(/* leave_on= */ true);
      from.print("y");
    } else {
      Serial.print("Bad pixel count ");
      Serial.println(got, DEC);
      from.print("n");
    }
  } else if (command == 'c') {
    Serial.println("Updating framebuffer to a cat");
    draw_cat();
    display.display(/* leave_on= */ true);
    from.print("y");
  } else {
    Serial.print("Unknown command: ");
    Serial.println(command);
    from.print("Unknown command: ");
    from.print("n");
    from.flush(); // flushes recv too for some crazy reason
  }
}

void setup() {
  framebuffer = reinterpret_cast<uint8_t *>(ps_malloc(DISPLAY_RESOLUTION));

  Serial.begin(460800);
  Serial.setTimeout(20000 /* ms */);
  display.begin();
  display.clearDisplay();
  display.einkOn();
  display.display(/* leave_on= */ true);

  setup_wifi();
  draw_cat();
  display.display(/* leave_on= */ true);

  WiFiServer server(PORT);
  server.begin();
  Serial.print(WiFi.localIP().toString());
  Serial.print(":");
  Serial.println(PORT, DEC);

  const size_t max_clients = 4;
  WiFiClient clients[max_clients];
  size_t client_count = 0;

  while (true) {
    WiFiClient new_connection = server.available();
    if (new_connection) {
      if (client_count < max_clients) {
        clients[client_count] = new_connection;
        client_count += 1;
      } else {
        new_connection.println("Too many connections, closing");
        new_connection.stop();
      }
    }

    for (size_t i = 0; i < client_count; ++i) {
      WiFiClient &client = clients[i];
      if (client.available()) {
        handle_message(client);
      }

      if (!client.connected()) {
        Serial.print("Closing client ");
        Serial.println(client.remoteIP().toString());

        for (size_t j = i; j < client_count - 1; ++j) {
          clients[j] = clients[j + 1];
        }
        client_count -= 1;
        i -= 1;
        continue;
      }
    }

    delay(50);
  }
}

void loop() {}

