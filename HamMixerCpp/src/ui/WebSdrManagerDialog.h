/*
 * WebSdrManagerDialog.h
 *
 * Dialog for managing SDR sites (WebSDR 2.x and KiwiSDR)
 * Part of HamMixer CT7BAC
 */

#ifndef WEBSDRMANAGERDIALOG_H
#define WEBSDRMANAGERDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QList>

#include "websdr/WebSdrSite.h"

/**
 * @brief Dialog for managing SDR sites
 *
 * Allows user to add, edit, delete, and reorder SDR sites.
 * Supports both WebSDR 2.x and KiwiSDR site types.
 */
class WebSdrManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WebSdrManagerDialog(const QList<WebSdrSite>& sites, QWidget* parent = nullptr);
    ~WebSdrManagerDialog() override = default;

    /**
     * Get the modified list of sites
     */
    QList<WebSdrSite> sites() const { return m_sites; }

private slots:
    void onMoveUp();
    void onMoveDown();
    void onAdd();
    void onEdit();
    void onDelete();
    void onSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void setupUI();
    void refreshList();
    void updateButtonStates();
    bool showSiteDialog(WebSdrSite& site, bool isEdit);

    QListWidget* m_listWidget;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;
    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;
    QDialogButtonBox* m_buttonBox;

    QList<WebSdrSite> m_sites;
};

#endif // WEBSDRMANAGERDIALOG_H
