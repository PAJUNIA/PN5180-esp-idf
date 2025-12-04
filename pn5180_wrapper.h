#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le bus SPI et le PN5180.
 * Vérifie si la puce répond (lecture version EEPROM).
 * * @return true si le PN5180 est détecté et prêt.
 * @return false si erreur SPI ou problème de câblage (BUSY/RST).
 */
bool pn5180_init_wrapper(void);

/**
 * @brief BOUCLE DE TEST HARDWARE (Pas encore de lecture de carte)
 * * Cette fonction interroge le PN5180 pour lire sa version de Firmware.
 * Cela permet de valider que les lignes MOSI/MISO/SCK/BUSY fonctionnent parfaitement.
 * * @param uid_buffer Buffer pour stocker le résultat
 * @param max_len Taille max du buffer
 * @return int Retourne 4 octets (Format de test : [0xAA, VerMaj, VerMin, 0xBB])
 */
int pn5180_read_card_wrapper(uint8_t* uid_buffer, int max_len);

#ifdef __cplusplus
}
#endif
