uint8_t fan_target_speed_pct[4] = { 0, 0, 0, 0 };  //0...100


const uint8_t PWM_STEPS = 10;
uint8_t pwm_step = 0;  //0...PWM_STEPS-1

constexpr uint8_t PWM_STEPS_PER_PCT = (PWM_STEPS / 100);
inline void check_fan_pwm_step(uint8_t fan_id, uint8_t pin) {
  uint8_t max_step = fan_target_speed_pct[fan_id] * PWM_STEPS_PER_PCT;
  digitalWrite(pin, pwm_step < max_step);
}
ISR() {
  check_fan_pwm_step(0, PA5);
  check_fan_pwm_step(1, PA6);
  check_fan_pwm_step(2, PA7);
  check_fan_pwm_step(3, PB2);
  if (pwm_step >= PWM_STEPS) pwm_step = 0;
  else pwm_step++;
}

void setup_fan_pwm() {  //setup timer ~~with roughly 25kHz~~
}
