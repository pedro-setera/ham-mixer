/*
 * AudioDevicesDialog.h
 *
 * Modal dialog for configuring audio input/output devices
 * Part of HamMixer CT7BAC
 */

#ifndef AUDIODEVICESDIALOG_H
#define AUDIODEVICESDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include "DevicePanel.h"

/**
 * @brief Dialog for managing audio device selection
 *
 * Provides a modal dialog containing the DevicePanel widget
 * for selecting radio input, WebSDR loopback, and audio output devices.
 */
class AudioDevicesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioDevicesDialog(QWidget* parent = nullptr);
    ~AudioDevicesDialog() override = default;

    // Access the device panel for configuration
    DevicePanel* devicePanel() { return m_devicePanel; }

private:
    void setupUI();

    DevicePanel* m_devicePanel;
    QDialogButtonBox* m_buttonBox;
};

#endif // AUDIODEVICESDIALOG_H
