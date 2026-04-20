int f1() {
  return -5;
}
long checksum = 100;
int main() {
  checksum = checksum + f1();
  if (checksum != 95) return 1;
  return 0;
}
