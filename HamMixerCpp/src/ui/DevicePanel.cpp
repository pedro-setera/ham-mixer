#include "ui/DevicePanel.h"
#include "ui/Styles.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>

DevicePanel::DevicePanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    connectSignals();
}

void DevicePanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* groupBox = new QGroupBox("Audio Devices", this);
    QVBoxLayout* groupLayout = new QVBoxLayout(groupBox);
    groupLayout->setSpacing(8);

    // Row 1: Radio Input
    QHBoxLayout* inputRow = new QHBoxLayout();
    QLabel* inputLabel = new QLabel("Radio:", this);
    inputLabel->setFixedWidth(55);
    m_inputCombo = new QComboBox(this);
    m_inputCombo->setMinimumWidth(200);
    m_inputCombo->setToolTip("Select radio audio input (e.g., transceiver USB Audio CODEC)");
    inputRow->addWidget(inputLabel);
    inputRow->addWidget(m_inputCombo, 1);
    groupLayout->addLayout(inputRow);

    // Row 2: WebSDR (Loopback)
    QHBoxLayout* loopbackRow = new QHBoxLayout();
    QLabel* loopbackLabel = new QLabel("WebSDR:", this);
    loopbackLabel->setFixedWidth(55);
    m_loopbackCombo = new QComboBox(this);
    m_loopbackCombo->setMinimumWidth(200);
    m_loopbackCombo->setToolTip("Select system audio loopback (WebSDR from browser)");
    loopbackRow->addWidget(loopbackLabel);
    loopbackRow->addWidget(m_loopbackCombo, 1);
    groupLayout->addLayout(loopbackRow);

    // Row 3: Output
    QHBoxLayout* outputRow = new QHBoxLayout();
    QLabel* outputLabel = new QLabel("Output:", this);
    outputLabel->setFixedWidth(55);
    m_outputCombo = new QComboBox(this);
    m_outputCombo->setMinimumWidth(200);
    m_outputCombo->setToolTip("Select audio output device (headphones/speakers)");
    outputRow->addWidget(outputLabel);
    outputRow->addWidget(m_outputCombo, 1);

    groupLayout->addLayout(outputRow);

    mainLayout->addWidget(groupBox);
}

void DevicePanel::connectSignals()
{
    connect(m_inputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0) {
            emit inputDeviceChanged(m_inputCombo->currentData().toString());
        }
    });

    connect(m_loopbackCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0) {
            emit loopbackDeviceChanged(m_loopbackCombo->currentData().toString());
        }
    });

    connect(m_outputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0) {
            emit outputDeviceChanged(m_outputCombo->currentData().toString());
        }
    });
}

void DevicePanel::populateInputDevices(const QList<DeviceInfo>& devices)
{
    QString currentId = m_inputCombo->currentData().toString();
    m_inputCombo->blockSignals(true);
    m_inputCombo->clear();

    for (const auto& device : devices) {
        m_inputCombo->addItem(device.name, device.id);
    }

    // Restore selection
    int index = m_inputCombo->findData(currentId);
    if (index >= 0) {
        m_inputCombo->setCurrentIndex(index);
    }
    m_inputCombo->blockSignals(false);
}

void DevicePanel::populateLoopbackDevices(const QList<DeviceInfo>& devices)
{
    QString currentId = m_loopbackCombo->currentData().toString();
    m_loopbackCombo->blockSignals(true);
    m_loopbackCombo->clear();

    for (const auto& device : devices) {
        m_loopbackCombo->addItem(device.name, device.id);
    }

    // Restore selection
    int index = m_loopbackCombo->findData(currentId);
    if (index >= 0) {
        m_loopbackCombo->setCurrentIndex(index);
    }
    m_loopbackCombo->blockSignals(false);
}

void DevicePanel::populateOutputDevices(const QList<DeviceInfo>& devices)
{
    QString currentId = m_outputCombo->currentData().toString();
    m_outputCombo->blockSignals(true);
    m_outputCombo->clear();

    for (const auto& device : devices) {
        m_outputCombo->addItem(device.name, device.id);
    }

    // Restore selection
    int index = m_outputCombo->findData(currentId);
    if (index >= 0) {
        m_outputCombo->setCurrentIndex(index);
    }
    m_outputCombo->blockSignals(false);
}

QString DevicePanel::getSelectedInputId() const
{
    return m_inputCombo->currentData().toString();
}

QString DevicePanel::getSelectedLoopbackId() const
{
    return m_loopbackCombo->currentData().toString();
}

QString DevicePanel::getSelectedOutputId() const
{
    return m_outputCombo->currentData().toString();
}

void DevicePanel::setSelectedInputByName(const QString& name)
{
    int index = m_inputCombo->findText(name);
    if (index >= 0) {
        m_inputCombo->setCurrentIndex(index);
    }
}

void DevicePanel::setSelectedLoopbackByName(const QString& name)
{
    int index = m_loopbackCombo->findText(name);
    if (index >= 0) {
        m_loopbackCombo->setCurrentIndex(index);
    }
}

void DevicePanel::setSelectedOutputByName(const QString& name)
{
    int index = m_outputCombo->findText(name);
    if (index >= 0) {
        m_outputCombo->setCurrentIndex(index);
    }
}

QString DevicePanel::getSelectedInputName() const
{
    return m_inputCombo->currentText();
}

QString DevicePanel::getSelectedLoopbackName() const
{
    return m_loopbackCombo->currentText();
}

QString DevicePanel::getSelectedOutputName() const
{
    return m_outputCombo->currentText();
}
