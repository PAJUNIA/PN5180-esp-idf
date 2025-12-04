#pragma once

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h" // Nécessaire pour esp_rom_delay_us

// --- Modes SPI ---
#ifndef SPI_MODE0
#define SPI_MODE0 0
#endif

#ifndef SPI_MODE1
#define SPI_MODE1 1
#endif

#ifndef SPI_MODE2
#define SPI_MODE2 2
#endif

#ifndef SPI_MODE3
#define SPI_MODE3 3
#endif

// --- Pins SPI par défaut (Pour satisfaire le compilateur) ---
#ifndef SS
#define SS 5   // Pin CS par défaut (souvent GPIO 5 sur ESP32)
#endif

#ifndef MOSI
#define MOSI 23
#endif

#ifndef MISO
#define MISO 19
#endif

#ifndef SCK
#define SCK 18
#endif

// --- Compatibilité Mémoire Flash (AVR -> ESP32) ---
#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef PSTR
#define PSTR(s) (s)
#endif

#ifndef F
#define F(str) (str)
#endif

#ifndef __FlashStringHelper
#define __FlashStringHelper char
#endif

// --- Lecture Mémoire Flash (AVR -> ESP32) ---
#ifndef pgm_read_byte
// On lit simplement la valeur pointée par l'adresse
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#endif

// --- Types & Constantes Arduino ---
typedef uint8_t byte;
#define LOW  0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define MSBFIRST 1

// --- Gestion du Temps ---
#define delay(ms) vTaskDelay(pdMS_TO_TICKS(ms))
inline void delayMicroseconds(uint32_t us) {
    esp_rom_delay_us(us);
}
inline unsigned long millis() { return (unsigned long)(esp_timer_get_time() / 1000); }

inline void yield() {
    taskYIELD();
}

// --- Gestion GPIO ---
inline void pinMode(uint8_t pin, uint8_t mode) {
    gpio_reset_pin((gpio_num_t)pin);
    gpio_set_direction((gpio_num_t)pin, mode == OUTPUT ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
}
inline void digitalWrite(uint8_t pin, uint8_t val) {
    gpio_set_level((gpio_num_t)pin, val);
}
inline int digitalRead(uint8_t pin) {
    return gpio_get_level((gpio_num_t)pin);
}

// --- Paramètres SPI (Stub) ---
class SPISettings {
public:
    // Le constructeur appelé par PN5180
    SPISettings(uint32_t freq, uint8_t order, uint8_t mode) {}
    // Un constructeur vide par sécurité
    SPISettings() {}
};

// --- Fausse Classe SPI ---
// On utilise une variable globale externe pour stocker le handle ESP-IDF
extern spi_device_handle_t global_nfc_spi;

class SPIClass {
public:
    void begin() {}

    void end() {}
    
    // CORRECTION ICI : On change le type de l'argument 'int' en 'SPISettings'
    void beginTransaction(SPISettings settings) {
        // On laisse vide : La config SPI est gérée par ESP-IDF dans le wrapper
    }

    void endTransaction() {}
    
    // ... Garde ta méthode transfer existante ...
    uint8_t transfer(uint8_t data) {
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        
        // CORRECTION : On active TXDATA ET RXDATA
        // Cela dit au driver : "Ne regarde pas les pointeurs tx_buffer/rx_buffer (qui sont NULL),
        // regarde directement dans les tableaux tx_data/rx_data de la structure."
        t.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
        
        t.length = 8;
        t.tx_data[0] = data; // On met la donnée dans le buffer interne
        
        esp_err_t ret = spi_device_polling_transmit(global_nfc_spi, &t);
        
        // Si tout va bien, on renvoie l'octet reçu (buffer interne)
        return (ret == ESP_OK) ? t.rx_data[0] : 0;
    }
};

extern SPIClass SPI;

// --- Constantes d'affichage ---
#define DEC 10
#define HEX 16
#define BIN 2

// --- Fausse Classe Serial (Redirection vers ESP_LOG) ---
class HardwareSerial {
public:
    void begin(unsigned long baud) { }

    // 1. Strings (char*)
    void print(const char* str) { ESP_LOGI("PN51080_MSG", "%s", str); }
    void println(const char* str) { ESP_LOGI("PN51080_MSG", "%s", str); }

    // 2. Char (c)
    void print(char c) { ESP_LOGI("PN51080_MSG", "%c", c); }
    void println(char c) { print(c); }

    // 3. Unsigned Int (unsigned int)
    void print(unsigned int n, int base = DEC) {
        if (base == HEX) ESP_LOGI("PN51080_MSG", "%X", n);
        else ESP_LOGI("PN51080_MSG", "%u", n);
    }
    void println(unsigned int n, int base = DEC) { print(n, base); }

    // 4. Int (int)
    void print(int n, int base = DEC) {
        if (base == HEX) ESP_LOGI("PN51080_MSG", "%X", (unsigned int)n);
        else ESP_LOGI("PN51080_MSG", "%d", n);
    }
    void println(int n, int base = DEC) { print(n, base); }

    // 5. Unsigned Char / Byte (uint8_t) - Lève l'ambiguïté byte vs char
    void print(unsigned char n, int base = DEC) {
        print((unsigned int)n, base);
    }
    void println(unsigned char n, int base = DEC) { print(n, base); }

    // 6. Long (int32_t) - C'est celui qui te manquait !
    void print(long n, int base = DEC) {
        if (base == HEX) ESP_LOGI("PN51080_MSG", "%lX", n);
        else ESP_LOGI("PN51080_MSG", "%ld", n);
    }
    void println(long n, int base = DEC) { print(n, base); }

    // 7. Unsigned Long (uint32_t)
    void print(unsigned long n, int base = DEC) {
        if (base == HEX) ESP_LOGI("PN51080_MSG", "%lX", n);
        else ESP_LOGI("PN51080_MSG", "%lu", n);
    }
    void println(unsigned long n, int base = DEC) { print(n, base); }

    // Saut de ligne vide
    void println(void) { }
};

static HardwareSerial Serial __attribute__((unused));
