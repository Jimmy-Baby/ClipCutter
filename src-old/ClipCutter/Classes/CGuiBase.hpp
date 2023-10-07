#pragma once

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
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>

#include <VLCQtWidgets/WidgetSeek.h>
#include <VLCQtWidgets/WidgetVideo.h>
#include <VLCQtWidgets/WidgetVolumeSlider.h>

#include "CCutterApp.hpp"
#include "CGuiBase.hpp"

QT_BEGIN_NAMESPACE

class CGuiBase
{
public:
	QAction* Action_Quit;
	QAction* Action_Pause;
	QAction* Action_Stop;
	QAction* Action_OpenFolder;
	QWidget* Widget_Central;
	QGridLayout* Grid_Layout;
	VlcWidgetSeek* VideoSeek;
	VlcWidgetVideo* VideoFrame;
	VlcWidgetVolumeSlider* VolumeSlider;
	QPushButton* Button_Pause;
	QPushButton* Button_Next;
	QPushButton* Button_Skip;
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
	QCheckBox* Checkbox_ReEncode;
	QComboBox* Combobox_ReEncodeQuality;


	void SetupUserInterface(QMainWindow* player)
	{
		if (player->objectName().isEmpty())
		{
			player->setObjectName(QStringLiteral("player"));
		}

		player->resize(1262, 900);
		player->setMaximumSize(1262, 900);

		Action_Quit = new QAction(player);
		Action_Quit->setObjectName(QStringLiteral("Action_Quit"));
		Action_Pause = new QAction(player);
		Action_Pause->setObjectName(QStringLiteral("Action_Pause"));
		Action_Pause->setCheckable(true);
		Action_Stop = new QAction(player);
		Action_Stop->setObjectName(QStringLiteral("Action_Stop"));
		Action_OpenFolder = new QAction(player);
		Action_OpenFolder->setObjectName(QStringLiteral("Action_OpenFolder"));
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

		/*
		 * Items ordered by order shown on screen, top to bottom, left to right
		 */

		// Video frame
		VideoFrame = new VlcWidgetVideo(Widget_Central);
		VideoFrame->setObjectName(QStringLiteral("VideoFrame"));

		Grid_Layout->addWidget(VideoFrame, 0, 0, 1, 4);

		// Video seek
		Grid_Layout->addWidget(VideoSeek, 2, 0, 1, 4);

		// Play/Pause button
		Button_Pause = new QPushButton(Widget_Central);
		Button_Pause->setObjectName(QStringLiteral("Button_Pause"));
		Button_Pause->setMaximumWidth(132);

		Grid_Layout->addWidget(Button_Pause, 3, 0, 1, 1);

		// Next clip button
		Button_Next = new QPushButton(Widget_Central);
		Button_Next->setObjectName(QStringLiteral("Button_Next"));

		Grid_Layout->addWidget(Button_Next, 3, 1, 1, 1);

		// Skip clip button
		Button_Skip = new QPushButton(Widget_Central);
		Button_Skip->setObjectName(QStringLiteral("Button_Skip"));

		Grid_Layout->addWidget(Button_Skip, 3, 2, 1, 1);

		// Toggle postfix/filename button
		Button_ToggleRenamePostfix = new QPushButton(player);
		Button_ToggleRenamePostfix->setObjectName("Button_ToggleRenamePostfix");

		Grid_Layout->addWidget(Button_ToggleRenamePostfix, 3, 3, 1, 1);

		// Process clips button
		Button_CutAll = new QPushButton(Widget_Central);
		Button_CutAll->setObjectName(QStringLiteral("Button_CutAll"));
		Button_CutAll->setMaximumWidth(132);

		Grid_Layout->addWidget(Button_CutAll, 4, 0, 1, 1);

		// Set start point button
		Button_SetStart = new QPushButton(Widget_Central);
		Button_SetStart->setObjectName(QStringLiteral("Button_SetStart"));
		//Button_SetStart->setFixedHeight(Button_SetStart->height() * 2);

		//Grid_Layout->addWidget(Button_SetStart, 4, 1, 2, 1);
		Grid_Layout->addWidget(Button_SetStart, 4, 1, 1, 1);

		// Filename line edit
		LineEdit_RenameOrPostfix = new QLineEdit(player);
		LineEdit_RenameOrPostfix->setObjectName(QStringLiteral("LineEdit_RenameOrPostfix"));
		LineEdit_RenameOrPostfix->setMaximumWidth(300);

		Grid_Layout->addWidget(LineEdit_RenameOrPostfix, 4, 3, 1, 1);

		// Delete original checkbox
		Checkbox_DelOrig = new QCheckBox(player);
		Checkbox_DelOrig->setObjectName("Checkbox_DelOrig");

		Grid_Layout->addWidget(Checkbox_DelOrig, 5, 0, 1, 1);

		// ReEncode checkbox
		Checkbox_ReEncode = new QCheckBox(player);
		Checkbox_ReEncode->setObjectName("Checkbox_ReEncode");

		Grid_Layout->addWidget(Checkbox_ReEncode, 5, 1, 1, 1);

		// ReEncodeQuality combobox
		Combobox_ReEncodeQuality = new QComboBox(player);
		Combobox_ReEncodeQuality->setObjectName("Combobox_ReEncodeQuality");
		Combobox_ReEncodeQuality->addItem("Very High Quality");
		Combobox_ReEncodeQuality->addItem("High Quality");
		Combobox_ReEncodeQuality->addItem("Medium Quality");
		Combobox_ReEncodeQuality->addItem("Low Quality");

		Grid_Layout->addWidget(Combobox_ReEncodeQuality, 6, 1, 1, 1);

		// Set end point button
		Button_SetEnd = new QPushButton(Widget_Central);
		Button_SetEnd->setObjectName(QStringLiteral("Button_SetEnd"));
		//Button_SetEnd->setFixedHeight(Button_SetEnd->height() * 2);

		//Grid_Layout->addWidget(Button_SetEnd, 4, 2, 2, 1);
		Grid_Layout->addWidget(Button_SetEnd, 4, 2, 1, 1);

		// Clip info text box
		LineEdit_ClipInfo = new QLineEdit(Widget_Central);
		LineEdit_ClipInfo->setObjectName(QStringLiteral("LineEdit_ClipInfo"));
		LineEdit_ClipInfo->setReadOnly(true);
		LineEdit_ClipInfo->setMaximumWidth(300);

		Grid_Layout->addWidget(LineEdit_ClipInfo, 5, 3, 1, 1);

		// Status bar + progress bar
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

		// Volume slider (not shown)
		VolumeSlider = new VlcWidgetVolumeSlider(Widget_Central);
		VolumeSlider->setObjectName(QStringLiteral("VolumeSlider"));
		sizePolicy.setHeightForWidth(VolumeSlider->sizePolicy().hasHeightForWidth());
		VolumeSlider->setSizePolicy(sizePolicy);

		// Grid_Layout->addWidget(VolumeSlider, 4, 0, 1, 2);

		// Menu bar
		player->setCentralWidget(Widget_Central);
		MenuBar = new QMenuBar(player);
		MenuBar->setObjectName(QStringLiteral("MenuBar"));
		MenuBar->setGeometry(QRect(0, 0, 640, 21));

		Menu_File = new QMenu(MenuBar);
		Menu_File->setObjectName(QStringLiteral("Menu_File"));
		player->setMenuBar(MenuBar);

		MenuBar->addAction(Menu_File->menuAction());
		Menu_File->addAction(Action_OpenFolder);
		Menu_File->addSeparator();
		Menu_File->addAction(Action_Quit);

		SetStrings(player);
		QMetaObject::connectSlotsByName(player);

		Button_Next->setEnabled(false);
		Button_Skip->setEnabled(false);
		Button_Pause->setEnabled(false);
		Button_SetEnd->setEnabled(false);
		Button_SetStart->setEnabled(false);
		Button_ToggleRenamePostfix->setEnabled(false);
	}


	void SetStrings(QMainWindow* player) const
	{
		player->setWindowTitle("ClipCutter");

		Action_Quit->setText("Quit");
		Action_Pause->setText("Pause");
		Action_Stop->setText("Stop");
		Action_OpenFolder->setText("Open local folder");
		Button_Pause->setText("Play/Pause");
		Button_Next->setText("Next Clip");
		Button_Skip->setText("Skip Clip");
		Button_CutAll->setText("Process Clips");
		LineEdit_ClipInfo->setText("[ Clip 0 of 0 ]: Start: 00:00:00.000 - End: 00:00:00.000");
		Label_StatusBar->setText("Ready");
		Checkbox_DelOrig->setText("Delete Original");

		// Disable delete original AGAIn
		Checkbox_DelOrig->setDisabled(true);

		Checkbox_ReEncode->setText("Re-encode Video");
		Combobox_ReEncodeQuality->setCurrentIndex(1);
		Button_ToggleRenamePostfix->setText("Use File Postfix");
		Button_SetEnd->setText("Set End Point");
		Button_SetStart->setText("Set Start Point");

		LineEdit_RenameOrPostfix->setPlaceholderText("Filename");

		Menu_File->setTitle("File");
	}
};
