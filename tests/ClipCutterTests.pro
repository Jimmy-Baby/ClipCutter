QT += core testlib
QT -= gui

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = ClipCutterTests

INCLUDEPATH += ../src/ClipCutter

SOURCES += \
    RegressionTests.cpp \
    ../src/ClipCutter/ClipLogic.cpp \
    ../src/ClipCutter/Utility.cpp

HEADERS += \
    ../src/ClipCutter/ClipLogic.h \
    ../src/ClipCutter/Utility.h
