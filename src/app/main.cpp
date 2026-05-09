#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QMenu>
#include <QMessageBox>
#include <QStyle>
#include <QSystemTrayIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Meet Sentinel");
    QApplication::setOrganizationName("Meet Sentinel");
    QApplication::setQuitOnLastWindowClosed(false);

    if(!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "Meet Sentinel", "No system tray is available on this desktop.");
        return 1;
    }

    QMenu tray_menu;
    QAction* quit_action = tray_menu.addAction("Quit");
    QObject::connect(quit_action, &QAction::triggered, &app, &QCoreApplication::quit);

    QSystemTrayIcon tray_icon;
    tray_icon.setContextMenu(&tray_menu);
    tray_icon.setIcon(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation));
    tray_icon.setToolTip("Meet Sentinel");
    tray_icon.show();

    return QApplication::exec();
}
