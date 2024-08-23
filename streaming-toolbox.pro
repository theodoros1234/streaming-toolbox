QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
CONFIG += object_parallel_to_source

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/chat/channel.cpp \
    src/common/strescape.cpp \
    src/config/id_type.cpp \
    src/config/interface.cpp \
    src/config/system.cpp \
    src/gui/chat_subscription_thread.cpp \
    src/gui/chat_tab.cpp \
    src/gui/main_window.cpp \
    src/gui/plugin_tab.cpp \
    src/json/parser.cpp \
    src/json/value.cpp \
    src/json/value_array.cpp \
    src/json/value_bool.cpp \
    src/json/value_float.cpp \
    src/json/value_int.cpp \
    src/json/value_null.cpp \
    src/json/value_object.cpp \
    src/json/value_string.cpp \
    src/json/value_utils.cpp \
    src/logging/logging.cpp \
    src/main.cpp \
    src/plugins/plugin.cpp \
    src/plugins/loader.cpp \
    src/chat/provider.cpp \
    src/chat/queue.cpp \
    src/chat/subscription.cpp \
    src/chat/system.cpp \
    src/unicode/unicode.cpp

HEADERS += \
    src/chat/channel.h \
    src/chat/interface.h \
    src/chat/message.h \
    src/chat/provider.h \
    src/chat/queue.h \
    src/chat/subscription.h \
    src/chat/system.h \
    src/common/deregistration_interface.h \
    src/common/strescape.h \
    src/common/version.h \
    src/config/id_type.h \
    src/config/interface.h \
    src/config/system.h \
    src/gui/chat_subscription_thread.h \
    src/gui/chat_tab.h \
    src/gui/main_window.h \
    src/gui/plugin_tab.h \
    src/json/parser.h \
    src/json/value.h \
    src/json/value_array.h \
    src/json/value_bool.h \
    src/json/value_float.h \
    src/json/value_int.h \
    src/json/value_null.h \
    src/json/value_object.h \
    src/json/value_string.h \
    src/json/value_utils.h \
    src/logging/logging.h \
    src/plugins/plugin.h \
    src/plugins/link.h \
    src/plugins/list.h \
    src/plugins/loader.h \
    src/unicode/unicode.h

FORMS += \
    src/gui/chat_tab.ui \
    src/gui/main_window.ui \
    src/gui/plugin_tab.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=
