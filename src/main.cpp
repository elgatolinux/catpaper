#include "Backend.h"
#include "Paths.h"
#include "PersistentSettings.h"
#include "PywalTheme.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QUrl>

#if defined(CATPAPER_LAYER_SHELL)
#include <LayerShellQt/window.h>
#endif

#ifndef CATPAPER_QML_DIR
#define CATPAPER_QML_DIR "qml"
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("catpaper");
    app.setOrganizationName("catpaper");
    app.setQuitOnLastWindowClosed(true);

    qmlRegisterType<Paths>("Catpaper", 1, 0, "Paths");
    qmlRegisterType<PywalTheme>("Catpaper", 1, 0, "PywalTheme");
    qmlRegisterType<Backend>("Catpaper", 1, 0, "Backend");
    qmlRegisterType<PersistentSettings>("Catpaper", 1, 0, "PersistentSettings");

    QQmlApplicationEngine engine;
    QString url = QStringLiteral("qrc:/qml/main.qml");
    const QString override = qEnvironmentVariable("CATPAPER_QML_DIR");
    if (!override.isEmpty())
        url = QUrl::fromLocalFile(override + "/main.qml").toString();
    engine.load(QUrl(url));

    const auto roots = engine.rootObjects();
    if (roots.isEmpty()) {
        fprintf(stderr, "ERROR: no se pudo cargar la UI (revisa errores QML arriba)\n");
        return 1;
    }
    QObject *root = roots.first();

#if defined(CATPAPER_LAYER_SHELL)
    if (app.platformName() == "wayland") {
        using LS = LayerShellQt::Window;
        auto *win = qobject_cast<QWindow *>(root);
        LS *ls = win ? LS::get(win) : nullptr;
        if (!ls) {
            fprintf(stderr, "AVISO: no se pudo activar layer-shell\n");
            return 1;
        }
        ls->setLayer(LS::LayerOverlay);
        ls->setAnchors(QFlags<LS::Anchor>(LS::AnchorLeft) | LS::AnchorRight | LS::AnchorTop |
                       LS::AnchorBottom);
        ls->setExclusiveZone(0);
        ls->setKeyboardInteractivity(LS::KeyboardInteractivityExclusive);
        ls->setWantsToBeOnActiveScreen(true);
    }
#endif
    root->setProperty("visible", QVariant::fromValue(true));

    if (qEnvironmentVariableIsSet("CATPAPER_SMOKE")) {
        QTimer::singleShot(5000, &app, &QCoreApplication::quit);
    }

    Backend *backend = root->findChild<Backend *>("backend");
    if (backend && qEnvironmentVariableIsSet("CATPAPER_AUTO_APPLY")) {
        const QString name = qEnvironmentVariable("CATPAPER_AUTO_APPLY");
        QObject::connect(backend, &Backend::wallpaperApplied, &app,
                         [](const QString &) { QCoreApplication::quit(); });
        QTimer::singleShot(700, backend, [backend, name] {
            backend->applyWallpaper(name, false, "all");
        });
    }

    return app.exec();
}
