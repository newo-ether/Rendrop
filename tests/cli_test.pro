QT += core testlib
QT -= gui widgets
CONFIG += console testcase c++20
CONFIG -= windows app_bundle
TEMPLATE = app
TARGET = cli_test

SOURCES += cli_test.cpp
