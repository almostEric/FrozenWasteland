
# Tutorial VCV plugin

This plugin is a template and self-contained tutorial for creating your own VCV plugins.

### Prerequisites for VCV plugin development

0. Some C++ familiarity is required.
1. Compile Rack from source, following the instructions at https://github.com/VCVRack/Rack. Be sure to check out the desired version you wish to build your plugin against.
2. Clone this repository in the `plugins/` directory.

### File structure

- `Makefile`: The build system configuration file. Edit this to add compiler flags and custom targets.
- `src/`: C++ and C source files
	- `Tutorial.cpp / Tutorial.hpp`: Plugin-wide source and header for plugin initialization and configuration. Rename this to the name of your plugin.
	- `MyModule.cpp`: A single module's source code. Duplicate this file for every module you wish to add. You may have multiple modules per file or multiple files for a single module, but one module per file is recommended.
- `res/`: Resource directory for SVG graphics and anything else you need to `fopen()`
- `LICENSE.txt`: This tutorial is released under public domain (CC0), but you may wish to replace this with your own license.

## Tutorial

This tutorial will not instruct you to create a pre-planned module, like a Voltage Controlled Oscillator.
Instead, I have already done that for you, and your task is to change it to something of your choice, using generic instructions that apply to any module type.

Before diving in, familiarize yourself with the above file structure.

1. Set up the names of the parameters, inputs, and outputs in `MyModule.cpp` by editing the enums defined in the `MyModule` class.

2. Write the DSP code of your module in the `MyModule::step()` function, using the values from the `params`, `inputs`, and `outputs` vectors.
Rack includes a work-in-progress DSP framework in the `include/dsp/` directory that you may use.
Writing a high quality audio processor is easier said than done, but there are many fantastic books and online resources on audio DSP that will teach you what you need to know.
My word of advice: *mind the aliasing*.

3. Design the module's panel with a vector graphics editor (Inkscape, Illustrator, CorelDRAW, etc) and save it in the `res/` directory as an SVG file.
Make sure the path to the .svg file is correctly specified in the `SVG::Load()` function.
*Note: The Rack renderer is currently only capable of rendering path and group objects with solid fill and stroke. Text must be converted to paths. Clipping masks, gradients, etc. are not supported.*

4. Add widgets to the panel including params (knobs, buttons, switches, etc.), input ports, and output ports.
Helper functions `createParam()`, `createInput()`, and `createOutput()` are used to construct a particular `Widget` subclass, set its (x, y) position, range of values, and default value.
Rack Widgets are defined in `include/widgets.hpp` and `include/app.hpp`, and helpers are found in `include/rack.hpp`.
*Note: Widgets from `include/components.hpp` using Component Library SVG graphics are licensed under CC BY-NC 4.0 and are free to use for noncommercial purposes.
Contact contact@grayscale.info for information about licensing for commercial use.*

5. Eventually, you will need to change the name of your plugin from "Tutorial".
Rename `Tutorial.cpp` and `Tutorial.hpp`.
Change references of `#include "Tutorial.hpp"` in each of the source files.
In the `init()` function, change the name and slug (unique identifier), which will affect the labels given to your plugin in the "Add module" context menu of Rack.
In the `Makefile`, change `DIST_NAME` so that running `make dist` builds a correctly named .zip file.

6. Build your plugin with `make`, or `make dist` to produce a distributable .zip file.
Subscribe to the [Plugin API Updates Thread](https://github.com/VCVRack/Rack/issues/258) to receive notifications when the Rack API changes or a discussion about a change is being held.
Follow the [plugin versioning guidelines](https://github.com/VCVRack/Rack/issues/266) and `git tag vX.Y.Z` (`git tag v0.4.0` for example) and `git push --tags` when finalizing a new release.
Finally, add your plugin to the [List of plugins](https://github.com/VCVRack/Rack/wiki/List-of-plugins) wiki page.

## Contributing

I welcome Issues and Pull Requests to this repository if you have suggestions for improvement.