/*
 * CIVProtocol.h
 *
 * Icom CI-V Protocol constants and parsing utilities for IC-7300
 * Part of HamMixer CT7BAC
 */

#ifndef CIVPROTOCOL_H
#define CIVPROTOCOL_H

#include <QByteArray>
#include <QString>
#include <cstdint>

namespace CIVProtocol {

// Frame structure bytes
constexpr uint8_t PREAMBLE = 0xFE;
constexpr uint8_t EOM = 0xFD;           // End of message

// Default addresses
constexpr uint8_t ADDR_IC7300 = 0x94;   // IC-7300 default address
constexpr uint8_t ADDR_CONTROLLER = 0xE0; // Controller (PC) address

// Command codes
constexpr uint8_t CMD_TRANSCEIVE_FREQ = 0x00;  // Transceive frequency (unsolicited)
constexpr uint8_t CMD_TRANSCEIVE_MODE = 0x01;  // Transceive mode (unsolicited)
constexpr uint8_t CMD_READ_FREQ = 0x03;        // Read operating frequency
constexpr uint8_t CMD_READ_MODE = 0x04;        // Read operating mode
constexpr uint8_t CMD_WRITE_FREQ = 0x05;       // Set operating frequency
constexpr uint8_t CMD_WRITE_MODE = 0x06;       // Set operating mode
constexpr uint8_t CMD_READ_METER = 0x15;       // Read meter levels
constexpr uint8_t CMD_OK = 0xFB;               // OK response
constexpr uint8_t CMD_NG = 0xFA;               // NG (error) response

// Sub-commands for CMD_READ_METER (0x15)
constexpr uint8_t SUBCMD_SMETER = 0x02;        // S-meter level
constexpr uint8_t SUBCMD_SQUELCH = 0x01;       // Squelch status
constexpr uint8_t SUBCMD_POWER = 0x11;         // RF power meter
constexpr uint8_t SUBCMD_SWR = 0x12;           // SWR meter
constexpr uint8_t SUBCMD_ALC = 0x13;           // ALC meter

// Mode codes (IC-7300)
constexpr uint8_t MODE_LSB = 0x00;
constexpr uint8_t MODE_USB = 0x01;
constexpr uint8_t MODE_AM = 0x02;
constexpr uint8_t MODE_CW = 0x03;
constexpr uint8_t MODE_RTTY = 0x04;
constexpr uint8_t MODE_FM = 0x05;
constexpr uint8_t MODE_WFM = 0x06;
constexpr uint8_t MODE_CW_R = 0x07;
constexpr uint8_t MODE_RTTY_R = 0x08;
constexpr uint8_t MODE_DV = 0x17;
constexpr uint8_t MODE_LSB_D = 0x00;  // With data mode flag
constexpr uint8_t MODE_USB_D = 0x01;  // With data mode flag

// Filter width codes
constexpr uint8_t FILTER_WIDE = 0x01;
constexpr uint8_t FILTER_MEDIUM = 0x02;
constexpr uint8_t FILTER_NARROW = 0x03;

// Minimum frame size: FE FE TO FROM CMD FD = 6 bytes
constexpr int MIN_FRAME_SIZE = 6;

// Maximum frame size for frequency: FE FE TO FROM CMD 5-bytes FD = 11 bytes
constexpr int MAX_FRAME_SIZE = 64;

/**
 * Build a CI-V command frame
 * @param toAddr Destination address (radio)
 * @param fromAddr Source address (controller)
 * @param data Command and data bytes
 * @return Complete frame with preamble and EOM
 */
QByteArray buildFrame(uint8_t toAddr, uint8_t fromAddr, const QByteArray& data);

/**
 * Build a simple command frame (no data payload)
 * @param command Command byte
 * @return Complete frame
 */
QByteArray buildCommand(uint8_t command);

/**
 * Build a command with subcommand frame
 * @param command Command byte
 * @param subCommand Sub-command byte
 * @return Complete frame
 */
QByteArray buildCommand(uint8_t command, uint8_t subCommand);

/**
 * Parse frequency from BCD-encoded data
 * IC-7300 uses 5-byte BCD encoding (10 digits, reversed order)
 * @param bcdData BCD-encoded frequency bytes (5 bytes)
 * @return Frequency in Hz
 */
uint64_t parseFrequency(const QByteArray& bcdData);

/**
 * Encode frequency to BCD format
 * @param freqHz Frequency in Hz
 * @return BCD-encoded bytes (5 bytes)
 */
QByteArray encodeFrequency(uint64_t freqHz);

/**
 * Parse mode from CI-V response
 * @param data Mode data (1-2 bytes: mode, optional filter)
 * @return Mode code
 */
uint8_t parseMode(const QByteArray& data);

/**
 * Parse S-meter value from CI-V response
 * @param data S-meter data (2 BCD digits)
 * @return S-meter value 0-255
 */
int parseSMeter(const QByteArray& data);

/**
 * Convert CI-V mode code to human-readable string
 * @param civMode CI-V mode code
 * @return Mode name (e.g., "USB", "LSB", "CW")
 */
QString modeToString(uint8_t civMode);

/**
 * Convert CI-V mode code to WebSDR mode string
 * @param civMode CI-V mode code
 * @return WebSDR mode string (e.g., "usb", "lsb", "cw")
 */
QString modeToWebSdr(uint8_t civMode);

/**
 * Convert S-meter raw value (0-255) to dB scale
 * Based on IC-7300 S-meter calibration:
 *   0 = S0 (-54 dBm reference)
 *   120 = S9
 *   241 = S9+60dB
 * @param rawValue Raw S-meter value (0-255)
 * @return Value in dB (approximately -80 to 0 range for S-meter display)
 */
float smeterToDb(int rawValue);

/**
 * Validate a CI-V frame
 * Checks preamble, length, and EOM
 * @param frame Frame to validate
 * @return true if frame is valid
 */
bool isValidFrame(const QByteArray& frame);

/**
 * Extract command byte from a validated frame
 * @param frame Validated CI-V frame
 * @return Command byte
 */
uint8_t getCommand(const QByteArray& frame);

/**
 * Extract data payload from a validated frame
 * @param frame Validated CI-V frame
 * @return Data bytes (excluding preamble, addresses, command, and EOM)
 */
QByteArray getData(const QByteArray& frame);

/**
 * Extract source address from a validated frame
 * @param frame Validated CI-V frame
 * @return Source address byte
 */
uint8_t getSourceAddress(const QByteArray& frame);

/**
 * Extract destination address from a validated frame
 * @param frame Validated CI-V frame
 * @return Destination address byte
 */
uint8_t getDestAddress(const QByteArray& frame);

} // namespace CIVProtocol

#endif // CIVPROTOCOL_H
