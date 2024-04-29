uint8_t fan_ticks[4] = { 0, 0, 0, 0 };

ISR(PCINT0_vect) {
  if (fan_ticks[0] < 0xFF) fan_ticks[0] += 1;
}
ISR(PCINT1_vect) {
  if (fan_ticks[1] < 0xFF) fan_ticks[1] += 1;
}
ISR(PCINT2_vect) {
  if (fan_ticks[2] < 0xFF) fan_ticks[2] += 1;
}
ISR(PCINT3_vect) {
  if (fan_ticks[3] < 0xFF) fan_ticks[3] += 1;
}
