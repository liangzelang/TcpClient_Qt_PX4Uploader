#-------------------------------------------------
#
# Project created by QtCreator 2016-06-11T10:53:22
#
#-------------------------------------------------

QT += core gui
QT += network
QT += serialport
QT += core

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Tcp
TEMPLATE = app


SOURCES += main.cpp\
        tcpdialog.cpp

HEADERS  += tcpdialog.h

FORMS    += tcpdialog.ui
