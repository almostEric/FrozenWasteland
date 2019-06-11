#include <string.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"

#define BUFFER_SIZE 512

struct TheOneRingModulator : Module {
	enum ParamIds {
		FORWARD_BIAS_PARAM,
		LINEAR_VOLTAGE_PARAM,
		SLOPE_PARAM,
		FORWARD_BIAS_ATTENUVERTER_PARAM,
		LINEAR_VOLTAGE_ATTENUVERTER_PARAM,
		SLOPE_ATTENUVERTER_PARAM,
		MIX_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CARRIER_INPUT,
		SIGNAL_INPUT,
		FORWARD_BIAS_CV_INPUT,
		LINEAR_VOLTAGE_CV_INPUT,
		SLOPE_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		WET_OUTPUT,
		MIX_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT_1,
		BLINK_LIGHT_2,
		BLINK_LIGHT_3,
		BLINK_LIGHT_4,
		NUM_LIGHTS
	};


	float bufferX1[BUFFER_SIZE] = {};
	float bufferY1[BUFFER_SIZE] = {};
	float bufferX2[BUFFER_SIZE] = {};
	float bufferY2[BUFFER_SIZE] = {};
	int bufferIndex = 0;
	float frameIndex = 0;
	float deltaTime = powf(2.0, -8);

	float voltageBias = 0;
	float voltageLinear = 0.5;
	float h = 1; //Slope

	//SchmittTrigger resetTrigger;


	inline float diode_sim(float inVoltage )
	  {
	  	//Original
	  	//if( inVoltage < 0 ) return 0;
      	//	else return 0.2 * log( 1.0 + exp( 10 * ( inVoltage - 1 ) ) );
	  	
      	//Mine
	  	if( inVoltage <= voltageBias ) 
	  		return 0;
	    if( inVoltage <= voltageLinear) {
	    	return h * (inVoltage - voltageBias) * (inVoltage - voltageBias) / ((2.0 * voltageLinear) - (2.0 * voltageBias));
	    } else {
	    	return (h * inVoltage) - (h * voltageLinear) + (h * ((voltageLinear - voltageBias) * (voltageLinear - voltageBias) / ((2.0 * voltageLinear) - (2.0 * voltageBias))));
	    }	    
	  }

	TheOneRingModulator()  {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		configParam(FORWARD_BIAS_PARAM, 0.0, 10.0, 0.0,"Forward Bias","v");
		configParam(FORWARD_BIAS_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Forward Bias CV Attenuation","%",0,100);
		configParam(LINEAR_VOLTAGE_PARAM, 0.0, 10.0, 0.5,"Linear Voltage","v");
		configParam(LINEAR_VOLTAGE_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Linear Voltage CV Attenuation","%",0,100);
		configParam(SLOPE_PARAM, 0.1, 1.0, 1.0,"Slope","v/v");
		configParam(SLOPE_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Slope CV Attenuation","%",0,100);
		configParam(MIX_PARAM, -0.0, 1.0, 0.5,"Mix","%",0,100);

	}
	void process(const ProcessArgs &args) override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};


void TheOneRingModulator::process(const ProcessArgs &args) {

	
    float vIn = inputs[ SIGNAL_INPUT ].getVoltage();
    float vC  = inputs[ CARRIER_INPUT ].getVoltage();
    float wd  = params[ MIX_PARAM ].getValue();

    voltageBias = clamp(params[FORWARD_BIAS_PARAM].getValue() + (inputs[FORWARD_BIAS_CV_INPUT].getVoltage() * params[FORWARD_BIAS_ATTENUVERTER_PARAM].getValue()),0.0,10.0);
	voltageLinear = clamp(params[LINEAR_VOLTAGE_PARAM].getValue() + (inputs[LINEAR_VOLTAGE_CV_INPUT].getVoltage() * params[LINEAR_VOLTAGE_ATTENUVERTER_PARAM].getValue()),voltageBias + 0.001f,10.0);
    h = clamp(params[SLOPE_PARAM].getValue() + (inputs[SLOPE_CV_INPUT].getVoltage() / 10.0 * params[SLOPE_ATTENUVERTER_PARAM].getValue()),0.1f,1.0f);

    float A = 0.5 * vIn + vC;
    float B = vC - 0.5 * vIn;

    float dPA = diode_sim( A );
    float dMA = diode_sim( -A );
    float dPB = diode_sim( B );
    float dMB = diode_sim( -B );

    float res = dPA + dMA - dPB - dMB;
    //outputs[WET_OUTPUT].setVoltage(res);
    outputs[MIX_OUTPUT].setVoltage(wd * res + ( 1.0 - wd ) * vIn);
}





struct DiodeResponseDisplay : TransparentWidget {
	TheOneRingModulator *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	struct Stats {
		float vrms, vpp, vmin, vmax;
		void calculate(float *values) {
			vrms = 0.0;
			vmax = -INFINITY;
			vmin = INFINITY;
			for (int i = 0; i < BUFFER_SIZE; i++) {
				float v = values[i];
				vrms += v*v;
				vmax = fmaxf(vmax, v);
				vmin = fminf(vmin, v);
			}
			vrms = sqrtf(vrms / BUFFER_SIZE);
			vpp = vmax - vmin;
		}
	};
	Stats statsX, statsY;

	DiodeResponseDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Sudo.ttf"));
	}

	void drawWaveform(const DrawArgs &args, float vB, float vL, float h) {
		nvgStrokeWidth(args.vg, 2);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 10, 122);
		nvgLineTo(args.vg, 10 + vB/10.0*127, 122);
		//Draw response as a bunch of small lines for now until I can convert to bezier	
		for (float inX=vB+.1;inX<=vL;inX+=.1) {
			float nonLinearY = h * (inX - vB) * (inX - vB) / ((2.0 * vL) - (2.0 * vB));
			nvgLineTo(args.vg, 10 + inX/10*127.0,10+(1-nonLinearY/10.0)*112.0);
		}

		float voltLinearConstant = 0 - (h * vL) + (h * ((vL - vB) * (vL - vB) / ((2.0 * vL) - (2.0 * vB))));
		for (float inX=vL+.1;inX<=10;inX+=.1) {		
			float linearY = (h * inX) + voltLinearConstant;
			nvgLineTo(args.vg, 10 + inX/10*127.0,10+(1-linearY/10.0)*112.0);
		}
		//nvgLineTo(args.vg, 137, 12 + (1-h) * 122);
		nvgStroke(args.vg);
	}

	

	
	void draw(const DrawArgs &args) override {
		if (!module)
					return;

		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));	
		drawWaveform(args, module->voltageBias, module->voltageLinear, module->h);
	}
};

struct TheOneRingModulatorWidget : ModuleWidget {
	TheOneRingModulatorWidget(TheOneRingModulator *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TheOneRingModulator.svg")));
		

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			DiodeResponseDisplay *display = new DiodeResponseDisplay();
			display->module = module;
			display->box.pos = Vec(0, 35);
			display->box.size = Vec(box.size.x-10, 90);
			addChild(display);
		}

		addParam(createParam<RoundFWKnob>(Vec(10, 190), module, TheOneRingModulator::FORWARD_BIAS_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(12, 254), module, TheOneRingModulator::FORWARD_BIAS_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(60, 190), module, TheOneRingModulator::LINEAR_VOLTAGE_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(62, 254), module, TheOneRingModulator::LINEAR_VOLTAGE_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(110, 190), module, TheOneRingModulator::SLOPE_PARAM));
		addParam(createParam<RoundSmallFWKnob>(Vec(112, 254), module, TheOneRingModulator::SLOPE_ATTENUVERTER_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(90, 325), module, TheOneRingModulator::MIX_PARAM));

		addInput(createInput<PJ301MPort>(Vec(14, 330), module, TheOneRingModulator::CARRIER_INPUT));
		addInput(createInput<PJ301MPort>(Vec(50, 330), module, TheOneRingModulator::SIGNAL_INPUT));
		addInput(createInput<PJ301MPort>(Vec(13, 225), module, TheOneRingModulator::FORWARD_BIAS_CV_INPUT));
		addInput(createInput<PJ301MPort>(Vec(63, 225), module, TheOneRingModulator::LINEAR_VOLTAGE_CV_INPUT));
		addInput(createInput<PJ301MPort>(Vec(113, 225), module, TheOneRingModulator::SLOPE_CV_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(122, 330), module, TheOneRingModulator::MIX_OUTPUT));
		
	}
};

Model *modelTheOneRingModulator = createModel<TheOneRingModulator, TheOneRingModulatorWidget>("TheOneRingModulator");
