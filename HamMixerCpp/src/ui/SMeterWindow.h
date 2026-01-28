#ifndef SMETERWINDOW_H
#define SMETERWINDOW_H

#include <QWidget>
#include "ui/SMeter.h"

/**
 * @brief Floating window displaying dual S-Meters for Radio and WebSDR
 */
class SMeterWindow : public QWidget {
    Q_OBJECT

public:
    explicit SMeterWindow(QWidget* parent = nullptr);
    ~SMeterWindow() override = default;

    /**
     * @brief Update Radio S-Meter level
     * @param db Level in dBFS
     */
    void updateRadioLevel(float db);

    /**
     * @brief Update WebSDR S-Meter level
     * @param db Level in dBFS
     */
    void updateWebsdrLevel(float db);

    /**
     * @brief Reset both meters to minimum (silence)
     */
    void reset();

    /**
     * @brief Force close the window (for app shutdown)
     */
    void forceClose();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    SMeter* m_radioMeter;
    SMeter* m_websdrMeter;
    bool m_forceClose;

    void setupWindow();
    void setupUI();
};

#endif // SMETERWINDOW_H
