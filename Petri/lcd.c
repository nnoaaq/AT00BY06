#include "lcd.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "config.h"

// Näytön omat kuvamerkit (CGRAM-paikat 1–4). Paikka 0 jätetään käyttämättä,
// koska merkkiä 0 ei voi kirjoittaa C-merkkijonoon.
#define LCD_CHAR_BATTERY 1
#define LCD_CHAR_DEGREE 2
#define LCD_CHAR_DROPLET 3
#define LCD_CHAR_THERMOMETER 4

static const uint8_t custom_battery[8] = {
    0x0E, 0x1B, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x1F};

static const uint8_t custom_degree[8] = {
    0x06, 0x06, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

static const uint8_t custom_droplet[8] = {
    0x04, 0x04, 0x0E, 0x0E,
    0x1F, 0x1F, 0x0E, 0x00};

static const uint8_t custom_thermometer[8] = {
    0x04, 0x0A, 0x04, 0x04,
    0x0E, 0x1F, 0x1F, 0x0E};

static bool lcd_available = false;

// Laajentimen bitit: D7..D4 = bitit 7..4, taustavalo = bitti 3,
// Enable = bitti 2, RW = bitti 1 (aina 0 = kirjoitus), RS = bitti 0.
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE 0x04
#define LCD_RS_DATA 0x01

static bool lcd_write_expander(uint8_t value)
{
    return i2c_write_timeout_us(
               I2C_PORT,
               LCD_ADDR,
               &value,
               1,
               false,
               5000) == 1;
}

// Lähettää yhden nelibittisen arvon LCD:n D4–D7-linjoille.
// Arvo luetaan näytölle Enable-pulssin laskevalla reunalla.
static void lcd_send_nibble(uint8_t nibble, bool data_mode)
{
    uint8_t value =
        ((nibble & 0x0F) << 4) |
        LCD_BACKLIGHT |
        (data_mode ? LCD_RS_DATA : 0x00);

    lcd_write_expander(value);
    lcd_write_expander(value | LCD_ENABLE);
    sleep_us(1);
    lcd_write_expander(value);
    sleep_us(50);
}

// Lähettää kokonaisen tavun kahtena nibblenä (ylempi ensin).
static void lcd_send_byte(uint8_t value, bool data_mode)
{
    lcd_send_nibble(value >> 4, data_mode);
    lcd_send_nibble(value & 0x0F, data_mode);
}

static void lcd_command(uint8_t command)
{
    lcd_send_byte(command, false);

    // Tyhjennys (0x01) ja kotiasento (0x02) kestävät muita komentoja kauemmin.
    if (command == 0x01 || command == 0x02)
    {
        sleep_ms(2);
    }
}

// Siirtää kursorin riville (0 tai 1) ja sarakkeeseen.
// DDRAM-osoitteet: rivi 0 alkaa 0x00:sta, rivi 1 0x40:stä; 0x80 on komentobitti.
static void lcd_set_cursor(unsigned line, unsigned column)
{
    uint8_t address = (line == 0 ? 0x80 : 0xC0) + column;
    lcd_command(address);
}

// Tallentaa 5x8-pisteisen kuvamerkin näytön CGRAMiin (paikat 0–7).
static void lcd_create_char(
    uint8_t location,
    const uint8_t bitmap[8])
{
    lcd_command(0x40 | ((location & 7) << 3));

    for (unsigned i = 0; i < 8; ++i)
    {
        lcd_send_byte(bitmap[i], true);
    }
}

void lcd_write_line(unsigned line, const char *text)
{
    if (!lcd_available)
    {
        return;
    }

    lcd_set_cursor(line, 0);

    bool ended = false;

    // Kirjoitetaan aina täydet 16 merkkiä, jotta edellisen tekstin
    // jäämät pyyhkiytyvät pois.
    for (unsigned i = 0; i < 16; ++i)
    {
        uint8_t character = ' ';

        if (!ended)
        {
            if (*text != '\0')
            {
                character = (uint8_t)*text++;
            }
            else
            {
                ended = true;
            }
        }

        lcd_send_byte(character, true);
    }
}

bool lcd_init(void)
{
    i2c_init(I2C_PORT, 100000);

    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Näytön käynnistyminen kestää hetken virran kytkemisestä.
    sleep_ms(50);

    // Jos laajennin ei kuittaa osoitettaan, näyttöä ei ole kytketty.
    if (!lcd_write_expander(LCD_BACKLIGHT))
    {
        lcd_available = false;
        return false;
    }

    // HD44780:n määrätty alustusjakso: kolme kertaa 8-bittitila (0x3),
    // sitten siirtyminen 4-bittitilaan (0x2).
    lcd_send_nibble(0x03, false);
    sleep_ms(5);

    lcd_send_nibble(0x03, false);
    sleep_us(150);

    lcd_send_nibble(0x03, false);
    sleep_us(150);

    lcd_send_nibble(0x02, false);
    sleep_us(150);

    lcd_command(0x28); // 4-bittinen väylä, 2 riviä, 5x8-fontti
    lcd_command(0x08); // näyttö pois päältä
    lcd_command(0x01); // tyhjennä näyttö
    lcd_command(0x06); // kursori liikkuu oikealle kirjoitettaessa
    lcd_command(0x0C); // näyttö päälle, kursori piilossa

    lcd_create_char(LCD_CHAR_BATTERY, custom_battery);
    lcd_create_char(LCD_CHAR_DEGREE, custom_degree);
    lcd_create_char(LCD_CHAR_DROPLET, custom_droplet);
    lcd_create_char(LCD_CHAR_THERMOMETER, custom_thermometer);

    lcd_available = true;
    return true;
}

void lcd_show_measurement(
    float temperature,
    float humidity,
    uint8_t battery_percent,
    float internal_temperature)
{
    char line1[40];
    char line2[40];

    // Kuvamerkit kirjoitetaan heksaescapeina (\x04 = lämpömittari,
    // \x02 = aste, \x03 = pisara, \x01 = akku). Merkkijono katkaistaan
    // kohdassa "\x02" "C", koska muuten C luettaisiin osaksi escapea
    // \x02C (C on heksadesimaalinumero).
    snprintf(
        line1,
        sizeof(line1),
        "\x04%.1f\x02"
        "C \x03%.0f%%",
        (double)temperature,
        (double)humidity);

    snprintf(
        line2,
        sizeof(line2),
        "\x01%3u%% S:%.1f\x02"
        "C",
        (unsigned)battery_percent,
        (double)internal_temperature);

    lcd_write_line(0, line1);
    lcd_write_line(1, line2);
}
