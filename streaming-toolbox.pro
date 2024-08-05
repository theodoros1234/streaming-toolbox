QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/chat/channel.cpp \
    src/logging/logging.cpp \
    src/main.cpp \
    src/gui/mainwindow.cpp \
    src/plugins/plugin.cpp \
    src/plugins/loader.cpp \
    src/chat/provider.cpp \
    src/chat/queue.cpp \
    src/chat/subscription.cpp \
    src/chat/system.cpp

HEADERS += \
    src/chat/channel.h \
    src/chat/interface.h \
    src/chat/message.h \
    src/chat/provider.h \
    src/chat/queue.h \
    src/chat/subscription.h \
    src/chat/system.h \
    src/common/deregistrationinterface.h \
    src/logging/logging.h \
    src/gui/mainwindow.h \
    src/plugins/plugin.h \
    src/plugins/link.h \
    src/plugins/list.h \
    src/plugins/loader.h

FORMS += \
    src/gui/mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
