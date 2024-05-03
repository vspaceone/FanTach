uint16_t fan_rpm[4] = { 0, 0, 0, 0 };
uint8_t fan_ticks[4] = { 0, 0, 0, 0 };

bool get_bit(b, n) {
  return b & (1 << n);
}
void set_bit(b, n, x) {
  if (x) b |= (1 << n);
  else b &= !(1 << n);
}
void inv_bit(b, n) {
  b ^= (1 << n);
}

byte last_pin_states = 0x00;
ISR(PCINT0_vect) {
  if (get_bit(last_pin_states, 0) != digitalRead(PA0)) {
    inv_bit(last_pin_states, 0);
    if (fan_ticks[0] < 0xFF) fan_ticks[0] += 1;
  }
  if (get_bit(last_pin_states, 1) != digitalRead(PA1)) {
    inv_bit(last_pin_states, 1);
    if (fan_ticks[1] < 0xFF) fan_ticks[1] += 1;
  }
  if (get_bit(last_pin_states, 2) != digitalRead(PA2)) {
    inv_bit(last_pin_states, 2);
    if (fan_ticks[2] < 0xFF) fan_ticks[2] += 1;
  }
  if (get_bit(last_pin_states, 3) != digitalRead(PA3)) {
    inv_bit(last_pin_states, 3);
    if (fan_ticks[3] < 0xFF) fan_ticks[3] += 1;
  }
}
