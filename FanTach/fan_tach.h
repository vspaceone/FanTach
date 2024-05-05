#include <EEPROM.h>

const uint8_t TICKS_PER_ROTATION = 4;
#define FAN_MON_EN_ADDR 0

uint16_t fan_rpm[4] = { 0, 0, 0, 0 };  //rpm/10
uint8_t fan_ticks[4] = { 0, 0, 0, 0 };
byte fan_mon_enabled = 0xFF;

#define bitFlip(b, n) b ^= (1 << n)

/*
  dividing ticks by time since last check is quite the crude alg.
  with quite the high response time
  but probably works better with a shared interrupt
  than trying to calculate the time delta between fan ticks
*/
void calc_fan_speed(uint16_t delta_ms) {
  const uint16_t fac = (1000 / delta_ms) * 6;
  for (uint8_t i = 0; i < 4; i++) {
    fan_rpm[i] = fan_ticks[i] * fac;
  }
}

bool fans_below(uint16_t rpm) {
  for (uint8_t i = 0; i < 4; i++)
    if (bitRead(fan_mon_enabled, i))
      if (fan_rpm[i] < rpm) return true;
  return false;
}

//DO NOT execute often, writes to EEPROM
void detect_fans() {
  for (uint8_t i = 0; i < 4; i++) bitWrite(fan_mon_enabled, i, fan_rpm[i] > ERROR_RPM);
  EEPROM.write(FAN_MON_EN_ADDR, fan_mon_enabled);

  softSerialWrite(FAN_MON);
  softSerialWrite(fan_mon_enabled);
  softSerialWrite(EOM);
}

//speed meas.
byte last_pin_states = 0x00;
inline void check_fan_input(uint8_t fan_id, uint8_t pin) {
  if (bitRead(last_pin_states, fan_id) != digitalRead(pin)) {
    bitFlip(last_pin_states, fan_id);
    if (fan_ticks[fan_id] < 0xFF) fan_ticks[fan_id] += 1;
  }
}
ISR(PCINT0_vect) {
  for (uint8_t i = 0; i < 4; i++) check_fan_input(i, FAN_TACH_PINS[i]);
}

void setup_fan_tach() {
  fan_mon_enabled = EEPROM.read(FAN_MON_EN_ADDR);

  noInterrupts();
  //        PA7......PA0
  PCMSK0 |= 0b00001111;  //enable PCINT0 for pins PA0 to PA3
  GIMSK |= 1 << PCIE0;   //enable PCINT0
  interrupts();
}
