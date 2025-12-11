#include <Adafruit_NeoPixel.h>
#include "WS2812_Definitions.h"

#define PIN 4
#define LED_COUNT 60

// Button pins for the Color Shooter game
#define SHOOT_BUTTON_PIN 2    // Button to shoot
#define COLOR_BUTTON_PIN 3    // Button to change player color

// Library for WS2812 by Adafruit
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, PIN, NEO_GRB + NEO_KHZ800);

// ============================================
// COLOR SHOOTER GAME VARIABLES
// ============================================

// Game colors (distinct, easy to recognize)
const uint32_t GAME_COLORS[] = {RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN};
const byte NUM_COLORS = 6;

// Falling enemies
#define MAX_ENEMIES 10
int enemyPositions[MAX_ENEMIES];      // Position on strip (-1 = inactive)
byte enemyColors[MAX_ENEMIES];        // Color index into GAME_COLORS

// Player bullets (multiple allowed)
#define MAX_BULLETS 10
int bulletPositions[MAX_BULLETS];     // Position on strip (-1 = inactive)
byte bulletColors[MAX_BULLETS];       // Color index of each bullet

// Player state
byte playerColorIndex = 0;            // Current selected color index
const int PLAYER_ZONE = 3;            // LEDs at the end for player indicator

// Game state
unsigned long lastEnemyMove = 0;
unsigned long lastBulletMove = 0;
unsigned long lastColorBlink = 0;
unsigned long lastSpawn = 0;
unsigned long gameStartTime = 0;

int enemySpeed = 200;                 // ms between enemy moves (lower = faster)
int bulletSpeed = 30;                 // ms between bullet moves
int spawnInterval = 2000;             // ms between enemy spawns

int score = 0;
bool gameOver = false;
bool colorBlinkState = true;

// Button debouncing
unsigned long lastShootPress = 0;
unsigned long lastColorPress = 0;
const unsigned long DEBOUNCE_DELAY = 150;

void setup() {
    Serial.begin(9600);
    
    // Setup button pins
    pinMode(SHOOT_BUTTON_PIN, INPUT_PULLUP);
    pinMode(COLOR_BUTTON_PIN, INPUT_PULLUP);
    
    leds.begin();
    leds.setBrightness(100);
    clearLEDs();
    leds.show();
    
    // Initialize game
    initColorShooterGame();
}

/*
    0: Ping pong        A-->B-->A..
    1: Rainbow
    2: Ping no-Pong     A-->B
    3: Color Shooter Game
*/
void loop() {
    // Run the Color Shooter Game
    colorShooterGame();
    
    // Uncomment below for other modes:
    /*
    // Ping pong
    for (int i=0; i<3; i++) {
        cylon(RED, 75); 
    }
    */
    
    //for (int i=0; i < LED_COUNT * 2; i++) {
    //    rainbow(i);
    //    delay(150);
    //}
    
    /*
    // Cascade
    cascade(DEEPSKYBLUE, TOP_DOWN, 100);
    cascade(GREEN, TOP_DOWN, 100);
    cascade(ORANGERED, TOP_DOWN, 100);
    cascade(RED, TOP_DOWN, 100);
    
    setAllColor(255,100,0);
    
    delay(20000);
    */

    //rainbowFull(15);
}

void rainbowFull(int delay_ms) {
  for (int transition = 0; transition < 6; transition++) {
    for (int color = 0; color < 255; color++) {
      for (int j=0; j < 255; j++) {
          // 192 total colors in rainbowOrder function.
          leds.setPixelColor(j, rainbowOrderFull(color, transition));
      }
      leds.show();
      delay(delay_ms);
    }
  }
}

uint32_t rainbowOrderFull(byte position, int transition) {
  if (transition == 0)  
    // Red -> Yellow (Red = FF, blue = 0, green goes 00-FF)
    return leds.Color(0xFF, position, 0);
  else if (transition == 1) {
    // Yellow -> Green (Green = FF, blue = 0, red goes FF->00)
    return leds.Color(0xff - position, 0xFF, 0);
  }
  else if (transition == 2) {
    // Green->Aqua (Green = FF, red = 0, blue goes 00->FF)
    return leds.Color(0, 0xFF, position);
  }
  else if (transition == 3) {
    // Aqua->Blue (Blue = FF, red = 0, green goes FF->00)
    return leds.Color(0, 0xff - position, 0xFF);
  }
  else if (transition == 4) {
    // Blue->Fuchsia (Blue = FF, green = 0, red goes 00->FF)
    return leds.Color(position, 0, 0xFF);
  }
  else {
    //160 <position< 191   Fuchsia->Red (Red = FF, green = 0, blue goes FF->00)
    return leds.Color(0xFF, 0x00, 0xff - position);
  }
  
}

// ============================================
// COLOR SHOOTER GAME FUNCTIONS
// ============================================

void initColorShooterGame() {
    // Reset all enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemyPositions[i] = -1;
    }
    
    // Reset all bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        bulletPositions[i] = -1;
    }
    
    // Reset player
    playerColorIndex = 0;
    
    // Reset game state
    score = 0;
    gameOver = false;
    enemySpeed = 200;
    spawnInterval = 2000;
    gameStartTime = millis();
    lastSpawn = millis();
    
    Serial.println("=== COLOR SHOOTER GAME ===");
    Serial.println("Shoot matching colors to destroy enemies!");
    Serial.println("");
    Serial.println("Controls (Hardware Buttons):");
    Serial.println("  Pin 2: SHOOT");
    Serial.println("  Pin 3: CHANGE COLOR");
    Serial.println("");
    Serial.println("Controls (Serial):");
    Serial.println("  'a' or 'A': SHOOT");
    Serial.println("  'd' or 'D': CHANGE COLOR");
    Serial.println("==========================");
}

void colorShooterGame() {
    unsigned long currentTime = millis();
    
    if (gameOver) {
        showGameOver();
        // Check for restart (press shoot button or 'a' via serial)
        bool restartPressed = false;
        
        // Check serial for restart
        if (Serial.available() > 0) {
            char input = Serial.read();
            if (input == 'a' || input == 'A') {
                restartPressed = true;
            }
        }
        
        // Check hardware button for restart
        if (digitalRead(SHOOT_BUTTON_PIN) == LOW) {
            restartPressed = true;
        }
        
        if (restartPressed && currentTime - lastShootPress > DEBOUNCE_DELAY) {
            lastShootPress = currentTime;
            initColorShooterGame();
        }
        return;
    }
    
    // Handle input
    handleGameInput(currentTime);
    
    // Move bullet
    if (currentTime - lastBulletMove >= bulletSpeed) {
        lastBulletMove = currentTime;
        moveBullets();
    }
    
    // Move enemies
    if (currentTime - lastEnemyMove >= enemySpeed) {
        lastEnemyMove = currentTime;
        moveEnemies();
    }
    
    // Spawn new enemies
    if (currentTime - lastSpawn >= spawnInterval) {
        lastSpawn = currentTime;
        spawnEnemy();
    }
    
    // Check collisions
    checkCollisions();
    
    // Increase difficulty over time
    updateDifficulty(currentTime);
    
    // Blink player color indicator
    if (currentTime - lastColorBlink >= 300) {
        lastColorBlink = currentTime;
        colorBlinkState = !colorBlinkState;
    }
    
    // Render the game
    renderGame();
}

void handleGameInput(unsigned long currentTime) {
    // Check serial input first
    if (Serial.available() > 0) {
        char input = Serial.read();
        
        // 'a' or 'A' to shoot
        if ((input == 'a' || input == 'A') && 
            currentTime - lastShootPress > DEBOUNCE_DELAY) {
            lastShootPress = currentTime;
            shootBullet();
        }
        
        // 'd' or 'D' to change color
        if ((input == 'd' || input == 'D') && 
            currentTime - lastColorPress > DEBOUNCE_DELAY) {
            lastColorPress = currentTime;
            playerColorIndex = (playerColorIndex + 1) % NUM_COLORS;
            Serial.print("Color changed to: ");
            Serial.println(playerColorIndex);
        }
    }
    
    // Hardware button: Shoot (active LOW due to INPUT_PULLUP)
    if (digitalRead(SHOOT_BUTTON_PIN) == LOW && 
        currentTime - lastShootPress > DEBOUNCE_DELAY) {
        lastShootPress = currentTime;
        shootBullet();
    }
    
    // Hardware button: Color change
    if (digitalRead(COLOR_BUTTON_PIN) == LOW && 
        currentTime - lastColorPress > DEBOUNCE_DELAY) {
        lastColorPress = currentTime;
        playerColorIndex = (playerColorIndex + 1) % NUM_COLORS;
        Serial.print("Color changed to: ");
        Serial.println(playerColorIndex);
    }
}

void shootBullet() {
    // Find an inactive bullet slot
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bulletPositions[i] < 0) {
            bulletPositions[i] = LED_COUNT - PLAYER_ZONE - 1;
            bulletColors[i] = playerColorIndex;
            Serial.println("SHOOT!");
            return;
        }
    }
    // No available bullet slots (max bullets in flight)
}

void moveBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bulletPositions[i] >= 0) {
            bulletPositions[i]--;
            if (bulletPositions[i] < 0) {
                // Bullet went off the strip
            }
        }
    }
}

void moveEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemyPositions[i] >= 0) {
            enemyPositions[i]++;
            
            // Check if enemy reached player zone - GAME OVER!
            if (enemyPositions[i] >= LED_COUNT - PLAYER_ZONE) {
                gameOver = true;
                Serial.println("GAME OVER!");
                Serial.print("Final Score: ");
                Serial.println(score);
            }
        }
    }
}

void spawnEnemy() {
    // Find an inactive enemy slot
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemyPositions[i] < 0) {
            enemyPositions[i] = 0;  // Start at beginning of strip
            enemyColors[i] = random(NUM_COLORS);  // Random color
            return;
        }
    }
}

void checkCollisions() {
    // Check each bullet against each enemy
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (bulletPositions[b] < 0) continue;
        
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (enemyPositions[e] >= 0) {
                // Check if bullet hit this enemy
                if (abs(bulletPositions[b] - enemyPositions[e]) <= 1) {
                    // Check if colors match
                    if (bulletColors[b] == enemyColors[e]) {
                        // HIT! Destroy enemy and bullet
                        int hitPos = enemyPositions[e];
                        enemyPositions[e] = -1;
                        bulletPositions[b] = -1;
                        score += 10;
                        Serial.print("HIT! Score: ");
                        Serial.println(score);
                        
                        // Show hit effect
                        showHitEffect(hitPos);
                    } else {
                        // Wrong color - bullet is destroyed, enemy survives
                        bulletPositions[b] = -1;
                        Serial.println("Wrong color!");
                    }
                    break;  // This bullet is done, check next bullet
                }
            }
        }
    }
}

void showHitEffect(int position) {
    // Quick flash effect at hit position
    for (int flash = 0; flash < 3; flash++) {
        for (int j = max(0, position - 2); j <= min(LED_COUNT - 1, position + 2); j++) {
            leds.setPixelColor(j, WHITE);
        }
        leds.show();
        delay(30);
        for (int j = max(0, position - 2); j <= min(LED_COUNT - 1, position + 2); j++) {
            leds.setPixelColor(j, BLACK);
        }
        leds.show();
        delay(30);
    }
}

void updateDifficulty(unsigned long currentTime) {
    // Increase difficulty every 10 seconds
    unsigned long elapsed = currentTime - gameStartTime;
    
    // Speed up enemies (minimum 80ms)
    enemySpeed = max(80, 200 - (elapsed / 10000) * 20);
    
    // Spawn faster (minimum 800ms)
    spawnInterval = max(800, 2000 - (elapsed / 15000) * 200);
}

void renderGame() {
    clearLEDs();
    
    // Draw enemies (falling colors)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemyPositions[i] >= 0 && enemyPositions[i] < LED_COUNT - PLAYER_ZONE) {
            // Draw enemy with a small trail
            leds.setPixelColor(enemyPositions[i], GAME_COLORS[enemyColors[i]]);
            if (enemyPositions[i] > 0) {
                leds.setPixelColor(enemyPositions[i] - 1, dimColor(GAME_COLORS[enemyColors[i]], 4));
            }
        }
    }
    
    // Draw all bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bulletPositions[i] >= 0) {
            leds.setPixelColor(bulletPositions[i], GAME_COLORS[bulletColors[i]]);
            // Bullet trail
            if (bulletPositions[i] < LED_COUNT - 1) {
                leds.setPixelColor(bulletPositions[i] + 1, dimColor(GAME_COLORS[bulletColors[i]], 3));
            }
            if (bulletPositions[i] < LED_COUNT - 2) {
                leds.setPixelColor(bulletPositions[i] + 2, dimColor(GAME_COLORS[bulletColors[i]], 6));
            }
        }
    }
    
    // Draw player zone (last few LEDs show selected color)
    uint32_t playerColor = GAME_COLORS[playerColorIndex];
    if (colorBlinkState) {
        for (int i = LED_COUNT - PLAYER_ZONE; i < LED_COUNT; i++) {
            leds.setPixelColor(i, playerColor);
        }
    } else {
        for (int i = LED_COUNT - PLAYER_ZONE; i < LED_COUNT; i++) {
            leds.setPixelColor(i, dimColor(playerColor, 3));
        }
    }
    
    leds.show();
}

uint32_t dimColor(uint32_t color, byte divisor) {
    byte r = ((color >> 16) & 0xFF) / divisor;
    byte g = ((color >> 8) & 0xFF) / divisor;
    byte b = (color & 0xFF) / divisor;
    return leds.Color(r, g, b);
}

void showGameOver() {
    static unsigned long lastBlink = 0;
    static bool blinkState = true;
    
    unsigned long currentTime = millis();
    if (currentTime - lastBlink >= 500) {
        lastBlink = currentTime;
        blinkState = !blinkState;
    }
    
    // Flash red to indicate game over
    if (blinkState) {
        for (int i = 0; i < LED_COUNT; i++) {
            leds.setPixelColor(i, RED);
        }
    } else {
        clearLEDs();
    }
    leds.show();
}

void police() {

    for(int i = 0; i < 30; i++){
        for(i = 0; i < LED_COUNT/2; i++) {
            leds.setPixelColor(i, 0,0,255);
        }
        for(i = LED_COUNT/2; i < LED_COUNT; i++) {
            leds.setPixelColor(i, 0,0,0);
        }
        leds.show();
        delay(250);
        for(i = 0; i < LED_COUNT/2; i++) {
            leds.setPixelColor(i, 0,0,0);
        }
        for(i = LED_COUNT/2; i < LED_COUNT; i++) {
            leds.setPixelColor(i, 0,0,255);
        }
        leds.show();
        delay(250);
    }
    
}

void setAllColor(byte r, byte g, byte b) {
    for (int i = 0; i < LED_COUNT; i++) {
        leds.setPixelColor(i, r,g,b);
    }
    leds.show();
}

void cylon(unsigned long color, byte wait) {
    
    // weight determines how much lighter the outer "eye" colors are
    const byte weight = 4;  
    
    byte red    = (color & 0xFF0000) >> 16;
    byte green  = (color & 0x00FF00) >> 8;
    byte blue   = (color & 0x0000FF);

    // A--->B
    for (int i=0; i<=LED_COUNT-1; i++) {
        clearLEDs();
        leds.setPixelColor(i, red, green, blue);  // Set the bright middle eye
        // Two eyes on each side get progressively dimmer
        for (int j=1; j<3; j++) {
            if (i-j >= 0)
                leds.setPixelColor(i-j, red/(weight*j), green/(weight*j), blue/(weight*j));
            if (i-j <= LED_COUNT)
                leds.setPixelColor(i+j, red/(weight*j), green/(weight*j), blue/(weight*j));
        }
        leds.show();
        delay(wait);
    }
    // B--->A
    for (int i=LED_COUNT-2; i>=1; i--) {
        clearLEDs();
        leds.setPixelColor(i, red, green, blue);
        for (int j=1; j<3; j++) {
            if (i-j >= 0)
                leds.setPixelColor(i-j, red/(weight*j), green/(weight*j), blue/(weight*j));
            if (i-j <= LED_COUNT)
                leds.setPixelColor(i+j, red/(weight*j), green/(weight*j), blue/(weight*j));
        }
        leds.show();
        delay(wait);
    }
}

void cascade(unsigned long color, byte direction, byte wait) {
    if (direction == TOP_DOWN) {
        for (int i=0; i<LED_COUNT; i++) {
            clearLEDs();  // Turn off all LEDs
            leds.setPixelColor(i, color);  // Set just this one
            leds.show();
            delay(wait);
        }
    }
    else {
        for (int i=LED_COUNT-1; i>=0; i--) {
            clearLEDs();
            leds.setPixelColor(i, color);
            leds.show();
            delay(wait);
        }
    }
}

void clearLEDs() {
    for (int i=0; i<LED_COUNT; i++) {
        leds.setPixelColor(i, 0);
    }
}

void rainbow(byte startPosition) {
    // Scale our rainbow
    int rainbowScale = 192 / LED_COUNT;
  
    for (int i=0; i<LED_COUNT; i++) {
        // 192 total colors in rainbowOrder function.
        leds.setPixelColor(i, rainbowOrder((rainbowScale * (i + startPosition)) % 192));
    }
    leds.show();
}

uint32_t rainbowOrder(byte position) {
  // Red -> Yellow (Red = FF, blue = 0, green goes 00-FF)
  if (position < 31)  
    return leds.Color(0xFF, position * 8, 0);
  // Yellow -> Green (Green = FF, blue = 0, red goes FF->00)
  else if (position < 63) {
    position -= 31;
    return leds.Color(0xFF - position * 8, 0xFF, 0);
  }
  // Green->Aqua (Green = FF, red = 0, blue goes 00->FF)
  else if (position < 95) {
    position -= 63;
    return leds.Color(0, 0xFF, position * 8);
  }
  // Aqua->Blue (Blue = FF, red = 0, green goes FF->00)
  else if (position < 127) {
    position -= 95;
    return leds.Color(0, 0xFF - position * 8, 0xFF);
  }
  // Blue->Fuchsia (Blue = FF, green = 0, red goes 00->FF)
  else if (position < 159) {
    position -= 127;
    return leds.Color(position * 8, 0, 0xFF);
  }
  //160 <position< 191   Fuchsia->Red (Red = FF, green = 0, blue goes FF->00)
  else {
    position -= 159;
    return leds.Color(0xFF, 0x00, 0xFF - position * 8);
  }
  
}
