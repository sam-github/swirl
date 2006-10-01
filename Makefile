# Makefile
#

default: qt.run

.PHONY: qt.run
qt.run: qt
	open qt/qchat.app

.PHONY: qt
qt:
	$(MAKE) -C qt

