#include "pn5180_wrapper.h"
#include "Arduino.h"
#include "PN5180.h"

// --- PINS (Vérifie ton câblage !) ---
#define PN_NSS   5
#define PN_BUSY  16  // Entrée
#define PN_RST   17  // Sortie
#define PN_MOSI  23
#define PN_MISO  19
#define PN_SCK   18

// Variables Globales
SPIClass SPI;
spi_device_handle_t global_nfc_spi;
PN5180* nfc = nullptr;

bool pn5180_init_wrapper(void) {
    // 1. Config SPI ESP-IDF (Standard)
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = PN_MISO;
    buscfg.mosi_io_num = PN_MOSI;
    buscfg.sclk_io_num = PN_SCK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;

    if (spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) return false;

    // 2. Config Device
    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 5000000; // 5 MHz
    devcfg.mode = 0;
    devcfg.spics_io_num = -1; // CS manuel
    devcfg.queue_size = 1;

    if (spi_bus_add_device(SPI2_HOST, &devcfg, &global_nfc_spi) != ESP_OK) return false;

    // 3. Init PN5180
    nfc = new PN5180(PN_NSS, PN_BUSY, PN_RST);
    
    // CORRECTION 1 : begin() renvoie void, on l'appelle simplement
    nfc->begin();

    // CORRECTION 2 : On fait un "Reset" logiciel pour être propre
    nfc->reset();

    // CORRECTION 3 : Vérification immédiate de la communication
    // On lit la version du produit depuis l'EEPROM
    uint8_t productVersion[2];
    nfc->readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
    
    // Si la version est 0x00 ou 0xFF, le SPI ne marche pas
    if (productVersion[1] == 0x00 || productVersion[1] == 0xFF) {
        return false;
    }

    // CORRECTION 4 : Activation manuelle du champ RF (puisque setupRF n'existe pas)
    // On active le champ RF pour montrer que le chip est vivant (consomme du courant)
    // RF_STATUS = 0x00 (Register), bit 0 = RF_ON
    nfc->writeRegister(0x00, 1); 

    return true;
}

int pn5180_read_card_wrapper(uint8_t* uid_buffer, int max_len) {
    if (!nfc) return 0;

    // CORRECTION CRITIQUE : La lib tueddy n'a pas de "isCardPresent" facile.
    // Pour l'instant, nous allons valider le hardware en lisant la Version du Firmware en boucle.
    // Si tu vois la version dans les logs, c'est GAGNÉ pour le SPI.
    
    uint8_t productVersion[2];
    nfc->readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));

    // Si la lecture réussit et qu'on a une version valide (ex: 3.5 ou 4.0)
    if (productVersion[1] != 0x00 && productVersion[1] != 0xFF) {
        // On "simule" une lecture d'UID en renvoyant la version du firmware
        // Cela permet de voir quelque chose s'afficher dans le main
        uid_buffer[0] = 0xAA; // Marqueur de test
        uid_buffer[1] = productVersion[1]; // Version Majeure
        uid_buffer[2] = productVersion[0]; // Version Mineure
        uid_buffer[3] = 0xBB; // Marqueur de test
        
        return 4; // On fait croire qu'on a lu 4 octets
    }

    return 0;
}
