/*
 * VoiceMemoryDialog.cpp
 *
 * Dialog for configuring voice memory button labels
 * Part of HamMixer CT7BAC
 */

#include "VoiceMemoryDialog.h"
#include "Styles.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>

VoiceMemoryDialog::VoiceMemoryDialog(const QStringList& labels, QWidget* parent)
    : QDialog(parent)
    , m_originalLabels(labels)
{
    setupUI();
}

void VoiceMemoryDialog::setupUI()
{
    setWindowTitle("Voice Memory Labels");
    setMinimumSize(400, 400);
    resize(400, 420);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // Info label
    QLabel* infoLabel = new QLabel(this);
    infoLabel->setText(
        "Configure custom labels for voice memory buttons.\n"
        "Labels longer than 12 characters will scroll on the button."
    );
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(
        "QLabel { background-color: #2D2D30; padding: 12px; border-radius: 4px; border: 1px solid #3F3F46; }"
    );
    mainLayout->addWidget(infoLabel);

    // Group box for labels
    QGroupBox* labelsGroup = new QGroupBox("Button Labels", this);
    QFormLayout* formLayout = new QFormLayout(labelsGroup);
    formLayout->setSpacing(10);
    formLayout->setContentsMargins(15, 20, 15, 15);

    // Create 8 label edit fields
    for (int i = 0; i < 8; i++) {
        m_labelEdits[i] = new QLineEdit(this);
        m_labelEdits[i]->setPlaceholderText(QString("M%1").arg(i + 1));

        // Set current label if available
        if (i < m_originalLabels.size()) {
            m_labelEdits[i]->setText(m_originalLabels[i]);
        }

        formLayout->addRow(QString("Memory %1:").arg(i + 1), m_labelEdits[i]);
    }

    mainLayout->addWidget(labelsGroup);

    // Button row with Reset Defaults
    QHBoxLayout* buttonRowLayout = new QHBoxLayout();

    m_resetButton = new QPushButton("Reset Defaults", this);
    m_resetButton->setFixedWidth(120);
    connect(m_resetButton, &QPushButton::clicked, this, &VoiceMemoryDialog::onResetDefaults);
    buttonRowLayout->addWidget(m_resetButton);

    buttonRowLayout->addStretch();

    // Dialog buttons (Cancel / OK)
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttonRowLayout->addWidget(m_buttonBox);

    mainLayout->addLayout(buttonRowLayout);
}

QStringList VoiceMemoryDialog::labels() const
{
    QStringList result;
    for (int i = 0; i < 8; i++) {
        QString text = m_labelEdits[i]->text().trimmed();
        if (text.isEmpty()) {
            text = QString("M%1").arg(i + 1);  // Default if empty
        }
        result.append(text);
    }
    return result;
}

void VoiceMemoryDialog::onResetDefaults()
{
    // Reset to factory defaults
    for (int i = 0; i < 8; i++) {
        m_labelEdits[i]->setText(QString("M%1").arg(i + 1));
    }
}
