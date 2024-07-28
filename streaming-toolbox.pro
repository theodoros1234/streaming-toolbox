QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/chatchannel.cpp \
    src/chatprovider.cpp \
    src/chatqueue.cpp \
    src/chatsubscription.cpp \
    src/chatsystem.cpp \
    src/deregistrationqueue.cpp \
    src/logging.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/plugin.cpp \
    src/pluginloader.cpp

HEADERS += \
    src/chatchannel.h \
    src/chatinterface.h \
    src/chatmessage.h \
    src/chatprovider.h \
    src/chatqueue.h \
    src/chatsubscription.h \
    src/chatsystem.h \
    src/deregistrationinterface.h \
    src/deregistrationqueue.h \
    src/logging.h \
    src/mainwindow.h \
    src/plugin.h \
    src/pluginlink.h \
    src/pluginlist.h \
    src/pluginloader.h

FORMS += \
    src/mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
