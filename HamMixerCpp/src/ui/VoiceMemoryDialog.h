/*
 * VoiceMemoryDialog.h
 *
 * Dialog for configuring voice memory button labels
 * Part of HamMixer CT7BAC
 */

#ifndef VOICEMEMORYDIALOG_H
#define VOICEMEMORYDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QStringList>

/**
 * @brief Dialog for configuring voice memory button labels
 *
 * Allows user to set custom labels for the 8 voice memory buttons (M1-M8).
 * Labels are saved to the configuration file and persist across sessions.
 */
class VoiceMemoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VoiceMemoryDialog(const QStringList& labels, QWidget* parent = nullptr);
    ~VoiceMemoryDialog() override = default;

    /**
     * Get the modified list of labels
     */
    QStringList labels() const;

private slots:
    void onResetDefaults();

private:
    void setupUI();

    QLineEdit* m_labelEdits[8];
    QDialogButtonBox* m_buttonBox;
    QPushButton* m_resetButton;

    QStringList m_originalLabels;
};

#endif // VOICEMEMORYDIALOG_H
