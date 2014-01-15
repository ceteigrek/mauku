
all: mauku

mauku: main.o mauku-widget.o mauku-item.o mauku-view.o mauku-contacts.o mauku-write.o mauku-scrolling-box.o miaouwmarshalers.o
	@echo Linking $@...
	@$(CC) -g -O0 -o $@ $^ $(LIBS) $(shell pkg-config --libs microfeed-subscriber-0 hildon-1)

%.o: %.c
	@echo Compiling $<...
	@$(CC) -g -c $(CPPFLAGS) $(CFLAGS) $(shell pkg-config --cflags microfeed-subscriber-0 hildon-1) -o $@ $<

clean:
	@echo Cleaning....
	@rm -f mauku *.o

install: mauku
	@echo Installing...
	@mkdir -p "$(DESTDIR)/usr/bin"
	@cp -f mauku "$(DESTDIR)/usr/bin"
	@mkdir -p "$(DESTDIR)/usr/share/mauku"
	@cp -Rf images "$(DESTDIR)/usr/share/mauku"
	@mkdir -p "$(DESTDIR)/usr/share/icons/hicolor/26x26/hildon"
	@cp -f data/mauku_logo_26x26.png "$(DESTDIR)/usr/share/icons/hicolor/26x26/hildon/mauku.png"
	@mkdir -p "$(DESTDIR)/usr/share/icons/hicolor/48x48/hildon"
	@cp -f data/mauku_logo_48x48.png "$(DESTDIR)/usr/share/icons/hicolor/48x48/hildon/mauku.png"
	@mkdir -p "$(DESTDIR)/usr/share/icons/hicolor/64x64/hildon"
	@cp -f data/mauku_logo_64x64.png "$(DESTDIR)/usr/share/icons/hicolor/64x64/hildon/mauku.png"
	@mkdir -p "$(DESTDIR)/usr/share/dbus-1/services"
	@cp -f data/mauku.service "$(DESTDIR)/usr/share/dbus-1/services"
	@mkdir -p "$(DESTDIR)/usr/share/applications/hildon"
	@cp -f data/mauku.desktop "$(DESTDIR)/usr/share/applications/hildon"

uninstall:
	@echo Uninstalling...
	@rm -f "$(DESTDIR)/usr/bin/mauku"
	@rm -Rf "$(DESTDIR)/usr/share/mauku"
	@rm -f "$(DESTDIR)/usr/share/icons/hicolor/26x26/mauku.png"
	@rm -f "$(DESTDIR)/usr/share/icons/hicolor/48x48/mauku.png"
	@rm -f "$(DESTDIR)/usr/share/icons/hicolor/64x64/mauku.png"
	@rm -f "$(DESTDIR)/usr/share/dbus-1/services/mauku.service"
	@rm -f "$(DESTDIR)/usr/share/applications/hildon/mauku.desktop"
