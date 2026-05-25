#include <M5Unified.h>
#include <bsec2.h>
#include <Wire.h>

#define BME_SDA_PIN 9
#define BME_SCL_PIN 10

/* BSEC2 config for 3.3V supply, LP mode (3s interval), 4-day sensor age */
const uint8_t bsec_config[] = {
    #include "config/bme688/bme688_sel_33v_3s_4d/bsec_selectivity.txt"
};

TwoWire I2C_BME = TwoWire(0);
Bsec2 envSensor;
static LGFX_Sprite canvas(&M5.Display);

static float last_iaq         = 0.0f;
static uint8_t last_iaq_acc   = 0;
static float last_co2         = 0.0f;
static float last_temp        = 0.0f;
static float last_hum         = 0.0f;
static float last_pres        = 0.0f;
static bool bsec_ready        = false;

static uint16_t iaq_color(float iaq) {
    if (iaq <= 50.0f)  return TFT_GREEN;
    if (iaq <= 100.0f) return TFT_YELLOW;
    if (iaq <= 150.0f) return TFT_ORANGE;
    if (iaq <= 200.0f) return TFT_RED;
    if (iaq <= 300.0f) return TFT_PURPLE;
    return TFT_MAROON;
}

static void draw_metric(const char* label, float value, int decimals,
                        const char* unit, int y, uint16_t color) {
    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    canvas.setCursor(8, y);
    canvas.print(label);

    canvas.setTextFont(4);
    canvas.setTextSize(1);
    canvas.setTextColor(color, TFT_BLACK);
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
    canvas.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    canvas.print(" ");
    canvas.print(unit);
}

static void bsecDataCallback(const bme68xData data, const bsecOutputs outputs,
                             Bsec2 bsec) {
    if (!outputs.nOutputs) return;

    for (uint8_t i = 0; i < outputs.nOutputs; i++) {
        const bsecData out = outputs.output[i];
        switch (out.sensor_id) {
            case BSEC_OUTPUT_IAQ:
                last_iaq    = out.signal;
                last_iaq_acc = out.accuracy;
                break;
            case BSEC_OUTPUT_CO2_EQUIVALENT:
                last_co2 = out.signal;
                break;
            case BSEC_OUTPUT_RAW_TEMPERATURE:
                last_temp = out.signal;
                break;
            case BSEC_OUTPUT_RAW_HUMIDITY:
                last_hum = out.signal;
                break;
            case BSEC_OUTPUT_RAW_PRESSURE:
                last_pres = out.signal;
                break;
            default:
                break;
        }
    }
}

static void check_bsec_status() {
    if (envSensor.status < BSEC_OK) {
        Serial.printf("BSEC error: %d\n", envSensor.status);
    } else if (envSensor.status > BSEC_OK) {
        Serial.printf("BSEC warning: %d\n", envSensor.status);
    }
    if (envSensor.sensor.status < BME68X_OK) {
        Serial.printf("BME68X error: %d\n", envSensor.sensor.status);
    } else if (envSensor.sensor.status > BME68X_OK) {
        Serial.printf("BME68X warning: %d\n", envSensor.sensor.status);
    }
}

void setup() {
    auto cfg          = M5.config();
    cfg.output_power  = true;
    cfg.clear_display = true;
    M5.begin(cfg);

    Serial.begin(115200);
    Serial.println("ENV-Pro BSEC2 init");

    M5.Display.setBrightness(128);

    canvas.createSprite(M5.Display.width(), M5.Display.height());
    canvas.fillSprite(TFT_BLACK);

    I2C_BME.begin(BME_SDA_PIN, BME_SCL_PIN);

    if (!envSensor.begin(BME68X_I2C_ADDR_HIGH, I2C_BME)) {
        Serial.println("BSEC2 begin failed");
        check_bsec_status();
        canvas.setTextFont(4);
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.setCursor(8, 100);
        canvas.print("BSEC2");
        canvas.setCursor(8, 130);
        canvas.print("ERR");
        canvas.pushSprite(0, 0);
        bsec_ready = false;
        M5.delay(3000);
        return;
    }

    if (!envSensor.setConfig(bsec_config)) {
        Serial.println("BSEC2 config failed");
        check_bsec_status();
        canvas.setTextFont(4);
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.setCursor(8, 100);
        canvas.print("CFG");
        canvas.setCursor(8, 130);
        canvas.print("ERR");
        canvas.pushSprite(0, 0);
        bsec_ready = false;
        M5.delay(3000);
        return;
    }

    envSensor.setTemperatureOffset(TEMP_OFFSET_LP);

    bsecSensor sensorList[] = {
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_RAW_TEMPERATURE,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_HUMIDITY,
    };

    if (!envSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList),
                                      BSEC_SAMPLE_RATE_LP)) {
        Serial.printf("BSEC2 subscription failed, status=%d\n", envSensor.status);
        check_bsec_status();
        canvas.setTextFont(4);
        canvas.setTextColor(TFT_RED, TFT_BLACK);
        canvas.setCursor(8, 90);
        canvas.print("SUB");
        canvas.setCursor(8, 120);
        canvas.print("ERR");
        canvas.setTextFont(2);
        canvas.setCursor(8, 150);
        canvas.printf("st:%d", envSensor.status);
        canvas.pushSprite(0, 0);
        bsec_ready = false;
        M5.delay(3000);
        return;
    }

    envSensor.attachCallback(bsecDataCallback);
    bsec_ready = true;

    Serial.printf("BSEC2 ver %d.%d.%d.%d ready\n",
                  envSensor.version.major, envSensor.version.minor,
                  envSensor.version.major_bugfix,
                  envSensor.version.minor_bugfix);
}

void loop() {
    M5.update();

    if (!bsec_ready) {
        M5.delay(100);
        return;
    }

    if (!envSensor.run()) {
        check_bsec_status();
    }

    // BSEC2 LP mode outputs every ~3s; redraw on each new output
    canvas.fillSprite(TFT_BLACK);

    // Title
    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.setCursor(36, 4);
    canvas.print("ENV-Pro");

    // IAQ (largest, color-coded)
    draw_metric("IAQ", last_iaq, 0, "",
                28, iaq_color(last_iaq));
    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.setCursor(8, 72);
    canvas.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    canvas.printf("acc:%d", last_iaq_acc);

    // CO2
    draw_metric("CO2", last_co2, 0, "ppm", 90, TFT_WHITE);

    // Temperature
    draw_metric("TEMP", last_temp, 1, "C", 140, TFT_WHITE);

    // Humidity
    draw_metric("HUM", last_hum, 1, "%", 190, TFT_WHITE);

    canvas.pushSprite(0, 0);

    if (M5.BtnA.wasClicked()) {
        Serial.println("BtnA clicked");
    }
    if (M5.BtnB.wasClicked()) {
        Serial.println("BtnB clicked");
    }
}
