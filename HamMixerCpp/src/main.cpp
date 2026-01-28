#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

#include "HamMixer/Version.h"
#include "ui/MainWindow.h"
#include "ui/VBCableWizard.h"
#include "audio/WasapiDevice.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_WIN
// Check if Visual C++ Redistributable is installed by looking for required DLLs
bool checkVCRedistInstalled()
{
    // Check for key MSVC runtime DLLs in System32
    HMODULE hVcruntime = LoadLibraryA("vcruntime140.dll");
    HMODULE hVcruntime1 = LoadLibraryA("vcruntime140_1.dll");
    HMODULE hMsvcp = LoadLibraryA("msvcp140.dll");

    bool installed = (hVcruntime != nullptr && hMsvcp != nullptr);

    if (hVcruntime) FreeLibrary(hVcruntime);
    if (hVcruntime1) FreeLibrary(hVcruntime1);
    if (hMsvcp) FreeLibrary(hMsvcp);

    return installed;
}
#endif

int main(int argc, char *argv[])
{
    // Set high DPI settings before creating QApplication
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    // Set application info
    app.setApplicationName(HAMMIXER_APP_NAME);
    app.setApplicationVersion(HAMMIXER_VERSION_STRING);
    app.setOrganizationName(HAMMIXER_ORG_NAME);
    app.setOrganizationDomain(HAMMIXER_ORG_DOMAIN);

#ifdef Q_OS_WIN
    // Check for Visual C++ Redistributable
    if (!checkVCRedistInstalled()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Missing Dependency");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("Microsoft Visual C++ Redistributable is not installed.");
        msgBox.setInformativeText(
            "HamMixer requires the Visual C++ 2015-2022 Redistributable (x64) to run.\n\n"
            "Click 'Download' to open the Microsoft download page, then install and restart HamMixer.");
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.button(QMessageBox::Ok)->setText("Download");
        msgBox.setDefaultButton(QMessageBox::Ok);

        if (msgBox.exec() == QMessageBox::Ok) {
            QDesktopServices::openUrl(QUrl("https://aka.ms/vs/17/release/vc_redist.x64.exe"));
        }
        return -1;
    }

    // Hide console window on Windows
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd) {
        ShowWindow(consoleWnd, SW_HIDE);
    }
#endif

    qDebug() << "Starting" << HAMMIXER_APP_NAME << "v" << HAMMIXER_VERSION_STRING;

    // Initialize COM for WASAPI
    if (!WasapiDevice::initializeCOM()) {
        qCritical() << "Failed to initialize COM";
        return -1;
    }

    // Create recordings directory next to the executable (not current working dir)
    QString recordingsDir = QCoreApplication::applicationDirPath() + "/recordings";
    QDir().mkpath(recordingsDir);

    // Check for VB-Cable (optional - don't block startup)
    // VBCableWizard::checkAndShowWizard(nullptr);

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();

    qDebug() << "Main window shown";

    // Run event loop
    int result = app.exec();

    // Cleanup
    WasapiDevice::uninitializeCOM();

    qDebug() << "Application exiting with code" << result;
    return result;
}
