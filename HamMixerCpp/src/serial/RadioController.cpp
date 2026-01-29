/*
 * RadioController.cpp
 *
 * Auto-detection factory for radio control protocols
 * Part of HamMixer CT7BAC
 */

#include "RadioController.h"
#include "CIVController.h"
#include "KenwoodController.h"
#include <QEventLoop>
#include <QTimer>
#include <QDebug>

RadioController* RadioController::detectAndConnect(const QString& port, QObject* parent)
{
    qDebug() << "RadioController: Auto-detecting radio on port" << port;

    // === Try Icom CI-V first ===
    QList<int> icomBauds = {57600, 115200};
    for (int baud : icomBauds) {
        qDebug() << "RadioController: Trying Icom CI-V at" << baud << "baud...";

        CIVController* icom = new CIVController(parent);
        if (icom->connect(port, baud)) {
            // Send frequency request and wait for response
            icom->requestFrequency();

            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);

            bool gotResponse = false;
            QObject::connect(icom, &RadioController::frequencyChanged,
                [&](uint64_t) { gotResponse = true; loop.quit(); });
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

            timeout.start(800);  // 800ms timeout
            loop.exec();

            if (gotResponse && icom->currentFrequency() > 0) {
                qDebug() << "RadioController: Detected Icom CI-V radio at" << baud << "baud";
                qDebug() << "RadioController: Frequency:" << icom->currentFrequency() << "Hz";
                return icom;
            }

            qDebug() << "RadioController: No CI-V response at" << baud << "baud";
            icom->disconnect();
        }
        delete icom;
    }

    // === Try Kenwood/Elecraft/Yaesu CAT ===
    QList<int> kenwoodBauds = {38400, 9600, 4800};
    for (int baud : kenwoodBauds) {
        qDebug() << "RadioController: Trying Kenwood CAT at" << baud << "baud...";

        KenwoodController* kenwood = new KenwoodController(parent);
        if (kenwood->connect(port, baud)) {
            // Send frequency request and wait for response
            kenwood->requestFrequency();

            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);

            bool gotResponse = false;
            QObject::connect(kenwood, &RadioController::frequencyChanged,
                [&](uint64_t) { gotResponse = true; loop.quit(); });
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

            timeout.start(800);  // 800ms timeout
            loop.exec();

            if (gotResponse && kenwood->currentFrequency() > 0) {
                qDebug() << "RadioController: Detected Kenwood/Elecraft/Yaesu CAT radio at" << baud << "baud";
                qDebug() << "RadioController: Frequency:" << kenwood->currentFrequency() << "Hz";
                return kenwood;
            }

            qDebug() << "RadioController: No CAT response at" << baud << "baud";
            kenwood->disconnect();
        }
        delete kenwood;
    }

    qDebug() << "RadioController: No compatible radio detected on port" << port;
    return nullptr;
}
