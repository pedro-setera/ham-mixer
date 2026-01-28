#include "ui/TransportControls.h"
#include "ui/Styles.h"
#include <QHBoxLayout>

TransportControls::TransportControls(QWidget* parent)
    : QWidget(parent)
    , m_audioSourceMode(Both)
    , m_recording(false)
    , m_blinkState(false)
{
    setupUI();
}

void TransportControls::setupUI()
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(10);

    // Audio source toggle button (BOTH/RADIO/WEBSDR)
    m_sourceToggleButton = new QPushButton("BOTH", this);
    m_sourceToggleButton->setFixedWidth(100);
    m_sourceToggleButton->setToolTip("Toggle audio source: BOTH → RADIO → WEBSDR → BOTH");
    connect(m_sourceToggleButton, &QPushButton::clicked,
            this, &TransportControls::onSourceToggleClicked);
    layout->addWidget(m_sourceToggleButton);

    // Record button
    m_recordButton = new QPushButton("REC", this);
    m_recordButton->setProperty("buttonType", "record");
    m_recordButton->setCheckable(true);
    m_recordButton->setFixedWidth(60);
    m_recordButton->setEnabled(false);  // Disabled until connected
    connect(m_recordButton, &QPushButton::clicked,
            this, &TransportControls::recordClicked);
    layout->addWidget(m_recordButton);

    // Recording indicator (12px red circle, blinks when recording)
    m_recordIndicator = new QLabel(this);
    m_recordIndicator->setFixedSize(12, 12);
    m_recordIndicator->setStyleSheet("background-color: transparent; border-radius: 6px;");
    m_recordIndicator->hide();
    layout->addWidget(m_recordIndicator);

    layout->addStretch();

    // Blink timer for recording indicator
    m_blinkTimer = new QTimer(this);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
        m_blinkState = !m_blinkState;
        m_recordIndicator->setStyleSheet(
            m_blinkState ?
            "background-color: #F44336; border-radius: 6px;" :
            "background-color: #800000; border-radius: 6px;"
        );
    });

    // Initialize button appearance
    updateSourceButtonText();
}

void TransportControls::onSourceToggleClicked()
{
    // Circular toggle: BOTH → RADIO → WEBSDR → BOTH
    switch (m_audioSourceMode) {
        case Both:
            m_audioSourceMode = RadioOnly;
            break;
        case RadioOnly:
            m_audioSourceMode = WebSdrOnly;
            break;
        case WebSdrOnly:
            m_audioSourceMode = Both;
            break;
    }
    updateSourceButtonText();
    emit audioSourceModeChanged(m_audioSourceMode);
}

void TransportControls::setAudioSourceMode(AudioSourceMode mode)
{
    if (m_audioSourceMode != mode) {
        m_audioSourceMode = mode;
        updateSourceButtonText();
        emit audioSourceModeChanged(m_audioSourceMode);
    }
}

void TransportControls::updateSourceButtonText()
{
    switch (m_audioSourceMode) {
        case Both:
            m_sourceToggleButton->setText("BOTH");
            m_sourceToggleButton->setStyleSheet("");  // Default style
            break;
        case RadioOnly:
            m_sourceToggleButton->setText("RADIO");
            m_sourceToggleButton->setStyleSheet(
                "QPushButton { background-color: #2E7D32; }"
                "QPushButton:hover { background-color: #388E3C; }"
            );
            break;
        case WebSdrOnly:
            m_sourceToggleButton->setText("WEBSDR");
            m_sourceToggleButton->setStyleSheet(
                "QPushButton { background-color: #1565C0; }"
                "QPushButton:hover { background-color: #1976D2; }"
            );
            break;
    }
}

void TransportControls::setRecordingActive(bool recording)
{
    m_recording = recording;
    m_recordButton->setChecked(recording);

    if (recording) {
        m_recordIndicator->show();
        m_blinkState = true;
        m_recordIndicator->setStyleSheet("background-color: #F44336; border-radius: 6px;");
        m_blinkTimer->start(500);  // Blink every 500ms
    } else {
        m_recordIndicator->hide();
        m_blinkTimer->stop();
    }
}

void TransportControls::setRecordEnabled(bool enabled)
{
    m_recordButton->setEnabled(enabled);
}
