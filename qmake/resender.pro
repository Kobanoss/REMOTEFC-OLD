TEMPLATE = app
TARGET = resender
INCLUDEPATH += .
QT       += core gui widgets network

CONFIG += c++23


DEFINES += QT_DEPRECATED_WARNINGS


#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    


HEADERS += \
    src/resender.h
SOURCES += \
    main.cpp \
    src/resender.cpp 
