/*
 * Copyright 2018 Scott A Dixon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <arduino.h>
#include <Adafruit_NeoPixel.h>
#include <cstdlib>

#define PIN_NEOPIXL 10
#define PIXEL_COUNT 32
#define PIN_POT_SINK 9
#define PIN_POT_0 A0
#define PIN_POT_1 A1
#define PIN_POT_2 A2
#define PIN_POTSW_2 A3

#define POT_H PIN_POT_2
#define POT_S PIN_POT_1
#define POT_V PIN_POT_0

typedef int32_t int32_1715_t;

constexpr int32_t adc_min = 120;
constexpr int32_t adc_max = 4095;

static Adafruit_NeoPixel wing(PIXEL_COUNT, PIN_NEOPIXL, NEO_GRB + NEO_KHZ800);

static bool is_light_on = true;
static uint32_t primary_colour = 0xFFFFFFFF;
static bool is_led_on = true;
static uint32_t last_blink_millis = 0;
static bool event_log = false;

void resync_light() {
    if (is_light_on) {
        const uint8_t r = 0xFF & (primary_colour >> 24);
        const uint8_t g = 0xFF & (primary_colour >> 16);
        const uint8_t b = 0xFF & (primary_colour >> 8);
        const uint8_t a = 0xFF & (primary_colour >> 0);
        wing.setBrightness(a);
        for(size_t i = 0; i < PIXEL_COUNT; ++i) {
            wing.setPixelColor(i, r, g, b);
        }
    } else {
        wing.setBrightness(0);
    }
    wing.show();
}

void setup() {
    analogReadResolution(12);
    //SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    EIC->WAKEUP.reg = EIC_WAKEUP_WAKEUPEN4;
    EIC->CONFIG[0].reg = EIC_CONFIG_SENSE4(EIC_CONFIG_SENSE4_HIGH_Val);
    EIC->CTRL.reg = EIC_CTRL_ENABLE;
    Serial.begin(115200);
    Serial.println("Heyya, Huck!");
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_POT_SINK, OUTPUT);
    pinMode(PIN_POT_0, INPUT);
    pinMode(PIN_POT_1, INPUT);
    pinMode(PIN_POT_2, INPUT);
    pinMode(PIN_POTSW_2, INPUT_PULLDOWN);
    digitalWrite(PIN_POT_SINK, LOW);
    digitalWrite(PIN_LED, HIGH);
    wing.begin();
}

int32_t read_pot(uint32_t which) {
    const int32_t x = static_cast<int32_t>(analogRead(which));
    if (event_log) {
        Serial.print(which);
        Serial.print(" = ");
        Serial.print(x, HEX);
        Serial.print(", ");
    }
    return x;
}

int32_1715_t read_hprime() {
    const int32_1715_t h_deg = map(read_pot(POT_H), adc_min, adc_max, 0, 360);
    return ((h_deg << 15) / 60);
}

int32_1715_t read_s() {
    return map(read_pot(POT_S), adc_min, adc_max, 0, 1 << 15);
}

int32_1715_t read_v() {
    return map(read_pot(POT_V), adc_min, adc_max, 0, 1 << 15);
}

uint32_t to_rgba_8(int32_1715_t r, int32_1715_t g, int32_1715_t b) {
    return map( r, 0, 1 << 15, 0, 0xFF) << 24 |
           map( g, 0, 1 << 15, 0, 0xFF) << 16 |
           map( b, 0, 1 << 15, 0, 0xFF) << 8 |
           0xFF;
}

void piecewise_from_hprime_x_c(int32_1715_t h_prime, int32_1715_t X, int32_1715_t C, int32_1715_t& r, int32_1715_t& g, int32_1715_t& b) {
           if ((0 << 15) <= h_prime && h_prime <= (1 << 15)) {
        r = C;
        g = X;
        b = 0;
    } else if ((1 << 15) <= h_prime && h_prime <= (2 << 15)) {
        r = X;
        g = C;
        b = 0;
    } else if ((2 << 15) <= h_prime && h_prime <= (3 << 15)) {
        r = 0;
        g = C;
        b = X;
    } else if ((3 << 15) <= h_prime && h_prime <= (4 << 15)) {
        r = 0;
        g = X;
        b = C;
    } else if ((4 << 15) <= h_prime && h_prime <= (5 << 15)) {
        r = X;
        g = 0;
        b = C;
    } else if ((5 << 15) <= h_prime && h_prime <= (6 << 15)) {
        r = C;
        g = 0;
        b = X;
    } else {
        r = 0;
        g = 0;
        b = 0;
    }
}

uint32_t read_rgba() {
    // from https://en.wikipedia.org/wiki/HSL_and_HSV
    const int32_1715_t V = read_v();
    const int32_1715_t S = read_s();
    const int32_1715_t C = (((int64_t)V * (int64_t)S) >> 15);
    const int32_1715_t h_prime = read_hprime();
    const int32_1715_t h_mod = (h_prime % (2 << 15));
    const int32_1715_t h_mod_abs = (1 << 15) - std::abs(h_mod - (1 << 15));
    const int32_1715_t X = (((int64_t)C * (int64_t)h_mod_abs) >> 15);
    int32_1715_t r, g, b;
    piecewise_from_hprime_x_c(h_prime, X, C, r, g, b);
    const int32_1715_t m = V - C;

    if (event_log) {
        Serial.print("V:");
        Serial.print(V);
        Serial.print(",");
        Serial.print("C:");
        Serial.print(C);
        Serial.print(",");
        Serial.print("S:");
        Serial.print(S);
        Serial.print(",");
        Serial.print("X:");
        Serial.print(X);
        Serial.print(",");
        Serial.print("m:");
        Serial.print(m);
        Serial.print(",");
        Serial.print("H':");
        Serial.print(h_prime);
        Serial.print(", ");
    }
    return to_rgba_8(r + m, g + m, b + m);
}

static uint32_t s_v_switch_debounce = 0;

bool is_v_switch_on() {
    if (digitalRead(PIN_POTSW_2)) {
        // HIGH, CLOSED, ON
        s_v_switch_debounce = (s_v_switch_debounce << 1) | 0x1;
    } else {
        // LOW, OPEN, OFF
        s_v_switch_debounce = (s_v_switch_debounce << 1);
    }
    return (s_v_switch_debounce & 0xFFF) == 0xFFF;
}

void loop() {
    const uint32_t now = millis();
    if (now - last_blink_millis > 1000) {
        Serial.print("                                                                                                               \r");
        last_blink_millis = now;
        event_log = true;
    }

    primary_colour = read_rgba();

    is_light_on = is_v_switch_on();

    digitalWrite(PIN_LED, (is_light_on && is_led_on) ? HIGH : LOW);

    resync_light();

    if (!is_light_on) {
        digitalWrite(PIN_POT_SINK, HIGH);
        Serial.println("Good night!");
        __WFI();
    } else {
        digitalWrite(PIN_POT_SINK, LOW);
    }

    if (event_log) {
        is_led_on = !is_led_on;

        Serial.print("rgb: ");
        Serial.print(primary_colour, HEX);
        Serial.print(", vsw: ");
        Serial.print((!is_light_on) ? "op" : "cl");
        Serial.print("\r");
        event_log = false;
    }
}
