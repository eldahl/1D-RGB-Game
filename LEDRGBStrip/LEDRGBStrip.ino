#include <Adafruit_NeoPixel.h>
#include "WS2812_Definitions.h"

#define PIN 5
#define LED_COUNT 60
#define BTN1 0
#define BTN2 1
#define BTN3 2 
#define BTN4 3
#define BTN5 4

Adafruit_NeoPixel leds(LED_COUNT, PIN, NEO_GRB + NEO_KHZ800);

int8_t game = 0;
unsigned long lastPress = 0;

// Color Shooter
#define MAX_OBJ 4
int8_t ePos[MAX_OBJ], bPos[MAX_OBJ];
byte eCol[MAX_OBJ], bCol[MAX_OBJ], pCol = 0;
unsigned long lastE = 0, lastB = 0, lastS = 0;
bool csOver = false;

// Tug of War
int8_t rope = 0;
bool towOver = false, towWon = false;
unsigned long towEnd = 0;

// Reaction Zone
int8_t rzPos = 59, rzDir = -1, rzStart = 0, rzSize = 20, rzLvl = 1;
byte rzSpd = 35;
bool rzOver = false, rzWait = false;
unsigned long rzLast = 0, rzEnd = 0;

const uint32_t COLORS[] = {WHITE, RED, BLUE, YELLOW, GREEN};
const byte PINS[] = {BTN1, BTN2, BTN3, BTN4, BTN5};

void setup() {
    for (int i = 0; i < 5; i++) pinMode(PINS[i], INPUT_PULLUP);
    leds.begin();
    leds.setBrightness(100);
}

bool btn(byte p) { return digitalRead(p) == LOW; }

bool anyBtn() {
    for (int i = 0; i < 5; i++) if (btn(PINS[i])) return true;
    return false;
}

void fill(uint32_t c) {
    for (int i = 0; i < LED_COUNT; i++) leds.setPixelColor(i, c);
    leds.show();
}

void loop() {
    if (game == 0) {
        for (int i = 0; i < 20; i++) leds.setPixelColor(i, BLUE);
        for (int i = 20; i < 40; i++) leds.setPixelColor(i, RED);
        for (int i = 40; i < 60; i++) leds.setPixelColor(i, WHITE);
        leds.show();
        if (btn(BTN1)) { game = 1; initCS(); delay(200); }
        else if (btn(BTN2)) { game = 2; initTow(); delay(200); }
        else if (btn(BTN3)) { game = 3; initRZ(); delay(200); }
    }
    else if (game == 1) runCS();
    else if (game == 2) runTow();
    else if (game == 3) runRZ();
}

// COLOR SHOOTER
void initCS() {
    for (int i = 0; i < MAX_OBJ; i++) { ePos[i] = -1; bPos[i] = -1; }
    pCol = 0; csOver = false; lastS = millis();
}

void runCS() {
    unsigned long t = millis();
    if (csOver) {
        fill(RED);
        if (anyBtn() && t - lastPress > 200) { lastPress = t; initCS(); }
        return;
    }
    
    // Input
    for (int i = 0; i < 5; i++) {
        if (btn(PINS[i]) && t - lastPress > 150) {
            pCol = i; lastPress = t;
            for (int j = 0; j < MAX_OBJ; j++) if (bPos[j] < 0) { bPos[j] = 3; bCol[j] = i; break; }
            break;
        }
    }
    
    // Move bullets
    if (t - lastB > 30) {
        lastB = t;
        for (int i = 0; i < MAX_OBJ; i++) if (bPos[i] >= 0 && ++bPos[i] >= 60) bPos[i] = -1;
    }
    
    // Move enemies
    if (t - lastE > 150) {
        lastE = t;
        for (int i = 0; i < MAX_OBJ; i++) if (ePos[i] >= 0 && --ePos[i] < 3) csOver = true;
    }
    
    // Spawn
    if (t - lastS > 1500) {
        lastS = t;
        for (int i = 0; i < MAX_OBJ; i++) if (ePos[i] < 0) { ePos[i] = 59; eCol[i] = random(5); break; }
    }
    
    // Collisions
    for (int b = 0; b < MAX_OBJ; b++) {
        if (bPos[b] < 0) continue;
        for (int e = 0; e < MAX_OBJ; e++) {
            if (ePos[e] >= 0 && abs(bPos[b] - ePos[e]) <= 1) {
                if (bCol[b] == eCol[e]) ePos[e] = -1;
                bPos[b] = -1; break;
            }
        }
    }
    
    // Render
    for (int i = 0; i < 60; i++) leds.setPixelColor(i, 0);
    for (int i = 0; i < MAX_OBJ; i++) {
        if (ePos[i] >= 3) leds.setPixelColor(ePos[i], COLORS[eCol[i]]);
        if (bPos[i] >= 0 && bPos[i] < 60) leds.setPixelColor(bPos[i], COLORS[bCol[i]]);
    }
    for (int i = 0; i < 3; i++) leds.setPixelColor(i, COLORS[pCol]);
    leds.show();
}

// TUG OF WAR
void initTow() { rope = 0; towOver = false; }

void runTow() {
    unsigned long t = millis();
    if (towOver) {
        fill(towWon ? GREEN : RED);
        if (anyBtn() && t - towEnd > 1000) initTow();
        return;
    }
    
    static bool g = true, r = true;
    if (btn(BTN5) && g) { g = false; rope -= 2; } else if (!btn(BTN5)) g = true;
    if (btn(BTN2) && r) { r = false; rope += 2; } else if (!btn(BTN2)) r = true;
    
    if (rope <= -25) { towOver = true; towWon = true; towEnd = t; }
    else if (rope >= 25) { towOver = true; towWon = false; towEnd = t; }
    
    int c = 30 - rope * 30 / 25;
    c = constrain(c, 1, 58);
    for (int i = 0; i < 60; i++) leds.setPixelColor(i, i < 30 ? RED : GREEN);
    leds.setPixelColor(c, WHITE);
    leds.show();
}

// REACTION ZONE
void initRZ() {
    rzPos = 59; rzDir = -1; rzSize = 20; rzSpd = 35; rzLvl = 1;
    rzStart = random(0, 40); rzOver = false; rzWait = false; rzLast = millis();
}

void runRZ() {
    unsigned long t = millis();
    if (rzOver) {
        fill(RED);
        if (anyBtn() && t - rzEnd > 1000) initRZ();
        return;
    }
    
    bool pressed = anyBtn();
    if (!pressed) rzWait = false;
    else if (!rzWait) {
        rzWait = true;
        if (rzPos >= rzStart && rzPos < rzStart + rzSize) {
            rzLvl++; rzSize = max(3, 20 - rzLvl); rzSpd = max(12, 35 - rzLvl * 2);
            rzStart = random(0, 60 - rzSize);
        } else { rzOver = true; rzEnd = t; }
    }
    
    if (t - rzLast >= rzSpd) {
        rzLast = t; rzPos += rzDir;
        if (rzPos >= 59) { rzPos = 59; rzDir = -1; }
        else if (rzPos <= 0) { rzPos = 0; rzDir = 1; }
    }
    
    for (int i = 0; i < 60; i++) leds.setPixelColor(i, 0);
    for (int i = rzStart; i < rzStart + rzSize && i < 60; i++) leds.setPixelColor(i, 0x004000);
    leds.setPixelColor(rzPos, YELLOW);
    leds.show();
}
