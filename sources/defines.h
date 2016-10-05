#ifndef DEFINES_H
#define DEFINES_H

#ifdef UNIX
#include <unistd.h> // for write syscall
void _write(char *buf, size_t nbyte) {
  write(1, buf, nbyte);
}
#else
#ifndef UJKL_CUSTOM_WRITE
#include <Arduino.h> // for Serial obj
void _write(char *buf, size_t nbyte) {
  Serial.write(buf, nbyte);
  yield();
}
#endif // UJKL_CUSTOM_WRITE
#endif // UNIX

#endif // DEFINES_H