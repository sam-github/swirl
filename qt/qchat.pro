TEMPLATE	= app
TARGET		= qchat

CONFIG		+= qt thread moc warn_on debug # release

HEADERS		=
SOURCES		= main.cpp

DEFINES         = $$system(pkg-config --cflags-only-other vortex | sed -es/-D//g)
INCLUDEPATH     = $$system(pkg-config --cflags-only-I vortex | sed -es/-I//g)
LIBS            = $$system(pkg-config --libs vortex)

