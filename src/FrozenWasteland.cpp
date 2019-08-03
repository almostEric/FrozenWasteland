#include "FrozenWasteland.hpp"


// The pluginInstance-wide instance of the Plugin class
Plugin *pluginInstance;

void init(rack::Plugin *p) {
	pluginInstance = p;
	// The "slug" is the unique identifier for your pluginInstance and must never change after release, so choose wisely.
	// It must only contain letters, numbers, and characters "-" and "_". No spaces.
	// To guarantee uniqueness, it is a good idea to prefix the slug by your name, alias, or company name if available, e.g. "MyCompany-MyPlugin".
	// The ZIP package must only contain one folder, with the name equal to the pluginInstance's slug.
	//p->website = "https://github.com/almostEric/FrozenWasteland";
	//p->manual = "https://github.com/almostEric/FrozenWasteland/blob/master/README.md";

	// For each module, specify the ModuleWidget subclass, manufacturer slug (for saving in patches), manufacturer human-readable name, module slug, and module name
	p->addModel(modelBPMLFO);
	p->addModel(modelBPMLFO2);
	p->addModel(modelDamianLillard);
	p->addModel(modelEverlastingGlottalStopper);
	p->addModel(modelHairPick);
	//p->addModel(modelHyperMeasures);
	p->addModel(modelLissajousLFO);
	p->addModel(modelMrBlueSky);
	p->addModel(modelTheOneRingModulator);
	p->addModel(modelPhasedLockedLoop);
	p->addModel(modelPortlandWeather);
	p->addModel(modelQuadAlgorithmicRhythm);
	p->addModel(modelQARGrooveExpander);
	p->addModel(modelQARProbabilityExpander);
	p->addModel(modelQuantussyCell);
	p->addModel(modelSeedsOfChange);
	p->addModel(modelRouletteLFO);
	p->addModel(modelSeriouslySlowLFO);
	p->addModel(modelVoxInhumana);
	p->addModel(modelVoxInhumanaExpander);
	p->addModel(modelCDCSeriouslySlowLFO);


	// Any other pluginInstance initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
