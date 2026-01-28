/*
 * WebSdrManagerDialog.cpp
 *
 * Dialog for managing WebSDR sites (Add, Edit, Delete, Reorder)
 * Part of HamMixer CT7BAC
 */

#include "WebSdrManagerDialog.h"
#include "Styles.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QUuid>
#include <QDebug>

WebSdrManagerDialog::WebSdrManagerDialog(const QList<WebSdrSite>& sites, QWidget* parent)
    : QDialog(parent)
    , m_sites(sites)
{
    setupUI();
    refreshList();
    updateButtonStates();
}

void WebSdrManagerDialog::setupUI()
{
    setWindowTitle("Manage WebSDR Sites");
    setMinimumSize(650, 450);
    resize(650, 450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // Horizontal layout: list + buttons
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(10);

    // List widget showing sites
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setStyleSheet(
        "QListWidget { background-color: #2D2D30; border: 1px solid #3F3F46; }"
        "QListWidget::item { padding: 8px; background-color: #2D2D30; }"
        "QListWidget::item:selected { background-color: #0078D4; }"
    );
    contentLayout->addWidget(m_listWidget, 1);

    // Button column
    QVBoxLayout* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(8);

    m_moveUpButton = new QPushButton("Move Up", this);
    m_moveDownButton = new QPushButton("Move Down", this);
    m_addButton = new QPushButton("Add", this);
    m_editButton = new QPushButton("Edit", this);
    m_deleteButton = new QPushButton("Delete", this);

    // Set fixed width for buttons
    int buttonWidth = 110;
    m_moveUpButton->setFixedWidth(buttonWidth);
    m_moveDownButton->setFixedWidth(buttonWidth);
    m_addButton->setFixedWidth(buttonWidth);
    m_editButton->setFixedWidth(buttonWidth);
    m_deleteButton->setFixedWidth(buttonWidth);

    // Style delete button
    m_deleteButton->setStyleSheet(
        "QPushButton { background-color: #8B0000; }"
        "QPushButton:hover { background-color: #A52A2A; }"
        "QPushButton:disabled { background-color: #4A4A4A; color: #808080; }"
    );

    buttonLayout->addWidget(m_moveUpButton);
    buttonLayout->addWidget(m_moveDownButton);
    buttonLayout->addSpacing(15);
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addStretch();

    contentLayout->addLayout(buttonLayout);
    mainLayout->addLayout(contentLayout);

    // Info label about WebSDR 2.x compatibility
    QLabel* infoLabel = new QLabel(this);
    infoLabel->setText(
        "<b>Note:</b> Only WebSDR 2.x sites are supported (software by PA3FWM).<br>"
        "Visit <a href='http://websdr.org' style='color: #0078D4;'>http://websdr.org</a> for a list of available sites."
    );
    infoLabel->setOpenExternalLinks(true);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(
        "QLabel { background-color: #2D2D30; padding: 12px; border-radius: 4px; border: 1px solid #3F3F46; }"
    );
    mainLayout->addWidget(infoLabel);

    // Dialog buttons (Cancel / Save)
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Save, this);
    m_buttonBox->button(QDialogButtonBox::Save)->setText("Save");
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);

    // Connect signals
    connect(m_moveUpButton, &QPushButton::clicked, this, &WebSdrManagerDialog::onMoveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &WebSdrManagerDialog::onMoveDown);
    connect(m_addButton, &QPushButton::clicked, this, &WebSdrManagerDialog::onAdd);
    connect(m_editButton, &QPushButton::clicked, this, &WebSdrManagerDialog::onEdit);
    connect(m_deleteButton, &QPushButton::clicked, this, &WebSdrManagerDialog::onDelete);
    connect(m_listWidget, &QListWidget::itemSelectionChanged,
            this, &WebSdrManagerDialog::onSelectionChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &WebSdrManagerDialog::onItemDoubleClicked);
}

void WebSdrManagerDialog::refreshList()
{
    int currentRow = m_listWidget->currentRow();
    m_listWidget->clear();

    for (const WebSdrSite& site : m_sites) {
        // Create item with name and URL
        QString displayText = QString("%1\n%2").arg(site.name, site.url);
        QListWidgetItem* item = new QListWidgetItem(displayText, m_listWidget);
        item->setData(Qt::UserRole, site.id);
    }

    // Restore selection
    if (currentRow >= 0 && currentRow < m_listWidget->count()) {
        m_listWidget->setCurrentRow(currentRow);
    } else if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    }

    updateButtonStates();
}

void WebSdrManagerDialog::updateButtonStates()
{
    int row = m_listWidget->currentRow();
    int count = m_listWidget->count();

    bool hasSelection = row >= 0;
    bool canMoveUp = row > 0;
    bool canMoveDown = row >= 0 && row < count - 1;

    m_moveUpButton->setEnabled(canMoveUp);
    m_moveDownButton->setEnabled(canMoveDown);
    m_editButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
}

void WebSdrManagerDialog::onSelectionChanged()
{
    updateButtonStates();
}

void WebSdrManagerDialog::onItemDoubleClicked(QListWidgetItem* item)
{
    Q_UNUSED(item)
    onEdit();
}

void WebSdrManagerDialog::onMoveUp()
{
    int row = m_listWidget->currentRow();
    if (row > 0) {
        m_sites.swapItemsAt(row, row - 1);
        refreshList();
        m_listWidget->setCurrentRow(row - 1);
    }
}

void WebSdrManagerDialog::onMoveDown()
{
    int row = m_listWidget->currentRow();
    if (row >= 0 && row < m_sites.size() - 1) {
        m_sites.swapItemsAt(row, row + 1);
        refreshList();
        m_listWidget->setCurrentRow(row + 1);
    }
}

void WebSdrManagerDialog::onAdd()
{
    WebSdrSite site;
    site.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);

    if (showSiteDialog(site, false)) {
        m_sites.append(site);
        refreshList();
        m_listWidget->setCurrentRow(m_sites.size() - 1);
    }
}

void WebSdrManagerDialog::onEdit()
{
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_sites.size()) {
        return;
    }

    WebSdrSite site = m_sites[row];
    if (showSiteDialog(site, true)) {
        m_sites[row] = site;
        refreshList();
    }
}

void WebSdrManagerDialog::onDelete()
{
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_sites.size()) {
        return;
    }

    const WebSdrSite& site = m_sites[row];

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Delete Site",
        QString("Are you sure you want to delete \"%1\"?").arg(site.name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        m_sites.removeAt(row);
        refreshList();
    }
}

bool WebSdrManagerDialog::showSiteDialog(WebSdrSite& site, bool isEdit)
{
    QDialog dialog(this);
    dialog.setWindowTitle(isEdit ? "Edit WebSDR Site" : "Add WebSDR Site");
    dialog.setMinimumWidth(400);

    QFormLayout* layout = new QFormLayout(&dialog);
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    QLineEdit* nameEdit = new QLineEdit(&dialog);
    nameEdit->setText(site.name);
    nameEdit->setPlaceholderText("e.g., Utah US");

    QLineEdit* urlEdit = new QLineEdit(&dialog);
    urlEdit->setText(site.url);
    urlEdit->setPlaceholderText("http://example.com:8901");

    layout->addRow("Name:", nameEdit);
    layout->addRow("URL:", urlEdit);

    // Info label
    QLabel* hintLabel = new QLabel(
        "Enter the name as you want it to appear in the dropdown,\n"
        "and the full URL of the WebSDR 2.x site.",
        &dialog
    );
    hintLabel->setStyleSheet("QLabel { color: #808080; font-size: 10pt; }");
    layout->addRow(hintLabel);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);

    // Focus on name field
    nameEdit->setFocus();
    nameEdit->selectAll();

    if (dialog.exec() == QDialog::Accepted) {
        QString name = nameEdit->text().trimmed();
        QString url = urlEdit->text().trimmed();

        if (name.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Please enter a name for the site.");
            return false;
        }

        if (url.isEmpty()) {
            QMessageBox::warning(this, "Invalid Input", "Please enter the URL for the site.");
            return false;
        }

        // Ensure URL starts with http:// or https://
        if (!url.startsWith("http://") && !url.startsWith("https://")) {
            url = "http://" + url;
        }

        site.name = name;
        site.url = url;
        return true;
    }

    return false;
}
