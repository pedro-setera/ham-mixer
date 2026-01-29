/*
 * CIVProtocol.cpp
 *
 * Icom CI-V Protocol implementation for IC-7300
 * Part of HamMixer CT7BAC
 */

#include "CIVProtocol.h"
#include <cmath>

namespace CIVProtocol {

QByteArray buildFrame(uint8_t toAddr, uint8_t fromAddr, const QByteArray& data)
{
    QByteArray frame;
    frame.append(static_cast<char>(PREAMBLE));
    frame.append(static_cast<char>(PREAMBLE));
    frame.append(static_cast<char>(toAddr));
    frame.append(static_cast<char>(fromAddr));
    frame.append(data);
    frame.append(static_cast<char>(EOM));
    return frame;
}

QByteArray buildCommand(uint8_t command)
{
    QByteArray data;
    data.append(static_cast<char>(command));
    return buildFrame(ADDR_IC7300, ADDR_CONTROLLER, data);
}

QByteArray buildCommand(uint8_t command, uint8_t subCommand)
{
    QByteArray data;
    data.append(static_cast<char>(command));
    data.append(static_cast<char>(subCommand));
    return buildFrame(ADDR_IC7300, ADDR_CONTROLLER, data);
}

uint64_t parseFrequency(const QByteArray& bcdData)
{
    if (bcdData.size() < 5) {
        return 0;
    }

    // IC-7300 frequency format: 5 bytes BCD, least significant first
    // Each byte contains two BCD digits
    // Example: 14.200.000 Hz = 00 00 20 41 00 (bytes reversed)
    //   Byte 0: 00 = 0,0 (1 Hz, 10 Hz)
    //   Byte 1: 00 = 0,0 (100 Hz, 1 kHz)
    //   Byte 2: 20 = 2,0 (10 kHz, 100 kHz)
    //   Byte 3: 41 = 4,1 (1 MHz, 10 MHz)
    //   Byte 4: 00 = 0,0 (100 MHz, 1 GHz)

    uint64_t freq = 0;
    uint64_t multiplier = 1;

    for (int i = 0; i < 5; i++) {
        uint8_t byte = static_cast<uint8_t>(bcdData[i]);
        uint8_t lowNibble = byte & 0x0F;
        uint8_t highNibble = (byte >> 4) & 0x0F;

        freq += lowNibble * multiplier;
        multiplier *= 10;
        freq += highNibble * multiplier;
        multiplier *= 10;
    }

    return freq;
}

QByteArray encodeFrequency(uint64_t freqHz)
{
    QByteArray bcd(5, 0);

    for (int i = 0; i < 5; i++) {
        uint8_t lowNibble = freqHz % 10;
        freqHz /= 10;
        uint8_t highNibble = freqHz % 10;
        freqHz /= 10;
        bcd[i] = static_cast<char>((highNibble << 4) | lowNibble);
    }

    return bcd;
}

uint8_t parseMode(const QByteArray& data)
{
    if (data.isEmpty()) {
        return MODE_USB;  // Default
    }
    return static_cast<uint8_t>(data[0]);
}

int parseSMeter(const QByteArray& data)
{
    if (data.size() < 2) {
        return 0;
    }

    // S-meter data is 2 BCD digits (0x00 to 0xFF)
    // Format: high byte and low byte, each containing BCD
    uint8_t highByte = static_cast<uint8_t>(data[0]);
    uint8_t lowByte = static_cast<uint8_t>(data[1]);

    // Convert from BCD to integer
    int highDigit = ((highByte >> 4) & 0x0F) * 10 + (highByte & 0x0F);
    int lowDigit = ((lowByte >> 4) & 0x0F) * 10 + (lowByte & 0x0F);

    // Combine: result is 0-255 range
    return highDigit * 100 + lowDigit;
}

QString modeToString(uint8_t civMode)
{
    switch (civMode) {
        case MODE_LSB:    return "LSB";
        case MODE_USB:    return "USB";
        case MODE_AM:     return "AM";
        case MODE_CW:     return "CW";
        case MODE_RTTY:   return "RTTY";
        case MODE_FM:     return "FM";
        case MODE_WFM:    return "WFM";
        case MODE_CW_R:   return "CW-R";
        case MODE_RTTY_R: return "RTTY-R";
        case MODE_DV:     return "DV";
        default:          return "USB";
    }
}

QString modeToWebSdr(uint8_t civMode)
{
    switch (civMode) {
        case MODE_LSB:    return "lsb";
        case MODE_USB:    return "usb";
        case MODE_AM:     return "am";
        case MODE_CW:
        case MODE_CW_R:   return "cw";
        case MODE_FM:
        case MODE_WFM:    return "fm";
        case MODE_RTTY:
        case MODE_RTTY_R: return "usb";  // No RTTY in WebSDR, use USB
        case MODE_DV:     return "fm";   // Digital voice, use FM
        default:          return "usb";
    }
}

float smeterToDb(int rawValue)
{
    // IC-7300 S-meter calibration (approximate):
    //   0   = S0   = approximately -127 dBm = -80 dBFS (our scale)
    //   120 = S9   = approximately -73 dBm  = -26 dBFS
    //   241 = S9+60= approximately -13 dBm  = 0 dBFS
    //
    // For our S-meter display which uses -80 to 0 dB range:
    // Linear mapping from 0-241 to -80 to 0 dB

    if (rawValue <= 0) {
        return -80.0f;
    }
    if (rawValue >= 241) {
        return 0.0f;
    }

    // Linear interpolation
    float normalized = static_cast<float>(rawValue) / 241.0f;
    return -80.0f + (normalized * 80.0f);
}

bool isValidFrame(const QByteArray& frame)
{
    if (frame.size() < MIN_FRAME_SIZE) {
        return false;
    }

    // Check preamble (two 0xFE bytes)
    if (static_cast<uint8_t>(frame[0]) != PREAMBLE ||
        static_cast<uint8_t>(frame[1]) != PREAMBLE) {
        return false;
    }

    // Check EOM
    if (static_cast<uint8_t>(frame[frame.size() - 1]) != EOM) {
        return false;
    }

    return true;
}

uint8_t getCommand(const QByteArray& frame)
{
    if (frame.size() < 5) {
        return 0;
    }
    // Command is at position 4 (after FE FE TO FROM)
    return static_cast<uint8_t>(frame[4]);
}

QByteArray getData(const QByteArray& frame)
{
    if (frame.size() <= MIN_FRAME_SIZE) {
        return QByteArray();
    }
    // Data starts at position 5, ends before EOM
    return frame.mid(5, frame.size() - 6);
}

uint8_t getSourceAddress(const QByteArray& frame)
{
    if (frame.size() < 4) {
        return 0;
    }
    // Source is at position 3 (after FE FE TO)
    return static_cast<uint8_t>(frame[3]);
}

uint8_t getDestAddress(const QByteArray& frame)
{
    if (frame.size() < 3) {
        return 0;
    }
    // Destination is at position 2 (after FE FE)
    return static_cast<uint8_t>(frame[2]);
}

QString addressToModelName(uint8_t civAddress)
{
    switch (civAddress) {
        case ADDR_IC705:   return "IC-705";
        case ADDR_IC7100:  return "IC-7100";
        case ADDR_IC7300:  return "IC-7300";
        case ADDR_IC7610:  return "IC-7610";
        case ADDR_IC7851:  return "IC-7851";
        case ADDR_IC9700:  return "IC-9700";
        default:           return QString();
    }
}

} // namespace CIVProtocol
