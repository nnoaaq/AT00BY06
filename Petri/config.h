#ifndef CONFIG_H
#define CONFIG_H

// Yhteiset asetukset: nastat, ajastukset ja mittausten korjauskertoimet.
// Wi-Fi-tunnukset ovat erillisessä wifi_settings.h-tiedostossa ja
// AWS-asetukset aws_settings.h-tiedostossa.

// ---------------- Nastat ----------------

#define DHT_PIN 28          // DHT22 / AM2302 -anturin datalinja
#define BATTERY_ADC_PIN 26  // akkujännitteen mittaus (GPIO26 = ADC0)
#define BATTERY_ADC_INPUT 0 // ADC-tulo, joka vastaa nastaa BATTERY_ADC_PIN
#define ADC_TEMP_SENSOR_INPUT 4 // Picon sisäisen lämpötila-anturin ADC-tulo
#define RESET_BUTTON_PIN 15 // pohjassa käynnistyksessä: tyhjennä historia

#define I2C_PORT i2c0
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define LCD_ADDR 0x27 // I2C-laajennin (PCF8574) LCD:n takana

// ---------------- Ajastukset ----------------

#define SAMPLE_INTERVAL_US 10000000ULL  // anturin lukeminen: 10 s välein
#define DISPLAY_INTERVAL_US 60000000ULL // LCD ja minuuttikeskiarvo: 1 min
#define DEFAULT_SAVE_MINUTES 15u        // tallennusjakso käynnistyksen jälkeen

// ---------------- Mittausten korjaukset ----------------
// Lisätään anturin lukemaan ennen keskiarvojen laskemista.

#define TEMP_CORRECTION_C (-0.5f)
#define HUMIDITY_CORRECTION (-4.0f)

// ---------------- Akku ja ADC ----------------
// Akkuprosentti lasketaan lineaarisesti jännitteiden EMPTY ja FULL välillä.
// Se on karkea arvio, koska pariston purkautumiskäyrä ei ole lineaarinen.

#define BATTERY_EMPTY_V 2.1f
#define BATTERY_FULL_V 3.0f
#define BATTERY_DIVIDER 2.0f // jännitteenjakajan suhde (todellinen / mitattu)
#define ADC_REFERENCE_V 3.3f // oletettu ADC:n referenssijännite

#endif
