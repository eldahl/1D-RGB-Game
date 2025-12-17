#include <Adafruit_NeoPixel.h>
#include "WS2812_Definitions.h"

#define PIN 16            // ESP32: Use GPIO16 for NeoPixel data
#define LED_COUNT 60

// Color buttons (ESP32-compatible GPIOs)
// All game controls use these 5 buttons
#define BTN_WHITE_PIN  18
#define BTN_RED_PIN    19
#define BTN_BLUE_PIN   21
#define BTN_YELLOW_PIN 22
#define BTN_GREEN_PIN  23

// Library for WS2812 by Adafruit
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, PIN, NEO_GRB + NEO_KHZ800);

// ============================================
// GAME SELECTION
// ============================================
#define GAME_MENU 0
#define GAME_COLOR_SHOOTER 1
#define GAME_TUG_OF_WAR 2
#define GAME_REACTION_ZONE 3

int currentGame = GAME_MENU;

// ============================================
// COLOR SHOOTER GAME VARIABLES
// ============================================

// Game colors matching the button order: WHITE, RED, BLUE, YELLOW, GREEN
const uint32_t GAME_COLORS[] = {WHITE, RED, BLUE, YELLOW, GREEN};
const byte NUM_COLORS = 5;

// Color button pins array (matches GAME_COLORS order)
const int COLOR_BUTTON_PINS[] = {BTN_WHITE_PIN, BTN_RED_PIN, BTN_BLUE_PIN, BTN_YELLOW_PIN, BTN_GREEN_PIN};

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
const unsigned long COLOR_CHANGE_DELAY = 50;  // Faster color cycling
bool shootButtonReleased = true;              // Require release between shots

// ============================================
// TUG OF WAR GAME VARIABLES
// ============================================
float ropePosition = 0.0;             // Position: negative = player side, positive = CPU side
const float ROPE_MAX = 25.0;          // Distance to win (in LED units)
float playerPullStrength = 2.0;       // How much each pull moves the rope
float cpuPullStrength = 0.15;         // CPU pulls continuously at this rate
float cpuPullVariance = 0.1;          // Random variance in CPU pull
unsigned long lastCpuPull = 0;
unsigned long cpuPullInterval = 50;   // CPU pulls every X ms
unsigned long lastRopeRender = 0;
bool towGameOver = false;
bool playerWon = false;
unsigned long towGameOverTime = 0;

// CPU difficulty increases over time
unsigned long towGameStartTime = 0;
float cpuDifficultyMultiplier = 1.0;

// ============================================
// REACTION ZONE GAME VARIABLES
// ============================================
int rzLightPosition = 0;              // Current position of the bouncing light
int rzLightDirection = 1;             // 1 = moving up (towards high index), -1 = moving down
int rzZoneStart = 0;                  // Start of the goal zone (at bottom/low index)
int rzZoneSize = 10;                  // Size of the goal zone in LEDs
int rzSpeed = 30;                     // ms between light moves (lower = faster)
int rzLevel = 1;                      // Current level
int rzScore = 0;                      // Score
bool rzGameOver = false;
bool rzWaitingForRelease = false;     // Prevent holding the button
unsigned long rzLastMove = 0;
unsigned long rzGameOverTime = 0;

void setup() {
    Serial.begin(9600);
    randomSeed(analogRead(0));
    
    // Setup color buttons
    for (int i = 0; i < NUM_COLORS; i++) {
        pinMode(COLOR_BUTTON_PINS[i], INPUT_PULLUP);
    }
    
    leds.begin();
    leds.setBrightness(100);
    clearLEDs();
    leds.show();
    
    // Show game menu
    showGameMenu();
}

void loop() {
    // Check for game selection via serial
    checkGameSelection();
    
    // Run the currently selected game
    switch (currentGame) {
        case GAME_MENU:
            runGameMenu();
            break;
        case GAME_COLOR_SHOOTER:
            colorShooterGame();
            break;
        case GAME_TUG_OF_WAR:
            tugOfWarGame();
            break;
        case GAME_REACTION_ZONE:
            reactionZoneGame();
            break;
    }
}

// ============================================
// HELPER FUNCTIONS
// ============================================

// Check if any color button is pressed
bool anyColorButtonPressed() {
    for (int i = 0; i < NUM_COLORS; i++) {
        if (digitalRead(COLOR_BUTTON_PINS[i]) == LOW) {
            return true;
        }
    }
    return false;
}

// Count how many color buttons are pressed
int countPressedButtons() {
    int count = 0;
    for (int i = 0; i < NUM_COLORS; i++) {
        if (digitalRead(COLOR_BUTTON_PINS[i]) == LOW) {
            count++;
        }
    }
    return count;
}

// Check if multiple buttons pressed (return to menu)
bool multipleButtonsPressed() {
    return countPressedButtons() >= 4;
}

// Check if all color buttons are released
bool allColorButtonsReleased() {
    for (int i = 0; i < NUM_COLORS; i++) {
        if (digitalRead(COLOR_BUTTON_PINS[i]) == LOW) {
            return false;
        }
    }
    return true;
}

// ============================================
// GAME MENU FUNCTIONS
// ============================================

void showGameMenu() {
    Serial.println("");
    Serial.println("========================================");
    Serial.println("       LED STRIP GAME SELECTOR");
    Serial.println("========================================");
    Serial.println("");
    Serial.println("Press a color button to select a game:");
    Serial.println("");
    Serial.println("  [WHITE] Color Shooter");
    Serial.println("          Shoot matching colors!");
    Serial.println("");
    Serial.println("  [RED]   Tug of War");
    Serial.println("          Mash to beat the CPU!");
    Serial.println("");
    Serial.println("  [BLUE]  Reaction Zone");
    Serial.println("          Stop the light in the zone!");
    Serial.println("");
    Serial.println("========================================");
    currentGame = GAME_MENU;
}

void checkGameSelection() {
    // Only check for game selection when in menu
    if (currentGame != GAME_MENU) return;
    
    // Check serial input
    while (Serial.available() > 0) {
        char input = Serial.read();
        
        // W or 1 = Color Shooter
        if (input == 'w' || input == 'W' || input == '1') {
            currentGame = GAME_COLOR_SHOOTER;
            initColorShooterGame();
            return;
        }
        // R or 2 = Tug of War
        if (input == 'r' || input == 'R' || input == '2') {
            currentGame = GAME_TUG_OF_WAR;
            initTugOfWarGame();
            return;
        }
        // B or 3 = Reaction Zone
        if (input == 'b' || input == 'B' || input == '3') {
            currentGame = GAME_REACTION_ZONE;
            initReactionZoneGame();
            return;
        }
    }
    
    // Check hardware color buttons
    if (digitalRead(BTN_WHITE_PIN) == LOW) {
        currentGame = GAME_COLOR_SHOOTER;
        initColorShooterGame();
        delay(200);  // Debounce
        return;
    }
    if (digitalRead(BTN_RED_PIN) == LOW) {
        currentGame = GAME_TUG_OF_WAR;
        initTugOfWarGame();
        delay(200);  // Debounce
        return;
    }
    if (digitalRead(BTN_BLUE_PIN) == LOW) {
        currentGame = GAME_REACTION_ZONE;
        initReactionZoneGame();
        delay(200);  // Debounce
        return;
    }
}

void runGameMenu() {
    // Show game selection colors on LED strip
    static unsigned long lastUpdate = 0;
    static bool blinkState = true;
    
    if (millis() - lastUpdate > 500) {
        lastUpdate = millis();
        blinkState = !blinkState;
    }
    
    clearLEDs();
    
    // Divide strip into 3 sections for the 3 games
    int sectionSize = LED_COUNT / 3;
    
    // Section 1: WHITE = Color Shooter
    uint32_t whiteColor = blinkState ? WHITE : leds.Color(50, 50, 50);
    for (int i = 0; i < sectionSize; i++) {
        leds.setPixelColor(i, whiteColor);
    }
    
    // Section 2: RED = Tug of War
    uint32_t redColor = blinkState ? RED : leds.Color(50, 0, 0);
    for (int i = sectionSize; i < sectionSize * 2; i++) {
        leds.setPixelColor(i, redColor);
    }
    
    // Section 3: BLUE = Reaction Zone
    uint32_t blueColor = blinkState ? BLUE : leds.Color(0, 0, 50);
    for (int i = sectionSize * 2; i < LED_COUNT; i++) {
        leds.setPixelColor(i, blueColor);
    }
    
    leds.show();
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
    shootButtonReleased = true;
    
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
    Serial.println("Controls (press to shoot that color):");
    Serial.println("  [W] White | [R] Red | [B] Blue");
    Serial.println("  [Y] Yellow | [G] Green");
    Serial.println("  [4+ buttons] - Back to Menu");
    Serial.println("==========================");
}

void colorShooterGame() {
    unsigned long currentTime = millis();
    
    // Check for multi-button press to return to menu
    if (multipleButtonsPressed()) {
        delay(200);  // Debounce
        showGameMenu();
        return;
    }
    
    if (gameOver) {
        showGameOver();
        // Check for restart (press shoot button or 'a' via serial)
        bool restartPressed = false;
        
        // Check serial for restart (drain buffer)
        while (Serial.available() > 0) {
            char input = Serial.read();
            if (input == 'a' || input == 'A') {
                restartPressed = true;
            } else if (input == '0') {
                showGameMenu();
                return;
            }
        }
        
        // Check hardware button for restart (any color button)
        if (anyColorButtonPressed()) {
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
    // Drain ALL available serial input (prevents buffer backup)
    while (Serial.available() > 0) {
        char input = Serial.read();
        
        // '0' to return to menu
        if (input == '0') {
            showGameMenu();
            return;
        }
        
        // Serial color selection: w/r/b/y/g - select color AND shoot
        bool colorSelected = false;
        if (input == 'w' || input == 'W') { playerColorIndex = 0; colorSelected = true; }
        if (input == 'r' || input == 'R') { playerColorIndex = 1; colorSelected = true; }
        if (input == 'b' || input == 'B') { playerColorIndex = 2; colorSelected = true; }
        if (input == 'y' || input == 'Y') { playerColorIndex = 3; colorSelected = true; }
        if (input == 'g' || input == 'G') { playerColorIndex = 4; colorSelected = true; }
        
        if (colorSelected && currentTime - lastShootPress > DEBOUNCE_DELAY) {
            lastShootPress = currentTime;
            shootBullet();
        }
        
        // 'd' or 'D' to cycle colors and shoot
        if ((input == 'd' || input == 'D') && 
            currentTime - lastColorPress > COLOR_CHANGE_DELAY) {
            lastColorPress = currentTime;
            playerColorIndex = (playerColorIndex + 1) % NUM_COLORS;
            shootBullet();
        }
    }
    
    // Hardware color buttons: select color AND shoot (requires release between shots)
    bool anyButtonDown = false;
    for (int i = 0; i < NUM_COLORS; i++) {
        if (digitalRead(COLOR_BUTTON_PINS[i]) == LOW) {
            anyButtonDown = true;
            if (shootButtonReleased && currentTime - lastShootPress > DEBOUNCE_DELAY) {
                playerColorIndex = i;
                lastShootPress = currentTime;
                shootButtonReleased = false;  // Must release before next shot
                shootBullet();
            }
            break;
        }
    }
    
    // Track button release
    if (!anyButtonDown) {
        shootButtonReleased = true;
    }
}

void shootBullet() {
    // Find an inactive bullet slot
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bulletPositions[i] < 0) {
            bulletPositions[i] = PLAYER_ZONE;  // Start just past player zone
            bulletColors[i] = playerColorIndex;
            // Don't print on every shot - causes buffer backup!
            return;
        }
    }
    // No available bullet slots (max bullets in flight)
}

void moveBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bulletPositions[i] >= 0) {
            bulletPositions[i]++;  // Move towards the right (high indices)
            if (bulletPositions[i] >= LED_COUNT) {
                // Bullet went off the strip
                bulletPositions[i] = -1;
            }
        }
    }
}

void moveEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemyPositions[i] >= 0) {
            enemyPositions[i]--;  // Move towards the left (player)
            
            // Check if enemy reached player zone - GAME OVER!
            if (enemyPositions[i] < PLAYER_ZONE) {
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
            enemyPositions[i] = LED_COUNT - 1;  // Start at end of strip (right side)
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
                        
                        // Show hit effect (non-blocking version)
                        showHitEffect(hitPos);
                    } else {
                        // Wrong color - bullet is destroyed, enemy survives
                        bulletPositions[b] = -1;
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
    enemySpeed = max((long unsigned) 80, 200 - (elapsed / 10000) * 20);
    
    // Spawn faster (minimum 800ms)
    spawnInterval = max((long unsigned) 800, 2000 - (elapsed / 15000) * 200);
}

void renderGame() {
    clearLEDs();
    
    // Draw enemies (coming from the right)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemyPositions[i] >= PLAYER_ZONE && enemyPositions[i] < LED_COUNT) {
            // Draw enemy with a small trail (trail to the right, behind enemy)
            leds.setPixelColor(enemyPositions[i], GAME_COLORS[enemyColors[i]]);
            if (enemyPositions[i] < LED_COUNT - 1) {
                leds.setPixelColor(enemyPositions[i] + 1, dimColor(GAME_COLORS[enemyColors[i]], 4));
            }
        }
    }
    
    // Draw all bullets (moving to the right)
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bulletPositions[i] >= 0 && bulletPositions[i] < LED_COUNT) {
            leds.setPixelColor(bulletPositions[i], GAME_COLORS[bulletColors[i]]);
            // Bullet trail (to the left, behind bullet)
            if (bulletPositions[i] > 0) {
                leds.setPixelColor(bulletPositions[i] - 1, dimColor(GAME_COLORS[bulletColors[i]], 3));
            }
            if (bulletPositions[i] > 1) {
                leds.setPixelColor(bulletPositions[i] - 2, dimColor(GAME_COLORS[bulletColors[i]], 6));
            }
        }
    }
    
    // Draw player zone (first few LEDs show selected color)
    uint32_t playerColor = GAME_COLORS[playerColorIndex];
    if (colorBlinkState) {
        for (int i = 0; i < PLAYER_ZONE; i++) {
            leds.setPixelColor(i, playerColor);
        }
    } else {
        for (int i = 0; i < PLAYER_ZONE; i++) {
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

// ============================================
// TUG OF WAR GAME FUNCTIONS
// ============================================

void initTugOfWarGame() {
    ropePosition = 0.0;
    towGameOver = false;
    playerWon = false;
    towGameStartTime = millis();
    cpuDifficultyMultiplier = 1.0;
    lastCpuPull = millis();
    
    Serial.println("");
    Serial.println("=== TUG OF WAR ===");
    Serial.println("Pull the rope to YOUR side (right)!");
    Serial.println("Mash ANY button to win!");
    Serial.println("");
    Serial.println("Controls:");
    Serial.println("  [Any Button] - PULL!");
    Serial.println("  [4+ buttons] - Back to Menu");
    Serial.println("==================");
}

void tugOfWarGame() {
    unsigned long currentTime = millis();
    
    // Check for multi-button press to return to menu
    if (multipleButtonsPressed()) {
        delay(200);  // Debounce
        showGameMenu();
        return;
    }
    
    if (towGameOver) {
        showTugOfWarResult();
        
        // Check for restart or menu (drain buffer)
        while (Serial.available() > 0) {
            char input = Serial.read();
            if (input == '0') {
                showGameMenu();
                return;
            }
            // Any color key restarts
            if (input == 'w' || input == 'W' ||
                input == 'r' || input == 'R' ||
                input == 'b' || input == 'B' ||
                input == 'y' || input == 'Y' ||
                input == 'g' || input == 'G') {
                initTugOfWarGame();
                return;
            }
        }
        if (anyColorButtonPressed() && 
            currentTime - towGameOverTime > 1000) {
            initTugOfWarGame();
        }
        return;
    }
    
    // Handle player input
    handleTugOfWarInput(currentTime);
    
    // CPU pulls continuously
    if (currentTime - lastCpuPull >= cpuPullInterval) {
        lastCpuPull = currentTime;
        
        // Increase difficulty over time
        unsigned long elapsed = currentTime - towGameStartTime;
        cpuDifficultyMultiplier = 1.0 + (elapsed / 5000.0) * 0.2;  // +20% every 5 seconds
        
        // CPU pull with some randomness
        float pull = cpuPullStrength * cpuDifficultyMultiplier;
        pull += random(-100, 100) / 1000.0 * cpuPullVariance;
        ropePosition += pull;  // Positive = towards CPU side (left)
    }
    
    // Check win conditions
    if (ropePosition <= -ROPE_MAX) {
        // Player wins!
        towGameOver = true;
        playerWon = true;
        towGameOverTime = currentTime;
        Serial.println("");
        Serial.println("*** YOU WIN! ***");
        Serial.println("Press [A] to play again or [0] for menu");
    } else if (ropePosition >= ROPE_MAX) {
        // CPU wins
        towGameOver = true;
        playerWon = false;
        towGameOverTime = currentTime;
        Serial.println("");
        Serial.println("*** CPU WINS! ***");
        Serial.println("Press [A] to play again or [0] for menu");
    }
    
    // Render the game
    renderTugOfWar();
}

void handleTugOfWarInput(unsigned long currentTime) {
    // Drain ALL available serial input (prevents buffer backup when mashing)
    while (Serial.available() > 0) {
        char input = Serial.read();
        
        // '0' to return to menu
        if (input == '0') {
            showGameMenu();
            return;
        }
        
        // Any color key (w/r/b/y/g) to pull - each keypress counts!
        if (input == 'w' || input == 'W' ||
            input == 'r' || input == 'R' ||
            input == 'b' || input == 'B' ||
            input == 'y' || input == 'Y' ||
            input == 'g' || input == 'G') {
            ropePosition -= playerPullStrength;  // Negative = towards player side
        }
    }
    
    // Hardware buttons: Pull (any color button)
    static unsigned long lastButtonPull = 0;
    if (anyColorButtonPressed() && 
        currentTime - lastButtonPull > 50) {  // Fast repeat for mashing
        lastButtonPull = currentTime;
        ropePosition -= playerPullStrength;
    }
}

void renderTugOfWar() {
    clearLEDs();
    
    // Calculate center position based on rope position
    // ropePosition: -ROPE_MAX (player wins) to +ROPE_MAX (CPU wins)
    // Map to LED strip: 0 (CPU side) to LED_COUNT-1 (Player side)
    int center = LED_COUNT / 2;
    int ropeCenter = center - (int)(ropePosition * (LED_COUNT / 2) / ROPE_MAX);
    ropeCenter = constrain(ropeCenter, 2, LED_COUNT - 3);
    
    // Draw CPU side (left/start) - Red gradient
    for (int i = 0; i < center; i++) {
        int brightness = map(i, 0, center, 50, 150);
        leds.setPixelColor(i, leds.Color(brightness, 0, 0));
    }
    
    // Draw Player side (right/end) - Green gradient
    for (int i = center; i < LED_COUNT; i++) {
        int brightness = map(i, center, LED_COUNT, 150, 50);
        leds.setPixelColor(i, leds.Color(0, brightness, 0));
    }
    
    // Draw the rope marker (white with glow)
    leds.setPixelColor(ropeCenter, WHITE);
    if (ropeCenter > 0) {
        leds.setPixelColor(ropeCenter - 1, leds.Color(100, 100, 100));
    }
    if (ropeCenter < LED_COUNT - 1) {
        leds.setPixelColor(ropeCenter + 1, leds.Color(100, 100, 100));
    }
    if (ropeCenter > 1) {
        leds.setPixelColor(ropeCenter - 2, leds.Color(50, 50, 50));
    }
    if (ropeCenter < LED_COUNT - 2) {
        leds.setPixelColor(ropeCenter + 2, leds.Color(50, 50, 50));
    }
    
    leds.show();
}

void showTugOfWarResult() {
    static unsigned long lastBlink = 0;
    static bool blinkState = true;
    
    unsigned long currentTime = millis();
    if (currentTime - lastBlink >= 200) {
        lastBlink = currentTime;
        blinkState = !blinkState;
    }
    
    if (playerWon) {
        // Flash green for player win
        uint32_t color = blinkState ? GREEN : leds.Color(0, 50, 0);
        for (int i = 0; i < LED_COUNT; i++) {
            leds.setPixelColor(i, color);
        }
    } else {
        // Flash red for CPU win
        uint32_t color = blinkState ? RED : leds.Color(50, 0, 0);
        for (int i = 0; i < LED_COUNT; i++) {
            leds.setPixelColor(i, color);
        }
    }
    leds.show();
}

// ============================================
// REACTION ZONE GAME FUNCTIONS
// ============================================

void initReactionZoneGame() {
    rzLightPosition = LED_COUNT - 1;  // Start at top
    rzLightDirection = -1;            // Moving down initially
    rzZoneSize = 20;                  // Start with a big zone
    rzZoneStart = random(0, LED_COUNT - rzZoneSize);  // Random position
    rzSpeed = 35;                     // Starting speed (ms per move)
    rzLevel = 1;
    rzScore = 0;
    rzGameOver = false;
    rzWaitingForRelease = false;
    rzLastMove = millis();
    
    Serial.println("");
    Serial.println("=== REACTION ZONE ===");
    Serial.println("Press ANY button when the light is in the GREEN zone!");
    Serial.println("");
    Serial.println("Controls:");
    Serial.println("  [Any Button] - STOP!");
    Serial.println("  [4+ buttons] - Back to Menu");
    Serial.println("=====================");
}

void reactionZoneGame() {
    unsigned long currentTime = millis();
    
    // Check for multi-button press to return to menu
    if (multipleButtonsPressed()) {
        delay(200);  // Debounce
        showGameMenu();
        return;
    }
    
    if (rzGameOver) {
        showReactionZoneResult();
        
        // Check for restart or menu (drain buffer)
        while (Serial.available() > 0) {
            char input = Serial.read();
            if (input == '0') {
                showGameMenu();
                return;
            }
            // Any color key restarts
            if (input == 'w' || input == 'W' ||
                input == 'r' || input == 'R' ||
                input == 'b' || input == 'B' ||
                input == 'y' || input == 'Y' ||
                input == 'g' || input == 'G') {
                initReactionZoneGame();
                return;
            }
        }
        
        // Hardware button restart (any color button)
        if (anyColorButtonPressed() && 
            currentTime - rzGameOverTime > 1000) {
            initReactionZoneGame();
        }
        return;
    }
    
    // Handle input
    handleReactionZoneInput(currentTime);
    
    // Move the light
    if (currentTime - rzLastMove >= rzSpeed) {
        rzLastMove = currentTime;
        
        rzLightPosition += rzLightDirection;
        
        // Bounce at ends
        if (rzLightPosition >= LED_COUNT - 1) {
            rzLightPosition = LED_COUNT - 1;
            rzLightDirection = -1;
        } else if (rzLightPosition <= 0) {
            rzLightPosition = 0;
            rzLightDirection = 1;
        }
    }
    
    // Render the game
    renderReactionZone();
}

void handleReactionZoneInput(unsigned long currentTime) {
    bool buttonPressed = false;
    
    // Check serial input (drain buffer)
    while (Serial.available() > 0) {
        char input = Serial.read();
        
        if (input == '0') {
            showGameMenu();
            return;
        }
        
        // Any color key triggers action
        if (input == 'w' || input == 'W' ||
            input == 'r' || input == 'R' ||
            input == 'b' || input == 'B' ||
            input == 'y' || input == 'Y' ||
            input == 'g' || input == 'G') {
            buttonPressed = true;
        }
    }
    
    // Check hardware buttons (any color button)
    if (anyColorButtonPressed()) {
        buttonPressed = true;
    } else {
        rzWaitingForRelease = false;  // All buttons released
    }
    
    // Process button press (only once per press)
    if (buttonPressed && !rzWaitingForRelease) {
        rzWaitingForRelease = true;
        
        // Check if light is in the zone
        if (rzLightPosition >= rzZoneStart && rzLightPosition < rzZoneStart + rzZoneSize) {
            // SUCCESS! Level up!
            rzScore += rzLevel * 10;
            rzLevel++;
            
            // Make it harder
            rzZoneSize = max(3, 20 - rzLevel);    // Zone gets smaller (min 3 LEDs)
            rzSpeed = max(12, 35 - rzLevel * 2);  // Light gets faster (min 12ms)
            
            // Move zone to a new random position
            rzZoneStart = random(0, LED_COUNT - rzZoneSize);
            
            // Flash success
            showReactionSuccess();
        } else {
            // MISS! Game over
            rzGameOver = true;
            rzGameOverTime = currentTime;
            Serial.println("");
            Serial.print("GAME OVER! Level: ");
            Serial.print(rzLevel);
            Serial.print(" Score: ");
            Serial.println(rzScore);
            Serial.println("Press [A] to play again or [0] for menu");
        }
    }
}

void renderReactionZone() {
    clearLEDs();
    
    // Draw the goal zone (green)
    for (int i = rzZoneStart; i < rzZoneStart + rzZoneSize; i++) {
        if (i < LED_COUNT) {
            leds.setPixelColor(i, leds.Color(0, 100, 0));  // Dark green background
        }
    }
    
    // Draw fading trail behind the light
    for (int t = 1; t <= 5; t++) {
        int trailPos = rzLightPosition - (rzLightDirection * t);
        if (trailPos >= 0 && trailPos < LED_COUNT) {
            int brightness = 255 / (t + 1);
            leds.setPixelColor(trailPos, leds.Color(brightness, brightness / 2, 0));
        }
    }
    
    // Draw the bouncing light (bright white/yellow)
    if (rzLightPosition >= 0 && rzLightPosition < LED_COUNT) {
        leds.setPixelColor(rzLightPosition, leds.Color(255, 200, 50));
    }
    
    leds.show();
}

void showReactionSuccess() {
    // Quick green flash to show success
    for (int flash = 0; flash < 3; flash++) {
        for (int i = 0; i < LED_COUNT; i++) {
            leds.setPixelColor(i, GREEN);
        }
        leds.show();
        delay(50);
        clearLEDs();
        leds.show();
        delay(50);
    }
}

void showReactionZoneResult() {
    static unsigned long lastBlink = 0;
    static bool blinkState = true;
    
    unsigned long currentTime = millis();
    if (currentTime - lastBlink >= 300) {
        lastBlink = currentTime;
        blinkState = !blinkState;
    }
    
    // Flash red for game over
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
