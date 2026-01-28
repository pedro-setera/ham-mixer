#include "ui/MasterStrip.h"
#include "ui/Styles.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

MasterStrip::MasterStrip(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    connectSignals();
}

void MasterStrip::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(8);

    // Title row with label and percentage side by side, centered
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(2);
    titleLayout->setAlignment(Qt::AlignCenter);

    m_titleLabel = new QLabel("Master", this);
    m_titleLabel->setProperty("channelLabel", true);
    titleLayout->addWidget(m_titleLabel);

    m_volumeLabel = new QLabel("80%", this);
    m_volumeLabel->setProperty("channelLabel", true);
    m_volumeLabel->setFixedWidth(45);  // Fixed width to prevent shifting
    m_volumeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    titleLayout->addWidget(m_volumeLabel);

    mainLayout->addLayout(titleLayout);

    // Horizontal layout for slider and meter
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(10);
    controlsLayout->setAlignment(Qt::AlignCenter);

    // Volume slider (vertical) - same height as level meter
    // Height: 40 segments × 6px (4 height + 2 gap) - 2 + 4 padding = 242px
    m_volumeSlider = new QSlider(Qt::Vertical, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedHeight(242);
    m_volumeSlider->setToolTip("Master Volume");
    controlsLayout->addWidget(m_volumeSlider, 0, Qt::AlignCenter);

    // Stereo level meter
    m_levelMeter = new LevelMeter(true, this);
    m_levelMeter->setFixedHeight(242);
    controlsLayout->addWidget(m_levelMeter, 0, Qt::AlignCenter);

    mainLayout->addLayout(controlsLayout);

    // Mute button
    m_muteButton = new QPushButton("MUTE", this);
    m_muteButton->setCheckable(true);
    m_muteButton->setProperty("buttonType", "mute");
    m_muteButton->setToolTip("Mute master output");
    m_muteButton->setFixedWidth(70);
    mainLayout->addWidget(m_muteButton, 0, Qt::AlignCenter);
}

void MasterStrip::connectSignals()
{
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        updateVolumeLabel(value);
        emit volumeChanged(value);
    });

    connect(m_muteButton, &QPushButton::toggled, this, &MasterStrip::muteChanged);
}

void MasterStrip::updateVolumeLabel(int volume)
{
    m_volumeLabel->setText(QString("%1%").arg(volume));
}

void MasterStrip::setVolume(int volume)
{
    m_volumeSlider->setValue(volume);
    updateVolumeLabel(volume);
}

int MasterStrip::getVolume() const
{
    return m_volumeSlider->value();
}

void MasterStrip::updateLevel(float leftDb, float rightDb)
{
    m_levelMeter->setLevels(leftDb, rightDb);
}

void MasterStrip::resetMeter()
{
    m_levelMeter->reset();
}

void MasterStrip::setMuted(bool muted)
{
    m_muteButton->setChecked(muted);
}

bool MasterStrip::isMuted() const
{
    return m_muteButton->isChecked();
}
