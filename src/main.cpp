// halo: settings for the Glorious GHS Wireless InfinitePlay headset on Linux.
// An unofficial, open-source companion; not made by or affiliated with Glorious.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "cli.h"
#include "headset.h"
#include "hidtransport.h"
#include "theme.h"
#include "translator.h"

static constexpr auto Version = "0.1.0";

int main(int argc, char *argv[]) {
    QStringList args;
    for (int i = 0; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);

    // Everything but `halo open` runs without a display.
    if (Cli::handles(args)) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("halo");
        app.setApplicationVersion(Version);
        return Cli::run(args);
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("halo");
    app.setApplicationVersion(Version);

    // Matches the window to halo.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("halo");
    app.setWindowIcon(QIcon::fromTheme("halo"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    HidTransport transport;
    Headset headset(&transport);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("headset", &headset);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
