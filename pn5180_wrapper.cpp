#include "pn5180_wrapper.h"
#include "Arduino.h"
#include "PN5180ISO14443.h"
#include "PN5180ISO15693.h"

// --- Pins (identiques au sketch.ino) ---
#define PN_NSS   5
#define PN_BUSY  39
#define PN_RST   0
#define PN_IRQ   35
#define PN_MISO  19
#define PN_MOSI  18
#define PN_SCK   23

SPIClass SPI;
spi_device_handle_t global_nfc_spi;

static PN5180ISO14443 *nfc = nullptr;
static PN5180ISO15693 *nfc15693 = nullptr;

bool pn5180_init_spi(void) {
    constexpr spi_host_device_t pn5180_spi_host = SPI3_HOST;

    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num     = PN_MISO;
    buscfg.mosi_io_num     = PN_MOSI;
    buscfg.sclk_io_num     = PN_SCK;
    buscfg.quadwp_io_num   = -1;
    buscfg.quadhd_io_num   = -1;
    buscfg.max_transfer_sz = 4096;

    if (spi_bus_initialize(pn5180_spi_host, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) return false;

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 5000000;
    devcfg.mode           = 0;
    devcfg.spics_io_num   = -1;
    devcfg.queue_size     = 1;

    if (spi_bus_add_device(pn5180_spi_host, &devcfg, &global_nfc_spi) != ESP_OK) return false;

    pinMode(PN_IRQ, INPUT);

    nfc = new PN5180ISO14443(PN_NSS, PN_BUSY, PN_RST);
    nfc15693 = new PN5180ISO15693(PN_NSS, PN_BUSY, PN_RST);
    return true;
}

void pn5180_begin(void) {
    if (nfc) nfc->begin();
    if (nfc15693) nfc15693->begin();
}

void pn5180_reset(void) {
    if (nfc) nfc->reset();
}

bool pn5180_read_eeprom(uint8_t addr, uint8_t *buffer, int len) {
    if (!nfc) return false;
    return nfc->readEEprom(addr, buffer, len);
}

bool pn5180_write_eeprom(uint8_t addr, uint8_t *buffer, uint8_t len) {
    if (!nfc) return false;
    return nfc->writeEEprom(addr, buffer, len);
}

uint32_t pn5180_get_irq_status(void) {
    if (!nfc) return 0;
    return nfc->getIRQStatus();
}

bool pn5180_clear_irq_status(uint32_t irq_mask) {
    if (!nfc) return false;
    return nfc->clearIRQStatus(irq_mask);
}

bool pn5180_read_register(uint8_t reg, uint32_t *value) {
    if (!nfc) return false;
    return nfc->readRegister(reg, value);
}

bool pn5180_switch_to_lpcd(uint16_t wakeup_counter_ms) {
    if (!nfc) return false;
    return nfc->switchToLPCD(wakeup_counter_ms);
}

bool pn5180_setup_rf(void) {
    if (!nfc) return false;
    return nfc->setupRF();
}

bool pn5180_set_rf_off(void) {
    if (!nfc) return false;
    return nfc->setRF_off();
}

uint8_t pn5180_activate_type_a(uint8_t *buffer, uint8_t kind) {
    if (!nfc) return 0;
    return nfc->activateTypeA(buffer, kind);
}

bool pn5180_is_card_present(void) {
    if (!nfc) return false;
    return nfc->isCardPresent();
}

uint8_t pn5180_read_card_serial(uint8_t *buffer) {
    if (!nfc) return 0;
    return nfc->readCardSerial(buffer);
}

bool pn5180_mifare_halt(void) {
    if (!nfc) return false;
    return nfc->mifareHalt();
}

void pn5180_iso15693_reset(void) {
    if (nfc15693) nfc15693->reset();
}

bool pn5180_iso15693_setup_rf(void) {
    if (!nfc15693) return false;
    return nfc15693->setupRF();
}

int pn5180_iso15693_get_inventory(uint8_t *uid) {
    if (!nfc15693) return -2;
    return static_cast<int>(nfc15693->getInventory(uid));
}
