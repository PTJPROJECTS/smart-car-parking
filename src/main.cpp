// Smart car parking - Arduino Uno
// 4 slot IR sensors, entry/exit IR at the gate, SG90 servo boom, 16x2 I2C LCD.

#include <Arduino.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "parking.h"

static const uint8_t SLOT_PINS[PK_NUM_SLOTS] = {2, 3, 4, 5};
#define PIN_ENTRY   6
#define PIN_EXIT    7
#define PIN_SERVO   9

#define LOOP_MS     10

LiquidCrystal_I2C lcd(0x27, 16, 2);   // some backpacks are 0x3F
Servo gate;
pk_lot_t lot;

static int lastAngle = -1;
static unsigned long showFullUntil;

// FC-51 IR modules pull the output LOW when they see something
static uint8_t present(uint8_t pin)
{
    return digitalRead(pin) == LOW;
}

static void readInputs(pk_inputs_t *in)
{
    for (uint8_t i = 0; i < PK_NUM_SLOTS; i++)
        in->slot[i] = present(SLOT_PINS[i]);
    in->entry = present(PIN_ENTRY);
    in->exit = present(PIN_EXIT);
}

static void drawLcd()
{
    char row[17];
    int freeSlots = pk_free_slots(&lot);

    lcd.setCursor(0, 0);
    if (millis() < showFullUntil)
        snprintf(row, sizeof(row), "SORRY, LOT FULL ");
    else
        snprintf(row, sizeof(row), "Free: %d/%d      ", freeSlots, PK_NUM_SLOTS);
    lcd.print(row);

    lcd.setCursor(0, 1);
    for (uint8_t i = 0; i < PK_NUM_SLOTS; i++) {
        lcd.print(i + 1);
        lcd.print(lot.slot[i].stable ? 'X' : '-');
        lcd.print(' ');
    }
    lcd.print("    ");
}

void setup()
{
    Serial.begin(9600);
    for (uint8_t i = 0; i < PK_NUM_SLOTS; i++)
        pinMode(SLOT_PINS[i], INPUT);
    pinMode(PIN_ENTRY, INPUT);
    pinMode(PIN_EXIT, INPUT);

    lcd.init();
    lcd.backlight();

    gate.attach(PIN_SERVO);
    gate.write(0);

    pk_init(&lot, millis());
    drawLcd();
    Serial.println(F("# parking ready"));
}

void loop()
{
    static unsigned long last;
    unsigned long now = millis();
    if (now - last < LOOP_MS)
        return;
    last = now;

    pk_inputs_t in;
    readInputs(&in);
    int ev = pk_update(&lot, &in, now);

    // write the servo only when the angle changes, avoids jitter
    int angle = pk_gate_angle(&lot, now);
    if (angle != lastAngle) {
        gate.write(angle);
        lastAngle = angle;
    }

    if (ev & PK_EV_FULL) {
        Serial.println(F("EV full"));
        showFullUntil = now + 3000;
    }
    if (ev) {
        char line[48];
        if (pk_status_line(&lot, line, sizeof(line)) > 0)
            Serial.println(line);
        drawLcd();
    }
    if (showFullUntil && now >= showFullUntil) {
        showFullUntil = 0;
        drawLcd();
    }
}
