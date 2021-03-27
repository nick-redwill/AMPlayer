#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QPaintEvent>
#include <QPainterPath>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QInputDialog>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QMouseEventTransition>
#include <QMessageBox>
#include <QFileDialog>
#include <QPainter>
#include <QLabel>
#include <QSlider>
#include <QKeyEvent>
#include <QTimer>
#include <QImage>
#include <QImageReader>
#include <QPixmap>
#include <QFontDatabase>
#include <QFont>
#include <QScrollBar>
#include <QCommonStyle>
#include <QLineEdit>
#include <QDirIterator>
#include <QWheelEvent>
#include <QHBoxLayout>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <math.h>

#include "bass.h"
#include "song.h"
#include "settingswindow.h"
#include "playlistreader.h"
#include "fifo_map.hpp"

using namespace std;
using nlohmann::fifo_map;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    void reloadStyles();
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    bool openFile ();
    void removeFile();
    bool openFolder ();

    void createPlaylist();
    void removePlaylist();

    void changeCurrentPlaylist (QListWidgetItem *);
    void setActive(QListWidgetItem *);
    void setActive(int index);

    void search (const QString & text);

    void moveSongUp ();
    void moveSongDown ();

    void backward();
    void forward();
    void pause();
    void changeVolume(int vol);
    void updateTime();

    void changeRepeat ();
    void changeShuffle ();

    void settings ();
    void audio3D () {
        QMessageBox msgBox;
        msgBox.setWindowTitle("3D Audio");
        msgBox.setText("3D Audio functions will be added soon!");
        msgBox.setStyleSheet("background-color: #141414; color: silver;");
        msgBox.exec();
    }
    void equalizer () {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Equalizer");
        msgBox.setText("Equalizer will be added soon!");
        msgBox.setStyleSheet("background-color: #141414; color: silver;");
        msgBox.exec();
    }
    void visualizations () {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Visualizations");
        msgBox.setText("Visualizations will be added soon!");
        msgBox.setStyleSheet("background-color: #141414; color: silver;");
        msgBox.exec();
    }
    void metronome () {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Metronome");
        msgBox.setText("Metronome will be added soon!");
        msgBox.setStyleSheet("background-color: #141414; color: silver;");
        msgBox.exec();
    }

    void colorChange();

    void slot_minimize() { setWindowState(Qt::WindowMinimized); };
    void slot_close() { this->close(); };

private:
    HSTREAM channel;

    bool isCoverDrawed = false;
    bool paused = true;
    bool repeat = false;
    bool shuffle = false;
    bool liveSpec = false;
    bool colorChanging = false;

    float volume = 1;
    float prerenderedFft[1024];

    fifo_map <QString, vector <Song>> playlists;

    QString currentPlaylistName;
    QString playingSongPlaylist;
    vector <Song> playlist;
    int currentID;

    QLineEdit * searchSong;

    QWidget * titlebarWidget;

    QListWidget * playlistsWidget;
    QListWidget * playlistWidget;

    QLabel * songTitle;
    QLabel * songDuration;
    QLabel * songPosition;
    QLabel * timecode;
    QLabel * windowTitle;
    QSlider * volumeSlider;

    QImage cover;

    QPushButton * repeatBtn;
    QPushButton * shuffleBtn;
    QPushButton * pauseBtn;
    QPushButton * audio3dBtn;
    QPushButton * equoBtn;
    QPushButton * metronomeBtn;
    QPushButton * timerBtn;
    QPushButton * visualBtn;
    QPushButton * closeBtn;
    QPushButton * minimizeBtn;

    QPushButton * moveSongUpBtn;
    QPushButton * moveSongDownBtn;

    QPoint lastMousePosition;
    bool moving;

    QTimer * timer;

    Ui::MainWindow *ui;

    PlaylistReader * XMLreader;
    settingsWindow * settingsWin = nullptr;

    void searchInPlaylist(const QString & text);
    void drawAllPlaylists();
    void drawPlaylist();
    void setTitle();
    void prerenderFft ();
    void clearPrerenderedFft() {
        for (int i = 0; i < 1024; i++)
            prerenderedFft[i] = 3;
    }

    void closeEvent(QCloseEvent * event) {
        this->settingsWin->close();
    };
    void paintEvent(QPaintEvent * event);
    void mousePressEvent (QMouseEvent * event);
    void mouseMoveEvent (QMouseEvent * event);
    void wheelEvent (QWheelEvent * event);

    string seconds2string (float seconds);

    float getDuration () {
        QWORD len = BASS_ChannelGetLength(channel, BASS_POS_BYTE); // the length in bytes
        return BASS_ChannelBytes2Seconds(channel, len); // the length in seconds
    }
    float getPosition () {
        QWORD pos = BASS_ChannelGetPosition(channel, BASS_POS_BYTE);
        return BASS_ChannelBytes2Seconds(channel, pos);
    }

};
#endif // MAINWINDOW_H
