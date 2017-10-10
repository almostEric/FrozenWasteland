
# Tutorial VCV plugin

This plugin is a template and self-contained tutorial for creating your own VCV plugins.

### Prerequisites for VCV plugin development

0. Some C++ familiarity is required.
1. Compile Rack from source, following the instructions at https://github.com/VCVRack/Rack.
2. Clone this repository in the `plugins/` directory.

### File structure

- `Makefile`: The build system configuration file. Edit this to add compiler flags and custom targets.
- `src/`: C++ and C source files
	- `Tutorial.cpp / Tutorial.hpp`: Plugin-wide source and header for plugin initialization and configuration. Rename this to the name of your plugin.
	- `MyModule.cpp`: A single module's source code. Duplicate this file for every module you wish to add. You may have multiple modules per file or multiple files for a single module, but one module per file is recommended.
- `res/`: Resource directory for SVG graphics and anything else you need to `fopen()`
- `LICENSE.txt`: This tutorial is released under the public domain (CC0), but you may wish to replace this with your own license.
