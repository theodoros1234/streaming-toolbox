QT       += core gui

TEMPLATE = subdirs
SUBDIRS = src/libstrtb src/streaming-toolbox
src/streaming-toolbox.depends = src/libstrtb

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=
