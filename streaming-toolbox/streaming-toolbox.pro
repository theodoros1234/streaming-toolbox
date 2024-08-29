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
    ../src/config/system.cpp \
    ../src/gui/chat_subscription_thread.cpp \
    ../src/gui/chat_tab.cpp \
    ../src/gui/main_window.cpp \
    ../src/gui/plugin_tab.cpp \
    ../src/main.cpp \
    ../src/plugins/plugin.cpp \
    ../src/plugins/loader.cpp \
    ../src/chat/system.cpp \

HEADERS += \
    ../src/chat/channel.h \
    ../src/chat/interface.h \
    ../src/chat/message.h \
    ../src/chat/provider.h \
    ../src/chat/queue.h \
    ../src/chat/subscription.h \
    ../src/chat/system.h \
    ../src/common/deregistration_interface.h \
    ../src/common/strescape.h \
    ../src/common/version.h \
    ../src/config/id_type.h \
    ../src/config/interface.h \
    ../src/config/system.h \
    ../src/gui/chat_subscription_thread.h \
    ../src/gui/chat_tab.h \
    ../src/gui/main_window.h \
    ../src/gui/plugin_tab.h \
    ../src/json/parser.h \
    ../src/json/value.h \
    ../src/json/value_array.h \
    ../src/json/value_bool.h \
    ../src/json/value_float.h \
    ../src/json/value_int.h \
    ../src/json/value_null.h \
    ../src/json/value_object.h \
    ../src/json/value_string.h \
    ../src/json/value_utils.h \
    ../src/logging/logging.h \
    ../src/plugins/plugin.h \
    ../src/plugins/link.h \
    ../src/plugins/list.h \
    ../src/plugins/loader.h \
    ../src/unicode/unicode.h

FORMS += \
    ../src/gui/chat_tab.ui \
    ../src/gui/main_window.ui \
    ../src/gui/plugin_tab.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /usr/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=
