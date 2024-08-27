QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
CONFIG += object_parallel_to_source
TEMPLATE = lib
TARGET = strtb

VERSION = 0.2.2

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ../src/common/strescape.cpp \
    ../src/common/version.cpp \
    ../src/config/id_type.cpp \
    ../src/config/interface.cpp \
    ../src/json/parser.cpp \
    ../src/json/value.cpp \
    ../src/json/value_array.cpp \
    ../src/json/value_bool.cpp \
    ../src/json/value_float.cpp \
    ../src/json/value_int.cpp \
    ../src/json/value_null.cpp \
    ../src/json/value_object.cpp \
    ../src/json/value_string.cpp \
    ../src/json/value_utils.cpp \
    ../src/logging/logging.cpp \
    ../src/chat/channel.cpp \
    ../src/chat/provider.cpp \
    ../src/chat/queue.cpp \
    ../src/chat/subscription.cpp \
    ../src/unicode/unicode.cpp

HEADERS += \
    ../src/chat/channel.h \
    ../src/chat/interface.h \
    ../src/chat/message.h \
    ../src/chat/provider.h \
    ../src/chat/queue.h \
    ../src/chat/subscription.h \
    ../src/common/deregistration_interface.h \
    ../src/common/strescape.h \
    ../src/common/version.h \
    ../src/config/id_type.h \
    ../src/config/interface.h \
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
    ../src/plugins/link.h \
    ../src/unicode/unicode.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=
