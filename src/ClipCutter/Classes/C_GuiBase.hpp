#pragma once

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLineEdit>

#include <VLCQtWidgets/WidgetSeek.h>
#include <VLCQtWidgets/WidgetVideo.h>
#include <VLCQtWidgets/WidgetVolumeSlider.h>

#include "C_GuiApp.hpp"

QT_BEGIN_NAMESPACE

class C_GuiBase
{
public:
	QAction* Action_Quit;
	QAction* actionPause;
	QAction* actionStop;
	QAction* Action_OpenFolder;
	QAction* actionOpenUrl;
	QWidget* Widget_Central;
	QGridLayout* Grid_Layout;
	VlcWidgetSeek* VideoSeek;
	VlcWidgetVideo* VideoFrame;
	VlcWidgetVolumeSlider* VolumeSlider;
	QPushButton* Button_Pause;
	QPushButton* Button_Next;
	QPushButton* Button_CutAll;
	QCheckBox* Checkbox_DelOrig;
	QPushButton* Button_ToggleRenamePostfix;
	QPushButton* Button_SetStart;
	QPushButton* Button_SetEnd;
	QLineEdit* LineEdit_RenameOrPostfix;
	QLineEdit* LineEdit_ClipInfo;
	QLabel* Label_Times;
	QMenuBar* MenuBar;
	QMenu* Menu_File;
	QStatusBar* StatusBar;
	QLabel* Label_StatusBar;
	QProgressBar* ProgressBar_StatusBar;

	void SetupUserInterface(QMainWindow* player)
	{
		if (player->objectName().isEmpty())
			player->setObjectName(QStringLiteral("player"));

		player->resize(1262, 900);
		player->setMaximumSize(1262, 900);

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
		Widget_Central = new QWidget(player);
		Widget_Central->setObjectName(QStringLiteral("Widget_Central"));
		Grid_Layout = new QGridLayout(Widget_Central);
		Grid_Layout->setObjectName(QStringLiteral("Grid_Layout"));
		VideoSeek = new VlcWidgetSeek(Widget_Central);
		VideoSeek->setObjectName(QStringLiteral("VideoSeek"));
		QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
		sizePolicy.setHorizontalStretch(0);
		sizePolicy.setVerticalStretch(0);
		sizePolicy.setHeightForWidth(VideoSeek->sizePolicy().hasHeightForWidth());
		VideoSeek->setSizePolicy(sizePolicy);

		Grid_Layout->addWidget(VideoSeek, 2, 0, 1, 3);

		VideoFrame = new VlcWidgetVideo(Widget_Central);
		VideoFrame->setObjectName(QStringLiteral("VideoFrame"));

		Grid_Layout->addWidget(VideoFrame, 0, 0, 1, 3);

		VolumeSlider = new VlcWidgetVolumeSlider(Widget_Central);
		VolumeSlider->setObjectName(QStringLiteral("VolumeSlider"));
		sizePolicy.setHeightForWidth(VolumeSlider->sizePolicy().hasHeightForWidth());
		VolumeSlider->setSizePolicy(sizePolicy);

		// Grid_Layout->addWidget(VolumeSlider, 4, 0, 1, 2);

		Button_Pause = new QPushButton(Widget_Central);
		Button_Pause->setObjectName(QStringLiteral("Button_Pause"));
		Button_Pause->setMaximumWidth(132);

		Grid_Layout->addWidget(Button_Pause, 3, 0, 1, 1);

		Button_Next = new QPushButton(Widget_Central);
		Button_Next->setObjectName(QStringLiteral("Button_Next"));

		Grid_Layout->addWidget(Button_Next, 3, 1, 1, 1);

		Button_CutAll = new QPushButton(Widget_Central);
		Button_CutAll->setObjectName(QStringLiteral("Button_CutAll"));
		Button_CutAll->setMaximumWidth(132);

		Grid_Layout->addWidget(Button_CutAll, 4, 0, 1, 1);

		Button_SetStart = new QPushButton(Widget_Central);
		Button_SetStart->setObjectName(QStringLiteral("Button_SetStart"));

		Grid_Layout->addWidget(Button_SetStart, 4, 1, 1, 1);

		Button_SetEnd = new QPushButton(Widget_Central);
		Button_SetEnd->setObjectName(QStringLiteral("Button_SetEnd"));

		Grid_Layout->addWidget(Button_SetEnd, 5, 1, 1, 1);

		LineEdit_ClipInfo = new QLineEdit(Widget_Central);
		LineEdit_ClipInfo->setObjectName(QStringLiteral("LineEdit_ClipInfo"));
		LineEdit_ClipInfo->setReadOnly(true);
		LineEdit_ClipInfo->setMaximumWidth(300);

		Grid_Layout->addWidget(LineEdit_ClipInfo, 5, 2, 1, 1);

		Checkbox_DelOrig = new QCheckBox(player);
		Checkbox_DelOrig->setObjectName("Checkbox_DelOrig");
		Checkbox_DelOrig->setEnabled(false);

		Grid_Layout->addWidget(Checkbox_DelOrig, 5, 0, 1, 1);

		Button_ToggleRenamePostfix = new QPushButton(player);
		Button_ToggleRenamePostfix->setObjectName("Button_ToggleRenamePostfix");

		Grid_Layout->addWidget(Button_ToggleRenamePostfix, 3, 2, 1, 1);

		LineEdit_RenameOrPostfix = new QLineEdit(player);
		LineEdit_RenameOrPostfix->setObjectName(QStringLiteral("LineEdit_RenameOrPostfix"));
		LineEdit_RenameOrPostfix->setMaximumWidth(300);

		Grid_Layout->addWidget(LineEdit_RenameOrPostfix, 4, 2, 1, 1);

		player->setCentralWidget(Widget_Central);
		MenuBar = new QMenuBar(player);
		MenuBar->setObjectName(QStringLiteral("MenuBar"));
		MenuBar->setGeometry(QRect(0, 0, 640, 21));

		Menu_File = new QMenu(MenuBar);
		Menu_File->setObjectName(QStringLiteral("Menu_File"));
		player->setMenuBar(MenuBar);

		Label_StatusBar = new QLabel(player);
		Label_StatusBar->setObjectName(QStringLiteral("Label_StatusBar"));
		Label_StatusBar->setMaximumWidth(100);

		ProgressBar_StatusBar = new QProgressBar(player);
		ProgressBar_StatusBar->setObjectName(QStringLiteral("ProgressBar_StatusBar"));
		ProgressBar_StatusBar->setTextVisible(false);

		StatusBar = new QStatusBar(player);
		StatusBar->setObjectName(QStringLiteral("StatusBar"));
		StatusBar->addPermanentWidget(Label_StatusBar, Qt::AlignLeft);
		StatusBar->addPermanentWidget(ProgressBar_StatusBar, Qt::AlignLeft);
		player->setStatusBar(StatusBar);

		MenuBar->addAction(Menu_File->menuAction());
		Menu_File->addAction(Action_OpenFolder);
		Menu_File->addSeparator();
		Menu_File->addAction(Action_Quit);

		SetStrings(player);

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
		Button_CutAll->setText(QApplication::translate("player", "Process Clips", nullptr));
		LineEdit_ClipInfo->setText(
			QApplication::translate("player", "[ Clip 0 of 0 ]: Start: 00:00:00 - End: 00:00:00", nullptr));
		Label_StatusBar->setText(QApplication::translate("player", "Ready", nullptr));
		Checkbox_DelOrig->setText(QApplication::translate("player", "Delete Original", nullptr));
		Button_ToggleRenamePostfix->setText(QApplication::translate("player", "Use File Postfix", nullptr));
		Button_SetEnd->setText(QApplication::translate("player", "Set End Point", nullptr));
		Button_SetStart->setText(QApplication::translate("player", "Use Start Point", nullptr));
		LineEdit_RenameOrPostfix->setPlaceholderText(QApplication::translate("player", "Filename", nullptr));
		Menu_File->setTitle(QApplication::translate("player", "File", nullptr));
	}
};
