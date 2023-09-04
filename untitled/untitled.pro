TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        http_conn.cpp \
        main.cpp \
        threadpool.cpp

HEADERS += \
    http_conn.h \
    locker.h \
    threadpool.h
