#ifndef MES_MAINWINDOW
#define MES_MAINWINDOW

#include <QObject>
#include <QMainWindow>
#include "ui/ui_mainWindow.h"

class NotificationModel;

class MainWindow : public QMainWindow, private Ui_MainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent=0);
    ~MainWindow();

private:
};

#endif
