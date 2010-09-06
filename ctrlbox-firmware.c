/*
  ctrlbox-firmware
  Copyright 2009, Jonatan Liljedahl
  
  Firmware for Arduino-based USB controller

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with This program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <WProgram.h>

#define BAUDRATE 500000
//#define BAUDRATE 115200
//#define BAUDRATE 57600

int i,v,x = 0;
int accum[6] = {0,0,0,0,0,0};
int dval[10];
int tmr[10];

void setup() {
    for(i = 0; i < 10; ++i)
        pinMode(i + 2, INPUT);
    pinMode(13, OUTPUT);
    beginSerial(BAUDRATE);
}

int smooth(int data, int *accum, long filterVal) {
    *accum = ((data * filterVal) / 1000) + ((*accum * (1000 - filterVal) / 1000));
    return *accum;
}

void loop() {
    for(i = 0; i < 6; ++i) {
        int old = accum[i];
        v = analogRead(i);
        v = smooth(v, &accum[i], 100);
        if(v != old) {
            serialWrite(0x80 | i);
            serialWrite(v >> 7);
            serialWrite(v & 0x7F);
            x = 5;
        }
    }

    for(i = 0; i < 10; ++i) {
        v = digitalRead(i + 2);
        if(v != dval[i] && tmr[i] == 0) {
            serialWrite(0xC0 | i);
            serialWrite(v);
            tmr[i] = 10;
            x = 5;
            dval[i] = v;
        }
        if(tmr[i] > 0) --tmr[i];
    }

    if(x>1) {
        digitalWrite(13, HIGH);
        --x;
    } else if(x==1) {
        digitalWrite(13, LOW);
        x = 0;
    }
}

int main(void) {
	init();
	setup();

	for (;;)
		loop();

	return 0;
}

