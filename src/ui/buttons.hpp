#pragma once
#include "../FrozenWasteland.hpp"
#include "shadow.hpp"

struct RecButton : app::SvgSwitch {
protected:
  RShadow shadow = RShadow();

public:
  RecButton() {
    momentary = true;
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/component/button.svg")));
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/component/button2.svg")));
    shadow.setBox(box);
  }

  void draw(const DrawArgs &args) override {
    /** shadow */
    shadow.draw(args.vg);

    /** component */
    app::SvgSwitch::draw(args);
  }

};


struct FWLEDButton : app::SvgSwitch {
	FWLEDButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/FWLEDButton.svg")));
	}
};

