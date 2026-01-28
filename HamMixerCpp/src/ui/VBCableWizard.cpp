#include "ui/VBCableWizard.h"
#include "ui/Styles.h"
#include "audio/WasapiDevice.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>

VBCableWizard::VBCableWizard(QWidget* parent)
    : QDialog(parent)
    , m_vbCableFound(false)
{
    setWindowTitle("HamMixer - VB-Cable Setup");
    setFixedSize(500, 350);
    setStyleSheet(Styles::getStylesheet());

    setupUI();
    m_vbCableFound = isVBCableInstalled();
    updateUI();
}

void VBCableWizard::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Title
    QLabel* titleLabel = new QLabel("VB-Audio Virtual Cable Setup", this);
    titleLabel->setStyleSheet("QLabel { font-size: 14pt; font-weight: bold; color: #00BCD4; }");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Explanation
    QLabel* explainLabel = new QLabel(
        "HamMixer needs VB-Audio Virtual Cable to capture WebSDR audio from your browser.\n\n"
        "VB-Cable creates a virtual audio device that routes audio from Chrome/Firefox "
        "to HamMixer instead of your speakers.",
        this
    );
    explainLabel->setWordWrap(true);
    explainLabel->setStyleSheet("QLabel { color: #A0A0A0; }");
    mainLayout->addWidget(explainLabel);

    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("QLabel { font-size: 12pt; font-weight: bold; padding: 10px; }");
    mainLayout->addWidget(m_statusLabel);

    // Instructions
    m_instructionsLabel = new QLabel(this);
    m_instructionsLabel->setWordWrap(true);
    m_instructionsLabel->setStyleSheet("QLabel { color: #E8E8E8; }");
    mainLayout->addWidget(m_instructionsLabel);

    mainLayout->addStretch();

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    m_downloadButton = new QPushButton("Download VB-Cable", this);
    m_downloadButton->setStyleSheet("QPushButton { background-color: #2196F3; }");
    connect(m_downloadButton, &QPushButton::clicked, this, &VBCableWizard::onDownloadClicked);
    buttonLayout->addWidget(m_downloadButton);

    m_recheckButton = new QPushButton("Re-check", this);
    connect(m_recheckButton, &QPushButton::clicked, this, &VBCableWizard::onRecheckClicked);
    buttonLayout->addWidget(m_recheckButton);

    buttonLayout->addStretch();

    m_continueButton = new QPushButton("Continue", this);
    m_continueButton->setProperty("buttonType", "start");
    connect(m_continueButton, &QPushButton::clicked, this, &VBCableWizard::onContinueClicked);
    buttonLayout->addWidget(m_continueButton);

    mainLayout->addLayout(buttonLayout);
}

void VBCableWizard::updateUI()
{
    if (m_vbCableFound) {
        m_statusLabel->setText("VB-Cable Detected!");
        m_statusLabel->setStyleSheet("QLabel { font-size: 12pt; font-weight: bold; padding: 10px; color: #4CAF50; background-color: #1B3B1B; border-radius: 5px; }");

        m_instructionsLabel->setText(
            "VB-Cable is installed and ready.\n\n"
            "To capture WebSDR audio:\n"
            "1. Open Windows Sound Settings\n"
            "2. Set 'CABLE Input (VB-Audio Virtual Cable)' as Chrome's output\n"
            "3. In HamMixer, select the loopback device for 'System Audio'\n\n"
            "Click 'Continue' to start HamMixer."
        );

        m_downloadButton->hide();
        m_recheckButton->hide();
        m_continueButton->setEnabled(true);
    } else {
        m_statusLabel->setText("VB-Cable Not Found");
        m_statusLabel->setStyleSheet("QLabel { font-size: 12pt; font-weight: bold; padding: 10px; color: #FF9800; background-color: #3B2F1B; border-radius: 5px; }");

        m_instructionsLabel->setText(
            "Please install VB-Audio Virtual Cable:\n\n"
            "1. Click 'Download VB-Cable' to open the download page\n"
            "2. Download and run VBCABLE_Driver_PackXX.exe\n"
            "3. Restart your computer after installation\n"
            "4. Click 'Re-check' to verify installation\n\n"
            "You can also continue without VB-Cable, but WebSDR capture won't work."
        );

        m_downloadButton->show();
        m_recheckButton->show();
        m_continueButton->setEnabled(true);
    }
}

bool VBCableWizard::isVBCableInstalled()
{
    // Enumerate output devices and look for VB-Cable
    QList<DeviceInfo> devices = WasapiDevice::enumerateDevices(WasapiDevice::DeviceType::Render);

    for (const auto& device : devices) {
        if (device.name.contains("VB-Audio", Qt::CaseInsensitive) ||
            device.name.contains("CABLE", Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

bool VBCableWizard::checkAndShowWizard(QWidget* parent)
{
    // Always check on first run, but don't block if user continues
    VBCableWizard wizard(parent);

    if (wizard.m_vbCableFound) {
        // VB-Cable found, no need to show wizard
        return true;
    }

    // Show wizard for user to install or skip
    return wizard.exec() == QDialog::Accepted;
}

void VBCableWizard::onDownloadClicked()
{
    QDesktopServices::openUrl(QUrl("https://vb-audio.com/Cable/"));
}

void VBCableWizard::onRecheckClicked()
{
    m_vbCableFound = isVBCableInstalled();
    updateUI();
}

void VBCableWizard::onContinueClicked()
{
    accept();
}
