QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
CONFIG += object_parallel_to_source
TARGET = streaming-toolbox
LIBS += -L../libstrtb -lstrtb

include( ../version.pri )

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ../src/gui/chat_subscription_thread.cpp \
    ../src/gui/chat_tab.cpp \
    ../src/gui/event_viewer.cpp \
    ../src/gui/main_window.cpp \
    ../src/gui/plugin_tab.cpp \
    ../src/main.cpp \
    ../src/plugins/plugin.cpp \
    ../src/plugins/loader.cpp \

HEADERS += \
    ../src/common/version.h \
    ../src/gui/chat_subscription_thread.h \
    ../src/gui/chat_tab.h \
    ../src/gui/event_viewer.h \
    ../src/gui/main_window.h \
    ../src/gui/plugin_tab.h \
    ../src/plugins/plugin.h \
    ../src/plugins/link.h \
    ../src/plugins/list.h \
    ../src/plugins/loader.h

FORMS += \
    ../src/gui/chat_tab.ui \
    ../src/gui/event_viewer.ui \
    ../src/gui/main_window.ui \
    ../src/gui/plugin_tab.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /usr/bin
!isEmpty(target.path): INSTALLS += target

# Icons on Linux
linux {
    linux_icon_base = /usr/share/icons/hicolor

    linux_icon16.path = $${linux_icon_base}/16x16/apps
    linux_icon16.files = ../resources/icon/16/streaming-toolbox.png

    linux_icon22.path = $${linux_icon_base}/22x22/apps
    linux_icon22.files = ../resources/icon/22/streaming-toolbox.png

    linux_icon24.path = $${linux_icon_base}/24x24/apps
    linux_icon24.files = ../resources/icon/24/streaming-toolbox.png

    linux_icon32.path = $${linux_icon_base}/32x32/apps
    linux_icon32.files = ../resources/icon/32/streaming-toolbox.png

    linux_icon48.path = $${linux_icon_base}/48x48/apps
    linux_icon48.files = ../resources/icon/48/streaming-toolbox.png

    linux_icon64.path = $${linux_icon_base}/64x64/apps
    linux_icon64.files = ../resources/icon/64/streaming-toolbox.png

    linux_icon128.path = $${linux_icon_base}/128x128/apps
    linux_icon128.files = ../resources/icon/128/streaming-toolbox.png

    linux_icon256.path = $${linux_icon_base}/256x256/apps
    linux_icon256.files = ../resources/icon/256/streaming-toolbox.png

    linux_icon512.path = $${linux_icon_base}/512x512/apps
    linux_icon512.files = ../resources/icon/512/streaming-toolbox.png

    linux_icon_scalable.path = $${linux_icon_base}/scalable/apps
    linux_icon_scalable.files = ../resources/icon/streaming-toolbox.svg

    INSTALLS += linux_icon16 linux_icon22 linux_icon24 linux_icon32 linux_icon48 linux_icon64 linux_icon128 linux_icon256 linux_icon512 linux_icon_scalable
}

# Freedesktop desktop menu item on Linux
linux {
    linux_desktop_menu.path = /usr/share/applications
    linux_desktop_menu.files = ../resources/linux-desktop-menu/streaming-toolbox.desktop
    INSTALLS += linux_desktop_menu
}
