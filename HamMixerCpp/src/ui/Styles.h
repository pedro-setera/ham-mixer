#ifndef STYLES_H
#define STYLES_H

#include <QString>
#include <QColor>

/**
 * @brief Application color palette and stylesheet
 */
namespace Styles {

// Color palette
namespace Colors {
    // Background colors
    const QColor WindowBg("#1A1A1A");
    const QColor PanelBg("#252525");
    const QColor DarkBg("#1E1E1E");

    // Text colors
    const QColor TextPrimary("#E8E8E8");
    const QColor TextSecondary("#A0A0A0");
    const QColor TextDisabled("#606060");

    // Accent colors
    const QColor Accent("#00BCD4");
    const QColor AccentHover("#26C6DA");

    // Level meter colors
    const QColor MeterGreen("#4CAF50");
    const QColor MeterYellow("#FFEB3B");
    const QColor MeterRed("#F44336");
    const QColor MeterBackground("#1A1A1A");
    const QColor MeterSegmentOff("#2A2A2A");

    // Button colors
    const QColor ButtonBg("#3A3A3A");
    const QColor ButtonHover("#4A4A4A");
    const QColor ButtonPressed("#2A2A2A");
    const QColor StartButton("#4CAF50");
    const QColor StopButton("#FF9800");
    const QColor RecordButton("#F44336");
    const QColor MuteActive("#F44336");

    // Slider colors
    const QColor SliderTrack("#3A3A3A");
    const QColor SliderGroove("#2A2A2A");
    const QColor SliderHandle("#00BCD4");

    // Border colors
    const QColor Border("#3A3A3A");
    const QColor BorderLight("#4A4A4A");

    // Status colors
    const QColor StatusRunning("#4CAF50");
    const QColor StatusStopped("#808080");
    const QColor StatusError("#F44336");

    // S-Meter colors
    const QColor SMeterBackground("#2B2B2B");
    const QColor SMeterArc("#D4D4D4");
    const QColor SMeterNeedle("#F44336");
    const QColor SMeterText("#E8E8E8");
    const QColor SMeterScaleGreen("#4CAF50");
    const QColor SMeterScaleRed("#F44336");
}

// dB thresholds for meter colors
namespace Levels {
    constexpr float YellowThreshold = -12.0f;  // dB
    constexpr float RedThreshold = -3.0f;      // dB
    constexpr float MinLevel = -60.0f;         // dB
    constexpr float MaxLevel = 0.0f;           // dB
}

/**
 * @brief Get the main application stylesheet
 */
inline QString getStylesheet()
{
    return R"(
        /* Main Window */
        QMainWindow, QWidget {
            background-color: #1A1A1A;
            color: #E8E8E8;
            font-family: "Segoe UI";
            font-size: 10pt;
        }

        /* Group Box */
        QGroupBox {
            background-color: #252525;
            border: 1px solid #3A3A3A;
            border-radius: 4px;
            margin-top: 12px;
            padding: 10px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 5px;
            color: #E8E8E8;
        }

        /* Labels */
        QLabel {
            color: #E8E8E8;
            background: transparent;
        }
        QLabel[secondary="true"] {
            color: #A0A0A0;
            font-size: 9pt;
            background: transparent;
        }
        QLabel[channelLabel="true"] {
            font-size: 11pt;
            font-weight: bold;
            color: #00BCD4;
            background: transparent;
        }

        /* Push Buttons */
        QPushButton {
            background-color: #3A3A3A;
            border: 1px solid #4A4A4A;
            border-radius: 4px;
            padding: 6px 16px;
            color: #E8E8E8;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #4A4A4A;
        }
        QPushButton:pressed {
            background-color: #2A2A2A;
        }
        QPushButton:disabled {
            background-color: #2A2A2A;
            color: #606060;
        }

        /* Start Button */
        QPushButton[buttonType="start"] {
            background-color: #4CAF50;
            border-color: #66BB6A;
        }
        QPushButton[buttonType="start"]:hover {
            background-color: #66BB6A;
        }

        /* Stop Button */
        QPushButton[buttonType="stop"] {
            background-color: #FF9800;
            border-color: #FFA726;
        }
        QPushButton[buttonType="stop"]:hover {
            background-color: #FFA726;
        }

        /* Record Button */
        QPushButton[buttonType="record"] {
            background-color: #3A3A3A;
        }
        QPushButton[buttonType="record"]:checked {
            background-color: #F44336;
            border-color: #EF5350;
        }

        /* Mute Button */
        QPushButton[buttonType="mute"] {
            background-color: #3A3A3A;
        }
        QPushButton[buttonType="mute"]:checked {
            background-color: #F44336;
            border-color: #EF5350;
        }

        /* Combo Box */
        QComboBox {
            background-color: #3A3A3A;
            border: 1px solid #4A4A4A;
            border-radius: 4px;
            padding: 4px 8px;
            color: #E8E8E8;
            min-width: 150px;
        }
        QComboBox:hover {
            border-color: #00BCD4;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #2A2A2A;
            border: 1px solid #4A4A4A;
            selection-background-color: #00BCD4;
        }

        /* Sliders */
        QSlider {
            background: transparent;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #606060;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #00BCD4;
            width: 16px;
            margin: -5px 0;
            border-radius: 8px;
        }
        QSlider::groove:vertical {
            width: 6px;
            background: #606060;
            border-radius: 3px;
        }
        QSlider::handle:vertical {
            background: #00BCD4;
            height: 16px;
            margin: 0 -5px;
            border-radius: 8px;
        }

        /* Text Edit (Log) */
        QTextEdit {
            background-color: #1E1E1E;
            border: 1px solid #3A3A3A;
            border-radius: 4px;
            color: #E8E8E8;
            font-family: "Consolas";
            font-size: 9pt;
        }

        /* Tooltip */
        QToolTip {
            background-color: #2A2A2A;
            border: 1px solid #4A4A4A;
            color: #E8E8E8;
            padding: 4px;
        }

        /* Scroll Bar */
        QScrollBar:vertical {
            background: #1A1A1A;
            width: 10px;
        }
        QScrollBar::handle:vertical {
            background: #3A3A3A;
            border-radius: 5px;
            min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )";
}

} // namespace Styles

#endif // STYLES_H
