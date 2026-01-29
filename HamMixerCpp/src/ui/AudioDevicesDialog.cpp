/*
 * AudioDevicesDialog.cpp
 *
 * Modal dialog for configuring audio input/output devices
 * Part of HamMixer CT7BAC
 */

#include "AudioDevicesDialog.h"
#include <QVBoxLayout>

AudioDevicesDialog::AudioDevicesDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

void AudioDevicesDialog::setupUI()
{
    setWindowTitle("Audio Devices");
    setMinimumSize(450, 240);
    resize(500, 240);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // Device panel (contains the 3 device selectors)
    m_devicePanel = new DevicePanel(this);
    mainLayout->addWidget(m_devicePanel);

    // Spacer
    mainLayout->addStretch();

    // Button box (OK/Cancel)
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);
}
