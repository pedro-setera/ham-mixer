#ifndef DEVICEPANEL_H
#define DEVICEPANEL_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "audio/DeviceInfo.h"

/**
 * @brief Device selection panel with dropdowns for audio devices
 */
class DevicePanel : public QWidget {
    Q_OBJECT

public:
    explicit DevicePanel(QWidget* parent = nullptr);
    ~DevicePanel() override = default;

    /**
     * @brief Populate input device combo box
     */
    void populateInputDevices(const QList<DeviceInfo>& devices);

    /**
     * @brief Populate loopback device combo box
     */
    void populateLoopbackDevices(const QList<DeviceInfo>& devices);

    /**
     * @brief Populate output device combo box
     */
    void populateOutputDevices(const QList<DeviceInfo>& devices);

    /**
     * @brief Get selected input device ID
     */
    QString getSelectedInputId() const;

    /**
     * @brief Get selected loopback device ID
     */
    QString getSelectedLoopbackId() const;

    /**
     * @brief Get selected output device ID
     */
    QString getSelectedOutputId() const;

    /**
     * @brief Select device by name
     */
    void setSelectedInputByName(const QString& name);
    void setSelectedLoopbackByName(const QString& name);
    void setSelectedOutputByName(const QString& name);

    /**
     * @brief Get selected device names
     */
    QString getSelectedInputName() const;
    QString getSelectedLoopbackName() const;
    QString getSelectedOutputName() const;

signals:
    void inputDeviceChanged(const QString& deviceId);
    void loopbackDeviceChanged(const QString& deviceId);
    void outputDeviceChanged(const QString& deviceId);

private:
    QComboBox* m_inputCombo;
    QComboBox* m_loopbackCombo;
    QComboBox* m_outputCombo;

    void setupUI();
    void connectSignals();
};

#endif // DEVICEPANEL_H
