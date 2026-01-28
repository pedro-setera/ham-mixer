#include "ui/Crossfader.h"
#include "ui/Styles.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <cmath>

// Custom slider that resets to center on double-click
class DoubleClickSlider : public QSlider {
public:
    explicit DoubleClickSlider(Qt::Orientation orientation, QWidget* parent = nullptr)
        : QSlider(orientation, parent) {}

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        setValue(0);  // Reset to center
        event->accept();
    }
};

Crossfader::Crossfader(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    connectSignals();
}

void Crossfader::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 5);
    mainLayout->setSpacing(5);

    // Position label (CENTER, LEFT 50, RIGHT 75, etc.)
    m_positionLabel = new QLabel("CENTER", this);
    m_positionLabel->setAlignment(Qt::AlignCenter);
    m_positionLabel->setStyleSheet("QLabel { font-weight: bold; color: #00BCD4; }");
    mainLayout->addWidget(m_positionLabel);

    // Slider layout with Radio/WebSDR labels
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->setSpacing(10);

    // Radio label (left)
    m_radioLabel = new QLabel("RADIO", this);
    m_radioLabel->setStyleSheet("QLabel { font-weight: bold; color: #4CAF50; }");
    sliderLayout->addWidget(m_radioLabel);

    // Horizontal crossfader slider (double-click resets to center)
    m_slider = new DoubleClickSlider(Qt::Horizontal, this);
    m_slider->setRange(-100, 100);
    m_slider->setValue(0);  // Center
    m_slider->setTickPosition(QSlider::TicksBelow);
    m_slider->setTickInterval(25);
    m_slider->setMinimumWidth(200);
    m_slider->setToolTip("Double-click to center");
    sliderLayout->addWidget(m_slider, 1);

    // WebSDR label (right)
    m_websdrLabel = new QLabel("WEBSDR", this);
    m_websdrLabel->setStyleSheet("QLabel { font-weight: bold; color: #FF9800; }");
    sliderLayout->addWidget(m_websdrLabel);

    mainLayout->addLayout(sliderLayout);
}

void Crossfader::connectSignals()
{
    connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
        updatePositionLabel(value);
        emitCrossfaderChanged(value);
    });
}

void Crossfader::updatePositionLabel(int position)
{
    if (position == 0) {
        m_positionLabel->setText("CENTER");
    } else if (position < 0) {
        m_positionLabel->setText(QString("LEFT %1").arg(-position));
    } else {
        m_positionLabel->setText(QString("RIGHT %1").arg(position));
    }
}

void Crossfader::calculateChannelParams(int position,
                                         float& radioVol, float& radioPan,
                                         float& websdrVol, float& websdrPan)
{
    // Python-compatible crossfader behavior:
    // - At center (0): Radio full left, WebSDR full right (stereo separation)
    // - Moving left (-100): Radio moves to center pan, WebSDR fades out
    // - Moving right (+100): WebSDR moves to center pan, Radio fades out

    if (position <= 0) {
        // Moving towards Radio (left side)
        float t = -position / 100.0f;  // 0 at center, 1.0 at full left

        // Radio: always 100% volume
        radioVol = 1.0f;
        // Radio pan: -1.0 at center, 0.0 at full left (moves to center)
        radioPan = -1.0f + t;

        // WebSDR: volume fades out
        websdrVol = 1.0f - t;  // 1.0 at center, 0.0 at full left
        websdrPan = 1.0f;      // Always right
    } else {
        // Moving towards WebSDR (right side)
        float t = position / 100.0f;  // 0 at center, 1.0 at full right

        // Radio: volume fades out
        radioVol = 1.0f - t;  // 1.0 at center, 0.0 at full right
        radioPan = -1.0f;     // Always left

        // WebSDR: always 100% volume
        websdrVol = 1.0f;
        // WebSDR pan: 1.0 at center, 0.0 at full right (moves to center)
        websdrPan = 1.0f - t;
    }
}

void Crossfader::emitCrossfaderChanged(int position)
{
    float radioVol, radioPan, websdrVol, websdrPan;
    calculateChannelParams(position, radioVol, radioPan, websdrVol, websdrPan);
    emit crossfaderChanged(radioVol, radioPan, websdrVol, websdrPan);
}

void Crossfader::setPosition(int position)
{
    m_slider->setValue(position);
}

int Crossfader::getPosition() const
{
    return m_slider->value();
}
