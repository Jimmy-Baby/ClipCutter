#pragma once

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>

#include <VLCQtWidgets/WidgetSeek.h>
#include <VLCQtWidgets/WidgetVideo.h>
#include <VLCQtWidgets/WidgetVolumeSlider.h>

QT_BEGIN_NAMESPACE

class C_GuiBase
{
public:
	QAction* Action_Quit;
	QAction* actionPause;
	QAction* actionStop;
	QAction* Action_OpenFolder;
	QAction* actionOpenUrl;
	QWidget* centralwidget;
	QGridLayout* gridLayout_2;
	VlcWidgetSeek* VideoSeek;
	VlcWidgetVideo* VideoFrame;
	VlcWidgetVolumeSlider* VolumeSlider;
	QPushButton* Button_Pause;
	QPushButton* Button_Next;
	QLabel* Label_Times;
	QMenuBar* MenuBar;
	QMenu* Menu_File;
	QStatusBar* StatusBar;

	void SetupUserInterface(QMainWindow* player)
	{
		if (player->objectName().isEmpty())
			player->setObjectName(QStringLiteral("player"));

		player->resize(640, 465);

		Action_Quit = new QAction(player);
		Action_Quit->setObjectName(QStringLiteral("Action_Quit"));
		actionPause = new QAction(player);
		actionPause->setObjectName(QStringLiteral("actionPause"));
		actionPause->setCheckable(true);
		actionStop = new QAction(player);
		actionStop->setObjectName(QStringLiteral("actionStop"));
		Action_OpenFolder = new QAction(player);
		Action_OpenFolder->setObjectName(QStringLiteral("Action_OpenFolder"));
		actionOpenUrl = new QAction(player);
		actionOpenUrl->setObjectName(QStringLiteral("actionOpenUrl"));
		centralwidget = new QWidget(player);
		centralwidget->setObjectName(QStringLiteral("centralwidget"));
		gridLayout_2 = new QGridLayout(centralwidget);
		gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
		VideoSeek = new VlcWidgetSeek(centralwidget);
		VideoSeek->setObjectName(QStringLiteral("VideoSeek"));
		QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
		sizePolicy.setHorizontalStretch(0);
		sizePolicy.setVerticalStretch(0);
		sizePolicy.setHeightForWidth(VideoSeek->sizePolicy().hasHeightForWidth());
		VideoSeek->setSizePolicy(sizePolicy);

		gridLayout_2->addWidget(VideoSeek, 2, 0, 1, 3);

		VideoFrame = new VlcWidgetVideo(centralwidget);
		VideoFrame->setObjectName(QStringLiteral("VideoFrame"));

		gridLayout_2->addWidget(VideoFrame, 0, 0, 1, 3);

		VolumeSlider = new VlcWidgetVolumeSlider(centralwidget);
		VolumeSlider->setObjectName(QStringLiteral("VolumeSlider"));
		sizePolicy.setHeightForWidth(VolumeSlider->sizePolicy().hasHeightForWidth());
		VolumeSlider->setSizePolicy(sizePolicy);

		gridLayout_2->addWidget(VolumeSlider, 4, 0, 1, 2);

		Button_Pause = new QPushButton(centralwidget);
		Button_Pause->setObjectName(QStringLiteral("Button_Pause"));
		Button_Pause->setCheckable(true);

		gridLayout_2->addWidget(Button_Pause, 3, 0, 1, 1);

		Button_Next = new QPushButton(centralwidget);
		Button_Next->setObjectName(QStringLiteral("Button_Next"));

		gridLayout_2->addWidget(Button_Next, 3, 1, 1, 1);

		QFont timesLabelFont;
		timesLabelFont.setFamily(QStringLiteral("Segoe UI"));
		timesLabelFont.setPointSize(11);

		Label_Times = new QLabel(centralwidget);
		Label_Times->setObjectName(QStringLiteral("label"));
		Label_Times->setFont(timesLabelFont);

		gridLayout_2->addWidget(Label_Times, 3, 2, 1, 1);

		player->setCentralWidget(centralwidget);
		MenuBar = new QMenuBar(player);
		MenuBar->setObjectName(QStringLiteral("MenuBar"));
		MenuBar->setGeometry(QRect(0, 0, 640, 21));
		Menu_File = new QMenu(MenuBar);
		Menu_File->setObjectName(QStringLiteral("Menu_File"));
		player->setMenuBar(MenuBar);
		StatusBar = new QStatusBar(player);
		StatusBar->setObjectName(QStringLiteral("StatusBar"));
		player->setStatusBar(StatusBar);

		MenuBar->addAction(Menu_File->menuAction());
		Menu_File->addAction(Action_OpenFolder);
		Menu_File->addSeparator();
		Menu_File->addAction(Action_Quit);

		SetStrings(player);
		QObject::connect(Action_Quit, SIGNAL(triggered()), player, SLOT(close()));

		QMetaObject::connectSlotsByName(player);
	}

	void SetStrings(QMainWindow* player) const
	{
		player->setWindowTitle(QApplication::translate("player", "MassClipCutter", nullptr));

		Action_Quit->setText(QApplication::translate("player", "Quit", nullptr));
		actionPause->setText(QApplication::translate("player", "Pause", nullptr));
		actionStop->setText(QApplication::translate("player", "Stop", nullptr));
		Action_OpenFolder->setText(QApplication::translate("player", "Open local folder", nullptr));
		actionOpenUrl->setText(QApplication::translate("player", "Open URL", nullptr));
		Button_Pause->setText(QApplication::translate("player", "Play/Pause", nullptr));
		Button_Next->setText(QApplication::translate("player", "Next Clip", nullptr));
		Label_Times->setText(QApplication::translate("player", "[ 00:00:00 -> (00:00:00 - 00:00:00) ]", nullptr));
		Menu_File->setTitle(QApplication::translate("player", "File", nullptr));
	}
};
