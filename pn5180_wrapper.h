#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PN5180_IRQ_PIN 35

/**
 * @brief Initialise le bus SPI et instancie l'objet PN5180ISO14443.
 *        À appeler une seule fois avant tout autre appel.
 * @return true si le bus SPI est prêt.
 */
bool pn5180_init_spi(void);

/* --- Wrappers 1:1 des méthodes PN5180 / PN5180ISO14443 --- */

void     pn5180_begin(void);
void     pn5180_reset(void);
bool     pn5180_read_eeprom(uint8_t addr, uint8_t *buffer, int len);
bool     pn5180_write_eeprom(uint8_t addr, uint8_t *buffer, uint8_t len);
uint32_t pn5180_get_irq_status(void);
bool     pn5180_clear_irq_status(uint32_t irq_mask);
bool     pn5180_read_register(uint8_t reg, uint32_t *value);
bool     pn5180_switch_to_lpcd(uint16_t wakeup_counter_ms);
bool     pn5180_setup_rf(void);
bool     pn5180_set_rf_off(void);
uint8_t  pn5180_activate_type_a(uint8_t *buffer, uint8_t kind);
bool     pn5180_mifare_halt(void);

#ifdef __cplusplus
}
#endif
