QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
CONFIG += object_parallel_to_source
TEMPLATE = lib
TARGET = strtb
LIBS = -lssl -lcrypto -lpthread

include( ../../version.pri )

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    common/strescape.cpp \
    common/version.cpp \
    config/system.cpp \
    config/id_type.cpp \
    event/event_listener.cpp \
    event/item.cpp \
    event/provider.cpp \
    event/system.cpp \
    json/cast.cpp \
    json/holder.cpp \
    json/parser.cpp \
    json/value.cpp \
    json/value_array.cpp \
    json/value_bool.cpp \
    json/value_float.cpp \
    json/value_int.cpp \
    json/value_null.cpp \
    json/value_object.cpp \
    json/value_string.cpp \
    json/value_utils.cpp \
    logging/logging.cpp \
    chat/channel.cpp \
    chat/provider.cpp \
    chat/queue.cpp \
    chat/subscription.cpp \
    chat/system.cpp \
    networking/exceptions.cpp \
    networking/sigpipe_suppressor.cpp \
    networking/tcp_client.cpp \
    networking/tcp_client_ssl.cpp \
    networking/tcp_server.cpp \
    networking/tcp_server_connection.cpp \
    networking/tcp_server_connection_ssl.cpp \
    networking/tcp_server_ssl.cpp \
    networking/tcp_socket.cpp \
    networking/tcp_socket_ssl_thread.cpp \
    unicode/unicode.cpp

HEADERS += \
    chat/channel.h \
    chat/message.h \
    chat/provider.h \
    chat/queue.h \
    chat/subscription.h \
    chat/system.h \
    common/deregistration_interface.h \
    common/strescape.h \
    common/version.h \
    config/id_type.h \
    config/system.h \
    event/event_listener.h \
    event/item.h \
    event/provider.h \
    event/system.h \
    json/all_value_types.h \
    json/cast.h \
    json/holder.h \
    json/parser.h \
    json/value.h \
    json/value_array.h \
    json/value_bool.h \
    json/value_float.h \
    json/value_int.h \
    json/value_null.h \
    json/value_object.h \
    json/value_string.h \
    json/value_utils.h \
    logging/logging.h \
    networking/exceptions.h \
    networking/sigpipe_suppressor.h \
    networking/tcp_client.h \
    networking/tcp_client_ssl.h \
    networking/tcp_server.h \
    networking/tcp_server_connection.h \
    networking/tcp_server_connection_ssl.h \
    networking/tcp_server_ssl.h \
    networking/tcp_socket.h \
    networking/tcp_socket_ssl_thread.h \
    plugins/link.h \
    unicode/unicode.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /usr/lib64
!isEmpty(target.path): INSTALLS += target

DISTFILES +=

# Header files for plugin development
# HEADER_INCLUDE_DIR = /usr/include/$${TARGET}
# for(header, HEADERS) {
#     header_r = $$replace(header, "", "")
#     header_r_dir = $$dirname(header_r)
#     eval(header_include_dir_$${header_r_dir}.path = $${HEADER_INCLUDE_DIR}/$${header_r_dir}))
#     eval(header_include_dir_$${header_r_dir}.files += $$header))
#     eval(INSTALLS *= header_include_dir_$${header_r_dir})
# }
