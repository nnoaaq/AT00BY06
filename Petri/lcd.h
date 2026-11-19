#ifndef LCD_H
#define LCD_H

#include <stdbool.h>
#include <stdint.h>

// 16x2-merkkinäyttö I2C-laajentimen (PCF8574) kautta.

// Alustaa I2C-väylän ja näytön. Palauttaa false, jos näyttö ei vastaa.
// Jos alustus epäonnistuu, muut lcd_-funktiot eivät tee mitään.
bool lcd_init(void);

// Kirjoittaa tekstin riville (0 tai 1). Teksti katkaistaan 16 merkkiin ja
// loppu täytetään tyhjillä.
void lcd_write_line(unsigned line, const char *text);

// Näyttää viimeisimmän minuuttimittauksen kahdella rivillä:
// lämpötila ja kosteus / akku ja Picon sisäinen lämpötila.
void lcd_show_measurement(
    float temperature,
    float humidity,
    uint8_t battery_percent,
    float internal_temperature);

#endif
