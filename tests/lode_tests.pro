QT += core gui network quick quickcontrols2 concurrent testlib
CONFIG += c++17 testcase
TARGET = lode_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += $$files(../src/*.h)
SOURCES += lode_tests.cpp $$files(../src/*.cpp)
SOURCES -= ../src/main.cpp
