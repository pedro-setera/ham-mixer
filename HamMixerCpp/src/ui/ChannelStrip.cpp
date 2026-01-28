#include "ui/ChannelStrip.h"
#include "ui/Styles.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

ChannelStrip::ChannelStrip(const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_title(title)
{
    setupUI();
}

void ChannelStrip::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // Title row: label + percentage
    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setSpacing(4);

    m_titleLabel = new QLabel(m_title, this);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setProperty("channelLabel", true);
    titleRow->addWidget(m_titleLabel);

    m_volumeLabel = new QLabel("100%", this);
    m_volumeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_volumeLabel->setFixedWidth(40);
    m_volumeLabel->setStyleSheet("QLabel { color: #FFFFFF; }");
    titleRow->addWidget(m_volumeLabel);

    mainLayout->addLayout(titleRow);

    // Controls row: slider + meter side by side
    QHBoxLayout* controlsRow = new QHBoxLayout();
    controlsRow->setSpacing(4);
    controlsRow->setAlignment(Qt::AlignCenter);

    // Volume slider (vertical) - reduced height to fit in shorter Levels section
    m_volumeSlider = new QSlider(Qt::Vertical, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedHeight(130);  // Reduced by 30px from 160
    m_volumeSlider->setToolTip("Channel Volume");
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        updateVolumeLabel(value);
        emit volumeChanged(value);
    });
    controlsRow->addWidget(m_volumeSlider);

    // Level meter (mono) - reduced height to fit in shorter Levels section
    m_levelMeter = new LevelMeter(false, this);
    m_levelMeter->setFixedWidth(36);
    m_levelMeter->setFixedHeight(130);  // Reduced by 30px from 160
    controlsRow->addWidget(m_levelMeter);

    mainLayout->addLayout(controlsRow);

    // Mute button at bottom
    m_muteButton = new QPushButton("MUTE", this);
    m_muteButton->setCheckable(true);
    m_muteButton->setProperty("buttonType", "mute");
    m_muteButton->setToolTip("Mute this channel");
    m_muteButton->setFixedWidth(70);
    connect(m_muteButton, &QPushButton::toggled, this, &ChannelStrip::muteChanged);
    mainLayout->addWidget(m_muteButton, 0, Qt::AlignCenter);
}

void ChannelStrip::updateVolumeLabel(int value)
{
    m_volumeLabel->setText(QString("%1%").arg(value));
}

void ChannelStrip::updateLevel(float db)
{
    m_levelMeter->setLevel(db);
}

void ChannelStrip::resetMeter()
{
    m_levelMeter->reset();
}

void ChannelStrip::setMuted(bool muted)
{
    // Block signals to avoid triggering muteChanged when setting programmatically
    m_muteButton->blockSignals(true);
    m_muteButton->setChecked(muted);
    m_muteButton->blockSignals(false);
}

bool ChannelStrip::isMuted() const
{
    return m_muteButton->isChecked();
}

void ChannelStrip::setVolume(int volume)
{
    m_volumeSlider->blockSignals(true);
    m_volumeSlider->setValue(qBound(0, volume, 100));
    updateVolumeLabel(volume);
    m_volumeSlider->blockSignals(false);
}

int ChannelStrip::getVolume() const
{
    return m_volumeSlider->value();
}
