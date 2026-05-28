/*
 * OpenCode Traffic Light - Arduino Uno Firmware
 * ==============================================
 *
 * 黄灯 = OpenCode 正在干活
 * 绿灯 = 任务完成
 * 红灯 = 需要用户确认/授权 / 出错
 *
 * 接线:
 *   红色 LED (阳极) -> D9  (PWM) 串 220Ω 电阻 -> GND
 *   黄色 LED (阳极) -> D10 (PWM) 串 220Ω 电阻 -> GND
 *   绿色 LED (阳极) -> D11 (PWM) 串 220Ω 电阻 -> GND
 *   蜂鸣器(可选)    -> D8  -> GND
 */

// ===================== 引脚定义 =====================
#define RED_PIN      9
#define YELLOW_PIN   10
#define GREEN_PIN    11
#define BUZZER_PIN   8

// ===================== 时序参数 (ms) =====================
#define BLINK_ON_MS      400
#define BLINK_OFF_MS     400
#define PULSE_STEP_MS    12
#define PULSE_DELTA      5
#define BEEP_FREQ        880
#define BEEP_ON_MS       150
#define BEEP_GAP_MS      100

// ===================== 串口缓冲 =====================
#define CMD_BUF_SIZE   32

// ===================== LED 状态机 =====================
enum LedMode {
  MODE_OFF,
  MODE_SOLID,
  MODE_BLINK,
  MODE_PULSE
};

struct Led {
  uint8_t   pin;
  LedMode   mode;
  uint8_t   brightness;
  uint8_t   target;
  bool      blinkState;
  int       pulseDir;
  unsigned long lastTick;
};

Led leds[3];  // 0=Red, 1=Yellow, 2=Green

// ===================== 蜂鸣器状态 =====================
struct Beeper {
  bool      active;
  int       remaining;
  bool      isOn;
  unsigned long lastTick;
} beeper;

// ===================== 串口命令缓冲 =====================
char cmdBuf[CMD_BUF_SIZE];
uint8_t cmdIdx = 0;

// ===================== 初始化 =====================
void setup() {
  // 初始化 LED
  ledInit(0, RED_PIN);
  ledInit(1, YELLOW_PIN);
  ledInit(2, GREEN_PIN);

  // 初始化蜂鸣器
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
  beeper.active    = false;
  beeper.remaining = 0;
  beeper.isOn      = false;

  // 开机自检：三灯依次快闪
  startupSequence();

  Serial.begin(115200);
  while (!Serial);  // 等待串口就绪（原生 USB 的板子需要）

  Serial.println(F("RDY"));
}

void ledInit(uint8_t idx, uint8_t pin) {
  pinMode(pin, OUTPUT);
  leds[idx].pin        = pin;
  leds[idx].mode       = MODE_OFF;
  leds[idx].brightness = 0;
  leds[idx].target     = 255;
  leds[idx].blinkState = false;
  leds[idx].pulseDir   = 1;
  leds[idx].lastTick   = 0;
  analogWrite(pin, 0);
}

void startupSequence() {
  // 快速自检：三灯同时亮 60ms（避免长时间闪烁干扰正常使用）
  analogWrite(RED_PIN,    128);
  analogWrite(YELLOW_PIN, 128);
  analogWrite(GREEN_PIN,  128);
  delay(60);
  analogWrite(RED_PIN,    0);
  analogWrite(YELLOW_PIN, 0);
  analogWrite(GREEN_PIN,  0);

  leds[0].mode = MODE_OFF; leds[0].brightness = 0;
  leds[1].mode = MODE_OFF; leds[1].brightness = 0;
  leds[2].mode = MODE_OFF; leds[2].brightness = 0;
}

// ===================== 主循环 =====================
void loop() {
  readSerial();
  updateAllLeds();
  updateBeeper();
}

// ===================== 串口读取 =====================
void readSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdIdx > 0) {
        cmdBuf[cmdIdx] = '\0';
        dispatch(cmdBuf);
        cmdIdx = 0;
      }
    } else if (cmdIdx < CMD_BUF_SIZE - 1 && c >= ' ') {
      cmdBuf[cmdIdx++] = c;
    }
  }
}

// ===================== 命令分发 =====================
void dispatch(const char* cmd) {
  if (strlen(cmd) == 0) return;

  char head = cmd[0];
  switch (head) {
    case 'R': cmdRed(cmd);     break;
    case 'Y': cmdYellow(cmd);  break;
    case 'G': cmdGreen(cmd);   break;
    case 'O': cmdOff();        break;
    case 'B': cmdBeep(cmd);    break;
    case 'S': cmdStatus();     break;
    case '?': printHelp();     return;
    default:
      Serial.print(F("ERR:unknown cmd '"));
      Serial.print(head);
      Serial.println(F("'"));
      return;
  }
  Serial.println(F("OK"));
}

// ---- 红色命令 ----
void cmdRed(const char* cmd) {
  size_t len = strlen(cmd);
  if (len == 1) {
    setLed(0, MODE_SOLID, 255);
  } else {
    char sub = cmd[1];
    if (sub == 'B') {
      setLed(0, MODE_BLINK, 255);
    } else if (sub == 'P') {
      setLed(0, MODE_PULSE, 255);
    } else if (sub >= '0' && sub <= '9') {
      uint8_t b = (uint8_t)(sub - '0') * 28;
      if (b == 0 && sub == '0') b = 0;
      setLed(0, MODE_SOLID, b);
    }
  }
}

// ---- 黄色命令 ----
void cmdYellow(const char* cmd) {
  size_t len = strlen(cmd);
  if (len == 1) {
    setLed(1, MODE_SOLID, 255);
  } else {
    char sub = cmd[1];
    if (sub == 'B') {
      setLed(1, MODE_BLINK, 255);
    } else if (sub == 'P') {
      setLed(1, MODE_PULSE, 255);
    } else if (sub >= '0' && sub <= '9') {
      uint8_t b = (uint8_t)(sub - '0') * 28;
      if (b == 0 && sub == '0') b = 0;
      setLed(1, MODE_SOLID, b);
    }
  }
}

// ---- 绿色命令 ----
void cmdGreen(const char* cmd) {
  size_t len = strlen(cmd);
  if (len == 1) {
    setLed(2, MODE_SOLID, 255);
  } else {
    char sub = cmd[1];
    if (sub == 'B') {
      setLed(2, MODE_BLINK, 255);
    } else if (sub == 'P') {
      setLed(2, MODE_PULSE, 255);
    } else if (sub >= '0' && sub <= '9') {
      uint8_t b = (uint8_t)(sub - '0') * 28;
      if (b == 0 && sub == '0') b = 0;
      setLed(2, MODE_SOLID, b);
    }
  }
}

// ---- 全关 ----
void cmdOff() {
  allOff();
}

// ---- 蜂鸣 ----
void cmdBeep(const char* cmd) {
  // 格式: B:n  (n = 次数, 1-20)
  if (strlen(cmd) < 3 || cmd[1] != ':') {
    Serial.println(F("ERR:format B:n"));
    return;
  }
  int count = atoi(cmd + 2);
  if (count < 1)  count = 1;
  if (count > 20) count = 20;
  beeper.active    = true;
  beeper.remaining = count;
  beeper.isOn      = false;
  beeper.lastTick  = 0;
}

// ---- 状态查询 ----
void cmdStatus() {
  const char* modeNames[] = {"OFF", "SOLID", "BLINK", "PULSE"};
  Serial.print(F("R:"));
  Serial.print(modeNames[leds[0].mode]);
  Serial.print(F(" Y:"));
  Serial.print(modeNames[leds[1].mode]);
  Serial.print(F(" G:"));
  Serial.print(modeNames[leds[2].mode]);
  Serial.print(F(" BEEP:"));
  Serial.println(beeper.active ? beeper.remaining : 0);
}

void printHelp() {
  Serial.println(F("--- OpenCode Light ---"));
  Serial.println(F("R/Y/G     = solid on"));
  Serial.println(F("R/Y/G 0-9 = brightness"));
  Serial.println(F("R/Y/G B   = blink"));
  Serial.println(F("R/Y/G P   = pulse"));
  Serial.println(F("O         = all off"));
  Serial.println(F("B:n       = beep n times"));
  Serial.println(F("S         = status"));
  Serial.println(F("?         = this help"));
}

// ===================== LED 控制 =====================
void setLed(uint8_t idx, LedMode mode, uint8_t target) {
  leds[idx].mode   = mode;
  leds[idx].target  = target == 0 ? 255 : target;

  switch (mode) {
    case MODE_OFF:
      leds[idx].brightness = 0;
      analogWrite(leds[idx].pin, 0);
      break;
    case MODE_SOLID:
      leds[idx].brightness = leds[idx].target;
      analogWrite(leds[idx].pin, leds[idx].brightness);
      break;
    case MODE_BLINK:
      leds[idx].blinkState = true;
      leds[idx].brightness = leds[idx].target;
      analogWrite(leds[idx].pin, leds[idx].brightness);
      leds[idx].lastTick = millis();
      break;
    case MODE_PULSE:
      leds[idx].brightness = 0;
      leds[idx].pulseDir   = 1;
      analogWrite(leds[idx].pin, 0);
      leds[idx].lastTick = millis();
      break;
  }
}

void allOff() {
  setLed(0, MODE_OFF, 0);
  setLed(1, MODE_OFF, 0);
  setLed(2, MODE_OFF, 0);
}

// ===================== LED 更新 (非阻塞) =====================
void updateAllLeds() {
  updateLed(0);
  updateLed(1);
  updateLed(2);
}

void updateLed(uint8_t idx) {
  Led& l = leds[idx];
  unsigned long now = millis();

  switch (l.mode) {
    case MODE_OFF:
    case MODE_SOLID:
      break;  // 静态，无需更新

    case MODE_BLINK: {
      unsigned long interval = l.blinkState ? BLINK_ON_MS : BLINK_OFF_MS;
      if (now - l.lastTick >= interval) {
        l.lastTick   = now;
        l.blinkState = !l.blinkState;
        uint8_t val = l.blinkState ? l.target : 0;
        l.brightness = val;
        analogWrite(l.pin, val);
      }
      break;
    }

    case MODE_PULSE:
      if (now - l.lastTick >= PULSE_STEP_MS) {
        l.lastTick = now;
        int b = (int)l.brightness + l.pulseDir * PULSE_DELTA;
        if (b >= 255) {
          b = 255;
          l.pulseDir = -1;
        } else if (b <= 0) {
          b = 0;
          l.pulseDir = 1;
        }
        l.brightness = (uint8_t)b;
        analogWrite(l.pin, l.brightness);
      }
      break;
  }
}

// ===================== 蜂鸣器更新 (非阻塞) =====================
void updateBeeper() {
  if (!beeper.active || beeper.remaining <= 0) {
    if (beeper.isOn) {
      noTone(BUZZER_PIN);
      beeper.isOn = false;
      beeper.active = false;
    }
    return;
  }

  unsigned long now = millis();
  if (beeper.lastTick == 0) beeper.lastTick = now;

  if (beeper.isOn) {
    // 当前正在响，等 BEEP_ON_MS 后关闭
    if (now - beeper.lastTick >= BEEP_ON_MS) {
      noTone(BUZZER_PIN);
      beeper.isOn     = false;
      beeper.lastTick = now;
      beeper.remaining--;
    }
  } else {
    // 当前静音，等 BEEP_GAP_MS 后开启下一声
    if (beeper.remaining > 0 && now - beeper.lastTick >= BEEP_GAP_MS) {
      tone(BUZZER_PIN, BEEP_FREQ);
      beeper.isOn     = true;
      beeper.lastTick = now;
    }
    if (beeper.remaining <= 0) {
      beeper.active = false;
    }
  }
}
