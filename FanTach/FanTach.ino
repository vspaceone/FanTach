#include "uart.h"
#include "fan_tach.h"



const uint8_t INIT_TIMEOUT = 15;  //seconds
const uint8_t PROBLEM_WAIT_TIME = 60;
const uint16_t ERROR_RPM = 500;
const uint16_t OK_RPM = 600;

const uint8_t LED = PB3;


enum byte_meanings_e {
  EOM = 0,    //end of messange
  BOOT = 1,   //void
  STATE = 2,  //uint8_t state
  FAN_TACH,   //uint8_t fan ID | uint16_t RPM
  FAN_PWM     //uint8_t fan ID | uint8_t PWM
};

enum states_e {
  INIT = 0,  //waits for the PSU and fans to come online
  OK = 1,
  PROBLEM = 2,
  ERROR = 3
};

void setup() {
  UART_init();
  UART_tx(BOOT);

  pinMode(PA0, INPUT_PULLUP);
  pinMode(PA1, INPUT_PULLUP);
  pinMode(PA2, INPUT_PULLUP);
  pinMode(PA3, INPUT_PULLUP);


  pinMode(LED, OUTPUT);


  UART_tx(EOM);
}

uint8_t machine_state = INIT;

void send_state() {
  //State
  UART_tx(STATE);
  UART_tx(machine_state);
  UART_tx(EOM);
}

bool fans_below() {
  for (uint8_t i = 0; i < 4; i++)
    if (fan_rpm[i] < ERROR_RPM) return true;
  return false;
}

uint32_t problem_begin_ms = 0;
void loop() {
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
      }
      break;
    case PROBLEM:
      {
        if (!fans_below()) machine_state = OK;
        if (millis() - problem_begin_ms > PROBLEM_WAIT_TIME * 1000)
      }
      break;
    case ERROR:
      {
        if (!fans_below()) machine_state = OK;
      }
      break;
  }

  static uint32_t last_state_ms = 0;
  if (millis() - last_state_ms > 1000) {
    send_state();
    last_state_ms = millis();
  }
}
