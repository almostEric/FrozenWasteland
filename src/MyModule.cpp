#include "Tutorial.hpp"


struct MyModule : Module {
	enum ParamIds {
		PITCH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SINE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};

	float phase = 0.0;
	float blinkPhase = 0.0;

	MyModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void MyModule::step() {
	// Implement a simple sine oscillator
	float deltaTime = 1.0 / engineGetSampleRate();

	// Compute the frequency from the pitch parameter and input
	float pitch = params[PITCH_PARAM].value;
	pitch += inputs[PITCH_INPUT].value;
	pitch = clampf(pitch, -4.0, 4.0);
	float freq = 440.0 * powf(2.0, pitch);

	// Accumulate the phase
	phase += freq * deltaTime;
	if (phase >= 1.0)
		phase -= 1.0;

	// Compute the sine output
	float sine = sinf(2 * M_PI * phase);
	outputs[SINE_OUTPUT].value = 5.0 * sine;

	// Blink light at 1Hz
	blinkPhase += deltaTime;
	if (blinkPhase >= 1.0)
		blinkPhase -= 1.0;
	lights[BLINK_LIGHT].value = (blinkPhase < 0.5) ? 1.0 : 0.0;
}


MyModuleWidget::MyModuleWidget() {
	MyModule *module = new MyModule();
	setModule(module);
	box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/MyModule.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createScrew<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	addParam(createParam<Davies1900hBlackKnob>(Vec(28, 87), module, MyModule::PITCH_PARAM, -3.0, 3.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(33, 186), module, MyModule::PITCH_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(33, 275), module, MyModule::SINE_OUTPUT));

	addChild(createLight<MediumLight<RedLight>>(Vec(41, 59), module, MyModule::BLINK_LIGHT));
}
