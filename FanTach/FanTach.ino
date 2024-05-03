#include "uart/SoftwareSerial.h"

const uint8_t INIT_TIMEOUT = 15;       //seconds
const uint8_t PROBLEM_WAIT_TIME = 60;  //sec
const uint16_t ERROR_RPM = 50;         // rpm/10
const uint16_t OK_RPM = 60;            // rpm/10

const uint8_t FAN_TACH_PINS[4] = { PA0, PA1, PA2, PA3 };  //dont forget to change PCMSK in fan_tach.h
const uint8_t FAN_PWM_PINS[4] = { PA5, PA6, PA7, PB2 };
const uint8_t LED = PB3;
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
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(PS_ON, OUTPUT);
  digitalWrite(PS_ON, HIGH);

  softSerialBegin();
  softSerialWrite(BOOT);

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(FAN_TACH_PINS[i], INPUT_PULLUP);
    pinMode(FAN_PWM_PINS[i], OUTPUT);
  }

  setup_fan_tach();
  setup_fan_pwm();

  softSerialWrite(EOM);


  run_state_machine();  //only runs INIT step
}

uint8_t machine_state = INIT;

void send_state() {
  //State
  softSerialWrite(STATE);
  softSerialWrite(machine_state);
  softSerialWrite(EOM);
  //Fan Speeds
  for (uint8_t i = 0; i < 4; i++) {
    //RPM
    softSerialWrite(FAN_TACH);
    softSerialWrite(i);
    softSerialWrite(fan_rpm[i] & 0xFF);
    softSerialWrite((fan_rpm[i] >> 8) & 0xFF);
    softSerialWrite(EOM);
    //current PWM
    softSerialWrite(FAN_PWM);
    softSerialWrite(i);
    softSerialWrite(fan_pwm_pct[i]);
    softSerialWrite(EOM);
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
  yield();
}
