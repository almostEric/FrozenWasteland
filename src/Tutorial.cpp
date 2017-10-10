#include "Tutorial.hpp"


// The plugin-wide instance of the Plugin class
Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	plugin->slug = "Tutorial";
	plugin->name = "Tutorial";
	plugin->homepageUrl = "https://github.com/VCVRack/Tutorial";

	createModel<MyModuleWidget>(plugin, "MyModule", "My Module");

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables within this file or the individual module files to reduce startup times of Rack.
}
