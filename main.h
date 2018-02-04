#include "lib/i2c.h"
#include "lib/serial.h"
#include "lib/analog.h"
#include "lib/serial-dmx.h"

#define DISPLAY 0x3B
#define KEYPAD 0x21

//extern char labels[];

extern I2C i2c;
extern DMX dmx;
extern Serial pc;
extern AnalogIn ain;
extern AnalogOut aout;
