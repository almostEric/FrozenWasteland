#include <string.h>
#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"

#define BUFFER_SIZE 512

struct TheOneRingModulator : Module {
	enum ParamIds {
		FORWARD_BIAS_PARAM,
		LINEAR_VOLTAGE_PARAM,
		SLOPE_PARAM,
		NONLINEARITY_PARAM,
		FORWARD_BIAS_ATTENUVERTER_PARAM,
		LINEAR_VOLTAGE_ATTENUVERTER_PARAM,
		SLOPE_ATTENUVERTER_PARAM,
		NONLINEARITY_ATTENUVERTER_PARAM,
		MIX_PARAM,
		MIX_ATTENUVERTER_PARAM,
		DROP_COMPENSATE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CARRIER_INPUT,
		SIGNAL_INPUT,
		FORWARD_BIAS_CV_INPUT,
		LINEAR_VOLTAGE_CV_INPUT,
		SLOPE_CV_INPUT,
		NONLINEARITY_CV_INPUT,
		MIX_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		WET_OUTPUT,
		MIX_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		DROP_COMPENSATE_LIGHT,
		NUM_LIGHTS
	};

	bool dropCompensate = true;

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
	float nl = 2.0; //Non-Linearity

	float compensationCoefficient = 1.0;

	///Advanced Model
	double C=10e-9;
	double Cp=10e-9;
	double L=0.8;
	double Ra = 600;
	double Ri = 50;
	double Rm = 80;

	double lu1 = 0,lu2= 0,lu3= 0,li1= 0,li2 = 0.0;
	double u1= 0,u2= 0,u3= 0,u4= 0,u5= 0,u6= 0,u7= 0,i1= 0,i2 = 0.0;
	double g4=0.0,g5=0,g6=0,g7=0;


	dsp::SchmittTrigger dropCompensateTrigger;

	//percentages
	float forwardBiasPercentage = 0;
	float linearVoltagePercentage = 0;
	float slopePercentage = 0;
	float mixPercentage = 0;

int sampleCount = 0;

	inline float diode_sim(float inVoltage ) {
		//Original
		//if( inVoltage < 0 ) return 0;
		//	else return 0.2 * log( 1.0 + exp( 10 * ( inVoltage - 1 ) ) );
		
		//Mine
		if( inVoltage <= voltageBias ) 
			return 0;
		if( inVoltage <= voltageLinear) {
			return h * (inVoltage - voltageBias) * (inVoltage - voltageBias) / ((nl * voltageLinear) - (nl * voltageBias));
		} else {
			return (h * inVoltage) - (h * voltageLinear) + (h * ((voltageLinear - voltageBias) * (voltageLinear - voltageBias) / ((nl * voltageLinear) - (nl * voltageBias))));
		}	    
	}

	inline double germainiun_diode(double inVoltage) {
		if( inVoltage <= voltageBias ) 
			return 0;
		else
			return 0.17 * std::pow(inVoltage,4.0);
	}

	inline double silicon_diode(double inVoltage) {
		if( inVoltage <= voltageBias ) 
			return 0;
		else
			return 40.67286402 * std::exp(-9.0) * (std::exp(17.7493332 * (inVoltage+0.3)) - 1.0);
	}

	TheOneRingModulator()  {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		configParam(FORWARD_BIAS_PARAM, 0.0, 10.0, 0.0,"Forward Bias","v");
		configParam(FORWARD_BIAS_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Forward Bias CV Attenuation","%",0,100);
		configParam(LINEAR_VOLTAGE_PARAM, 0.0, 10.0, 0.5,"Linear Voltage","v");
		configParam(LINEAR_VOLTAGE_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Linear Voltage CV Attenuation","%",0,100);
		configParam(SLOPE_PARAM, 0.1, 1.0, 1.0,"Slope","v/v");
		configParam(SLOPE_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Slope CV Attenuation","%",0,100);
		// configParam(NONLINEARITY_PARAM, 0.5, 3.0, 2.0,"Nonlinearity");
		// configParam(NONLINEARITY_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Nonlinearity CV Attenuation","%",0,100);
		configParam(MIX_PARAM, 0.0, 1.0, 0.5,"Mix","%",0,100);
		configParam(MIX_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Mix CV Attenuation","%",0,100);

	}

	void process(const ProcessArgs &args) override;
	json_t* dataToJson() override;
	void dataFromJson(json_t *rootJ) override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

 json_t* TheOneRingModulator::dataToJson()  {
	json_t *rootJ = json_object();

	json_object_set_new(rootJ, "dropCompensate", json_integer(dropCompensate));
	
	return rootJ;
};

void TheOneRingModulator::dataFromJson(json_t *rootJ)  {
	json_t *csnJ = json_object_get(rootJ, "dropCompensate");
	if (csnJ) {
		dropCompensate = json_integer_value(csnJ);			
	}	
}


void TheOneRingModulator::process(const ProcessArgs &args) {

	if (dropCompensateTrigger.process(params[DROP_COMPENSATE_PARAM].getValue())) {
		dropCompensate = !dropCompensate;
	}
	lights[DROP_COMPENSATE_LIGHT].value = dropCompensate;

	
    // double vIn = inputs[ SIGNAL_INPUT ].getVoltage() / 50.0; //scaling to +/- 200mv
    // double vC  = inputs[ CARRIER_INPUT ].getVoltage() / 50.0;//scaling to +/- 200mv

    float vIn = inputs[ SIGNAL_INPUT ].getVoltage(); 
    float vC  = inputs[ CARRIER_INPUT ].getVoltage();

    float wd  = clamp(params[MIX_PARAM].getValue() + (inputs[MIX_INPUT].getVoltage() / 10.0 * params[MIX_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
	mixPercentage = wd;

    voltageBias = clamp(params[FORWARD_BIAS_PARAM].getValue() + (inputs[FORWARD_BIAS_CV_INPUT].getVoltage() * params[FORWARD_BIAS_ATTENUVERTER_PARAM].getValue()),0.0,10.0);
	forwardBiasPercentage = voltageBias / 10.0;
	voltageLinear = clamp(params[LINEAR_VOLTAGE_PARAM].getValue() + (inputs[LINEAR_VOLTAGE_CV_INPUT].getVoltage() * params[LINEAR_VOLTAGE_ATTENUVERTER_PARAM].getValue()),voltageBias + 0.001f,10.0);
	linearVoltagePercentage = voltageLinear / 10.0;
    h = clamp(params[SLOPE_PARAM].getValue() + (inputs[SLOPE_CV_INPUT].getVoltage() / 10.0 * params[SLOPE_ATTENUVERTER_PARAM].getValue()),0.1f,1.0f);
	slopePercentage = (h-0.1)/0.9;
	//nl = clamp(params[NONLINEARITY_PARAM].getValue() + (inputs[NONLINEARITY_CV_INPUT].getVoltage() / 10.0 * params[NONLINEARITY_ATTENUVERTER_PARAM].getValue()),0.5f,3.0f);

	compensationCoefficient = 10.0 / diode_sim(10.0);

    float A = vC + 0.5 * vIn;
    float B = vC - 0.5 * vIn;

    float dPA = diode_sim( A );
    float dMA = diode_sim( -A );
    float dPB = diode_sim( B );
    float dMB = diode_sim( -B );

    float res = dPA + dMA - dPB - dMB;
	if(dropCompensate) {
		res *= compensationCoefficient;
	}
    outputs[MIX_OUTPUT].setVoltage(wd * res + ( 1.0 - wd ) * vIn);

// 	double T = args.sampleTime;
// 	// double T = 1.0;

//    	g4 = germainiun_diode(u4);
// 	g5 = germainiun_diode(u5);
// 	g6 = germainiun_diode(u6);
// 	g7 = germainiun_diode(u7);


//    u1 = lu1 + (T/C) * (li1 - (g4 / 2.0) + (g7 / 2.0) + (g5 / 2.0) - (g6 / 2.0) - (lu1 - vIn) / Rm);
//    u2 = lu2 + (T/C) * (li2 + (g4 / 2.0) - (g6 / 2.0) - (g5 / 2.0) + (g7 / 2.0) - (lu2 / Ra));
//    u3 = lu3 + (T/Cp) * (g4 + g5 - g6 - g7 - (lu3 / Ri));
//    i1 = li1 + (T/L)*(-lu1);
//    i2 = li2 + (T/L)*(-lu2);

//    u4 = (u1/2.0) - u3 - vC - (u2/2.0);
//    u5 = (-u1/2.0) - u3 - vC + (u2/2.0);
//    u6 = (u1/2.0) + u3 + vC + (u2/2.0);
//    u7 = (-u1/2.0) + u3 + vC - (u2/2.0);

//    lu1 = u1;
//    lu2 = u2;
//    lu3 = u3;
//    li1 = i1;
//    li2 = i2;

// if(sampleCount < 20) {
// 		fprintf(stderr, "sc:%i vIn:%f vC:%f u1:%f u2:%f u3:%f u4:%f u5:%f u6:%f u7:%f i1:%f i2:%f\n", sampleCount, vIn,vC,u1,u2,u3,u4,u5,u6,u7,i1,i2 );
// 		fprintf(stderr, "     g4:%f g5:%f g6:%f g7:%f  t/c:%f\n", g4,g5,g6,g7,T/C);
// 		sampleCount++;
// }


// 	float res = u2 * 50.0; // Scale back volrage
// 	outputs[MIX_OUTPUT].setVoltage(wd * res + ( 1.0 - wd ) * vIn);

}





struct DiodeResponseDisplay : TransparentWidget {
	TheOneRingModulator *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	DiodeResponseDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Sudo.ttf"));
	}

	void drawWaveform(const DrawArgs &args, float vB, float vL, float h,float nl) {
		nvgStrokeWidth(args.vg, 2);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 10, 122);
		nvgLineTo(args.vg, 10 + vB/10.0*127, 122);
		//Draw response as a bunch of small lines for now until I can convert to bezier	
		for (float inX=vB+.1;inX<=vL;inX+=.1) {
			float nonLinearY = h * (inX - vB) * (inX - vB) / ((nl * vL) - (nl * vB));
			nvgLineTo(args.vg, 10 + inX/10*127.0,10+(1-nonLinearY/10.0)*112.0);
		}

		float voltLinearConstant = 0 - (h * vL) + (h * ((vL - vB) * (vL - vB) / ((nl * vL) - (nl * vB))));
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
		drawWaveform(args, module->voltageBias, module->voltageLinear, module->h,module->nl);
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

		addParam(createParam<LEDButton>(Vec(25, 264), module, TheOneRingModulator::DROP_COMPENSATE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(26.5, 265.5), module, TheOneRingModulator::DROP_COMPENSATE_LIGHT));


		ParamWidget* forwardBiasParam = createParam<RoundSmallFWKnob>(Vec(13, 190), module, TheOneRingModulator::FORWARD_BIAS_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(forwardBiasParam)->percentage = &module->forwardBiasPercentage;
		}
		addParam(forwardBiasParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(27, 216), module, TheOneRingModulator::FORWARD_BIAS_ATTENUVERTER_PARAM));
		ParamWidget* linearVoltageParam = createParam<RoundSmallFWKnob>(Vec(65, 190), module, TheOneRingModulator::LINEAR_VOLTAGE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(linearVoltageParam)->percentage = &module->linearVoltagePercentage;
		}
		addParam(linearVoltageParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(78, 216), module, TheOneRingModulator::LINEAR_VOLTAGE_ATTENUVERTER_PARAM));
		ParamWidget* slopeParam = createParam<RoundSmallFWKnob>(Vec(112, 190), module, TheOneRingModulator::SLOPE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(slopeParam)->percentage = &module->slopePercentage;
		}
		addParam(slopeParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(125, 216), module, TheOneRingModulator::SLOPE_ATTENUVERTER_PARAM));
		// addParam(createParam<RoundFWKnob>(Vec(110, 270), module, TheOneRingModulator::NONLINEARITY_PARAM));
		// addParam(createParam<RoundSmallFWKnob>(Vec(113, 302), module, TheOneRingModulator::NONLINEARITY_ATTENUVERTER_PARAM));
		ParamWidget* mixParam = createParam<RoundSmallFWKnob>(Vec(95, 264), module, TheOneRingModulator::MIX_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(mixParam)->percentage = &module->mixPercentage;
		}
		addParam(mixParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(110, 290), module, TheOneRingModulator::MIX_ATTENUVERTER_PARAM));

		addInput(createInput<FWPortInSmall>(Vec(14, 340), module, TheOneRingModulator::CARRIER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(35, 340), module, TheOneRingModulator::SIGNAL_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(6, 217), module, TheOneRingModulator::FORWARD_BIAS_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(57, 217), module, TheOneRingModulator::LINEAR_VOLTAGE_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(104, 217), module, TheOneRingModulator::SLOPE_CV_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(87, 291), module, TheOneRingModulator::MIX_INPUT));
		// addInput(createInput<PJ301MPort>(Vec(113, 283), module, TheOneRingModulator::NONLINEARITY_CV_INPUT));

		addOutput(createOutput<FWPortInSmall>(Vec(112, 340), module, TheOneRingModulator::MIX_OUTPUT));
		
	}
};

Model *modelTheOneRingModulator = createModel<TheOneRingModulator, TheOneRingModulatorWidget>("TheOneRingModulator");
