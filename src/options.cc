#include "options.hh"

Glib::OptionEntry Options::create_option_entry(const Glib::ustring & long_name, const gchar & short_name, const Glib::ustring & description) {
	Glib::OptionEntry entry;

	entry.set_long_name(long_name);
	entry.set_short_name(short_name);
	entry.set_description(description);

	return entry;
}
