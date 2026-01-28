#ifndef VBCABLEWIZARD_H
#define VBCABLEWIZARD_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>

/**
 * @brief Setup wizard for VB-Audio Virtual Cable installation
 *
 * Guides user through VB-Cable setup needed for WebSDR audio capture.
 */
class VBCableWizard : public QDialog {
    Q_OBJECT

public:
    explicit VBCableWizard(QWidget* parent = nullptr);
    ~VBCableWizard() override = default;

    /**
     * @brief Check if VB-Cable is installed and show wizard if needed
     * @param parent Parent widget
     * @return true if VB-Cable is available or wizard completed successfully
     */
    static bool checkAndShowWizard(QWidget* parent = nullptr);

    /**
     * @brief Check if VB-Cable is installed
     * @return true if VB-Cable device is found
     */
    static bool isVBCableInstalled();

private slots:
    void onDownloadClicked();
    void onRecheckClicked();
    void onContinueClicked();

private:
    QLabel* m_statusLabel;
    QLabel* m_instructionsLabel;
    QPushButton* m_downloadButton;
    QPushButton* m_recheckButton;
    QPushButton* m_continueButton;

    bool m_vbCableFound;

    void setupUI();
    void updateUI();
};

#endif // VBCABLEWIZARD_H
