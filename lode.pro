QT += core gui network qml quick quickcontrols2 concurrent

CONFIG += c++17 release
TARGET = lode
TEMPLATE = app

HEADERS += \
    src/accounts.h \
    src/backend.h \
    src/catalog.h \
    src/cli.h \
    src/content.h \
    src/downloads.h \
    src/http.h \
    src/instances.h \
    src/java.h \
    src/launcher.h \
    src/loaders.h \
    src/logmodel.h \
    src/modpacks.h \
    src/mojang.h \
    src/paths.h \
    src/settings.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/accounts.cpp \
    src/backend.cpp \
    src/catalog.cpp \
    src/cli.cpp \
    src/content.cpp \
    src/downloads.cpp \
    src/http.cpp \
    src/instances.cpp \
    src/java.cpp \
    src/launcher.cpp \
    src/loaders.cpp \
    src/logmodel.cpp \
    src/modpacks.cpp \
    src/mojang.cpp \
    src/paths.cpp \
    src/settings.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
