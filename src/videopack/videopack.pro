TEMPLATE = app
TARGET = 

CONFIG += console

INCLUDEPATH += ../internal
INCLUDEPATH += ../../deps/ffmpeg-3.3.4/include

include(../internal/common.pri)

QMAKE_LFLAGS += /SAFESEH:NO

QMAKE_LIBDIR += ../../deps/ffmpeg-3.3.4/lib

LIBS += avutil.lib
LIBS += avcodec.lib
LIBS += avformat.lib

#PRECOMPILED_HEADER = stable.h

SOURCES += main.cpp
HEADERS += audioencoder.h
SOURCES += audioencoder.cpp
HEADERS += videoencoder.h
SOURCES += videoencoder.cpp
HEADERS += muxer.h
SOURCES += muxer.cpp
