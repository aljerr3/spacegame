/*
 * SPACE NAVIGATOR 🚀
 * 
 * Demuestra el control ANALÓGICO del joystick:
 * - Velocidad PROPORCIONAL a la inclinación
 * - Movimiento en 360° continuo
 * - Indicadores visuales de X/Y en tiempo real
 * 
 * Conexiones:
 *   Joystick VRX  -> GPIO 34
 *   Joystick VRY  -> GPIO 35
 *   Touch CS      -> GPIO 5
 *   Touch IRQ     -> GPIO 21
 * 
 * Touch: Reiniciar juego
 */

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();

// ========== TOUCH ==========
#define TOUCH_CS  5
#define TOUCH_IRQ 21
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

// ========== JOYSTICK ==========
#define JOY_X 34
#define JOY_Y 35

// ========== COLORES ==========
#define COLOR_BG        tft.color565(10, 10, 25)
#define COLOR_SHIP      tft.color565(0, 220, 170)
#define COLOR_SHIP_LIGHT tft.color565(0, 255, 200)
#define COLOR_THRUST    tft.color565(255, 150, 50)
#define COLOR_CRYSTAL   tft.color565(100, 255, 200)
#define COLOR_CRYSTAL2  tft.color565(50, 200, 150)
#define COLOR_STAR      tft.color565(255, 255, 255)
#define COLOR_TEXT      tft.color565(255, 255, 255)
#define COLOR_ACCENT    tft.color565(0, 255, 170)
#define COLOR_BAR_BG    tft.color565(30, 30, 50)
#define COLOR_X_AXIS    tft.color565(255, 100, 100)
#define COLOR_Y_AXIS    tft.color565(100, 100, 255)
#define COLOR_SPEED     tft.color565(0, 255, 170)

// ========== ÁREA DE JUEGO ==========
#define GAME_X      10
#define GAME_Y      35
#define GAME_W      220
#define GAME_H      160
#define UI_X        240
#define UI_W        75

// ========== FÍSICA ==========
#define SHIP_SIZE       8
#define MAX_SPEED       3.0
#define DEAD_ZONE       300     // Zona muerta del joystick

// ========== ESTRUCTURAS ==========
struct Ship {
  float x, y;
  float vx, vy;
  float angle;
  float prevX, prevY;
};

struct Crystal {
  float x, y;
  bool collected;
  float rotation;
  uint16_t color;
};

struct Star {
  int x, y;
  uint8_t brightness;
  uint8_t twinkleSpeed;
};

struct Particle {
  float x, y;
  float vx, vy;
  float life;
  uint16_t color;
};

// ========== VARIABLES GLOBALES ==========
Ship ship;
Crystal crystals[8];
Star stars[40];
Particle particles[20];
int particleIndex = 0;

int joyXCenter = 2048;
int joyYCenter = 2048;
float joyX = 0, joyY = 0;  // Valores normalizados -1 a 1

int score = 0;
int totalCrystals = 8;
bool gameWon = false;
unsigned long lastUpdate = 0;
unsigned long gameStartTime = 0;

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 SPACE NAVIGATOR");
  Serial.println("==================\n");
  Serial.println("Demuestra control ANALOGICO del joystick");
  Serial.println("- Velocidad proporcional a inclinacion");
  Serial.println("- Movimiento 360 grados\n");
  
  // Inicializar pantalla
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);
  
  // Inicializar touch
  touch.begin();
  touch.setRotation(1);
  
  // Calibrar joystick
  delay(100);
  joyXCenter = analogRead(JOY_X);
  joyYCenter = analogRead(JOY_Y);
  Serial.printf("Joystick calibrado: X=%d, Y=%d\n", joyXCenter, joyYCenter);
  
  // Inicializar juego
  initGame();
  drawStaticUI();
}

// ========== INIT GAME ==========
void initGame() {
  // Nave al centro
  ship.x = GAME_W / 2;
  ship.y = GAME_H / 2;
  ship.vx = 0;
  ship.vy = 0;
  ship.angle = -90;
  ship.prevX = ship.x;
  ship.prevY = ship.y;
  
  // Generar estrellas
  for (int i = 0; i < 40; i++) {
    stars[i].x = random(GAME_W);
    stars[i].y = random(GAME_H);
    stars[i].brightness = random(100, 255);
    stars[i].twinkleSpeed = random(1, 4);
  }
  
  // Generar cristales
  for (int i = 0; i < 8; i++) {
    crystals[i].x = random(30, GAME_W - 30);
    crystals[i].y = random(30, GAME_H - 30);
    crystals[i].collected = false;
    crystals[i].rotation = random(360);
    // Colores variados cyan-verde
    int hue = random(150, 200);
    crystals[i].color = tft.color565(0, 150 + (hue - 150) * 2, 200 - (hue - 150));
  }
  
  // Limpiar partículas
  for (int i = 0; i < 20; i++) {
    particles[i].life = 0;
  }
  
  score = 0;
  gameWon = false;
  gameStartTime = millis();
  
  // Dibujar área de juego
  tft.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H, COLOR_BG);
  tft.drawRect(GAME_X - 1, GAME_Y - 1, GAME_W + 2, GAME_H + 2, COLOR_ACCENT);
  
  // Dibujar estrellas iniciales
  drawStars();
  
  // Dibujar cristales iniciales
  for (int i = 0; i < 8; i++) {
    drawCrystal(i);
  }
}

// ========== UI ESTÁTICA ==========
void drawStaticUI() {
  // Título
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(1);
  tft.setCursor(90, 8);
  tft.print("ARMakerLab");
  
  // Panel derecho - fondo
  tft.fillRect(UI_X, 30, UI_W, 180, tft.color565(15, 15, 30));
  tft.drawRect(UI_X, 30, UI_W, 180, tft.color565(40, 40, 60));
  
  // Labels
  tft.setTextColor(tft.color565(100, 100, 120));
  tft.setTextSize(1);
  
  tft.setCursor(UI_X + 5, 38);
  tft.print("CRISTALES");
  
  tft.setCursor(UI_X + 5, 75);
  tft.print("VELOCIDAD");
  
  tft.setCursor(UI_X + 5, 115);
  tft.print("JOYSTICK");
  
  // Labels X/Y
  tft.setTextColor(COLOR_X_AXIS);
  tft.setCursor(UI_X + 8, 130);
  tft.print("X");
  
  tft.setTextColor(COLOR_Y_AXIS);
  tft.setCursor(UI_X + 8, 145);
  tft.print("Y");
  
  // Instrucciones abajo
  tft.setTextColor(tft.color565(80, 80, 100));
  tft.setCursor(10, 225);
  tft.print("Mas inclinacion = Mas velocidad");
}

// ========== DIBUJAR ESTRELLAS ==========
void drawStars() {
  static uint8_t twinklePhase = 0;
  twinklePhase++;
  
  for (int i = 0; i < 40; i++) {
    // Calcular brillo con twinkle
    int b = stars[i].brightness;
    b += sin(twinklePhase * 0.1 * stars[i].twinkleSpeed) * 50;
    b = constrain(b, 50, 255);
    
    uint16_t color = tft.color565(b, b, b);
    tft.drawPixel(GAME_X + stars[i].x, GAME_Y + stars[i].y, color);
  }
}

// ========== DIBUJAR CRISTAL ==========
void drawCrystal(int i) {
  if (crystals[i].collected) return;
  
  int cx = GAME_X + (int)crystals[i].x;
  int cy = GAME_Y + (int)crystals[i].y;
  
  // Cristal simple (rombo)
  tft.fillTriangle(cx, cy - 8, cx - 6, cy, cx + 6, cy, crystals[i].color);
  tft.fillTriangle(cx, cy + 8, cx - 6, cy, cx + 6, cy, COLOR_CRYSTAL2);
  
  // Brillo
  tft.drawPixel(cx - 2, cy - 3, COLOR_TEXT);
}

// ========== BORRAR CRISTAL ==========
void eraseCrystal(int i) {
  int cx = GAME_X + (int)crystals[i].x;
  int cy = GAME_Y + (int)crystals[i].y;
  tft.fillRect(cx - 7, cy - 9, 15, 18, COLOR_BG);
}

// ========== DIBUJAR NAVE ==========
void drawShip() {
  int sx = GAME_X + (int)ship.x;
  int sy = GAME_Y + (int)ship.y;
  
  // Convertir ángulo a radianes
  float rad = ship.angle * PI / 180.0;
  float cosA = cos(rad);
  float sinA = sin(rad);
  
  // Puntos del triángulo de la nave
  int x1 = sx + cosA * 10;  // Punta
  int y1 = sy + sinA * 10;
  int x2 = sx + cos(rad + 2.5) * 7;  // Ala izquierda
  int y2 = sy + sin(rad + 2.5) * 7;
  int x3 = sx + cos(rad - 2.5) * 7;  // Ala derecha
  int y3 = sy + sin(rad - 2.5) * 7;
  
  // Dibujar nave
  tft.fillTriangle(x1, y1, x2, y2, x3, y3, COLOR_SHIP);
  
  // Cabina
  tft.fillCircle(sx, sy, 2, tft.color565(0, 80, 80));
  
  // Propulsor si hay movimiento
  float speed = sqrt(joyX * joyX + joyY * joyY);
  if (speed > 0.15) {
    int tx = sx - cosA * 8;
    int ty = sy - sinA * 8;
    int thrustLen = 4 + speed * 6;
    int tx2 = sx - cosA * (8 + thrustLen);
    int ty2 = sy - sinA * (8 + thrustLen);
    
    // Llama del propulsor
    tft.drawLine(tx, ty, tx2, ty2, COLOR_THRUST);
    tft.drawLine(tx - 1, ty, tx2, ty2, tft.color565(255, 200, 100));
  }
}

// ========== BORRAR NAVE ==========
void eraseShip() {
  int sx = GAME_X + (int)ship.prevX;
  int sy = GAME_Y + (int)ship.prevY;
  tft.fillCircle(sx, sy, 14, COLOR_BG);
}

// ========== ACTUALIZAR PARTÍCULAS ==========
void updateParticles(float dt) {
  for (int i = 0; i < 20; i++) {
    if (particles[i].life > 0) {
      // Borrar
      int px = GAME_X + (int)particles[i].x;
      int py = GAME_Y + (int)particles[i].y;
      tft.fillRect(px - 2, py - 2, 5, 5, COLOR_BG);
      
      // Actualizar
      particles[i].x += particles[i].vx * dt * 60;
      particles[i].y += particles[i].vy * dt * 60;
      particles[i].life -= dt * 3;
      
      // Dibujar si aún vive
      if (particles[i].life > 0) {
        px = GAME_X + (int)particles[i].x;
        py = GAME_Y + (int)particles[i].y;
        int size = particles[i].life * 4;
        if (size > 0 && px > GAME_X && px < GAME_X + GAME_W && py > GAME_Y && py < GAME_Y + GAME_H) {
          tft.fillCircle(px, py, size, particles[i].color);
        }
      }
    }
  }
}

// ========== SPAWN PARTÍCULA ==========
void spawnParticle(float x, float y, float vx, float vy, uint16_t color) {
  particles[particleIndex].x = x;
  particles[particleIndex].y = y;
  particles[particleIndex].vx = vx;
  particles[particleIndex].vy = vy;
  particles[particleIndex].life = 1.0;
  particles[particleIndex].color = color;
  particleIndex = (particleIndex + 1) % 20;
}

// ========== EXPLOSIÓN DE CRISTAL ==========
void crystalExplosion(int i) {
  for (int j = 0; j < 8; j++) {
    float angle = j * PI / 4;
    spawnParticle(
      crystals[i].x,
      crystals[i].y,
      cos(angle) * 2,
      sin(angle) * 2,
      crystals[i].color
    );
  }
}

// ========== LEER JOYSTICK ==========
void readJoystick() {
  int rawX = analogRead(JOY_X);
  int rawY = analogRead(JOY_Y);
  
  // Normalizar a -1 a 1 (EJES INTERCAMBIADOS E INVERTIDOS)
  float dx = -(rawY - joyYCenter) / 2048.0;  // VRY controla X (invertido)
  float dy = -(rawX - joyXCenter) / 2048.0;  // VRX controla Y (invertido)
  
  // Aplicar zona muerta
  float magnitude = sqrt(dx * dx + dy * dy);
  float deadZoneNorm = DEAD_ZONE / 2048.0;
  
  if (magnitude < deadZoneNorm) {
    joyX = 0;
    joyY = 0;
  } else {
    // Escalar para que empiece desde 0 después de la zona muerta
    float scale = (magnitude - deadZoneNorm) / (1.0 - deadZoneNorm);
    scale = constrain(scale, 0, 1);
    joyX = (dx / magnitude) * scale;
    joyY = (dy / magnitude) * scale;
  }
  
  // Limitar
  joyX = constrain(joyX, -1.0, 1.0);
  joyY = constrain(joyY, -1.0, 1.0);
}

// ========== ACTUALIZAR UI ==========
void updateUI() {
  // Score
  tft.fillRect(UI_X + 5, 50, 65, 18, tft.color565(15, 15, 30));
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(UI_X + 15, 52);
  tft.print(score);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.print("/8");
  
  // Barra de velocidad
  float speed = sqrt(joyX * joyX + joyY * joyY);
  int barWidth = speed * 60;
  tft.fillRect(UI_X + 5, 88, 65, 10, COLOR_BAR_BG);
  if (barWidth > 0) {
    uint16_t speedColor = speed > 0.7 ? tft.color565(255, 200, 50) : COLOR_SPEED;
    tft.fillRect(UI_X + 5, 88, barWidth, 10, speedColor);
  }
  tft.drawRect(UI_X + 5, 88, 65, 10, tft.color565(60, 60, 80));
  
  // Porcentaje de velocidad
  tft.fillRect(UI_X + 5, 100, 65, 10, tft.color565(15, 15, 30));
  tft.setTextColor(COLOR_SPEED);
  tft.setCursor(UI_X + 25, 101);
  tft.print((int)(speed * 100));
  tft.print("%");
  
  // Barras X/Y
  int barCenterX = UI_X + 40;
  
  // Barra X
  tft.fillRect(UI_X + 18, 130, 50, 8, COLOR_BAR_BG);
  int xBarLen = abs(joyX) * 25;
  if (xBarLen > 0) {
    if (joyX > 0) {
      tft.fillRect(barCenterX, 130, xBarLen, 8, COLOR_X_AXIS);
    } else {
      tft.fillRect(barCenterX - xBarLen, 130, xBarLen, 8, COLOR_X_AXIS);
    }
  }
  tft.drawRect(UI_X + 18, 130, 50, 8, tft.color565(60, 60, 80));
  
  // Barra Y
  tft.fillRect(UI_X + 18, 145, 50, 8, COLOR_BAR_BG);
  int yBarLen = abs(joyY) * 25;
  if (yBarLen > 0) {
    if (joyY > 0) {
      tft.fillRect(barCenterX, 145, yBarLen, 8, COLOR_Y_AXIS);
    } else {
      tft.fillRect(barCenterX - yBarLen, 145, yBarLen, 8, COLOR_Y_AXIS);
    }
  }
  tft.drawRect(UI_X + 18, 145, 50, 8, tft.color565(60, 60, 80));
  
  // Valores numéricos X/Y
  tft.fillRect(UI_X + 5, 160, 65, 22, tft.color565(15, 15, 30));
  
  tft.setTextColor(COLOR_X_AXIS);
  tft.setCursor(UI_X + 8, 162);
  tft.print("X:");
  tft.print(joyX >= 0 ? "+" : "");
  tft.print(joyX, 2);
  
  tft.setTextColor(COLOR_Y_AXIS);
  tft.setCursor(UI_X + 8, 173);
  tft.print("Y:");
  tft.print(joyY >= 0 ? "+" : "");
  tft.print(joyY, 2);
}

// ========== PANTALLA DE VICTORIA ==========
void drawWinScreen() {
  tft.fillRect(GAME_X + 30, GAME_Y + 50, 160, 60, tft.color565(10, 40, 30));
  tft.drawRect(GAME_X + 30, GAME_Y + 50, 160, 60, COLOR_ACCENT);
  
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(GAME_X + 40, GAME_Y + 60);
  tft.print("COMPLETADO!");
  
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(GAME_X + 50, GAME_Y + 85);
  
  unsigned long tiempo = (millis() - gameStartTime) / 1000;
  tft.print("Tiempo: ");
  tft.print(tiempo);
  tft.print("s");
  
  tft.setCursor(GAME_X + 45, GAME_Y + 100);
  tft.print("Toca para reiniciar");
}

// ========== LOOP PRINCIPAL ==========
void loop() {
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 1000.0;
  if (dt < 0.016) return;  // ~60 FPS max
  lastUpdate = now;
  
  // Verificar touch para reiniciar
  if (touch.touched()) {
    delay(200);
    initGame();
    drawStaticUI();
    return;
  }
  
  if (gameWon) {
    return;
  }
  
  // Leer joystick
  readJoystick();
  
  // Calcular magnitud (velocidad)
  float magnitude = sqrt(joyX * joyX + joyY * joyY);
  
  // Guardar posición anterior
  ship.prevX = ship.x;
  ship.prevY = ship.y;
  
  // Aplicar movimiento - VELOCIDAD PROPORCIONAL
  if (magnitude > 0.05) {
    float speed = magnitude * MAX_SPEED;
    ship.x += joyX * speed;
    ship.y += joyY * speed;
    
    // Calcular ángulo hacia dirección de movimiento
    ship.angle = atan2(joyY, joyX) * 180.0 / PI;
    
    // Generar partículas del motor ocasionalmente
    if (random(100) < magnitude * 40) {
      float thrustAngle = (ship.angle + 180) * PI / 180.0;
      spawnParticle(
        ship.x,
        ship.y,
        cos(thrustAngle) * 1.5 + (random(100) - 50) / 100.0,
        sin(thrustAngle) * 1.5 + (random(100) - 50) / 100.0,
        COLOR_THRUST
      );
    }
  }
  
  // Wrap around (aparecer del otro lado)
  if (ship.x < -5) ship.x = GAME_W + 5;
  if (ship.x > GAME_W + 5) ship.x = -5;
  if (ship.y < -5) ship.y = GAME_H + 5;
  if (ship.y > GAME_H + 5) ship.y = -5;
  
  // Detectar colisión con cristales
  for (int i = 0; i < 8; i++) {
    if (!crystals[i].collected) {
      float dx = ship.x - crystals[i].x;
      float dy = ship.y - crystals[i].y;
      float dist = sqrt(dx * dx + dy * dy);
      
      if (dist < 15) {
        crystals[i].collected = true;
        score++;
        eraseCrystal(i);
        crystalExplosion(i);
        
        Serial.printf("Cristal %d recolectado! Score: %d/8\n", i + 1, score);
        
        if (score >= 8) {
          gameWon = true;
          Serial.println("\n¡VICTORIA! Todos los cristales recolectados.");
        }
      }
    }
  }
  
  // Actualizar partículas
  updateParticles(dt);
  
  // Redibujar estrellas (twinkle)
  static int starCounter = 0;
  if (++starCounter >= 5) {
    starCounter = 0;
    drawStars();
  }
  
  // Borrar y dibujar nave
  eraseShip();
  drawShip();
  
  // Actualizar UI
  updateUI();
  
  // Mostrar victoria
  if (gameWon) {
    drawWinScreen();
  }
  
  // Debug serial
  static int debugCounter = 0;
  if (++debugCounter >= 30) {
    debugCounter = 0;
    Serial.printf("Joy: X=%.2f Y=%.2f | Vel: %.0f%% | Pos: %.0f,%.0f\n", 
                  joyX, joyY, magnitude * 100, ship.x, ship.y);
  }
}
