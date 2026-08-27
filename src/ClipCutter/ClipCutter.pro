QT       += core gui multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ClipLogic.cpp \
    FFmpeg.cpp \
    Main.cpp \
    CCMainWindow.cpp \
    QueueItem.cpp \
    Utility.cpp

HEADERS += \
    CCMainWindow.h \
    ClipLogic.h \
    FFmpeg.h \
    QueueItem.h \
    Utility.h

FORMS += \
    CCMainWindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    ClipCutterResources.qrc

UI_DIR = $$OUT_PWD/Generated
