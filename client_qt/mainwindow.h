#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include "streamer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateRPiImage(const QImage& image); // 라파캠 영상 표시

private:
    Ui::MainWindow *ui;
    Streamer* rpiStreamer;
};

#endif // MAINWINDOW_H
