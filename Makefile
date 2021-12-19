lua_gnome_wallpaper.so:	lua_gnome_wallpaper.c
	$(CC) -g -fPIC -shared -I../lua/src/ $< `pkg-config --cflags --libs dbus-1 glib-2.0 gio-2.0 lua-5.3` -o $@

