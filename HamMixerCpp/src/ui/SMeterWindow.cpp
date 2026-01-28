#include "ui/SMeterWindow.h"
#include "ui/Styles.h"
#include <QHBoxLayout>
#include <QFrame>
#include <QCloseEvent>

SMeterWindow::SMeterWindow(QWidget* parent)
    : QWidget(parent)
    , m_forceClose(false)
{
    setupWindow();
    setupUI();
}

void SMeterWindow::setupWindow()
{
    setWindowTitle("HamMixer - S-Meters");
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

    // Apply dark theme
    setStyleSheet(Styles::getStylesheet());

    // Fixed size for consistent appearance (115 widget height + 20 margins)
    setFixedSize(560, 140);
}

void SMeterWindow::setupUI()
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(15, 10, 15, 10);
    layout->setSpacing(15);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    // Radio S-Meter
    m_radioMeter = new SMeter("Radio", this);
    layout->addWidget(m_radioMeter, 0, Qt::AlignCenter);

    // Separator line
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet("QFrame { color: #3A3A3A; }");
    layout->addWidget(separator);

    // WebSDR S-Meter
    m_websdrMeter = new SMeter("WebSDR", this);
    layout->addWidget(m_websdrMeter, 0, Qt::AlignCenter);
}

void SMeterWindow::updateRadioLevel(float db)
{
    m_radioMeter->setLevel(db);
}

void SMeterWindow::updateWebsdrLevel(float db)
{
    m_websdrMeter->setLevel(db);
}

void SMeterWindow::reset()
{
    // Reset both meters to minimum level (silence) - use -80dB to reach S0
    m_radioMeter->setLevel(-80.0f);
    m_websdrMeter->setLevel(-80.0f);
}

void SMeterWindow::forceClose()
{
    m_forceClose = true;
    close();
}

void SMeterWindow::closeEvent(QCloseEvent* event)
{
    if (m_forceClose) {
        event->accept();
    } else {
        // Hide instead of close on user action
        hide();
        event->ignore();
    }
}
