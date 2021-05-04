#include "mainwindow.h"
#include "ui_mainwindow.h"

QColor mainColor(255, 37, 79);
QColor nextColor(255, 37, 79);
string mainColorStr = "rgb(255, 37, 79)";
int colorChangeSpeed = 20;

bool isOrderChanged = false;

QImage applyEffectToImage(QImage src, QGraphicsEffect * effect, int extent = 0)
{
    if(src.isNull()) return QImage();   // No need to do anything else!
    if(!effect) return src;             // No need to do anything else!

    QGraphicsScene scene;
    QGraphicsPixmapItem item;
    item.setPixmap(QPixmap::fromImage(src));
    item.setGraphicsEffect(effect);
    scene.addItem(&item);

    QImage res(src.size()+QSize(extent * 2, extent * 2), QImage::Format_ARGB32);
    res.fill(Qt::transparent);

    QPainter ptr(&res);
    scene.render(&ptr, QRectF(), QRectF(-extent, -extent, src.width() + extent * 2, src.height() + extent * 2));

    return res;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{   
    removeControlServer = new QWebSocketServer("Remote Control Server", QWebSocketServer::NonSecureMode, this);

    httpServer = new QTcpServer(this);
    httpServerPort = rand() % 9999;

    starttime = clock();

    if (logging) {
        int current = time(NULL);

        logfile.setFileName(QDir::currentPath() + "/logs/log_" + QString::number(current) + ".txt");

        if (!logfile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qDebug() << "Can't open log file";
            logging = false;
        } else {
            writeLog ("Program initialized");
        }
    }

    // Reading playlists from XML file
    XMLreader = new PlaylistReader(QDir::currentPath() + "/XML/playlists.xml");
    XMLreader->readPlaylists(playlists);

    if (playlists.size() > 0)
    {
        writeLog ("Playlists loaded from XML file");
        currentPlaylistName = playlists.begin()->first; // Setting current playlist name
        playlist = playlists[currentPlaylistName];      // Setting current playlist
    }
    else {
        writeErrorLog ("Can't load/open playlists XML file");
        currentPlaylistName = "Default";       // Setting current playlist name
        playlist = playlists ["Default"];      // Setting current playlist
    }

    clearPrerenderedFft();

    int id = QFontDatabase::addApplicationFont(":/Font Awesome 5 Pro Solid.ttf");
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    QFont fontAwesome(family);
    fontAwesome.setStyleStrategy(QFont::PreferAntialias);

    channel = NULL;

    timer = new QTimer();
    timer->setInterval(1000 / 60); // 60 FPS
    connect(timer, SIGNAL(timeout()), this, SLOT(updateTime()));
    timer->start();

    ui->setupUi(this);

    // Window settings
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint); // Window transparency
    this->setStyleSheet("QMainWindow { background-color: #101010; }"
                        "QInputDialog, QInputDialog * { background-color: #101010; color: silver; }"
                        "QToolTip { background-color: #101010; color: silver; font-size: 12px; border: 1px solid silver; }");

    this->setWindowTitle("AMPlayer v1.0a");
    this->setMouseTracking(true);
    this->centralWidget()->setMouseTracking(true);

    // Album cover load
    QImageReader reader(QDir::currentPath() + "/Images/cover-placeholder2.png");
    cover = reader.read();

    if (cover.isNull()) {
        coverLoaded = false;
        cover = QImage (":/Images/cover-placeholder.png");

        qDebug() << reader.errorString();
        writeLog ("Can't load cover! " + reader.errorString());
    }

    settingsWin = new settingsWindow();
    equalizerWin = new equalizerWindow();
    equalizerWin->mainColorStr = &mainColorStr;
    equalizerWin->reloadStyles();
    visualWin = new VisualizationWindow(nullptr, &channel);

    QHBoxLayout * horizontalLayout = new QHBoxLayout();
    horizontalLayout->setSpacing(0);
    horizontalLayout->setMargin(0);

    titlebarWidget = new QWidget(this);
    titlebarWidget->setObjectName("titlebarWidget");
    titlebarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    titlebarWidget->setLayout(horizontalLayout);
    titlebarWidget->setGeometry(0, 0, 800, 30);
    titlebarWidget->setStyleSheet("color: silver;");

    windowTitle = new QLabel(titlebarWidget);
    windowTitle->setGeometry(0, 0, 800, 30);
    windowTitle->setAlignment(Qt::AlignCenter);
    QFont font = windowTitle->font();
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    windowTitle->setFont(font);
    windowTitle->setText("AMPlayer");

    QPushButton * menuBtn = new QPushButton(this);
    menuBtn->setFont(fontAwesome);
    menuBtn->setToolTip("Menu");
    menuBtn->setGeometry(10, 10, 15, 15);
    menuBtn->setStyleSheet("QPushButton { font-size: 16px; border: 0px solid silver; background-color: #101010; color: gray; }");
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setText(QString::fromStdWString(L"\uf0c9"));
    menuBtn->show();

    closeBtn = new QPushButton(this);
    closeBtn->setFont(fontAwesome);
    closeBtn->setToolTip("Close");
    closeBtn->setGeometry(775, 11, 15, 15);
    closeBtn->setStyleSheet("QPushButton { font-size: 18px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setText(QString::fromStdWString(L"\uf00d"));
    closeBtn->show();

    minimizeBtn = new QPushButton(this);
    minimizeBtn->setFont(fontAwesome);
    minimizeBtn->setToolTip("Minimize");
    minimizeBtn->setGeometry(750, 10, 15, 15);
    minimizeBtn->setStyleSheet("QPushButton { font-size: 13px; border: 0px solid silver; background-color: #101010; color: silver; }");
    minimizeBtn->setCursor(Qt::PointingHandCursor);
    minimizeBtn->setText(QString::fromStdWString(L"\uf2d1"));
    minimizeBtn->show();

    pauseBtn = new QPushButton(this);
    pauseBtn->setFont(fontAwesome);
    pauseBtn->setGeometry(380, 285, 50, 50);
    pauseBtn->setToolTip("Play/Pause");
    pauseBtn->setStyleSheet("QPushButton { font-size: 36px; border: 0px solid silver; background-color: #101010; color: silver; }");
    pauseBtn->setText("\uf04b");
    pauseBtn->setCursor(Qt::PointingHandCursor);
    pauseBtn->show();

    QPushButton * forwardBtn = new QPushButton(this);
    forwardBtn->setFont(fontAwesome);
    forwardBtn->setGeometry(455, 290, 40, 40);
    forwardBtn->setToolTip("Next Track");
    forwardBtn->setStyleSheet("QPushButton { vertical-align: middle; border: 0px solid silver; font-size: 26px; background-color: #101010; color: silver; }");
    forwardBtn->setText("\uf04e");
    forwardBtn->setCursor(Qt::PointingHandCursor);
    forwardBtn->show();

    QPushButton * backwardBtn = new QPushButton(this);
    backwardBtn->setFont(fontAwesome);
    backwardBtn->setGeometry(315, 290, 40, 40);
    backwardBtn->setToolTip("Previous Track");
    backwardBtn->setStyleSheet("QPushButton { vertical-align: middle; border: 0px solid silver; font-size: 26px; background-color: #101010; color: silver; }");
    backwardBtn->setText("\uf04a");
    backwardBtn->setCursor(Qt::PointingHandCursor);
    backwardBtn->show();

    repeatBtn = new QPushButton(this);
    repeatBtn->setFont(fontAwesome);
    repeatBtn->setGeometry(510, 291, 30, 30);
    repeatBtn->setToolTip("Track Repeat");
    repeatBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
    repeatBtn->setText("\uf363");
    repeatBtn->setCursor(Qt::PointingHandCursor);
    repeatBtn->show();

    shuffleBtn = new QPushButton(this);
    shuffleBtn->setFont(fontAwesome);
    shuffleBtn->setGeometry(265, 291, 30, 30);
    shuffleBtn->setToolTip("Playlist Shuffle");
    shuffleBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
    shuffleBtn->setText("\uf074");
    shuffleBtn->setCursor(Qt::PointingHandCursor);
    shuffleBtn->show();

    equoBtn = new QPushButton(this);
    equoBtn->setFont(fontAwesome);
    equoBtn->setGeometry(225, 291, 30, 30);
    equoBtn->setToolTip("Equalizer");
    equoBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
    equoBtn->setText("\uf3f1");
    equoBtn->setCursor(Qt::PointingHandCursor);
    equoBtn->show();

    remoteBtn = new QPushButton(this);
    remoteBtn->setFont(fontAwesome);
    remoteBtn->setGeometry(145, 291, 30, 30);
    remoteBtn->setToolTip("Remote Control");
    remoteBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
    remoteBtn->setText("\uf3cd");
    remoteBtn->setCursor(Qt::PointingHandCursor);
    remoteBtn->setMouseTracking(true);
    remoteBtn->show();

    timerBtn = new QPushButton (this);
    timerBtn->setFont(fontAwesome);
    timerBtn->setGeometry(185, 291, 30, 30);
    timerBtn->setToolTip("Timer");
    timerBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
    timerBtn->setText("\uf017");
    timerBtn->setCursor(Qt::PointingHandCursor);
    timerBtn->show();

    audio3dBtn = new QPushButton(this);
    audio3dBtn->setFont(fontAwesome);
    audio3dBtn->setGeometry(550, 291, 30, 30);
    audio3dBtn->setToolTip("3D Audio");
    audio3dBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
    audio3dBtn->setText("\uf1b2");
    audio3dBtn->setCursor(Qt::PointingHandCursor);
    audio3dBtn->show();

    visualBtn = new QPushButton(this);
    visualBtn->setFont(fontAwesome);
    visualBtn->setGeometry(590, 291, 30, 30);
    visualBtn->setToolTip("Visualizations");
    visualBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
    visualBtn->setText("\uf26c");
    visualBtn->setCursor(Qt::PointingHandCursor);
    visualBtn->show();

    volumeBtn = new QPushButton (this);
    volumeBtn->setFont(fontAwesome);
    volumeBtn->setGeometry(630, 291, 30, 30);
    volumeBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
    volumeBtn->setCursor(Qt::PointingHandCursor);
    volumeBtn->setText("\uf028");
    volumeBtn->setMouseTracking(true);
    volumeBtn->show();

    songTitle = new QLabel(this);
    songTitle->setText("");
    songTitle->setGeometry(200, 220, 400, 30);
    songTitle->setAlignment(Qt::AlignCenter);
    songTitle->setStyleSheet("/* border: 1px solid silver; */ font-size: 20px; color: silver;");
    songTitle->show();

    songInfo = new QLabel(this);
    songInfo->setText("");
    songInfo->setGeometry(200, 255, 400, 16);
    songInfo->setAlignment(Qt::AlignCenter);
    songInfo->setStyleSheet("/* border: 1px solid silver; */ color: gray;");
    songInfo->show();

    timecode = new QLabel (this);
    timecode->raise();
    timecode->setGeometry(0, 0, 0, 0);
    timecode->setAlignment(Qt::AlignCenter);
    timecode->setMouseTracking(true);
    timecode->installEventFilter(this);
    timecode->setStyleSheet("font-size: 11px; border: 1px solid silver; background-color: #101010; color: silver;");

    playlistsBar = new QTabBar(this);
    playlistsBar->setFont(fontAwesome);
    playlistsBar->setDocumentMode(true);
    playlistsBar->setDrawBase(false);
    playlistsBar->setExpanding (false);
    playlistsBar->setMouseTracking(true);
    playlistsBar->setGeometry(15, 405, 775, 25);
    playlistsBar->lower();
    playlistsBar->setContextMenuPolicy(Qt::CustomContextMenu);
    playlistsBar->setStyleSheet("QTabBar { height: 25px; font-size: 12px; border: 0px solid silver; background-color: #101010; color: silver; }" \
                                "QTabBar::tab:selected { background-color: #141414; color: " + QString::fromStdString(mainColorStr) + ";}" \
                                "QTabBar::tab:last { border-right: 0px solid #101010; } " \
                                "QTabBar::scroller { width: 40px; }" \
                                "QTabBar::close-button { padding: 4px; image: url(images/close.png); }" \
                                "QTabBar QToolButton { border: 0px solid black; color: silver; background-color: #101010; }" \
                                "QTabBar::tab { height: 25px; background-color: #101010; padding: 0px 20px; max-width: 150px; border: 0px solid silver; border-bottom: 1px solid silver; border-right: 4px solid #101010; color: silver; }" \
                                "QTabBar::tear { border: 0px solid black; }");

    playlistsBar->show();


    playlistWidget = new QListWidget (this);
    playlistWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    playlistWidget->setMouseTracking(true);
    playlistWidget->installEventFilter(this);

    QScrollBar *vbar = playlistWidget->verticalScrollBar();
    vbar->setStyle( new QCommonStyle );
    vbar->setStyleSheet("QScrollBar:vertical { outline: 0; border-radius: 20px; border: 0px solid black; width: 5px; background: #101010; }" \
                        "QScrollBar::add-line:vertical { height: 0; }" \
                        "QScrollBar::sub-line:vertical { height: 0; }" \
                        "QScrollBar::handle:vertical { border-radius: 20px; width: 5px; background: gray; }" \
                        "QScrollBar::handle:vertical:hover { border-radius: 20px; width: 5px; background: " + tr(mainColorStr.c_str()) + "; }" \
                        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { height: 0px; }");

    QScrollBar *hbar = playlistWidget->horizontalScrollBar();
    hbar->setStyle( new QCommonStyle );
    hbar->setStyleSheet("QScrollBar:horizontal { outline: 0; border-radius: 20px; border: 0px solid black; height: 5px; background: #101010; }" \
                        "QScrollBar::add-line:horizontal { height: 0; }" \
                        "QScrollBar::sub-line:horizontal { height: 0; }" \
                        "QScrollBar::handle:horizontal { border-radius: 20px; height: 5px; background: gray; }" \
                        "QScrollBar::handle:horizontal:hover { border-radius: 20px; height: 5px; background: " + tr(mainColorStr.c_str()) + "; }" \
                        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:vertical { height: 0px; }");

    playlistWidget->setDragDropMode(QAbstractItemView::DragDrop);
    playlistWidget->setDefaultDropAction(Qt::MoveAction);
    playlistWidget->setGeometry(15, 460, 775, 160);
    playlistWidget->lower();
    playlistWidget->setStyleSheet("QListWidget { outline: 0; padding: 5px; padding-left: 15px; font-size: 14px; /*border: 1px solid silver;*/ border-radius: 5px; border-top-left-radius: 0px; background-color: #141414; color: silver; }" \
                                  "QListWidget::item { outline: none; color: silver; border: 0px solid black; background: rgba(0, 0, 0, 0); }" \
                                  "QListWidget::item:selected { outline: none; border: 0px solid black; color: " + tr(mainColorStr.c_str()) + "; }");
    playlistWidget->show();

    searchSong = new QLineEdit(this);
    searchSong->setMouseTracking(true);
    searchSong->setFont(fontAwesome);
    searchSong->setStyleSheet("QMenu { background-color: #101010; color: silver; }"
                              "QLineEdit { padding: 0px 20px; font-size: 12px; border: 0px solid silver; border-bottom: 1px solid #101010; background-color: #141414; color: silver; }" \
                              "QLineEdit:focus { /* border-bottom: 1px solid " + QString::fromStdString(mainColorStr) + "; */ }");
    searchSong->setGeometry(15, 430, 775, 30);
    searchSong->setPlaceholderText("\uf002 Search song");
    searchSong->show();

    songPosition = new QLabel(this);
    songPosition->setMouseTracking(true);
    songPosition->setText("00:00");
    songPosition->setGeometry(15, 363, 200, 20);
    songPosition->setAlignment(Qt::AlignLeft);
    songPosition->setStyleSheet("/* border: 1px solid silver; background-color: #101010; */ color: gray;");
    songPosition->show();

    songDuration = new QLabel(this);
    songDuration->setMouseTracking(true);
    songDuration->setText("00:00");
    songDuration->setGeometry(605, 363, 180, 20);
    songDuration->setAlignment(Qt::AlignRight);
    songDuration->setStyleSheet("/* border: 1px solid silver; background-color: #101010; */ color: gray;");
    songDuration->show();

    volumeSlider = new QSlider (Qt::Horizontal, this);
    volumeSlider->setMaximum(100);
    volumeSlider->setMinimum(0);
    volumeSlider->setValue(100);
    volumeSlider->setCursor(Qt::PointingHandCursor);
    volumeSlider->setGeometry(670, 302, 115, 20);
    volumeSlider->setStyleSheet("QSlider::groove:horizontal {" \
                                    "border: 1px solid #999999; " \
                                    "border-radius: 20px;" \
                                    "background: #141414;"\
                                    "margin: 7px 0px;"\
                                "}" \
                               "QSlider::handle:horizontal {" \
                                    "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f); "\
                                    "border: 1px solid #5c5c5c; "\
                                    "width: 5px; " \
                                    "margin: -5px -2px; " \
                                    "border-radius: 20px; " \
                                "}"
                                "QSlider::sub-page:horizontal {" \
                                    "border-radius: 20px;" \
                                    "margin: 7px 0px;" \
                                    "background: " + tr(mainColorStr.c_str()) + "; " \
                                "}" \
                                "QSlider::add-page:horizontal {" \
                                    "border-radius: 20px;" \
                                    "margin: 7px 0px;" \
                                    "background: silver; " \
                                "}");

    volumeSlider->hide();

    connect (equalizerWin->enabledCheckBox, &QCheckBox::stateChanged, [=](int state) {
        equoEnabled = (state == Qt::Checked);

        if (currentID == -1) return;

        Song temp = playlists[playingSongPlaylist][currentID];
        double pos = getPosition();

        BASS_StreamFree(channel);

        if (equoEnabled) {
            channel = BASS_StreamCreateFile(FALSE, temp.path.toStdWString().c_str(), 0, 0, BASS_STREAM_DECODE);
            channel = BASS_FX_TempoCreate(channel, NULL);
            equalizerWin->channel = &channel;
            equalizerWin->init();
            equoBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
        } else {
            channel = BASS_StreamCreateFile(FALSE, temp.path.toStdWString().c_str(), 0, 0, 0);
            equoBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
        }

        BASS_ChannelSetPosition(channel, BASS_ChannelSeconds2Bytes(channel, pos), BASS_POS_BYTE);
    });

    // If visualizations window closed
    connect (visualWin->closeBtn, &QPushButton::pressed, [=]() {
        qDebug() << "Closed!";
        visualWindowOpened = false;
        visualBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
        this->setFocus();
    });

    connect (searchSong, SIGNAL(textChanged(const QString &)), this, SLOT(search(const QString &)));

    connect (playlistsBar, &QTabBar::tabBarClicked, [=](int index) {
        if (playlistsBar->tabText(index) == "\uf067")
        {
            createPlaylist();
        }
    });
    connect (playlistsBar, SIGNAL (customContextMenuRequested(const QPoint&)), this, SLOT(playlistsBarContextMenu (const QPoint&)));
    connect (playlistsBar, SIGNAL (currentChanged(int)), this, SLOT(changeCurrentPlaylist (int)));
    connect (playlistsBar, SIGNAL (tabCloseRequested(int)), this, SLOT(removePlaylist (int)));

    connect (playlistWidget, &QListWidget::customContextMenuRequested, [=](const QPoint& point) {
        if (playlistWidget->itemAt(point)) {
            int itemIndex = playlistWidget->itemAt(point)->data(Qt::UserRole).toInt();
            qDebug() << itemIndex;

            QPoint globalPos = playlistWidget->mapToGlobal(point);

            QMenu myMenu;
            myMenu.setFont(fontAwesome);
            myMenu.setStyleSheet("background-color: #101010; color: silver");
            myMenu.addAction("Play");
            myMenu.addSeparator();
            myMenu.addAction("Edit");
            myMenu.addAction("Remove");

            QAction * selectedItem = myMenu.exec(globalPos);

            if (selectedItem)
            {
                if (selectedItem->text() == "Play") {
                    lastPlaylistName = playingSongPlaylist;
                    playingSongPlaylist = currentPlaylistName;
                    lastTrackID = currentID;
                    setActive(itemIndex);
                }
                if (selectedItem->text() == "Remove")
                    removeFile();
            }
            else
            {
                // nothing was chosen
            }
        }
    });
    connect (playlistWidget, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(setActive(QListWidgetItem *)));
    connect (playlistWidget->model(), SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)), this, SLOT(rowsMoved(QModelIndex, int, int, QModelIndex, int)));

    connect (menuBtn, SIGNAL(clicked()), this, SLOT(menuContext()));

    connect (minimizeBtn, SIGNAL(clicked()), this, SLOT(slot_minimize()));
    connect (closeBtn, SIGNAL(clicked()), this, SLOT(slot_close()));
    connect (forwardBtn, SIGNAL(clicked()), this, SLOT(forward()));
    connect (backwardBtn, SIGNAL(clicked()), this, SLOT(backward()));
    connect (pauseBtn, SIGNAL(clicked()), this, SLOT(pause()));
    connect (volumeBtn, &QPushButton::clicked, [=]() {
        muted = !muted;

        if (muted) {
            BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL, 0);
            volumeBtn->setText("\uf026");
        } else {
            BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL, volume);
            volumeBtn->setText("\uf028");
        }
    });
    connect (repeatBtn, SIGNAL(clicked()), this, SLOT(changeRepeat()));
    connect (shuffleBtn, SIGNAL(clicked()), this, SLOT(changeShuffle()));
    connect (audio3dBtn, SIGNAL(clicked()), this, SLOT(audio3D()));
    connect (equoBtn, SIGNAL(clicked()), this, SLOT(equalizer()));
    connect (timerBtn, SIGNAL(clicked()), this, SLOT(trackTimer()));

    connect (remoteBtn, &QPushButton::clicked, [=]() {

        if (!remoteServerEnabled) {
            if (removeControlServer->listen(QHostAddress::Any, 9999)) {
                qDebug() << "Remote control socket started listening in port 9999!";

                remoteServerEnabled = true;
                reloadStyles();

                connect(removeControlServer, &QWebSocketServer::newConnection, this, &MainWindow::remoteDeviceConnect);
            }
            else {
                qDebug() << "Failed to start socket! " << removeControlServer->errorString();
            }

            if (httpServer->listen(QHostAddress::Any, httpServerPort)) {
                qDebug() << "Http server started with port" << httpServerPort;

                connect(httpServer, &QTcpServer::newConnection, this, &MainWindow::httpNewConnection);
            } else
            {
                qDebug() << "Failed to start http server!";
            }
        }

        QMessageBox msgBox;
        msgBox.setWindowTitle("Remote Control Server");
        msgBox.setText("Remote control server address: " + getLocalAddress() + ":" + QString::number(httpServerPort));
        msgBox.setStyleSheet("background-color: #101010; color: silver;");
        msgBox.exec();

    });

    connect (visualBtn, SIGNAL(clicked()), this, SLOT(visualizations()));
    connect (volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(changeVolume(int)));

    drawPlaylist();
    drawAllPlaylists();
}

MainWindow::~MainWindow()
{
    remove("cover.png");    // Remove Cover file

    QString uptime = seconds2qstring((clock() - starttime) / 1000);
    writeLog("Exiting...");
    writeLog("Uptime: " + uptime + " (" + QString::number(clock() - starttime) + "ms)");

    logfile.close();

    if (searchSong->text() == "")
        playlists[currentPlaylistName] = playlist;

    XMLreader->writePlaylists(playlists);
    delete ui;
}
// Program menu
void MainWindow::menuContext () {
    QMenu myMenu;
    myMenu.setStyleSheet("background-color: #121212; color: silver");

    myMenu.addAction("Open File(s)");
    myMenu.addAction("Open Folder");
    myMenu.addAction("Open File by URL");
    myMenu.addAction("Add Online Radio");
    myMenu.addSeparator();
    myMenu.addAction("Settings");
    myMenu.addAction("About AMPlayer");
    myMenu.addSeparator();
    myMenu.addAction("Exit");

    QAction * selectedItem = myMenu.exec(this->mapToGlobal(QPoint(20, 20)));

    if (selectedItem)
    {
        if (selectedItem->text() == "Open File(s)") {
            openFile();
        }
        if (selectedItem->text() == "Open Folder") {
            openFolder();
        }
        if (selectedItem->text() == "Settings") {
            settings();
        }
        if (selectedItem->text() == "Exit") {
            slot_close();
        }
    }
}
// Open File
bool MainWindow::openFile ()
{
    searchSong->setText("");

    QFileDialog dialog(this);
    dialog.setDirectory(QDir::homePath());
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter(trUtf8("Audio files (*.mo3 *.mp3 *.mp2 *.mp1 *.ogg *.aif *.aiff *.wav)"));

    QStringList fileNames;

    if (dialog.exec())
        fileNames = dialog.selectedFiles();

    for (int i = 0; i < fileNames.length(); i++)
    {
        Song temp(fileNames[i]);

        TagLib::FileRef f(fileNames[i].toStdWString().c_str());

        wstring artist = L"", title = L"";

        // If file not load failed
        if (!f.isNull()) {
            artist = f.tag()->artist().toCWString();
            title = f.tag()->title().toCWString();
        }

        writeLog("Opened file: " + fileNames[i]);

        if (count(playlist.begin(), playlist.end(), temp) != 0)
        {
            continue;
        }

        if (artist == L"" && title.find('-') == wstring::npos) {
            artist = L"Unknown Artist";
        } else if (artist == L"" && title.find('-') != wstring::npos) {
            artist = L"";
        }

        if (title == L"") {
            temp.setNameFromPath();
        } else {
            if (artist != L"")
                temp.setName(QString::fromStdWString(artist) + " - " + QString::fromStdWString(title));
            else
                temp.setName(QString::fromStdWString(title));
        }


        playlist.push_back(temp);
    }

    // "Audio Files\0*.mo3;*.xm;*.mod;*.s3m;*.it;*.mtm;*.umx;*.mp3;*.mp2;*.mp1;*.ogg;*.wav;*.aif)\0All files\0*.*\0\0";

    playlists[currentPlaylistName] = playlist;

    drawPlaylist();
    if (playlist.size() == 1)
    {
        lastPlaylistName = playingSongPlaylist;
        playingSongPlaylist = currentPlaylistName;
        setActive(0);
    }
    return true;
}
void MainWindow::reorderPlaylist () {
    vector <Song> neworder;

    for (int i = 0; i < playlist.size(); i++)
    {
        int index = playlistWidget->item(i)->data(Qt::UserRole).toInt();
        neworder.push_back (playlist[index]);
    }

    if (channel != NULL)
    {
        QString currentPath = playlist[currentID].path;
        Song temp(currentPath);
        currentID = std::find(neworder.begin(), neworder.end(), temp) - neworder.begin();
    }

    playlist = neworder;
    playlists[currentPlaylistName] = playlist;

    drawPlaylist();
}
void MainWindow::rowsMoved(QModelIndex, int, int, QModelIndex, int) {
    reorderPlaylist();
}

void MainWindow::removeFile() {
    if (playlistWidget->currentRow() == -1) return;

    int index = playlistWidget->currentItem()->data(Qt::UserRole).toInt();

    writeLog(playlistWidget->currentItem()->text() + " removed from " + currentPlaylistName);

    Song temp(playlist[index].path);
    int globalIndex = std::find(playlists[currentPlaylistName].begin(), playlists[currentPlaylistName].end(), temp) - playlists[currentPlaylistName].begin();

    if (globalIndex == currentID)
    {
        playingSongPlaylist = "";
        BASS_ChannelStop(channel);
        channel = NULL;
        songTitle->setText("");
        songInfo->setText("");
        pauseBtn->setText("\uf04b");
        clearPrerenderedFft();
        songPosition->setText("00:00");
        songDuration->setText("00:00");
        coverLoaded = false;
        cover = QImage (":/Images/cover-placeholder.png");
        currentID = -1;
    }
    else if (globalIndex < currentID)
        currentID--;

    playlist.erase(playlist.begin() + index);
    playlists[currentPlaylistName].erase(playlists[currentPlaylistName].begin() + globalIndex);
    drawPlaylist();

    if (index == (int)playlist.size()) index--;
    playlistWidget->setCurrentRow(index);
}

bool MainWindow::openFolder () {
    searchSong->setText("");

    QString folder = QFileDialog::getExistingDirectory(0, ("Select Folder with Songs"), QDir::homePath());

    if (folder == "")
        return false;

    QDir directory(folder);
    QStringList musicFiles = directory.entryList(QStringList() << "*.ogg" << "*.aif" << "*.aiff" << "*.mp3" << "*.mo3" << "*.wav" << "*.mp2" << "*.mp1", QDir::Files | QDir::Readable);

    writeLog("Folder opened: " + folder);
    writeLog("Folder contains: " + QString::number (musicFiles.length()) + " files");

    int duplicatesCount = 0;
    for (int i = 0; i < musicFiles.length(); i++)
    {
        QString fullpath = folder + "/" + musicFiles[i];

        Song temp(fullpath);

        TagLib::FileRef f(fullpath.toStdWString().c_str());

        wstring artist = L"", title = L"";

        // If file not load failed
        if (!f.isNull()) {
            artist = f.tag()->artist().toCWString();
            title = f.tag()->title().toCWString();
        }

        writeLog("Opened file: " + musicFiles[i]);

        if (artist == L"" && title.find('-') == wstring::npos) {
            artist = L"Unknown Artist";
        } else if (artist == L"" && title.find('-') != wstring::npos) {
            artist = L"";
        }

        if (title == L"") {
            temp.setNameFromPath();
        } else {
            if (artist != L"")
                temp.setName(QString::fromStdWString(artist) + " - " + QString::fromStdWString(title));
            else
                temp.setName(QString::fromStdWString(title));
        }

        if (count(playlist.begin(), playlist.end(), temp) != 0)
        {
            duplicatesCount++;
            continue;
        }

        playlist.push_back(temp);
    }

    QMessageBox msgBox;
    msgBox.setWindowTitle("Files adding");

    if (musicFiles.length() > 0 && duplicatesCount != musicFiles.length()) {
        msgBox.setText(tr(to_string(musicFiles.length() - duplicatesCount).c_str()) + " new files added to playlist successfully!");
    }
    else if (duplicatesCount == musicFiles.length() && musicFiles.length() > 0) {
        msgBox.setText ("0 new files added!");
    }
    else if (musicFiles.length() == 0) {
        msgBox.setText("There are no audio files in this folder!");
    }

    msgBox.setStyleSheet("background-color: #101010; color: silver;");
    msgBox.exec();

    drawPlaylist();
    playlists[currentPlaylistName] = playlist;

    return true;
}
void MainWindow::createPlaylist ()
{
    QString name;

    do {
        bool ok;
        name = QInputDialog::getText(this, tr("Playlist Name"),  tr("Playlist name:"), QLineEdit::Normal, NULL, &ok);

        if (ok && !name.isEmpty())
        {
            if (playlists.find(name) != playlists.end()) {
                QMessageBox msgBox;
                msgBox.setText("Playlists \'" + name + "\' already exists!");
                msgBox.setStyleSheet("background-color: #101010; color: silver;");
                msgBox.exec();
            } else {
                writeLog("New playlist created: " + name);
                playlists[name] = vector <Song>();
                break;
            }
        } else {
            break;
        }
    } while (true);

    drawAllPlaylists();
}

void MainWindow::removePlaylist (int index) {
    QString playlistName = playlistsBar->tabText(index);

    if (playlists[playlistName].size() > 0) {
        QMessageBox msgBox;
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setText("Are you sure to delete \'" + playlistName + "\'playlist?");
        msgBox.setStyleSheet("background-color: #101010; color: silver;");
        msgBox.setInformativeText("Playlist contains " + QString::fromStdString(to_string(playlists[playlistName].size())) + " songs.");
        int ret = msgBox.exec();

        if (ret == QMessageBox::No)
            return;
    }

    if (channel != NULL) {
        bool currentSongInPlaylist = false;

        for (auto iter = playlist.begin(); iter != playlist.end(); iter++)
            if (iter->path == playlist[currentID].path && playingSongPlaylist == playlistName)
                currentSongInPlaylist = true;

        if (currentSongInPlaylist)
        {
            playingSongPlaylist = "";
            BASS_ChannelStop(channel);
            channel = NULL;
            pauseBtn->setText("\uf04b");
            songTitle->setText("");
            songInfo->setText("");
            clearPrerenderedFft();
            songPosition->setText("00:00");
            songDuration->setText("00:00");
            coverLoaded = false;
            cover = QImage (":/Images/cover-placeholder.png");
            currentID = -1;
        }
    }

    // Deleting playlist and setting first playlist as current
    writeLog("Removed playlist: " + playlistName);

    if (playlistName == "Default") playlists["Default"] = vector<Song>();
    else {
        playlists.erase(playlistName);
    }

    if (playingSongPlaylist == "")
        currentPlaylistName = "Default";
    else
        currentPlaylistName = playingSongPlaylist;

    playlist = playlists[currentPlaylistName];

    int playlistIndex = getPlaylistIndexByName(currentPlaylistName);

    drawPlaylist();
    drawAllPlaylists();
    playlistsBar->setCurrentIndex(playlistIndex);
}
void MainWindow::search(const QString & text)
{
    qDebug() << text;
    writeLog("Searching query: " + text);

    searchInPlaylist(text);
    drawPlaylist();
}
void MainWindow::drawAllPlaylists ()
{
    int i = 0;
    while (playlistsBar->count() != 0)
    {
        playlistsBar->removeTab(i);
        i++;

        if (i >= playlistsBar->count())
            i = 0;
    }

    for (auto &playlist : playlists)
    {
        playlistsBar->addTab(playlist.first);
    }

    playlistsBar->addTab("\uf067");
}
void MainWindow::searchInPlaylist(const QString & text) {
    if (text != "")
        playlistWidget->setDragDropMode(QAbstractItemView::NoDragDrop);
    else
        playlistWidget->setDragDropMode(QAbstractItemView::DragDrop);

    playlist = playlists[currentPlaylistName];

    for (int i = 0; i < (int)playlist.size(); i++) {
        QString name = playlist[i].getName();
        name = name.toLower();

        QString query = text.toLower();

        if (name.indexOf(query) == -1) {
            playlist.erase(playlist.begin() + i);
            i--;
        }
    }
}
void MainWindow::drawPlaylist() {
    playlistWidget->clear();

    for (int i = 0; i < (int)playlist.size(); i++)
    {
        QListWidgetItem * songItem = new QListWidgetItem(playlistWidget);
        QString name = playlist[i].getName();

        songItem->setData(Qt::UserRole, i);
        songItem->setText(name);

        songItem->setToolTip("Name: " + name +
                             "\nDuration: " + seconds2qstring(playlist[i].getDuration()) +
                             "\nFormat: " + playlist[i].getFormat() +
                             "\nSize: " + playlist[i].getFileSizeMB());

        playlistWidget->addItem(songItem);

        if (currentID != -1 && playlists[playingSongPlaylist][currentID] == playlist[i] && currentPlaylistName == playingSongPlaylist)
            playlistWidget->setCurrentRow(i);
   }
}
void MainWindow::setTitle () {
    QString name = playlists[playingSongPlaylist][currentID].getName();

    sendMessageToRemoteDevices(name);

    if (name.length() > 32)
        name = name.mid(0, 32) + "...";

    songTitle->setText(name);
}
void MainWindow::changeCurrentPlaylist (int index) {
    QString tabContent = playlistsBar->tabText(index);

    if (tabContent == "") {
        playlistsBar->removeTab(index);
        return;
    }

    if (tabContent == "\uF067")
    {
        index--;
        playlistsBar->setCurrentIndex(index);
        return;
    }

    if (playlists.find(tabContent) == playlists.end() && tabContent != "\uF067")
        return;

    searchSong->setText("");
    playlists[currentPlaylistName] = playlist;

    writeLog("\nPlaylist changed to: " + playlistsBar->tabText(index));

    playlist = playlists[playlistsBar->tabText(index)];
    currentPlaylistName = playlistsBar->tabText(index);

    drawPlaylist();
}

void MainWindow::playlistsBarContextMenu (const QPoint& point) {
    int tabIndex = playlistsBar->tabAt(point);
    qDebug() << tabIndex;

    QPoint globalPos = playlistsBar->mapToGlobal(point);

    QMenu myMenu;
    myMenu.setStyleSheet("background-color: #101010; color: silver");
    myMenu.addAction("Create Playlist");
    myMenu.addAction("Rename");
    myMenu.addAction("Remove");

    QAction* selectedItem = myMenu.exec(globalPos);

    if (selectedItem)
    {
        if (selectedItem->text() == "Create Playlist") {
            createPlaylist();
        }
        if (selectedItem->text() == "Remove") {
            removePlaylist(tabIndex);
        }
    }
    else
    {
        // nothing was chosen
    }
}

void MainWindow::setActive(QListWidgetItem * item) {
    lastTrackID = currentID;
    lastPlaylistName = playingSongPlaylist;
    playingSongPlaylist = currentPlaylistName;
    setActive (playlistWidget->currentRow());
}
void MainWindow::setActive(int index) {
    bool isSameTrack = false;

    paused = true;
    pauseBtn->setText("\uf04c"); // Set symbol to pause

    if (playingSongPlaylist == lastPlaylistName && index == lastTrackID)
        isSameTrack = true;

    Song temp;

    if (searchSong->text() == "" || playingSongPlaylist != currentPlaylistName)
    {
        temp = Song(playlists[playingSongPlaylist][index].path);
    }
    else if (searchSong->text() != "") {
        temp = Song(playlist[index].path);
    }

    if (!QFile::exists(temp.path))
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("File not found!");
        msgBox.setText("File \"" + temp.path + "\" deleted or moved!");
        msgBox.setStyleSheet("background-color: #101010; color: silver;");
        msgBox.exec();

        playlist.erase(playlist.begin() + index);
        int globalIndex = std::find(playlists[playingSongPlaylist].begin(), playlists[playingSongPlaylist].end(), temp) - playlists[playingSongPlaylist].begin();
        playlists[currentPlaylistName].erase(playlists[currentPlaylistName].begin() + globalIndex);
        drawPlaylist();

        if (globalIndex < currentID)
            currentID--;

        playlistWidget->setCurrentRow(currentID);
        searchSong->setText("");

        return;
    }

    currentID = std::find(playlists[playingSongPlaylist].begin(), playlists[playingSongPlaylist].end(), temp) - playlists[playingSongPlaylist].begin();
    cover = playlists[playingSongPlaylist][currentID].getCover();

    TagLib::FileRef file(temp.path.toStdWString().c_str());

    if (!file.isNull()) {
        QString bitrate = QString::number(file.audioProperties()->bitrate()) + " kbps";
        QString sampleRate = QString::number(file.audioProperties()->sampleRate() / 1000.0f) + " khz";

        songInfo->setText("Genre, " + bitrate + " " + sampleRate + ", " + temp.getFormat());
    } else {
        songInfo->setText("Genre, " + temp.getFormat());
    }

    coverLoaded = true;
    if (cover.isNull()) // Audio file don't contain cover, load custom cover
    {
        coverLoaded = false;
        cover = QImage (":/Images/cover-placeholder.png");
    }

    if (channel != NULL)
        BASS_StreamFree(channel);

    if (equoEnabled) {
        channel = BASS_StreamCreateFile(FALSE, temp.path.toStdWString().c_str(), 0, 0, BASS_STREAM_DECODE);
        channel = BASS_FX_TempoCreate(channel, NULL);
        equalizerWin->channel = &channel;
        equalizerWin->init();

    } else {
        channel = BASS_StreamCreateFile(FALSE, temp.path.toStdWString().c_str(), 0, 0, 0);
    }

    /* Will be added soon
    if (BASS_ErrorGetCode() != 0) {
        channel = BASS_MusicLoad(FALSE, path.toStdWString().c_str(), 0, 0, BASS_MUSIC_RAMP | BASS_SAMPLE_LOOP, 0);
    }
    */

    if (!isSameTrack)
    {
        clearPrerenderedFft();

        auto func = std::bind(&MainWindow::prerenderFft, this, temp.path);

        std::thread thr(func);
        thr.detach();

        auto coverFadeIn = std::bind(&MainWindow::coverBackgroundPopup, this);

        std::thread thr2 (coverFadeIn);
        thr2.detach();
    }

    BASS_ChannelPlay(channel, false);
    if (muted) BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL, 0);
    else BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL, volume);

    QString pos = seconds2qstring(getPosition());
    songPosition->setText(pos);
    QString len = seconds2qstring(getDuration());
    songDuration->setText(len);

    searchSong->setText("");

    if (currentPlaylistName == playingSongPlaylist)
    {
        playlistWidget->setCurrentRow(currentID);
        writeLog("Song changed to: " + playlistWidget->currentItem()->text());
    }
    setTitle();
    reloadStyles();
}
void MainWindow::forward () {
    if (channel == NULL || playlists[playingSongPlaylist].size() == 1)
        return;

    writeLog("Forward button pressed");

    lastTrackID = currentID;

    if (shuffle) {
        int songID;
        do {
            songID = rand() % playlists[playingSongPlaylist].size();
        } while (songID == currentID);

        currentID = songID;
    }
    else currentID++;

    if (currentID >= playlists[playingSongPlaylist].size()) currentID = 0;

    setActive(currentID);
}
void MainWindow::backward () {
    if (channel == NULL || playlists[playingSongPlaylist].size() == 1)
        return;

    writeLog("Backward button pressed");

    lastTrackID = currentID;

    if (shuffle) {
        int songID;
        do {
            songID = rand() % playlists[playingSongPlaylist].size();
        } while (songID == currentID);

        currentID = songID;
    }
    else currentID--;

    if (currentID < 0) currentID = playlists[playingSongPlaylist].size() - 1;

    setActive(currentID);
}
void MainWindow::pause()
{
    if (playlistWidget->currentRow() != -1 && channel == NULL) {
        lastTrackID = currentID;
        lastPlaylistName = playingSongPlaylist;
        playingSongPlaylist = currentPlaylistName;
        setActive(playlistWidget->currentRow());
        return;
    }

    if (playlistWidget->currentRow() == -1 && channel == NULL)
        return;

    paused = !paused;

    if (paused) {
        writeLog("Song resumed");
        BASS_ChannelPlay(channel, false);
        pauseBtn->setText("\uf04c"); // Set symbol to pause

        auto func = std::bind(&MainWindow::coverBackgroundPopup, this);

        std::thread thr(func);
        thr.detach();
    }
    else {
        writeLog("Song paused");
        BASS_ChannelPause(channel);
        pauseBtn->setText("\uf04b"); // Set symbol to play

        auto func = std::bind(&MainWindow::coverBackgroundHide, this);

        std::thread thr(func);
        thr.detach();
    }
}
void MainWindow::changeVolume (int vol)
{
    if (muted)
    {
        volumeBtn->setText("\uf028");
        muted = false;
    }
    volumeSlider->setValue(vol);
    volume = vol / 100.0f;
    writeLog("Volume changed to: " + QString::number(vol));
    BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL, volume);
}
void MainWindow::changeRepeat () {
    if (shuffle == true)
        changeShuffle();

    repeat = !repeat;

    if (repeat) {
        writeLog("Repeat enabled");
        repeatBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
    }
    else {
        writeLog("Repeat disabled");
        repeatBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
    }
}
void MainWindow::changeShuffle () {
    if (repeat == true)
        changeRepeat();

    shuffle = !shuffle;

    if (shuffle) {
        writeLog("Shuffle enabled");
        shuffleBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
    }
    else {
        writeLog("Shuffle disabled");
        shuffleBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: silver; }");
    }
}

QString MainWindow::seconds2qstring (float seconds) {
    int hours = (seconds > 3600) ? (int)seconds / 3600 : 0;
    int minutes = (seconds > 60) ? ((int)seconds - hours * 3600) / 60 : 0;
    int secs = (int)seconds % 60;

    QString result = "";
    if (hours != 0)
    {
        if (hours < 10) result += "0";
        result += QString::number (hours);
        result += ":";
    }

    if (minutes < 10) result += "0";
    result += QString::number (minutes);
    result += ":";

    if (secs < 10) result += "0";
    result += QString::number (secs);

    return result;
}
double MainWindow::qstring2seconds (QString time) {
    if (time.count(':') == 2) {
        int minutes = time.mid(0, time.indexOf(':')).toInt();
        int seconds = time.mid(time.indexOf(':') + 1).toInt();

        return minutes * 60.0 + seconds;
    }
}

void MainWindow::updateTime() {
    static float songPos = getPosition();   // Static variable of song position
    static int counter = 0;                 // Passed frames counter

    if (isOrderChanged)
    {
        reorderPlaylist();
        isOrderChanged = false;
    }

    if (channel != NULL) {
        // If song stay in same position, increase counter by 1
        if (songPos == getPosition())
            counter++;

        // if counter value equals 5 (5 frames (~80ms) passed) and position still same, then
        // the scrolling stopped, so continue playing (if track not paused) and reset counter
        if (counter >= 5 && songPos == getPosition() && paused) {
            BASS_ChannelPlay(channel, false);
            counter = 0;
        }
        else
            songPos = getPosition();

        QString pos = seconds2qstring(getPosition());
        songPosition->setText(pos);

        if (getPosition() >= getDuration())
        {
            writeLog("Song ended");

            if (repeat) {
                lastTrackID = currentID;
                setActive(currentID);
            }
            else if (shuffle && playlist.size() > 1) {
                lastTrackID = currentID;

                int songID;
                do {
                    songID = rand() % playlists[playingSongPlaylist].size();
                } while (songID == currentID);

                currentID = songID;
                setActive(currentID);
            } else {
                writeLog("Changing to next song");
                forward();
            }
        }
    }

    colorChange();
    repaint();
}

void MainWindow::paintEvent(QPaintEvent * event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(Qt::transparent, 0));

    if (!cover.isNull() && coverLoaded) {
        QGraphicsBlurEffect * blur = new QGraphicsBlurEffect;
        blur->setBlurRadius(10);
        QImage cover2 = cover.scaled(250, 250, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        cover2 = applyEffectToImage(cover2, blur);

        QBrush brush(cover2);
        QTransform transform;

        transform.translate(25, -220);
        brush.setTransform(transform);
        painter.setBrush(brush);
        painter.drawRoundedRect(275, 30, 250, 250, 10, 10);

        QRadialGradient gradient(400, 155, 200);
        gradient.setColorAt(0, QColor(16, 16, 16, coverBgOpacity));
        gradient.setColorAt(1, QColor(16, 16, 16, 255));

        QBrush brush2(gradient);
        painter.setBrush(brush2);

        // painter.setBrush(QBrush(QColor(16, 16, 16, coverBgOpacity)));
        painter.drawRect(275, 30, 250, 250);
    }

    if (!cover.isNull()) {
        cover = cover.scaled(150, 150, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

        QBrush brush(cover);
        QTransform transform;

        transform.translate(175, -90);
        brush.setTransform(transform);
        painter.setBrush(brush);
        painter.drawRoundedRect(325, 60, 150, 150, 5, 5);
    }

    QColor color = mainColor;

    if (liveSpec) {
        float fft[1024];
        BASS_ChannelGetData(channel, fft, BASS_DATA_FFT2048);

        for (int i = 0; i < 140; i++) {
            int h = sqrt(fft[i + 1]) * 3 * 40 - 4;
            if (h < 3) h = 3;
            if (h > 40) h = 40;

            if (channel == NULL) h = 3;

            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addRoundedRect(QRectF(50 + 5 * i, 350 + 10 + (20 - h) / 2, 3, h), 2, 2);
            QPen pen(Qt::transparent, 0);
            p.setPen(pen);
            if (i * (getDuration() / 140) < getPosition())
                p.fillPath(path, color);
            else
                p.fillPath(path, QColor (192, 192, 192));
            p.drawPath(path);
        }
    }
    else {
        for (int i = 0; i < 140; i++) {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addRoundedRect(QRectF(50 + 5 * i, 350 + 10 + (20 - prerenderedFft[i]) / 2, 3, prerenderedFft[i]), 2, 2);
            QPen pen(Qt::transparent, 0);
            p.setPen(pen);

            if (i * (getDuration() / 140) < getPosition())
                p.fillPath(path, color);
            else
                p.fillPath(path, QColor (192, 192, 192));

            p.drawPath(path);
        }
    }
}

void MainWindow::prerenderFft (QString file)
{   
    // std::this_thread::sleep_for(std::chrono::milliseconds(20));

    HSTREAM tmp;
    tmp = BASS_StreamCreateFile(FALSE, file.toStdWString().c_str(), 0, 0, 0);

    writeLog("Prerendering fft...");

    int k = 0;
    QWORD len = BASS_ChannelGetLength(tmp, BASS_POS_BYTE); // the length in bytes
    float time = BASS_ChannelBytes2Seconds(tmp, len);      // the length in seconds

    int avgLen = 512;

    BASS_ChannelSetAttribute(tmp, BASS_ATTRIB_VOL, 0);
    BASS_ChannelPlay(tmp, FALSE);

    float fft[1024];
    float tempfft[140];
    for (float i = 0; i < time; i+= time / 140, k++)
    {
        BASS_ChannelGetData(tmp, fft, BASS_DATA_FFT2048);

        float max = 0;
        int q = 0;
        for (int j = 1; j <= avgLen; j++)
        {
            if (sqrt(fft[j]) * 3 * 100 - 4 > 5)
            {
                max += sqrt(fft[j]) * 3 * 40 - 4;
                q++;
            }
        }
        max /= q;
        max *= 10;

        if (max <= 3) max = 3;
        else if (max > 40) max = 40;

        tempfft[(int)k] = max;

        BASS_ChannelSetPosition(tmp, BASS_ChannelSeconds2Bytes(tmp, i), BASS_POS_BYTE);
    }

    BASS_StreamFree(tmp);

    for (int i = 0; i < 140; i++)
    {
        do {
            int tmp = tempfft[i] / 10;
            if (tmp < 1) tmp = 1;

            prerenderedFft[i] += tmp;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (currentID == -1 || this->playlists[playingSongPlaylist][currentID].path != file) {
                clearPrerenderedFft();
                return;
            }
        } while (prerenderedFft[i] < tempfft[i]);

        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        if (currentID == -1 || this->playlists[playingSongPlaylist][currentID].path != file) {
            clearPrerenderedFft();
            return;
        }
    }

    writeLog("Prerendering fft ended");
}

void MainWindow::mouseMoveEvent (QMouseEvent * event) {
    float mouseX = event->pos().x();
    float mouseY = event->pos().y();

    if (mouseX > 50 && mouseX < 750 && mouseY > 350 && mouseY < 390) {
        this->setCursor(Qt::PointingHandCursor);

        timecode->repaint();
        timecode->clear();
        timecode->setGeometry (mouseX + 10, mouseY + 10, 50, 20);
        timecode->setText(seconds2qstring(((mouseX - 50) / (700.0f)) * getDuration()));
        timecode->show();
    }
    else {
        this->setCursor(Qt::ArrowCursor);
        timecode->hide();
    }

    if (mouseX >= 15 && mouseX <= 790 && mouseY >= 405 && mouseY <= 430)
    {
        playlistsBar->raise();
    } else {
        playlistsBar->lower();
    }

    if (mouseX >= 15 && mouseX <= 790 && mouseY >= 430 && mouseY <= 620)
    {
        playlistWidget->raise();
        searchSong->raise();
    }
    else {
        playlistWidget->lower();
        searchSong->lower();
    }

    if (remoteBtn->underMouse() && remoteServerEnabled)
        remoteBtn->setToolTip("Remote Control. " + QString::number(remoteDevices.size()) + " devices connected!");

    if (volumeBtn->underMouse())
    {
        volumeBtn->setToolTip("Volume: " + QString::number(volume * 100));
        volumeSliderToggled = true;
    }
    if (volumeSliderToggled && mouseX >= 630 && mouseX <= 785 && mouseY >= 291 && mouseY <= 321) volumeSlider->show();
    else {
        volumeSlider->hide();
        volumeSliderToggled = false;
    }

    if (!titlebarWidget->underMouse() && !windowTitle->underMouse())
        return;

    if(event->buttons().testFlag(Qt::LeftButton) && moving) {
        this->move(this->pos() + (event->pos() - lastMousePosition));
    }
}
void MainWindow::mousePressEvent (QMouseEvent * event) {
    float mouseX = event->pos().x();
    float mouseY = event->pos().y();

    if (mouseX > 50 && mouseX < 750 && mouseY > 350 && mouseY < 390) {
        if (event->button() == Qt::LeftButton)
        {
            if (getPosition() == getDuration())
            {
                BASS_ChannelStop(channel);
                BASS_ChannelPlay(channel, true);
            }

            double newtime = ((mouseX - 50) / (700.0f)) * getDuration();

            BASS_ChannelSetPosition(channel, BASS_ChannelSeconds2Bytes(channel, newtime), BASS_POS_BYTE);
            writeLog("Song position changed to: " + seconds2qstring(newtime) + " (" + QString::number(newtime) + "s)");
        }
        else if (event->button() == Qt::RightButton) {
            timecode->hide();

            QMenu myMenu;
            myMenu.setStyleSheet("background-color: #121212; color: silver");
            myMenu.addAction("Jump to...");
            myMenu.addAction("Make loop (A-B)");
            myMenu.setMouseTracking(true);
            QAction * action = new QAction("Live spectrum", this);
            action->setCheckable(true);
            action->setChecked(liveSpec);
            myMenu.addAction(action);

            // myMenu.addAction("Live spectrum");

            QAction * selectedItem = myMenu.exec(mapToGlobal(QPoint(mouseX, mouseY)));

            if (selectedItem)
            {
                if (selectedItem->text() == "Jump to...")
                {
                    bool ok;
                    QString pos = QInputDialog::getText(this, tr("Jump to position"),  tr("Jump to position:"), QLineEdit::Normal, "00:00", &ok);

                    if (ok)
                    {
                        double time = qstring2seconds(pos);

                        qDebug() << pos;
                        qDebug() << time;

                        if (time > getDuration())
                        {
                            QMessageBox msgBox;
                            msgBox.setWindowTitle("Error");
                            msgBox.setText("Incorrect position!");
                            msgBox.setStyleSheet("background-color: #101010; color: silver;");
                            msgBox.exec();
                        } else {
                            songPosition->setText(pos);
                            BASS_ChannelSetPosition(channel, BASS_ChannelSeconds2Bytes(channel, time), BASS_POS_BYTE);
                        }
                    }
                }
                if (selectedItem->text() == "Live spectrum")
                {
                    liveSpec = !liveSpec;
                    writeLog((liveSpec) ? "Live spectrum enabled" : "Live spectrum disabled");
                }
            }
            else
            {
                // nothing was chosen
            }
        }
    }

    if (!titlebarWidget->underMouse() && !windowTitle->underMouse())
        return;

    if(event->button() == Qt::LeftButton) {
        moving = true;
        lastMousePosition = event->pos();
    }
}
void MainWindow::wheelEvent(QWheelEvent * event) {
    float mouseX = event->position().x();
    float mouseY = event->position().y();

    double new_time = getPosition();

    if (mouseX > 50 && mouseX < 750 && mouseY > 350 && mouseY < 390) {
        // Pause channel while scrolling (for better perfomance)
        BASS_ChannelPause(channel);

        if (event->angleDelta().ry() < 0)
        {
            new_time -= 3;
        }
        else if (event->angleDelta().ry() > 0) {
            new_time += 3;
        }


        BASS_ChannelSetPosition(channel, BASS_ChannelSeconds2Bytes(channel, new_time), BASS_POS_BYTE);
    }  
}
void MainWindow::settings () {
    bool * colorChangePtr = &colorChanging;
    settingsWin->colorChanger = colorChangePtr;
    settingsWin->mainColor = &mainColor;
    settingsWin->mainColorStr = &mainColorStr;
    settingsWin->colorChangeSpeed = &colorChangeSpeed;

    settingsWin->init();
    settingsWin->raise();
    settingsWin->setFocus();
    settingsWin->show();
    settingsWin->move (this->pos().x() + 200, this->pos().y() + 150);

    for (int i = 0; i < 7; i++)
    {
        connect(settingsWin->colorBtns[i], &QPushButton::pressed, [=] () {
            reloadStyles();
            equalizerWin->reloadStyles();
        });
    }
}
void MainWindow::visualizations () {
    visualWin->raise();
    visualWin->setFocus();
    visualWin->show();

    visualWindowOpened = true;
    visualBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
}
void MainWindow::reloadStyles () {
    closeBtn->setStyleSheet("QPushButton { font-size: 18px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
    volumeSlider->setStyleSheet("QSlider::groove:horizontal {" \
                                    "border: 1px solid #999999; " \
                                    "border-radius: 20px;" \
                                    "background: #141414;"\
                                    "margin: 7px 0px;"\
                                "}" \
                               "QSlider::handle:horizontal {" \
                                    "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f); "\
                                    "border: 1px solid #5c5c5c; "\
                                    "width: 5px; " \
                                    "margin: -5px -2px; " \
                                    "border-radius: 20px; " \
                                "}"
                                "QSlider::sub-page:horizontal {" \
                                    "border-radius: 20px;" \
                                    "margin: 7px 0px;" \
                                    "background: " + tr(mainColorStr.c_str()) + "; " \
                                "}" \
                                "QSlider::add-page:horizontal {" \
                                    "border-radius: 20px;" \
                                    "margin: 7px 0px;" \
                                    "background: silver; " \
                                "}");

    playlistWidget->setStyleSheet("QListWidget { outline: 0; padding: 5px; padding-left: 15px; font-size: 14px; /*border: 1px solid silver;*/ border-radius: 5px; border-top-left-radius: 0px; background-color: #141414; color: silver; }" \
                                  "QListWidget::item { outline: none; color: silver; border: 0px solid black; background: rgba(0, 0, 0, 0); }" \
                                  "QListWidget::item:selected { outline: none; border: 0px solid black; color: " + tr(mainColorStr.c_str()) + "; }");

    QScrollBar *vbar = playlistWidget->verticalScrollBar();
    vbar->setStyleSheet("QScrollBar:vertical { outline: 0; border-radius: 20px; border: 0px solid black; width: 5px; background: #101010; }" \
                        "QScrollBar::add-line:vertical { height: 0; }" \
                        "QScrollBar::sub-line:vertical { height: 0; }" \
                        "QScrollBar::handle:vertical { border-radius: 20px; width: 5px; background: gray; }" \
                        "QScrollBar::handle:vertical:hover { border-radius: 20px; width: 5px; background: " + tr(mainColorStr.c_str()) + "; }" \
                        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { height: 0px; }");

    QScrollBar *hbar = playlistWidget->horizontalScrollBar();
    hbar->setStyleSheet("QScrollBar:horizontal { outline: 0; border-radius: 20px; border: 0px solid black; height: 5px; background: #101010; }" \
                        "QScrollBar::add-line:horizontal { height: 0; }" \
                        "QScrollBar::sub-line:horizontal { height: 0; }" \
                        "QScrollBar::handle:horizontal { border-radius: 20px; height: 5px; background: gray; }" \
                        "QScrollBar::handle:horizontal:hover { border-radius: 20px; height: 5px; background: " + tr(mainColorStr.c_str()) + "; }" \
                        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:vertical { height: 0px; }");

    playlistsBar->setStyleSheet("QTabBar { height: 25px; font-size: 12px; border: 0px solid silver; background-color: #101010; color: silver; }" \
                                "QTabBar::tab:selected { background-color: #141414; color: " + QString::fromStdString(mainColorStr) + ";}" \
                                "QTabBar::tab:last { border-right: 0px solid #101010; } " \
                                "QTabBar::scroller { width: 40px; }" \
                                "QTabBar::close-button { padding: 4px; image: url(images/close.png); }" \
                                "QTabBar QToolButton { border: 0px solid black; color: silver; background-color: #101010; }" \
                                "QTabBar::tab { height: 25px; background-color: #101010; padding: 0px 20px; max-width: 150px; border: 0px solid silver; border-bottom: 1px solid silver; border-right: 4px solid #101010; color: silver; }" \
                                "QTabBar::tear { border: 0px solid black; }");

    if (shuffle) shuffleBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
    if (repeat) repeatBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
    if (remoteServerEnabled) remoteBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
    if (visualWindowOpened)
        visualBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
    if (equoEnabled)
        equoBtn->setStyleSheet("QPushButton { font-size: 14px; margin-top: 10px; border: 0px solid silver; background-color: #101010; color: " + tr(mainColorStr.c_str()) + "; }");
}

void MainWindow::colorChange()
{
    if (!colorChanging) return;

    static int dr = 0;
    static int dg = 0;
    static int db = 0;

    if (mainColor == nextColor) {
        nextColor = settingsWin->colors[rand() % 7];

        dr = (mainColor.red() - nextColor.red()) / colorChangeSpeed;
        dg = (mainColor.green() - nextColor.green()) / colorChangeSpeed;
        db = (mainColor.blue() - nextColor.blue()) / colorChangeSpeed;

    } else {
        if (dr < 0) {
            dr++;
            mainColor.setRed(mainColor.red() + colorChangeSpeed);
        }
        if (dr > 0) {
            dr--;
            mainColor.setRed(mainColor.red() - colorChangeSpeed);
        }

        if (dg < 0) {
            dg++;
            mainColor.setGreen(mainColor.green() + colorChangeSpeed);
        }
        if (dg > 0) {
            dg--;
            mainColor.setGreen(mainColor.green() - colorChangeSpeed);
        }

        if (db < 0) {
            db++;
            mainColor.setBlue(mainColor.blue() + colorChangeSpeed);
        }
        if (db > 0) {
            db--;
            mainColor.setBlue(mainColor.blue() - colorChangeSpeed);
        }

        if (dr == 0) mainColor.setRed(nextColor.red());
        if (dg == 0) mainColor.setGreen(nextColor.green());
        if (db == 0) mainColor.setBlue(nextColor.blue());

        mainColorStr = settingsWin->qcolorToStr(mainColor);
    }

    reloadStyles ();
    equalizerWin->reloadStyles();
}

//           --- Web Socket Functions ---
void MainWindow::remoteDeviceConnect () {
    qDebug() << "New connection";

    auto socket = removeControlServer->nextPendingConnection();
    socket->setParent(this);

    connect(socket, &QWebSocket::textMessageReceived, this, &MainWindow::getRemoteCommands);

    remoteDevices << socket;

    if (channel != NULL) {
        sendMessageToRemoteDevices(playlists[currentPlaylistName][currentID].getName());
    }
}


void MainWindow::getRemoteCommands (const QString & command) {
    qDebug() << command;

    if (command.contains('|')) {
        if (command.mid(0, command.indexOf('|')) == "pos") {
            QString pos = command.mid(command.indexOf('|') + 1);
            double new_time = getDuration() * (pos.toDouble() / 1000.0f);

            qDebug() << "Pos: " << new_time;
            BASS_ChannelSetPosition(channel, BASS_ChannelSeconds2Bytes(channel, new_time), BASS_POS_BYTE);
        }
        if (command.mid(0, command.indexOf('|')) == "vol") {
            QString vol = command.mid(command.indexOf('|') + 1);
            qDebug() << "Pos: " << vol;
            changeVolume(vol.toDouble());
        }
    }
    else {
        if (command == "shuffle") changeShuffle();
        if (command == "pause") pause();
        if (command == "backward") backward();
        if (command == "forward") forward();
        if (command == "repeat") changeRepeat();
    }
}

void MainWindow::httpNewConnection() {
    QFile htmlFile("index.html");

    if (!htmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Error!";
    }

    qDebug() << "New http server connection!";

    auto socket = httpServer->nextPendingConnection();

    QTextStream in(&htmlFile);
    QString line = "";

    while (!in.atEnd()) {
       line += in.readLine();
    }

    line.replace("<serverip>", getLocalAddress());
    line.replace("<serverport>", QString::number(9999));

    QString html = QString::fromStdString("HTTP/1.0 200 Ok\r\n"
                   "Content-Type: text/html; charset=\"utf-8\"\r\n"
                   "\r\n") + line;

    socket->write(html.toStdString().c_str());
    socket->flush();
    socket->waitForBytesWritten(3000);

    QTimer::singleShot(3000, [=]() {
        qDebug() << "Connection closed!";
        socket->close();
    });
}
