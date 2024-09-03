QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
CONFIG += object_parallel_to_source
TEMPLATE = lib
TARGET = strtb

include( ../version.pri )

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ../src/common/strescape.cpp \
    ../src/common/version.cpp \
    ../src/config/system.cpp \
    ../src/config/id_type.cpp \
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
    ../src/chat/system.cpp \
    ../src/unicode/unicode.cpp

HEADERS += \
    ../src/chat/channel.h \
    ../src/chat/message.h \
    ../src/chat/provider.h \
    ../src/chat/queue.h \
    ../src/chat/subscription.h \
    ../src/chat/system.h \
    ../src/common/deregistration_interface.h \
    ../src/common/strescape.h \
    ../src/common/version.h \
    ../src/config/id_type.h \
    ../src/config/system.h \
    ../src/json/all_value_types.h \
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
else: unix:!android: target.path = /usr/lib64
!isEmpty(target.path): INSTALLS += target

DISTFILES +=

# Header files for plugin development
HEADER_INCLUDE_DIR = /usr/include/$${TARGET}
for(header, HEADERS) {
    header_r = $$replace(header, "../src/", ""))
    header_r_dir = $$dirname(header_r)
    eval(header_include_dir_$${header_r_dir}.path = $${HEADER_INCLUDE_DIR}/$${header_r_dir}))
    eval(header_include_dir_$${header_r_dir}.files += $$header))
    eval(INSTALLS *= header_include_dir_$${header_r_dir})
}
