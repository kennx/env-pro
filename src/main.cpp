#include <M5Unified.h>
#include <EEPROM.h>
#include <bsec2.h>
#include <Wire.h>
#include "bsec_ui_state.h"

#define BME_SDA_PIN 9
#define BME_SCL_PIN 10

/* BSEC2 config for 3.3V supply, LP mode (3s interval), 4-day sensor age */
const uint8_t bsec_config[] = {
    #include "config/bme680/bme680_iaq_33v_3s_4d/bsec_iaq.txt"
};

TwoWire I2C_BME = TwoWire(0);
Bsec2 envSensor;
static LGFX_Sprite canvas(&M5.Display);

static constexpr uint32_t BSEC_STATE_SAVE_PERIOD_MS = 360UL * 60UL * 1000UL;
static constexpr uint32_t BSEC_STATE_SAVE_RETRY_MS = 5UL * 60UL * 1000UL;
static constexpr uint32_t BSEC_STATE_MAGIC = 0x42534543UL;
static constexpr uint16_t BSEC_STATE_VERSION = 1;
static constexpr uint16_t BSEC_STATE_HEADER_BYTES = 16;
static constexpr size_t BSEC_EEPROM_BYTES =
    BSEC_STATE_HEADER_BYTES + BSEC_MAX_STATE_BLOB_SIZE;

static float last_iaq         = 0.0f;
static uint8_t last_iaq_acc   = 0;
static float last_co2         = 0.0f;
static float last_temp        = 0.0f;
static float last_hum         = 0.0f;
static float last_pres        = 0.0f;
static float last_stab_status = -1.0f;
static float last_runin_status = -1.0f;
static uint32_t last_output_ms = 0;
static bool has_output         = false;
static bool ui_dirty           = true;
static bool last_run_failed    = false;
static bool last_stale_state   = false;
static bool bsec_ready         = false;
static bool eeprom_ready       = false;
static uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE];
static uint32_t last_state_save_ms = 0;
static uint32_t last_state_save_attempt_ms = 0;
static bool last_state_save_failed = false;

static constexpr uint32_t BSEC_DIAG_LOG_PERIOD_MS = 30000UL;

static uint32_t last_diag_log_ms = 0;
static uint8_t last_logged_iaq_acc = 0xFF;
static int last_logged_runin_state = -2;
static int last_logged_stab_state = -2;

static void maybe_save_bsec_state(uint32_t now_ms);

static int bsec_diag_status_flag(float value) {
    if (value < 0.0f) {
        return -1;
    }
    return value >= 1.0f ? 1 : 0;
}

static void maybe_log_bsec_diag(uint32_t now_ms) {
    const int runin = bsec_diag_status_flag(last_runin_status);
    const int stab = bsec_diag_status_flag(last_stab_status);
    const bool accuracy_changed = last_logged_iaq_acc != last_iaq_acc;
    const bool runin_changed = last_logged_runin_state != runin;
    const bool stab_changed = last_logged_stab_state != stab;
    const bool periodic = last_diag_log_ms == 0 ||
                          (now_ms - last_diag_log_ms) >= BSEC_DIAG_LOG_PERIOD_MS;

    if (!(accuracy_changed || runin_changed || stab_changed || periodic)) {
        return;
    }

    Serial.printf("BSEC diag iaq=%.1f acc=%u eCO2=%.0f runin=%d stab=%d\n",
                  last_iaq, last_iaq_acc, last_co2, runin, stab);

    last_logged_iaq_acc = last_iaq_acc;
    last_logged_runin_state = runin;
    last_logged_stab_state = stab;
    last_diag_log_ms = now_ms;
}

static uint32_t fnv1a32(const uint8_t* data, size_t len) {
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619UL;
    }
    return hash;
}

static uint16_t read_u16_eeprom(int addr) {
    return static_cast<uint16_t>(EEPROM.read(addr)) |
           (static_cast<uint16_t>(EEPROM.read(addr + 1)) << 8);
}

static uint32_t read_u32_eeprom(int addr) {
    return static_cast<uint32_t>(EEPROM.read(addr)) |
           (static_cast<uint32_t>(EEPROM.read(addr + 1)) << 8) |
           (static_cast<uint32_t>(EEPROM.read(addr + 2)) << 16) |
           (static_cast<uint32_t>(EEPROM.read(addr + 3)) << 24);
}

static void write_u16_eeprom(int addr, uint16_t value) {
    EEPROM.write(addr, static_cast<uint8_t>(value & 0xFF));
    EEPROM.write(addr + 1, static_cast<uint8_t>((value >> 8) & 0xFF));
}

static void write_u32_eeprom(int addr, uint32_t value) {
    EEPROM.write(addr, static_cast<uint8_t>(value & 0xFF));
    EEPROM.write(addr + 1, static_cast<uint8_t>((value >> 8) & 0xFF));
    EEPROM.write(addr + 2, static_cast<uint8_t>((value >> 16) & 0xFF));
    EEPROM.write(addr + 3, static_cast<uint8_t>((value >> 24) & 0xFF));
}

static uint32_t bsec_config_hash() {
    return fnv1a32(bsec_config, sizeof(bsec_config));
}

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

static void draw_metric_text(const char* label, const char* value,
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
    canvas.print(value);

    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    canvas.print(" ");
    canvas.print(unit);
}

static uint16_t metric_color(bool dimmed) {
    return dimmed ? TFT_DARKGRAY : TFT_WHITE;
}

static void bsecDataCallback(const bme68xData data, const bsecOutputs outputs,
                                 Bsec2 bsec) {
    (void)data;
    (void)bsec;
    if (!outputs.nOutputs) return;

    for (uint8_t i = 0; i < outputs.nOutputs; i++) {
        const bsecData out = outputs.output[i];
        switch (out.sensor_id) {
            case BSEC_OUTPUT_IAQ:
                last_iaq = out.signal;
                last_iaq_acc = out.accuracy;
                break;
            case BSEC_OUTPUT_CO2_EQUIVALENT:
                last_co2 = out.signal;
                break;
            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
                last_temp = out.signal;
                break;
            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
                last_hum = out.signal;
                break;
            case BSEC_OUTPUT_RAW_PRESSURE:
                last_pres = out.signal;
                break;
            case BSEC_OUTPUT_STABILIZATION_STATUS:
                last_stab_status = out.signal;
                break;
            case BSEC_OUTPUT_RUN_IN_STATUS:
                last_runin_status = out.signal;
                break;
            default:
                break;
        }
    }

    has_output = true;
    last_output_ms = millis();
    last_run_failed = false;
    ui_dirty = true;
    maybe_log_bsec_diag(last_output_ms);
    if (bsec_state_save_allowed(has_output, last_run_failed,
                                last_iaq_acc, last_runin_status)) {
        maybe_save_bsec_state(last_output_ms);
    }
}

static void render_screen(uint32_t now_ms) {
    const bool stale = has_output && is_bsec_output_stale(now_ms, last_output_ms);
    const bool air_ready = iaq_data_is_stable(has_output, last_run_failed,
                                              stale, last_iaq_acc);
    const bool air_visible = iaq_values_visible(has_output, last_run_failed,
                                                stale, last_iaq_acc);
    const char* status = iaq_status_label(has_output, last_run_failed,
                                          stale, last_iaq_acc);
    const bool env_dimmed = last_run_failed || stale;
    const bool air_dimmed = !air_ready || stale || last_run_failed;

    canvas.fillSprite(TFT_BLACK);
    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.setCursor(36, 4);
    canvas.print("ENV-Pro");

    if (air_visible) {
        draw_metric("IAQ", last_iaq, 0, "", 24,
                    air_dimmed ? TFT_DARKGRAY : iaq_color(last_iaq));
        draw_metric("eCO2", last_co2, 0, "ppm", 82,
                    metric_color(air_dimmed));
    } else {
        draw_metric_text("IAQ", "--", "", 24, TFT_DARKGRAY);
        draw_metric_text("eCO2", "--", "ppm", 82, TFT_DARKGRAY);
    }

    canvas.setTextFont(2);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    canvas.setCursor(8, 70);
    canvas.print(status);

    if (has_output) {
        draw_metric("TEMP", last_temp, 1, "C", 128, metric_color(env_dimmed));
        draw_metric("HUM", last_hum, 1, "%", 166, metric_color(env_dimmed));
        draw_metric("PRES", last_pres, 0, "hPa", 204, metric_color(env_dimmed));
    } else {
        draw_metric_text("TEMP", "--", "C", 128, TFT_DARKGRAY);
        draw_metric_text("HUM", "--", "%", 166, TFT_DARKGRAY);
        draw_metric_text("PRES", "--", "hPa", 204, TFT_DARKGRAY);
    }

    canvas.pushSprite(0, 0);
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

static bool load_bsec_state() {
    if (!eeprom_ready) {
        return false;
    }

    const uint32_t magic = read_u32_eeprom(0);
    const uint16_t version = read_u16_eeprom(4);
    const uint16_t state_size = read_u16_eeprom(6);
    const uint32_t saved_config_hash = read_u32_eeprom(8);
    const uint32_t saved_state_hash = read_u32_eeprom(12);

    if (magic != BSEC_STATE_MAGIC || version != BSEC_STATE_VERSION) {
        Serial.println("No compatible BSEC state");
        return false;
    }

    if (state_size != BSEC_MAX_STATE_BLOB_SIZE) {
        Serial.println("Saved BSEC state size mismatch");
        return false;
    }

    if (saved_config_hash != bsec_config_hash()) {
        Serial.println("Saved BSEC state config mismatch");
        return false;
    }

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
        bsec_state[i] = EEPROM.read(BSEC_STATE_HEADER_BYTES + i);
    }

    if (fnv1a32(bsec_state, BSEC_MAX_STATE_BLOB_SIZE) != saved_state_hash) {
        Serial.println("Saved BSEC state checksum mismatch");
        return false;
    }

    if (!envSensor.setState(bsec_state)) {
        Serial.println("BSEC state restore failed");
        check_bsec_status();
        return false;
    }

    Serial.println("BSEC state restored");
    return true;
}

static bool save_bsec_state() {
    if (!eeprom_ready) {
        return false;
    }

    if (!envSensor.getState(bsec_state)) {
        Serial.println("BSEC state read failed");
        check_bsec_status();
        return false;
    }

    const uint32_t config_hash = bsec_config_hash();
    const uint32_t state_hash = fnv1a32(bsec_state, BSEC_MAX_STATE_BLOB_SIZE);

    write_u32_eeprom(0, BSEC_STATE_MAGIC);
    write_u16_eeprom(4, BSEC_STATE_VERSION);
    write_u16_eeprom(6, BSEC_MAX_STATE_BLOB_SIZE);
    write_u32_eeprom(8, config_hash);
    write_u32_eeprom(12, state_hash);

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
        EEPROM.write(BSEC_STATE_HEADER_BYTES + i, bsec_state[i]);
    }

    if (!EEPROM.commit()) {
        Serial.println("BSEC state commit failed");
        return false;
    }

    Serial.println("BSEC state saved");
    return true;
}

static void maybe_save_bsec_state(uint32_t now_ms) {
    if (last_state_save_failed && last_state_save_attempt_ms != 0 &&
        (now_ms - last_state_save_attempt_ms) < BSEC_STATE_SAVE_RETRY_MS) {
        return;
    }

    if (!last_state_save_failed && last_state_save_ms != 0 &&
        (now_ms - last_state_save_ms) < BSEC_STATE_SAVE_PERIOD_MS) {
        return;
    }

    last_state_save_attempt_ms = now_ms;
    if (save_bsec_state()) {
        last_state_save_ms = now_ms;
        last_state_save_failed = false;
    } else {
        last_state_save_failed = true;
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

    eeprom_ready = EEPROM.begin(BSEC_EEPROM_BYTES);
    if (!eeprom_ready) {
        Serial.println("EEPROM init failed");
    } else {
        load_bsec_state();
    }

    bsecSensor sensorList[] = {
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
        BSEC_OUTPUT_STABILIZATION_STATUS,
        BSEC_OUTPUT_RUN_IN_STATUS,
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

        const bool hard_error = bsec_has_hard_error(envSensor.status,
                                                    envSensor.sensor.status);
        if (hard_error != last_run_failed) {
            last_run_failed = hard_error;
            ui_dirty = true;
        }
    }

    const bool stale = has_output && is_bsec_output_stale(millis(), last_output_ms);
    if (stale != last_stale_state) {
        last_stale_state = stale;
        ui_dirty = true;
    }

    if (ui_dirty) {
        render_screen(millis());
        ui_dirty = false;
    }

    if (M5.BtnA.wasClicked()) {
        Serial.println("BtnA clicked");
    }
    if (M5.BtnB.wasClicked()) {
        Serial.println("BtnB clicked");
    }
}
