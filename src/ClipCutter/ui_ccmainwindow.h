/********************************************************************************
** Form generated from reading UI file 'ccmainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.5.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CCMAINWINDOW_H
#define UI_CCMAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ClipCutterWindow
{
public:
    QAction *actionOpenFolder;
    QAction *actionPlayPause;
    QAction *actionNext;
    QAction *actionPrev;
    QAction *actionSetStart;
    QAction *actionSetEnd;
    QAction *actionStop;
    QAction *actionSkip;
    QAction *actionOpenFiles;
    QAction *actionProcessClips;
    QWidget *centralWidget;
    QGridLayout *gridLayout;
    QGroupBox *clipsBox;
    QGridLayout *gridLayout_2;
    QGroupBox *clipsGroupBox;
    QGridLayout *gridLayout_3;
    QTreeWidget *clipsTree;
    QPushButton *skipAllButton;
    QGroupBox *clipNameBox;
    QGridLayout *gridLayout_5;
    QLineEdit *clipNameEdit;
    QLabel *clipNameLabel;
    QGroupBox *keywordsGroupBox;
    QGridLayout *gridLayout_4;
    QTreeWidget *keywordsTree;
    QLineEdit *keywordEdit;
    QPushButton *buttonAddKeyword;
    QPushButton *buttonUseSelected;
    QPushButton *buttonRemoveSelected;
    QGroupBox *processButtonBox;
    QHBoxLayout *horizontalLayout;
    QPushButton *processButton;
    QSpacerItem *horizontalSpacer_2;
    QLabel *qualityLabel;
    QComboBox *qualityCombo;
    QCheckBox *checkboxCopyMetadata;
    QCheckBox *checkboxShowFfmpeg;
    QSpacerItem *horizontalSpacer;
    QProgressBar *progressBar;
    QGroupBox *volumeBox;
    QGridLayout *gridLayout_7;
    QLabel *volumeLabel;
    QSlider *volumeSlider;
    QGroupBox *playerBox;
    QGridLayout *playerLayout;
    QGroupBox *timelineBox;
    QGridLayout *gridLayout_6;
    QSlider *timelineSlider;
    QLabel *startEndLabel;
    QLabel *timelineLabel;
    QToolBar *toolBar;
    QMenuBar *menuBar;
    QMenu *menuFile;

    void setupUi(QMainWindow *ClipCutterWindow)
    {
        if (ClipCutterWindow->objectName().isEmpty())
            ClipCutterWindow->setObjectName("ClipCutterWindow");
        ClipCutterWindow->resize(1280, 720);
        ClipCutterWindow->setMinimumSize(QSize(1280, 720));
        ClipCutterWindow->setAcceptDrops(true);
        ClipCutterWindow->setToolTipDuration(0);
        ClipCutterWindow->setStyleSheet(QString::fromUtf8(""));
        actionOpenFolder = new QAction(ClipCutterWindow);
        actionOpenFolder->setObjectName("actionOpenFolder");
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/icons/folder-open-regular.svg"), QSize(), QIcon::Normal, QIcon::Off);
        actionOpenFolder->setIcon(icon);
        actionPlayPause = new QAction(ClipCutterWindow);
        actionPlayPause->setObjectName("actionPlayPause");
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/icons/play-solid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        actionPlayPause->setIcon(icon1);
        actionNext = new QAction(ClipCutterWindow);
        actionNext->setObjectName("actionNext");
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/icons/forward-step-solid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        actionNext->setIcon(icon2);
        actionPrev = new QAction(ClipCutterWindow);
        actionPrev->setObjectName("actionPrev");
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icons/icons/backward-step-solid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        actionPrev->setIcon(icon3);
        actionSetStart = new QAction(ClipCutterWindow);
        actionSetStart->setObjectName("actionSetStart");
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/icons/s-solid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        actionSetStart->setIcon(icon4);
        actionSetEnd = new QAction(ClipCutterWindow);
        actionSetEnd->setObjectName("actionSetEnd");
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/icons/e-solid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        actionSetEnd->setIcon(icon5);
        actionStop = new QAction(ClipCutterWindow);
        actionStop->setObjectName("actionStop");
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/icons/icons/stop-solid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        actionStop->setIcon(icon6);
        actionSkip = new QAction(ClipCutterWindow);
        actionSkip->setObjectName("actionSkip");
        QIcon icon7;
        icon7.addFile(QString::fromUtf8(":/icons/icons/forward-fast-solid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        actionSkip->setIcon(icon7);
        actionOpenFiles = new QAction(ClipCutterWindow);
        actionOpenFiles->setObjectName("actionOpenFiles");
        actionOpenFiles->setIcon(icon);
        actionProcessClips = new QAction(ClipCutterWindow);
        actionProcessClips->setObjectName("actionProcessClips");
        actionProcessClips->setMenuRole(QAction::NoRole);
        centralWidget = new QWidget(ClipCutterWindow);
        centralWidget->setObjectName("centralWidget");
        gridLayout = new QGridLayout(centralWidget);
        gridLayout->setSpacing(6);
        gridLayout->setContentsMargins(11, 11, 11, 11);
        gridLayout->setObjectName("gridLayout");
        clipsBox = new QGroupBox(centralWidget);
        clipsBox->setObjectName("clipsBox");
        clipsBox->setMinimumSize(QSize(400, 0));
        clipsBox->setMaximumSize(QSize(400, 16777215));
        gridLayout_2 = new QGridLayout(clipsBox);
        gridLayout_2->setSpacing(6);
        gridLayout_2->setContentsMargins(11, 11, 11, 11);
        gridLayout_2->setObjectName("gridLayout_2");
        clipsGroupBox = new QGroupBox(clipsBox);
        clipsGroupBox->setObjectName("clipsGroupBox");
        gridLayout_3 = new QGridLayout(clipsGroupBox);
        gridLayout_3->setSpacing(6);
        gridLayout_3->setContentsMargins(11, 11, 11, 11);
        gridLayout_3->setObjectName("gridLayout_3");
        clipsTree = new QTreeWidget(clipsGroupBox);
        clipsTree->setObjectName("clipsTree");
        clipsTree->setIndentation(0);
        clipsTree->setAnimated(false);
        clipsTree->setHeaderHidden(false);
        clipsTree->setExpandsOnDoubleClick(true);
        clipsTree->header()->setDefaultSectionSize(35);

        gridLayout_3->addWidget(clipsTree, 4, 0, 1, 1);

        skipAllButton = new QPushButton(clipsGroupBox);
        skipAllButton->setObjectName("skipAllButton");

        gridLayout_3->addWidget(skipAllButton, 3, 0, 1, 1);


        gridLayout_2->addWidget(clipsGroupBox, 2, 0, 1, 1);

        clipNameBox = new QGroupBox(clipsBox);
        clipNameBox->setObjectName("clipNameBox");
        gridLayout_5 = new QGridLayout(clipNameBox);
        gridLayout_5->setSpacing(6);
        gridLayout_5->setContentsMargins(11, 11, 11, 11);
        gridLayout_5->setObjectName("gridLayout_5");
        clipNameEdit = new QLineEdit(clipNameBox);
        clipNameEdit->setObjectName("clipNameEdit");

        gridLayout_5->addWidget(clipNameEdit, 0, 1, 1, 1);

        clipNameLabel = new QLabel(clipNameBox);
        clipNameLabel->setObjectName("clipNameLabel");
        clipNameLabel->setMaximumSize(QSize(16777215, 16777215));

        gridLayout_5->addWidget(clipNameLabel, 0, 0, 1, 1);


        gridLayout_2->addWidget(clipNameBox, 1, 0, 1, 1);

        keywordsGroupBox = new QGroupBox(clipsBox);
        keywordsGroupBox->setObjectName("keywordsGroupBox");
        gridLayout_4 = new QGridLayout(keywordsGroupBox);
        gridLayout_4->setSpacing(6);
        gridLayout_4->setContentsMargins(11, 11, 11, 11);
        gridLayout_4->setObjectName("gridLayout_4");
        gridLayout_4->setSizeConstraint(QLayout::SetDefaultConstraint);
        keywordsTree = new QTreeWidget(keywordsGroupBox);
        keywordsTree->setObjectName("keywordsTree");
        keywordsTree->setIndentation(0);
        keywordsTree->setSortingEnabled(false);
        keywordsTree->setAnimated(false);
        keywordsTree->setHeaderHidden(false);
        keywordsTree->setExpandsOnDoubleClick(true);
        keywordsTree->setColumnCount(2);
        keywordsTree->header()->setDefaultSectionSize(35);

        gridLayout_4->addWidget(keywordsTree, 0, 0, 1, 3);

        keywordEdit = new QLineEdit(keywordsGroupBox);
        keywordEdit->setObjectName("keywordEdit");

        gridLayout_4->addWidget(keywordEdit, 1, 0, 1, 1);

        buttonAddKeyword = new QPushButton(keywordsGroupBox);
        buttonAddKeyword->setObjectName("buttonAddKeyword");

        gridLayout_4->addWidget(buttonAddKeyword, 1, 1, 1, 2);

        buttonUseSelected = new QPushButton(keywordsGroupBox);
        buttonUseSelected->setObjectName("buttonUseSelected");

        gridLayout_4->addWidget(buttonUseSelected, 2, 0, 1, 1);

        buttonRemoveSelected = new QPushButton(keywordsGroupBox);
        buttonRemoveSelected->setObjectName("buttonRemoveSelected");

        gridLayout_4->addWidget(buttonRemoveSelected, 2, 1, 1, 2);


        gridLayout_2->addWidget(keywordsGroupBox, 3, 0, 1, 1);


        gridLayout->addWidget(clipsBox, 4, 4, 2, 4);

        processButtonBox = new QGroupBox(centralWidget);
        processButtonBox->setObjectName("processButtonBox");
        processButtonBox->setMaximumSize(QSize(600050, 70));
        processButtonBox->setStyleSheet(QString::fromUtf8(""));
        horizontalLayout = new QHBoxLayout(processButtonBox);
        horizontalLayout->setSpacing(6);
        horizontalLayout->setContentsMargins(11, 11, 11, 11);
        horizontalLayout->setObjectName("horizontalLayout");
        processButton = new QPushButton(processButtonBox);
        processButton->setObjectName("processButton");
        processButton->setMinimumSize(QSize(150, 0));
        processButton->setMaximumSize(QSize(150, 16777215));

        horizontalLayout->addWidget(processButton);

        horizontalSpacer_2 = new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        qualityLabel = new QLabel(processButtonBox);
        qualityLabel->setObjectName("qualityLabel");
        qualityLabel->setMaximumSize(QSize(80, 16777215));

        horizontalLayout->addWidget(qualityLabel);

        qualityCombo = new QComboBox(processButtonBox);
        qualityCombo->setObjectName("qualityCombo");
        qualityCombo->setMinimumSize(QSize(150, 0));
        qualityCombo->setMaximumSize(QSize(150, 16777215));
        qualityCombo->setEditable(false);

        horizontalLayout->addWidget(qualityCombo);

        checkboxCopyMetadata = new QCheckBox(processButtonBox);
        checkboxCopyMetadata->setObjectName("checkboxCopyMetadata");
        checkboxCopyMetadata->setMaximumSize(QSize(110, 16777215));

        horizontalLayout->addWidget(checkboxCopyMetadata);

        checkboxShowFfmpeg = new QCheckBox(processButtonBox);
        checkboxShowFfmpeg->setObjectName("checkboxShowFfmpeg");
        checkboxShowFfmpeg->setMaximumSize(QSize(145, 16777215));

        horizontalLayout->addWidget(checkboxShowFfmpeg);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        gridLayout->addWidget(processButtonBox, 6, 2, 1, 1);

        progressBar = new QProgressBar(centralWidget);
        progressBar->setObjectName("progressBar");
        progressBar->setValue(0);

        gridLayout->addWidget(progressBar, 8, 2, 1, 5);

        volumeBox = new QGroupBox(centralWidget);
        volumeBox->setObjectName("volumeBox");
        volumeBox->setMaximumSize(QSize(400, 70));
        volumeBox->setStyleSheet(QString::fromUtf8(""));
        gridLayout_7 = new QGridLayout(volumeBox);
        gridLayout_7->setSpacing(6);
        gridLayout_7->setContentsMargins(11, 11, 11, 11);
        gridLayout_7->setObjectName("gridLayout_7");
        volumeLabel = new QLabel(volumeBox);
        volumeLabel->setObjectName("volumeLabel");

        gridLayout_7->addWidget(volumeLabel, 0, 0, 1, 1);

        volumeSlider = new QSlider(volumeBox);
        volumeSlider->setObjectName("volumeSlider");
        volumeSlider->setMaximum(100);
        volumeSlider->setValue(100);
        volumeSlider->setOrientation(Qt::Horizontal);

        gridLayout_7->addWidget(volumeSlider, 0, 1, 1, 1);


        gridLayout->addWidget(volumeBox, 6, 4, 1, 1);

        playerBox = new QGroupBox(centralWidget);
        playerBox->setObjectName("playerBox");
        playerBox->setMinimumSize(QSize(850, 0));
        playerLayout = new QGridLayout(playerBox);
        playerLayout->setSpacing(6);
        playerLayout->setContentsMargins(11, 11, 11, 11);
        playerLayout->setObjectName("playerLayout");

        gridLayout->addWidget(playerBox, 4, 2, 1, 1);

        timelineBox = new QGroupBox(centralWidget);
        timelineBox->setObjectName("timelineBox");
        timelineBox->setMaximumSize(QSize(16777215, 50));
        gridLayout_6 = new QGridLayout(timelineBox);
        gridLayout_6->setSpacing(6);
        gridLayout_6->setContentsMargins(11, 11, 11, 11);
        gridLayout_6->setObjectName("gridLayout_6");
        timelineSlider = new QSlider(timelineBox);
        timelineSlider->setObjectName("timelineSlider");
        timelineSlider->setStyleSheet(QString::fromUtf8(""));
        timelineSlider->setMaximum(1);
        timelineSlider->setValue(0);
        timelineSlider->setOrientation(Qt::Horizontal);

        gridLayout_6->addWidget(timelineSlider, 0, 1, 1, 1);

        startEndLabel = new QLabel(timelineBox);
        startEndLabel->setObjectName("startEndLabel");

        gridLayout_6->addWidget(startEndLabel, 0, 2, 1, 1);

        timelineLabel = new QLabel(timelineBox);
        timelineLabel->setObjectName("timelineLabel");

        gridLayout_6->addWidget(timelineLabel, 0, 0, 1, 1);


        gridLayout->addWidget(timelineBox, 5, 2, 1, 2);

        ClipCutterWindow->setCentralWidget(centralWidget);
        toolBar = new QToolBar(ClipCutterWindow);
        toolBar->setObjectName("toolBar");
        toolBar->setMovable(false);
        ClipCutterWindow->addToolBar(Qt::TopToolBarArea, toolBar);
        menuBar = new QMenuBar(ClipCutterWindow);
        menuBar->setObjectName("menuBar");
        menuBar->setGeometry(QRect(0, 0, 1280, 21));
        menuFile = new QMenu(menuBar);
        menuFile->setObjectName("menuFile");
        ClipCutterWindow->setMenuBar(menuBar);

        toolBar->addAction(actionPlayPause);
        toolBar->addAction(actionPrev);
        toolBar->addAction(actionStop);
        toolBar->addAction(actionNext);
        toolBar->addAction(actionSkip);
        toolBar->addAction(actionSetStart);
        toolBar->addAction(actionSetEnd);
        toolBar->addSeparator();
        menuBar->addAction(menuFile->menuAction());
        menuFile->addAction(actionOpenFolder);
        menuFile->addAction(actionOpenFiles);
        menuFile->addSeparator();
        menuFile->addAction(actionProcessClips);
        menuFile->addSeparator();
        menuFile->addAction(actionPlayPause);
        menuFile->addAction(actionNext);
        menuFile->addAction(actionPrev);
        menuFile->addAction(actionSkip);
        menuFile->addAction(actionStop);
        menuFile->addAction(actionSetStart);
        menuFile->addAction(actionSetEnd);

        retranslateUi(ClipCutterWindow);

        qualityCombo->setCurrentIndex(-1);


        QMetaObject::connectSlotsByName(ClipCutterWindow);
    } // setupUi

    void retranslateUi(QMainWindow *ClipCutterWindow)
    {
        ClipCutterWindow->setWindowTitle(QCoreApplication::translate("ClipCutterWindow", "ClipCutter", nullptr));
        actionOpenFolder->setText(QCoreApplication::translate("ClipCutterWindow", "Open Folder", nullptr));
#if QT_CONFIG(tooltip)
        actionOpenFolder->setToolTip(QCoreApplication::translate("ClipCutterWindow", "Open Folder (Ctrl + O)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionOpenFolder->setShortcut(QCoreApplication::translate("ClipCutterWindow", "Ctrl+O", nullptr));
#endif // QT_CONFIG(shortcut)
        actionPlayPause->setText(QCoreApplication::translate("ClipCutterWindow", "Play/Pause", nullptr));
#if QT_CONFIG(tooltip)
        actionPlayPause->setToolTip(QCoreApplication::translate("ClipCutterWindow", "Play/Pause (Spacebar)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionPlayPause->setShortcut(QCoreApplication::translate("ClipCutterWindow", "Space", nullptr));
#endif // QT_CONFIG(shortcut)
        actionNext->setText(QCoreApplication::translate("ClipCutterWindow", "Next", nullptr));
#if QT_CONFIG(tooltip)
        actionNext->setToolTip(QCoreApplication::translate("ClipCutterWindow", "Next Clip (E)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionNext->setShortcut(QCoreApplication::translate("ClipCutterWindow", "E", nullptr));
#endif // QT_CONFIG(shortcut)
        actionPrev->setText(QCoreApplication::translate("ClipCutterWindow", "Prev", nullptr));
#if QT_CONFIG(tooltip)
        actionPrev->setToolTip(QCoreApplication::translate("ClipCutterWindow", "Prev Clip (Q)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionPrev->setShortcut(QCoreApplication::translate("ClipCutterWindow", "Q", nullptr));
#endif // QT_CONFIG(shortcut)
        actionSetStart->setText(QCoreApplication::translate("ClipCutterWindow", "Set Start", nullptr));
#if QT_CONFIG(tooltip)
        actionSetStart->setToolTip(QCoreApplication::translate("ClipCutterWindow", "Set Start (A)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionSetStart->setShortcut(QCoreApplication::translate("ClipCutterWindow", "A", nullptr));
#endif // QT_CONFIG(shortcut)
        actionSetEnd->setText(QCoreApplication::translate("ClipCutterWindow", "Set End", nullptr));
#if QT_CONFIG(tooltip)
        actionSetEnd->setToolTip(QCoreApplication::translate("ClipCutterWindow", "Set End (D)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionSetEnd->setShortcut(QCoreApplication::translate("ClipCutterWindow", "D", nullptr));
#endif // QT_CONFIG(shortcut)
        actionStop->setText(QCoreApplication::translate("ClipCutterWindow", "Stop", nullptr));
#if QT_CONFIG(tooltip)
        actionStop->setToolTip(QCoreApplication::translate("ClipCutterWindow", "Stop (S)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionStop->setShortcut(QCoreApplication::translate("ClipCutterWindow", "S", nullptr));
#endif // QT_CONFIG(shortcut)
        actionSkip->setText(QCoreApplication::translate("ClipCutterWindow", "Skip", nullptr));
#if QT_CONFIG(tooltip)
        actionSkip->setToolTip(QCoreApplication::translate("ClipCutterWindow", "Mark as skipped, and go to next clip (Ctrl + E)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionSkip->setShortcut(QCoreApplication::translate("ClipCutterWindow", "Ctrl+E", nullptr));
#endif // QT_CONFIG(shortcut)
        actionOpenFiles->setText(QCoreApplication::translate("ClipCutterWindow", "Open File(s)", nullptr));
#if QT_CONFIG(tooltip)
        actionOpenFiles->setToolTip(QCoreApplication::translate("ClipCutterWindow", "Load Files into ClipCutter's video list (Ctrl + Alt + O)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionOpenFiles->setShortcut(QCoreApplication::translate("ClipCutterWindow", "Ctrl+Alt+O", nullptr));
#endif // QT_CONFIG(shortcut)
        actionProcessClips->setText(QCoreApplication::translate("ClipCutterWindow", "Process Clips", nullptr));
#if QT_CONFIG(tooltip)
        actionProcessClips->setToolTip(QCoreApplication::translate("ClipCutterWindow", "Process clips in the video list (Ctrl + P)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionProcessClips->setShortcut(QCoreApplication::translate("ClipCutterWindow", "Ctrl+P", nullptr));
#endif // QT_CONFIG(shortcut)
        clipsBox->setTitle(QCoreApplication::translate("ClipCutterWindow", "Clips", nullptr));
        clipsGroupBox->setTitle(QString());
        QTreeWidgetItem *___qtreewidgetitem = clipsTree->headerItem();
        ___qtreewidgetitem->setText(1, QCoreApplication::translate("ClipCutterWindow", "Name", nullptr));
        ___qtreewidgetitem->setText(0, QCoreApplication::translate("ClipCutterWindow", "Skip?", nullptr));
        skipAllButton->setText(QCoreApplication::translate("ClipCutterWindow", "Skip All", nullptr));
        clipNameBox->setTitle(QString());
        clipNameLabel->setText(QCoreApplication::translate("ClipCutterWindow", "Clip Name:", nullptr));
        keywordsGroupBox->setTitle(QString());
        QTreeWidgetItem *___qtreewidgetitem1 = keywordsTree->headerItem();
        ___qtreewidgetitem1->setText(1, QCoreApplication::translate("ClipCutterWindow", "Keyword", nullptr));
        ___qtreewidgetitem1->setText(0, QCoreApplication::translate("ClipCutterWindow", "Use?", nullptr));
        buttonAddKeyword->setText(QCoreApplication::translate("ClipCutterWindow", "Add", nullptr));
        buttonUseSelected->setText(QCoreApplication::translate("ClipCutterWindow", "Use Selected", nullptr));
        buttonRemoveSelected->setText(QCoreApplication::translate("ClipCutterWindow", "Remove Selected From List", nullptr));
        processButtonBox->setTitle(QCoreApplication::translate("ClipCutterWindow", "Output", nullptr));
        processButton->setText(QCoreApplication::translate("ClipCutterWindow", "Process Clips", nullptr));
        qualityLabel->setText(QCoreApplication::translate("ClipCutterWindow", "Quality Preset:", nullptr));
#if QT_CONFIG(tooltip)
        qualityCombo->setToolTip(QCoreApplication::translate("ClipCutterWindow", "<html><head/><body><p><span style=\" font-weight:700;\">Copy</span> - Very fast, and maintains original quality. Copies all video and audio data directly, may result in out-of-sync video/audio in some cases.<br/><br/><span style=\" font-weight:700;\">Lowest</span> - Lowest quality re-encoding (CRF 35).<br/><br/><span style=\" font-weight:700;\">Low</span> - Low quality re-encoding (CRF 30).<br/><br/><span style=\" font-weight:700;\">Medium</span> - Medium quality re-encoding (CRF 25).<br/><br/><span style=\" font-weight:700;\">High</span> - High quality re-encoding (CRF 20).<br/><br/><span style=\" font-weight:700;\">Best</span> - Highest quality re-encoding (CRF 15).</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(statustip)
        qualityCombo->setStatusTip(QString());
#endif // QT_CONFIG(statustip)
        qualityCombo->setCurrentText(QString());
        qualityCombo->setPlaceholderText(QCoreApplication::translate("ClipCutterWindow", "Select quality preset", nullptr));
#if QT_CONFIG(tooltip)
        checkboxCopyMetadata->setToolTip(QCoreApplication::translate("ClipCutterWindow", "If enabled, ClipCutter will copy the date/time information from the source videos onto the trimmed videos", nullptr));
#endif // QT_CONFIG(tooltip)
        checkboxCopyMetadata->setText(QCoreApplication::translate("ClipCutterWindow", "Copy Date/Time", nullptr));
        checkboxShowFfmpeg->setText(QCoreApplication::translate("ClipCutterWindow", "Show FFmpeg Window", nullptr));
        volumeBox->setTitle(QCoreApplication::translate("ClipCutterWindow", "Controls", nullptr));
        volumeLabel->setText(QCoreApplication::translate("ClipCutterWindow", "Volume:", nullptr));
#if QT_CONFIG(accessibility)
        volumeSlider->setAccessibleName(QString());
#endif // QT_CONFIG(accessibility)
        playerBox->setTitle(QCoreApplication::translate("ClipCutterWindow", "Player", nullptr));
        timelineBox->setTitle(QString());
        startEndLabel->setText(QCoreApplication::translate("ClipCutterWindow", "Start: 00:00:00.000 / End: 00:00:00.000", nullptr));
        timelineLabel->setText(QCoreApplication::translate("ClipCutterWindow", "00:00:00.000", nullptr));
        toolBar->setWindowTitle(QCoreApplication::translate("ClipCutterWindow", "toolBar", nullptr));
        menuFile->setTitle(QCoreApplication::translate("ClipCutterWindow", "File", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ClipCutterWindow: public Ui_ClipCutterWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CCMAINWINDOW_H
