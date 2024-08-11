#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <gio/gio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/random.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#define SETTINGS_PATH "org.gnome.desktop.background"
#define SETTINGS_KEY "picture-uri"
#define SETTINGS_KEY_DARK "picture-uri-dark"

static int set_wallpaper(lua_State* ls)
{
	GSettings* settings = NULL;
	gchar **key_list = NULL;
	const char* wp_path = NULL;
	gboolean rc = TRUE;

	wp_path = luaL_checkstring(ls, 1);
	if (0 != access(wp_path, F_OK))
		luaL_error(ls, "file not exist \'%s\'", wp_path ? wp_path : "(nil)");

	wp_path = lua_pushfstring(ls, "file://%s", wp_path);

	settings = g_settings_new(SETTINGS_PATH);
	if (settings == NULL)
		luaL_error(ls, "cannot open settings " SETTINGS_PATH);
	key_list = g_settings_list_keys(settings);
	if (key_list == NULL) {
		g_object_unref(settings);
		luaL_error(ls, "cannot list settings " SETTINGS_PATH);
	}
	for (int i = 0; rc && key_list[i]; i++) {
		const gchar *key = key_list[i];
		if (strcmp(key, SETTINGS_KEY) == 0 || strcmp(key, SETTINGS_KEY_DARK) == 0)
			rc = g_settings_set_string(settings, key, wp_path);
	}
	g_settings_sync();
	g_strfreev(key_list);
	g_object_unref(settings);

	if (!rc)
		luaL_error(ls, "cannot set wallpaper to \'%s\'", wp_path);
	return 0;
}

static int util_ls(lua_State* ls)
{
	const char* path = luaL_checkstring(ls, 1);
	DIR* dir = opendir(path);
	if (NULL == dir) {
		const char* err = strerror(errno);
		luaL_error(ls, "%s: \'%s\'", err, path);
	}
	lua_newtable(ls);
	do {
		struct dirent* ent = readdir(dir);
		if (NULL == ent)
			break;
		switch (ent->d_type) {
		case DT_DIR:
			lua_pushliteral(ls, "DIR");
			break;
		case DT_REG:
			lua_pushliteral(ls, "FILE");
			break;
		case DT_LNK:
			lua_pushliteral(ls, "LINK");
			break;
		default:
			lua_pushliteral(ls, "");
		}
		lua_setfield(ls, -2, ent->d_name);

	} while(1);
	closedir(dir);
	return 1;
}

static int util_sleep(lua_State* ls)
{
	int ms = luaL_checkinteger(ls, 1);
	struct timespec ts = {
		ms / 1000,
		(ms % 1000) * 1000 * 1000,
	};
	if (0 != nanosleep(&ts, NULL)) {
		luaL_error(ls, strerror(errno));
	}
	return 0;
}

static int util_random(lua_State* ls)
{
	int length = luaL_optinteger(ls, 1, 0);
	char buffer[0x100];
	if (length < 0 || length > 0x100)
		luaL_error(ls, "random: invalid size %d", length);
	if (length) {
		length = getrandom(buffer, length, 0);
		if (length < 0)
			luaL_error(ls, "random: failed %d", errno);
		lua_pushlstring(ls, buffer, length);
	} else {
		length = getrandom(buffer, sizeof(lua_Integer), 0);
		if (length != sizeof(lua_Integer))
			luaL_error(ls, "random: failed %d", errno);
		lua_pushinteger(ls, *(lua_Integer*)buffer);
	}
	return 1;
}

static int has_screenlock(lua_State* ls)
{
	static DBusConnection* conn = NULL;
	int timeout = luaL_optinteger(ls, 1, DBUS_TIMEOUT_USE_DEFAULT);
	int result = -1;
	DBusMessageIter it;
	DBusMessage* reply = NULL;
	DBusMessage* msg = NULL;

	if (!conn) {
		conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
		if (!conn)
			luaL_error(ls, "cannot open dbus");
		dbus_connection_set_exit_on_disconnect(conn, FALSE);
	}

	msg = dbus_message_new_method_call(
		"org.gnome.ScreenSaver",
		"/org/gnome/ScreenSaver",
		"org.gnome.ScreenSaver",
		"GetActive"
	);
	if (!msg)
		luaL_error(ls, "cannot build dbus message");
	reply = dbus_connection_send_with_reply_and_block(conn, msg, timeout, NULL);
	dbus_message_unref(msg);
	if (!reply)
		luaL_error(ls, "cannot call dbus method");
	if (dbus_message_iter_init(reply, &it) &&
		DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&it)
	) {
		dbus_message_iter_get_basic(&it, &result);
		lua_pushboolean(ls, result);
	}
	dbus_message_unref(reply);
	if (result < 0)
		luaL_error(ls, "invalid dbus result");
	return 1;
}

static const struct luaL_Reg lib[] = {
	{ "set_wallpaper", set_wallpaper },
	{ "ls", util_ls },
	{ "sleep", util_sleep },
	{ "random", util_random },
	{ "has_screenlock", has_screenlock },
	{ NULL, NULL },
};

int luaopen_lua_gnome_wallpaper(lua_State* ls)
{
	luaL_newlib(ls, lib);
	return 1;
}
