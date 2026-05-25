#include <M5Unified.h>
#include <Adafruit_BME680.h>
#include <Wire.h>

#define BME688_ADDR 0x77
#define BME_SDA_PIN 9
#define BME_SCL_PIN 10

TwoWire I2C_BME = TwoWire(0);
Adafruit_BME680 bme(&I2C_BME);
static LGFX_Sprite canvas(&M5.Display);

static uint32_t update_interval_ms = 1000;
static uint32_t last_update_ms       = 0;
static bool bme_ready                = false;

static void draw_metric(const char* label, float value, int decimals,
                        const char* unit, int y) {
    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    canvas.setCursor(8, y);
    canvas.print(label);

    canvas.setTextFont(4);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setCursor(8, y + 16);
    if (decimals == 0) {
        canvas.printf("%.0f", value);
    } else if (decimals == 1) {
        canvas.printf("%.1f", value);
    } else {
        canvas.printf("%.2f", value);
    }

    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.print(" ");
    canvas.print(unit);
}

static void draw_metric_u32(const char* label, uint32_t value,
                            const char* unit, int y) {
    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    canvas.setCursor(8, y);
    canvas.print(label);

    canvas.setTextFont(4);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setCursor(8, y + 16);
    canvas.print(value);

    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.print(" ");
    canvas.print(unit);
}

void setup() {
    auto cfg          = M5.config();
    cfg.output_power  = true;
    cfg.clear_display = true;
    M5.begin(cfg);

    Serial.begin(115200);
    Serial.println("ENV-Pro init started");

    M5.Display.setBrightness(128);

    canvas.createSprite(M5.Display.width(), M5.Display.height());
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);

    I2C_BME.begin(BME_SDA_PIN, BME_SCL_PIN);

    if (!bme.begin(BME688_ADDR)) {
        Serial.println("BME688 not found at 0x77");
        canvas.setTextFont(4);
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.setCursor(8, 100);
        canvas.print("BME688");
        canvas.setCursor(8, 130);
        canvas.print("ERR");
        canvas.pushSprite(0, 0);
        M5.delay(3000);
    } else {
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150);

        bme_ready = true;
        Serial.println("BME688 ready");
    }
}

void loop() {
    M5.update();

    uint32_t now = millis();
    if (now - last_update_ms < update_interval_ms) {
        M5.delay(10);
        return;
    }
    last_update_ms = now;

    canvas.fillSprite(TFT_BLACK);

    // 标题
    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.setCursor(42, 4);
    canvas.print("ENV-Pro");

    if (!bme_ready) {
        canvas.setTextFont(4);
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.setCursor(8, 100);
        canvas.print("BME688");
        canvas.setCursor(8, 130);
        canvas.print("ERR");
        canvas.pushSprite(0, 0);
        return;
    }

    if (!bme.performReading()) {
        Serial.println("BME688 read failed");
        canvas.setTextFont(4);
        canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
        canvas.setCursor(8, 100);
        canvas.print("Read");
        canvas.setCursor(8, 130);
        canvas.print("Fail");
        canvas.pushSprite(0, 0);
        return;
    }

    float temp   = bme.temperature;
    float hum    = bme.humidity;
    float pres   = bme.pressure / 100.0f;
    uint32_t gas = bme.gas_resistance;

    Serial.printf("T:%.1fC H:%.1f%% P:%.1fhPa G:%lu\n", temp, hum, pres, gas);

    draw_metric("TEMP", temp, 1, "C", 28);
    draw_metric("HUM", hum, 1, "%", 78);
    draw_metric("PRES", pres, 0, "hPa", 128);
    draw_metric_u32("GAS", gas, "Ohm", 178);

    canvas.pushSprite(0, 0);

    if (M5.BtnA.wasClicked()) {
        Serial.println("BtnA clicked");
    }
    if (M5.BtnB.wasClicked()) {
        Serial.println("BtnB clicked");
    }
}
