// NAME: PN5180ISO14443.h
//
// DESC: ISO14443 protocol on NXP Semiconductors PN5180 module for Arduino.
//
// Copyright (c) 2019 by Dirk Carstensen. All rights reserved.
//
// This file is part of the PN5180 library for the Arduino environment.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// #define DEBUG 1

#include <Arduino.h>
#include <inttypes.h>
#include "PN5180ISO14443.h"
#include <PN5180.h>
#include "Debug.h"

static const char *TAG_ISO14443 = "PN5180_ISO14443";
static constexpr bool kIso14443DiagLogs = false;

static void dump_activate_diag(PN5180ISO14443 *nfc, const char *step) {
  uint32_t irqStatus = nfc->getIRQStatus();
  uint32_t rxStatus = 0;
  uint32_t rfStatus = 0;
  uint32_t systemStatus = 0;
  nfc->readRegister(RX_STATUS, &rxStatus);
  nfc->readRegister(RF_STATUS, &rfStatus);
  nfc->readRegister(SYSTEM_STATUS, &systemStatus);
  if (kIso14443DiagLogs) {
    ESP_LOGW(TAG_ISO14443,
             "%s failed: IRQ=0x%08" PRIx32 " RX=0x%08" PRIx32 " RF=0x%08" PRIx32 " SYS=0x%08" PRIx32 " TS=%d",
             step,
             irqStatus,
             rxStatus,
             rfStatus,
             systemStatus,
             (int)nfc->getTransceiveState());
  }
}

static bool fallback_uid_from_cl1(const uint8_t *anticolData, uint8_t *buffer, uint8_t *uidLength) {
  const uint8_t uid0 = anticolData[0];
  const uint8_t uid1 = anticolData[1];
  const uint8_t uid2 = anticolData[2];
  const uint8_t uid3 = anticolData[3];
  const uint8_t bcc = anticolData[4];

  if (uid0 == 0x88) {
    return false;
  }

  if ((uid0 ^ uid1 ^ uid2 ^ uid3) != bcc) {
    return false;
  }

  buffer[2] = 0xFF;  // SAK unavailable in fallback mode.
  buffer[3] = uid0;
  buffer[4] = uid1;
  buffer[5] = uid2;
  buffer[6] = uid3;
  *uidLength = 4;
  return true;
}

static void log_cl1_data(const uint8_t *anticolData, uint8_t sak) {
  const uint8_t bcc_calc = anticolData[0] ^ anticolData[1] ^ anticolData[2] ^ anticolData[3];
  if (kIso14443DiagLogs) {
    ESP_LOGW(TAG_ISO14443,
             "CL1 data uid/bcc=%02X:%02X:%02X:%02X:%02X bcc_calc=%02X sak=%02X",
             anticolData[0],
             anticolData[1],
             anticolData[2],
             anticolData[3],
             anticolData[4],
             bcc_calc,
             sak);
  }
}

PN5180ISO14443::PN5180ISO14443(uint8_t SSpin, uint8_t BUSYpin, uint8_t RSTpin) 
              : PN5180(SSpin, BUSYpin, RSTpin) {
}

bool PN5180ISO14443::waitForRxReady(const char *step) {
  unsigned long startedWaiting = millis();
  while (millis() - startedWaiting <= 25) {
    uint32_t irqStatus = getIRQStatus();
    if (irqStatus & (RX_SOF_DET_IRQ_STAT | RX_IRQ_STAT)) {
      return true;
    }
    if (irqStatus & GENERAL_ERROR_IRQ_STAT) {
      dump_activate_diag(this, step);
      return false;
    }
    delay(1);
  }

  dump_activate_diag(this, step);
  return false;
}

bool PN5180ISO14443::setupRF() {
  PN5180DEBUG(F("Loading RF-Configuration...\n"));
  if (loadRFConfig(0x00, 0x80)) {  // ISO14443 parameters
    PN5180DEBUG(F("done.\n"));
  }
  else return false;

  PN5180DEBUG(F("Turning ON RF field...\n"));
  if (setRF_on()) {
    PN5180DEBUG(F("done.\n"));
  }
  else return false;

  return true;
}

uint16_t PN5180ISO14443::rxBytesReceived() {
	uint32_t rxStatus;
	uint16_t len = 0;
	readRegister(RX_STATUS, &rxStatus);
	// Lower 9 bits has length
	len = (uint16_t)(rxStatus & 0x000001ff);
	return len;
}
/*
* buffer : must be 10 byte array
* buffer[0-1] is ATQA
* buffer[2] is sak
* buffer[3..6] is 4 byte UID
* buffer[7..9] is remaining 3 bytes of UID for 7 Byte UID tags
* kind : 0  we send REQA, 1 we send WUPA
*
* return value: the uid length:
* -	zero if no tag was recognized
* -	single Size UID (4 byte)
* -	double Size UID (7 byte)
* -	triple Size UID (10 byte) - not yet supported
*/
uint8_t PN5180ISO14443::activateTypeA(uint8_t *buffer, uint8_t kind) {
		uint8_t cmd[7];
		uint8_t uidLength = 0;
		// Load standard TypeA protocol
		if (!loadRFConfig(0x0, 0x80)) {
		  dump_activate_diag(this, "loadRFConfig");
		  return 0;
		}

		// OFF Crypto
		if (!writeRegisterWithAndMask(SYSTEM_CONFIG, 0xFFFFFFBF)) {
		  dump_activate_diag(this, "disable crypto");
		  return 0;
		}
		// Clear RX CRC
		if (!writeRegisterWithAndMask(CRC_RX_CONFIG, 0xFFFFFFFE)) {
		  dump_activate_diag(this, "clear rx crc");
		  return 0;
		}
		// Clear TX CRC
		if (!writeRegisterWithAndMask(CRC_TX_CONFIG, 0xFFFFFFFE)) {
		  dump_activate_diag(this, "clear tx crc");
		  return 0;
		}
		//Send REQA/WUPA, 7 bits in last byte
			cmd[0] = (kind == 0) ? 0x26 : 0x52;
			if (!sendData(cmd, 1, 0x07)) {
			  dump_activate_diag(this, (kind == 0) ? "send REQA" : "send WUPA");
			  return 0;
			}
			if (!waitForRxReady((kind == 0) ? "wait ATQA after REQA" : "wait ATQA after WUPA")) {
			  return 0;
			}
			// READ 2 bytes ATQA into  buffer
			if (!readData(2, buffer)) {
			  dump_activate_diag(this, (kind == 0) ? "read ATQA after REQA" : "read ATQA after WUPA");
			  return 0;
			}
		//Send Anti collision 1, 8 bits in last byte
			cmd[0] = 0x93;
			cmd[1] = 0x20;
			if (!sendData(cmd, 2, 0x00)) {
			  dump_activate_diag(this, "send anticollision CL1");
			  return 0;
			}
			if (!waitForRxReady("wait anticollision CL1")) {
			  return 0;
			}
			//Read 5 bytes, we will store at offset 2 for later usage
			if (!readData(5, cmd+2)) {
			  dump_activate_diag(this, "read anticollision CL1");
			  return 0;
			}
		//Enable RX CRC calculation
		if (!writeRegisterWithOrMask(CRC_RX_CONFIG, 0x01)) {
		  dump_activate_diag(this, "enable rx crc");
		  return 0;
		}
		//Enable TX CRC calculation
		if (!writeRegisterWithOrMask(CRC_TX_CONFIG, 0x01)) {
		  dump_activate_diag(this, "enable tx crc");
		  return 0;
		}
			//Send Select anti collision 1, the remaining bytes are already in offset 2 onwards
			cmd[0] = 0x93;
			cmd[1] = 0x70;
			if (!sendData(cmd, 7, 0x00)) {
			  if (fallback_uid_from_cl1(cmd + 2, buffer, &uidLength)) {
			    if (kIso14443DiagLogs) ESP_LOGW(TAG_ISO14443, "select CL1 failed, returning UID from anticollision only");
			    return uidLength;
			  }
			  dump_activate_diag(this, "send select CL1");
			  return 0;
			}
			if (!waitForRxReady("wait SAK CL1")) {
			  if (fallback_uid_from_cl1(cmd + 2, buffer, &uidLength)) {
			    if (kIso14443DiagLogs) ESP_LOGW(TAG_ISO14443, "SAK wait failed, returning UID from anticollision only");
			    return uidLength;
			  }
			  return 0;
			}
			//Read 1 byte SAK into buffer[2]
			if (!readData(1, buffer+2)) {
			  if (fallback_uid_from_cl1(cmd + 2, buffer, &uidLength)) {
			    if (kIso14443DiagLogs) ESP_LOGW(TAG_ISO14443, "SAK read failed, returning UID from anticollision only");
			    return uidLength;
			  }
			  dump_activate_diag(this, "read SAK CL1");
			  return 0;
			}
	// Check if the tag is 4 Byte UID or 7 byte UID and requires anti collision 2
			// If Bit 3 is 0 it is 4 Byte UID
			if ((buffer[2] & 0x04) == 0) {
				// Take first 4 bytes of anti collision as UID store at offset 3 onwards. job done
				for (int i = 0; i < 4; i++) buffer[3+i] = cmd[2 + i];
				uidLength = 4;
			}
			else {
				log_cl1_data(cmd + 2, buffer[2]);
				// Take First 3 bytes of UID, Ignore first byte 88(CT)
				if (cmd[2] != 0x88) {
				  if (fallback_uid_from_cl1(cmd + 2, buffer, &uidLength)) {
				    if (kIso14443DiagLogs) ESP_LOGW(TAG_ISO14443, "cascade bit set but CL1 looks like 4-byte UID, returning fallback UID");
				    return uidLength;
				  }
				  dump_activate_diag(this, "cascade tag check");
				  return 0;
				}
			for (int i = 0; i < 3; i++) buffer[3+i] = cmd[3 + i];
			// Clear RX CRC
			if (!writeRegisterWithAndMask(CRC_RX_CONFIG, 0xFFFFFFFE)) {
		      dump_activate_diag(this, "clear rx crc cl2");
		      return 0;
		    }
			// Clear TX CRC
			if (!writeRegisterWithAndMask(CRC_TX_CONFIG, 0xFFFFFFFE)) {
		      dump_activate_diag(this, "clear tx crc cl2");
		      return 0;
		    }
			// Do anti collision 2
				cmd[0] = 0x95;
				cmd[1] = 0x20;
				if (!sendData(cmd, 2, 0x00)) {
		      dump_activate_diag(this, "send anticollision CL2");
		      return 0;
		    }
				if (!waitForRxReady("wait anticollision CL2")) {
		      return 0;
		    }
				//Read 5 bytes. we will store at offset 2 for later use
				if (!readData(5, cmd+2)) {
		      dump_activate_diag(this, "read anticollision CL2");
		      return 0;
		    }
		// first 4 bytes belongs to last 4 UID bytes, we keep it.
		for (int i = 0; i < 4; i++) {
		  buffer[6 + i] = cmd[2+i];
		}
		//Enable RX CRC calculation
			if (!writeRegisterWithOrMask(CRC_RX_CONFIG, 0x01)) {
		      dump_activate_diag(this, "enable rx crc cl2");
		      return 0;
		    }
			//Enable TX CRC calculation
			if (!writeRegisterWithOrMask(CRC_TX_CONFIG, 0x01)) {
		      dump_activate_diag(this, "enable tx crc cl2");
		      return 0;
		    }
			//Send Select anti collision 2 
				cmd[0] = 0x95;
				cmd[1] = 0x70;
				if (!sendData(cmd, 7, 0x00)) {
		      dump_activate_diag(this, "send select CL2");
		      return 0;
		    }
				if (!waitForRxReady("wait SAK CL2")) {
		      return 0;
		    }
				//Read 1 byte SAK into buffer[2]
				if (!readData(1, buffer + 2)) {
		      dump_activate_diag(this, "read SAK CL2");
		      return 0;
		    }	
			uidLength = 7;
		}
	    return uidLength;
}

bool PN5180ISO14443::mifareBlockRead(uint8_t blockno, uint8_t *buffer) {
	bool success = false;
	uint16_t len;
	uint8_t cmd[2];
	// Send mifare command 30,blockno
	cmd[0] = 0x30;
	cmd[1] = blockno;
	if (!sendData(cmd, 2, 0x00))
	  return false;
	//Check if we have received any data from the tag
	delay(5);
	len = rxBytesReceived();
	if (len == 16) {
		// READ 16 bytes into  buffer
		if (readData(16, buffer))
		  success = true;
	}
	return success;
}


uint8_t PN5180ISO14443::mifareBlockWrite16(uint8_t blockno, uint8_t *buffer) {
	uint8_t cmd[1];
	// Clear RX CRC
	writeRegisterWithAndMask(CRC_RX_CONFIG, 0xFFFFFFFE);

	// Mifare write part 1
	cmd[0] = 0xA0;
	cmd[1] = blockno;
	sendData(cmd, 2, 0x00);
	readData(1, cmd);

	// Mifare write part 2
	sendData(buffer,16, 0x00);
	delay(10);

	// Read ACK/NAK
	readData(1, cmd);

	//Enable RX CRC calculation
	writeRegisterWithOrMask(CRC_RX_CONFIG, 0x1);
	return cmd[0];
}

bool PN5180ISO14443::mifareHalt() {
	uint8_t cmd[1];
	//mifare Halt
	cmd[0] = 0x50;
	cmd[1] = 0x00;
	sendData(cmd, 2, 0x00);	
	return true;
}

uint8_t PN5180ISO14443::readCardSerial(uint8_t *buffer) {
  
    uint8_t response[10];
	uint8_t uidLength;
	// Always return 10 bytes
    // Offset 0..1 is ATQA
    // Offset 2 is SAK.
    // UID 4 bytes : offset 3 to 6 is UID, offset 7 to 9 to Zero
    // UID 7 bytes : offset 3 to 9 is UID
    for (int i = 0; i < 10; i++) response[i] = 0;
    uidLength = activateTypeA(response, 1);
	if ((response[0] == 0xFF) && (response[1] == 0xFF))
	  return 0;
	// check for valid uid
	if ((response[3] == 0x00) && (response[4] == 0x00) && (response[5] == 0x00) && (response[6] == 0x00))
	  return 0;
	if ((response[3] == 0xFF) && (response[4] == 0xFF) && (response[5] == 0xFF) && (response[6] == 0xFF))
	  return 0;
    for (int i = 0; i < 7; i++) buffer[i] = response[i+3];
	mifareHalt();
	return uidLength;  
}

bool PN5180ISO14443::isCardPresent() {
    uint8_t buffer[10];
	return (readCardSerial(buffer) >=4);
}
