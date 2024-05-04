#include <avr/io.h>
#include <avr/wdt.h>
#include <EEPROM.h>

#include "tss.h"
#undef USE_SOFTWARE_SERIAL

const uint8_t INIT_TIMEOUT = 15;       //seconds
const uint8_t PROBLEM_WAIT_TIME = 60;  //sec
const uint16_t ERROR_RPM = 50;         // rpm/10
const uint16_t OK_RPM = 60;            // rpm/10


#ifdef PINMAPPING_CCW
#error "Sketch was written for clockwise pin mapping!"
#endif
const uint8_t FAN_TACH_PINS[4] = { PA0, PA1, PA2, PA3 };  //dont forget to change PCMSK in fan_tach.h
const uint8_t FAN_PWM_PINS[4] = { PA5, PA6, PA7, 8 + PB2 };
const uint8_t LED = 8 + PB3;
const uint8_t PS_ON = PA4;
//PB0 and PB1 are UART

#include "fan_tach.h"
#include "fan_pwm.h"


enum byte_meanings_e {
  EOM = 0,       //end of messange
  BOOT = 1,      //void
  STATE = 2,     //uint8_t state
  FAN_TACH = 3,  //uint8_t fan ID | uint16_t RPM
  FAN_PWM = 4    //uint8_t fan ID | uint8_t PWM
};

enum states_e {
  INIT = 0,  //waits for the PSU and fans to come online
  OK = 1,
  PROBLEM = 2,
  ERROR = 3
};

void setup() {
  wdt_enable(WDTO_1S);
  wdt_reset();

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(PS_ON, OUTPUT);
  digitalWrite(PS_ON, HIGH);

  TinySer.begin(1200);
  TinySer.write(BOOT);

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(FAN_TACH_PINS[i], INPUT_PULLUP);
    pinMode(FAN_PWM_PINS[i], OUTPUT);
  }

  setup_fan_tach();
  setup_fan_pwm();

  run_state_machine();  //only runs INIT step

  TinySer.write(EOM);
  wdt_reset();
}

uint8_t machine_state = INIT;

void send_state() {
  //State
  TinySer.write(STATE);
  TinySer.write(machine_state);
  TinySer.write(EOM);
  //Fan Speeds
  for (uint8_t i = 0; i < 4; i++) {
    //RPM
    TinySer.write(FAN_TACH);
    TinySer.write(i);
    TinySer.write(fan_rpm[i] & 0xFF);
    TinySer.write((fan_rpm[i] >> 8) & 0xFF);
    TinySer.write(EOM);
    //current PWM
    TinySer.write(FAN_PWM);
    TinySer.write(i);
    TinySer.write(fan_pwm_pct[i]);
    TinySer.write(EOM);
  }
}

uint32_t problem_begin_ms = 0;
void run_state_machine() {
  switch (machine_state) {
    case INIT:
      if (millis() > INIT_TIMEOUT * 1000) machine_state = OK;
      break;
    case OK:
      {
        if (fans_below()) {
          problem_begin_ms = millis();
          machine_state = PROBLEM;
        }

        digitalWrite(LED, HIGH);
        digitalWrite(PS_ON, LOW);
      }
      break;
    case PROBLEM:
      {
        if (!fans_below()) machine_state = OK;
        if (millis() - problem_begin_ms > (uint32_t)PROBLEM_WAIT_TIME * 1000) machine_state = ERROR;

        digitalWrite(PS_ON, HIGH);
        digitalWrite(PS_ON, LOW);
      }
      break;
    case ERROR:
      {
        if (!fans_below()) machine_state = OK;

        digitalWrite(LED, LOW);
        digitalWrite(LED, (millis() >> 10) & 1);  //blink 512ms
      }
      break;
  }
}

void loop() {
  static uint32_t last_fan_calc_ms = 0;
  uint32_t fan_calc_delta = millis() - last_fan_calc_ms;
  if (fan_calc_delta > 5000) {
    last_fan_calc_ms += fan_calc_delta;
    calc_fan_speed(fan_calc_delta);
    send_state();
  }
  run_state_machine();

  wdt_reset();
  delay(10);  //give the interrupts some time
  wdt_reset();
}
