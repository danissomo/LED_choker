#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <DNSServer.h>
#include <LittleFS.h>
// ================= CONFIG =================

#define LED_PIN       1
#define BATTERY_PIN   2

#define LED_COUNT     18
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB

#define DEFAULT_BRIGHTNESS  100

const char* AP_SSID = "ESP32-LED";
const char* AP_PASS = "12345678";

// Battery divider
#define VOLTAGE_DIVIDER 2.0f

// ==========================================

CRGB leds[LED_COUNT];
DNSServer dnsServer;

const byte DNS_PORT = 53;

IPAddress apIP(192, 168, 4, 1);
IPAddress netMask(255, 255, 255, 0);
WebServer server(80);

uint8_t brightness = DEFAULT_BRIGHTNESS;
CRGB currentColor = CRGB::Blue;

uint8_t effectSpeed = 50;

String currentEffect = "rainbow";

float batteryVoltage = 0.0f;
int batteryPercent = 0;

uint8_t hue = 0;
uint8_t effectStep = 0;

uint8_t heat[LED_COUNT];

CRGB backgroundColor = CRGB::Black;
CRGB pixelColor = CRGB::Red;

// using in effectScanner(), effectRunning()
uint8_t trailLength = 5;


// ================= BATTERY =================

float readBatteryVoltage() {
    uint32_t sum = 0;

    for (int i = 0; i < 16; i++) {
        sum += analogReadMilliVolts(BATTERY_PIN);
    }

    float adcVoltage = (sum / 16.0f) / 1000.0f;

    return adcVoltage * VOLTAGE_DIVIDER;
}

int voltageToPercent(float voltage) {
    // Approximate Li-ion discharge curve

    const float voltageTable[] = {
        3.00, 3.30, 3.50, 3.60, 3.70,
        3.80, 3.90, 4.00, 4.10, 4.20
    };

    const int percentTable[] = {
        0, 5, 15, 25, 40,
        60, 75, 85, 95, 100
    };

    if (voltage <= voltageTable[0])
        return 0;

    if (voltage >= voltageTable[9])
        return 100;

    for (int i = 0; i < 9; i++) {
        if (voltage >= voltageTable[i] &&
            voltage <= voltageTable[i + 1]) {

            float t = (voltage - voltageTable[i]) /
                      (voltageTable[i + 1] - voltageTable[i]);

            return percentTable[i] +
                t * (percentTable[i + 1] - percentTable[i]);
        }
    }

    return 0;
}

void updateBattery() {
    static uint32_t lastBatteryRead = 0;
    if (millis() - lastBatteryRead < 2000)
        return;

    lastBatteryRead = millis();

    batteryVoltage = readBatteryVoltage();
    batteryPercent = voltageToPercent(batteryVoltage);
}

// ================= EFFECTS =================

void effectStrobe() {
    static bool strobeState = true;
    static uint32_t lastStrobeToggle = 0;
    uint32_t now = millis();

    // Полупериод вспышки: 500–30 мс
    uint16_t interval = map(
        effectSpeed,
        1,
        100,
        500,
        30
    );

    if (now - lastStrobeToggle >= interval) {
        lastStrobeToggle = now;
        strobeState = !strobeState;
    }

    if (strobeState) {
        fill_solid(leds, LED_COUNT, currentColor);
    } else {
        fill_solid(leds, LED_COUNT, backgroundColor);
    }
}

void effectScanner() {
    static int16_t scannerPosition = 0;
    static int8_t scannerDirection = 1;
    static uint32_t  lastUpdate = millis();
    uint16_t interval = map(
        effectSpeed,
        1,
        100,
        200,
        1
    );

    // Заполняем ленту цветом фона
    fill_solid(
        leds,
        LED_COUNT,
        backgroundColor
    );

    // Ограничиваем длину шлейфа
    uint8_t tail = constrain(
        trailLength,
        0,
        LED_COUNT - 1
    );

    // Рисуем пиксель и его шлейф
    for (int i = 0; i <= tail; i++) {

        int pos = scannerPosition -
                  scannerDirection * i;

        if (pos < 0 || pos >= LED_COUNT)
            continue;

        // Яркость пикселя в шлейфе
        uint8_t intensity = 255 -
            (i * 255 / (tail + 1));

        leds[pos] = blend(
            backgroundColor,
            pixelColor,
            intensity
        );
    }
    if (millis() - lastUpdate > interval){
      lastUpdate = millis();
    // Перемещаем пиксель
      scannerPosition += scannerDirection;

      // Отражение от краёв
      if (scannerPosition >= LED_COUNT - 1) {
          scannerPosition = LED_COUNT - 1;
          scannerDirection = -1;
      }
      else if (scannerPosition <= 0) {
          scannerPosition = 0;
          scannerDirection = 1;
      }
    }
}

void effectRunning() {

    static uint32_t  lastUpdate = millis();
    static int16_t runninPosition = 0; 
    uint16_t interval = map(
        effectSpeed,
        1,
        100,
        200,
        1
    );
    // Заполняем ленту цветом фона
    fill_solid(
        leds,
        LED_COUNT,
        backgroundColor
    );

    // Ограничиваем длину шлейфа
    uint8_t tail = constrain(
        trailLength,
        0,
        LED_COUNT - 1
    );

    // Рисуем пиксель и его шлейф
    for (int i = 0; i <= tail; i++) {

        int pos = runninPosition - i;
        if (pos < 0) break;

        // Яркость пикселя в шлейфе
        uint8_t intensity = 255 -
            (i * 255 / (tail + 1));

        leds[pos % LED_COUNT] = blend(
            backgroundColor,
            pixelColor,
            intensity
        );
    }

    // Перемещаем пиксель
    if (millis() - lastUpdate > interval){
      runninPosition += 1;
      lastUpdate = millis();
    }

}

void effectRainbow() {
    hue += map(effectSpeed, 1, 100, 1, 12);
    fill_rainbow(leds, LED_COUNT, hue, 7);
}

void effectColor() {
    fill_solid(leds, LED_COUNT, currentColor);
}

void effectBreathing() {
    uint8_t bpm = map(effectSpeed, 1, 100, 3, 30);

    uint8_t value = beatsin8(
        bpm,
        5,
        brightness
    );

    fill_solid(leds, LED_COUNT, currentColor);
    FastLED.setBrightness(value);
}

void effectConfetti() {
    uint8_t fade = map(effectSpeed, 1, 100, 3, 40);
    uint8_t probability = map(effectSpeed, 1, 100, 15, 180);

    fadeToBlackBy(leds, LED_COUNT, fade);

    if (random8() < probability) {
        leds[random16(LED_COUNT)] += CHSV(
            random8(),
            200,
            255
        );
    }
}

void effectFire() {
    // Скорость влияет на интенсивность охлаждения
    uint8_t cooling = map(effectSpeed, 1, 100, 2, 12);

    // Вероятность появления искр
    uint8_t sparkProbability = map(effectSpeed, 1, 100, 30, 180);

    // Максимальная высота появления искр
    const uint8_t sparkHeight = min(7, LED_COUNT);

    // 1. Охлаждение каждого светодиода
    for (int i = 0; i < LED_COUNT; i++) {
        heat[i] = qsub8(
            heat[i],
            random8(0, cooling + 1)
        );
    }

    // 2. Распространение тепла вверх по ленте
    for (int k = LED_COUNT - 1; k >= 2; k--) {
        heat[k] = (
            (uint16_t)heat[k - 1] +
            (uint16_t)heat[k - 2] * 2
        ) / 3;
    }

    // 3. Генерация случайных искр в основании огня
    if (random8() < sparkProbability) {
        uint8_t y = random8(sparkHeight);

        heat[y] = qadd8(
            heat[y],
            random8(160, 255)
        );
    }

    // 4. Преобразование температуры в цвет
    for (int j = 0; j < LED_COUNT; j++) {
        leds[j] = HeatColor(heat[j]);
    }
}

void effectTwinkle() {
    uint8_t fade = map(effectSpeed, 1, 100, 2, 35);
    uint8_t probability = map(effectSpeed, 1, 100, 10, 180);

    fadeToBlackBy(leds, LED_COUNT, fade);

    if (random8() < probability) {
        leds[random16(LED_COUNT)] = currentColor;
    }
}

void effectBpm() {
    uint8_t bpm = map(effectSpeed, 1, 100, 10, 150);

    uint8_t beat = beatsin8(bpm, 64, 255);

    for (int i = 0; i < LED_COUNT; i++) {
        leds[i] = ColorFromPalette(
            PartyColors_p,
            hue + (i * 10),
            beat - hue + (i * 2)
        );
    }

    hue += map(effectSpeed, 1, 100, 1, 8);
}

void effectOff() {
    fill_solid(leds, LED_COUNT, CRGB::Black);
}

void updateEffect() {
    uint32_t now = millis();
    static uint32_t lastEffectUpdate = 0;

    if (now - lastEffectUpdate < 20)
        return;

    lastEffectUpdate = now;

    // Restore selected brightness
    FastLED.setBrightness(brightness);

    if (currentEffect == "rainbow") {
        effectRainbow();
    }
    else if (currentEffect == "color") {
        effectColor();
    }
    else if (currentEffect == "breathing") {
        effectBreathing();
    }
    else if (currentEffect == "confetti") {
        effectConfetti();
    }
    else if (currentEffect == "fire") {
        effectFire();
    }
    else if (currentEffect == "twinkle") {
        effectTwinkle();
    }
    else if (currentEffect == "bpm") {
        effectBpm();
    }
    else if (currentEffect == "running"){
      effectRunning();
    }
    else if (currentEffect == "strobe") {
        effectStrobe();
    }
    else if (currentEffect == "scanner") {
        effectScanner();
    }
    else {
        effectOff();
    }

    FastLED.show();
}

bool parseColor(String value, CRGB &target) {
    if (value.length() != 6)
        return false;

    for (int i = 0; i < 6; i++) {
        if (!isxdigit(value[i]))
            return false;
    }

    uint32_t rgb = strtoul(
        value.c_str(),
        nullptr,
        16
    );

    target = CRGB(
        (rgb >> 16) & 0xFF,
        (rgb >> 8) & 0xFF,
        rgb & 0xFF
    );

    return true;
}


void handleRoot() {
    // server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
    File file = LittleFS.open("/index.html", "r");

    if (!file) {
        server.send(500, "text/plain", "index.html not found");
        return;
    }

    server.streamFile(file, "text/html; charset=utf-8");
    file.close();
}

void handleFile(String filename){
    File file = LittleFS.open("/"+filename, "r");

    if (!file) {
        server.send(500, "text/plain", "index.html not found");
        return;
    }

    server.streamFile(file, "text/html; charset=utf-8");
    file.close();
}

void handleStatus() {
    String json = "{";
    json += "\"voltage\":" + String(batteryVoltage, 3) + ",";
    json += "\"percent\":" + String(batteryPercent) + ",";
    json += "\"effect\":\"" + currentEffect + "\",";
    json += "\"speed\":" + String(effectSpeed) + ",";
    json += "\"brightness\":" + String(brightness);
    json += "}";

    server.send(200, "application/json", json);
}

void handleSet() {
    if (server.hasArg("effect")) {
        String effect = server.arg("effect");

        if (
            effect == "rainbow" ||
            effect == "color" ||
            effect == "breathing" ||
            effect == "confetti" ||
            effect == "fire" ||
            effect == "twinkle" ||
            effect == "bpm" ||
            effect == "running" ||
            effect == "strobe" ||
            effect == "scanner" ||
            effect == "off"
        ) {
            if (currentEffect != effect) {
              currentEffect = effect;

              fill_solid(leds, LED_COUNT, CRGB::Black);
              memset(heat, 0, sizeof(heat));
          }
        }
    }

    if (server.hasArg("brightness")) {
        brightness = constrain(
            server.arg("brightness").toInt(),
            1,
            255
        );
    }

    if (server.hasArg("color")) {
      parseColor(server.arg("color"), currentColor);
    }

    if (server.hasArg("speed")) {
    effectSpeed = constrain(
              server.arg("speed").toInt(),
              1,
              100
          );
      }
    if (server.hasArg("bg")) {
        parseColor(server.arg("bg"), backgroundColor);
    }

    if (server.hasArg("pixel")) {
        parseColor(server.arg("pixel"), pixelColor);
    }

    if (server.hasArg("trail")) {
        trailLength = constrain(
            server.arg("trail").toInt(),
            0,
            LED_COUNT - 1
        );
    }

    server.send(200, "application/json", "{\"ok\":true}");
}

// ================= SETUP =================

void setup() {
    Serial.begin(115200);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
        return;
    }

    // ADC
    analogReadResolution(12);
    analogSetPinAttenuation(BATTERY_PIN, ADC_11db);

    // LEDs
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(
        leds,
        LED_COUNT
    );

    FastLED.setBrightness(brightness);
    FastLED.clear(true);

    random16_add_entropy(analogRead(BATTERY_PIN));

    // WiFi Access Point
    WiFi.mode(WIFI_AP);

    WiFi.softAPConfig(
        apIP,
        apIP,
        netMask
    );

    WiFi.softAP(AP_SSID, AP_PASS);

    // Перенаправляем DNS на ESP32
    dnsServer.start(DNS_PORT, "*", apIP);

    Serial.println();
    Serial.println("ESP32 LED Controller");

    Serial.print("SSID: ");
    Serial.println(AP_SSID);

    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());

    // Web server
    server.on("/", HTTP_GET, handleRoot);
    server.on("/index.js", HTTP_GET, []() {handleFile("index.js");});
    server.on("/index.css", HTTP_GET, [](){handleFile("index.css");});
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/set", HTTP_GET, handleSet);

    // Android
    server.on("/generate_204", HTTP_GET, handleRoot);
    server.on("/gen_204", HTTP_GET, handleRoot);

    // Apple
    server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
    server.on("/library/test/success.html", HTTP_GET, handleRoot);

    // Windows
    server.on("/connecttest.txt", HTTP_GET, handleRoot);
    server.on("/ncsi.txt", HTTP_GET, handleRoot);

    // Любой неизвестный адрес открывает интерфейс
    server.onNotFound(handleRoot);

    server.begin();

    // Initial battery reading
    batteryVoltage = readBatteryVoltage();
    batteryPercent = voltageToPercent(batteryVoltage);
}

// ================= LOOP =================

void loop() {
    dnsServer.processNextRequest();

    server.handleClient();

    updateBattery();
    updateEffect();
}

