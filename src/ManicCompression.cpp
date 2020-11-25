#include "FrozenWasteland.hpp"
#include "dsp-compressor/SimpleComp.h"
#include "dsp-compressor/SimpleGain.h"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "filters/biquad.h"

struct ManicCompression : Module {
	enum ParamIds {		
		THRESHOLD_PARAM,
		THRESHOLD_CV_ATTENUVERTER_PARAM,
		ATTACK_PARAM,
		ATTACK_CV_ATTENUVERTER_PARAM,
		RELEASE_PARAM,
		RELEASE_CV_ATTENUVERTER_PARAM,
		ATTACK_CURVE_PARAM,
		ATTACK_CURVE_CV_ATTENUVERTER_PARAM,
		RELEASE_CURVE_PARAM,
		RELEASE_CURVE_CV_ATTENUVERTER_PARAM,
		RATIO_PARAM,
		RATIO_CV_ATTENUVERTER_PARAM,
		KNEE_PARAM,
		KNEE_CV_ATTENUVERTER_PARAM,
		MAKEUP_GAIN_PARAM,
		MAKEUP_CV_ATTENUVERTER_GAIN_PARAM,
		MIX_PARAM,
		MIX_CV_ATTENUVERTER_PARAM,
		BYPASS_PARAM,
		RMS_MODE_PARAM,
		RMS_WINDOW_PARAM,
		RMS_WINDOW_CV_ATTENUVERTER_PARAM,
		FILTER_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		THRESHOLD_CV_INPUT,
		ATTACK_CV_INPUT,
		RELEASE_CV_INPUT,
		ATTACK_CURVE_CV_INPUT,
		RELEASE_CURVE_CV_INPUT,
		RATIO_CV_INPUT,
		KNEE_CV_INPUT,
		MAKEUP_GAIN_INPUT,
		MIX_CV_INPUT,
		RMS_MODE_INPUT,
		RMS_WINDOW_INPUT,
		BYPASS_INPUT,
		SOURCE_L_INPUT,
		SOURCE_R_INPUT,
		SIDECHAIN_INPUT,
		FILTER_MODE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_L,
		OUTPUT_R,
		NUM_OUTPUTS
	};
	enum LightIds {
		BYPASS_LIGHT,
		RMS_MODE_LIGHT,
		FILTER_LIGHT,
		NUM_LIGHTS
	};


	//Stuff for S&Hs
	dsp::SchmittTrigger bypassTrigger,	rmsTrigger, lpFilterTrigger;
	
	chunkware_simple::SimpleComp compressor;
	chunkware_simple::SimpleCompRms compressorRms;
	float gainReduction;
	double threshold, ratio, knee;

	bool bypassed =  false;
	bool rmsMode = false;
	bool filterMode = false;

	Biquad* filterBank[3]; // 3 filters 2 for stereo inputs, one for sidechain

	ManicCompression() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(THRESHOLD_PARAM, -50.0, 0.0, -30.0,"Threshold", " db");
		configParam(RATIO_PARAM, 1.0, 20.0, 1.0,"Ratio");	
		configParam(ATTACK_PARAM, 0.02, 100.0, 2.0,"Attack", " ms");
		configParam(RELEASE_PARAM, 40.0, 5000.0, 100.0,"Release"," ms");	
		configParam(KNEE_PARAM, 0.0, 10.0, 0.0,"Knee", " db");	
		configParam(RMS_WINDOW_PARAM, 0.02, 50.0, 5.0,"RMS Window", " ms");
		configParam(MAKEUP_GAIN_PARAM, 0.0, 30.0, 0.0,"Makeup Gain", " db");
		configParam(MIX_PARAM, 0.0, 1.0, 1.0,"Mix","%",0,100);	

		configParam(THRESHOLD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Threshold CV Attenuation","%",0,100);
		configParam(RATIO_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Ratio CV Attenuation","%",0,100);	
		configParam(ATTACK_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Attack CV Attenuation","%",0,100);
		configParam(RELEASE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Release CV Attenuation","%",0,100);	
		configParam(KNEE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Knee CV Attenuation","%",0,100);	
		configParam(RMS_WINDOW_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"RMS Windows CV Attenuation","%",0,100);	
		configParam(MAKEUP_CV_ATTENUVERTER_GAIN_PARAM, -1.0, 1.0, 0.0,"Makeup Gain CV Attenuation","%",0,100);
		configParam(MIX_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Mix CV Attenuation","%",0,100);	


		for(int f=0;f<3;f++) {
			filterBank[f] = new Biquad(bq_type_lowpass, 0.15 , 0.707, 0);
		}

		compressor.initRuntime();
		compressorRms.initRuntime();

	}
	void process(const ProcessArgs &args) override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

double lerp(double v0, double v1, double t) {
		return (1 - t) * v0 + t * v1;
}

void ManicCompression::process(const ProcessArgs &args) {
	compressor.setSampleRate(double(args.sampleRate));
	compressorRms.setSampleRate(double(args.sampleRate));

	float bypassInput = inputs[BYPASS_INPUT].getVoltage();
	bypassInput += params[BYPASS_PARAM].getValue(); //BYPASS BUTTON ALWAYS WORKS		
	if(bypassTrigger.process(bypassInput)) {
		bypassed = !bypassed;
	}
	lights[BYPASS_LIGHT].value = bypassed ? 1.0 : 0.0;

	float rmsInput = inputs[RMS_MODE_INPUT].getVoltage();
	rmsInput += params[RMS_MODE_PARAM].getValue(); //RMS BUTTON ALWAYS WORKS		
	if(rmsTrigger.process(rmsInput)) {
		rmsMode = !rmsMode;
	}
	lights[RMS_MODE_LIGHT].value = rmsMode ? 1.0 : 0.0;

	float filterInput = inputs[FILTER_MODE_INPUT].getVoltage();
	filterInput += params[FILTER_MODE_PARAM].getValue(); //Filter BUTTON ALWAYS WORKS		
	if(lpFilterTrigger.process(filterInput)) {
		filterMode = !filterMode;
	}
	lights[FILTER_LIGHT].value = filterMode ? 1.0 : 0.0;



	ratio = clamp(params[RATIO_PARAM].getValue() + (inputs[RATIO_CV_INPUT].getVoltage() * 2.0 * params[RATIO_CV_ATTENUVERTER_PARAM].getValue()),1.0f,20.0f);
	threshold = clamp(params[THRESHOLD_PARAM].getValue() + (inputs[THRESHOLD_CV_INPUT].getVoltage() * 3.0 * params[THRESHOLD_CV_ATTENUVERTER_PARAM].getValue()),-50.0f,0.0f);
	double attack = clamp(params[ATTACK_PARAM].getValue() + (inputs[ATTACK_CV_INPUT].getVoltage() * 10.0 * params[ATTACK_CV_ATTENUVERTER_PARAM].getValue()),0.02f,100.0f);
	double release = clamp(params[RELEASE_PARAM].getValue() + (inputs[RELEASE_CV_INPUT].getVoltage() * 500.0 * params[RELEASE_CV_ATTENUVERTER_PARAM].getValue()),20.0f,5000.0f);
	double rmsWindow = clamp(params[RMS_WINDOW_PARAM].getValue() + (inputs[RMS_WINDOW_INPUT].getVoltage() * 500.0 * params[RMS_WINDOW_CV_ATTENUVERTER_PARAM].getValue()),0.02f,50.0f);

	knee = clamp(params[KNEE_PARAM].getValue() + (inputs[KNEE_CV_INPUT].getVoltage() * params[KNEE_CV_ATTENUVERTER_PARAM].getValue()),0.0f,10.0f);

	double makeupGain = clamp(params[MAKEUP_GAIN_PARAM].getValue() + (inputs[MAKEUP_GAIN_INPUT].getVoltage() * 3.0 * params[MAKEUP_CV_ATTENUVERTER_GAIN_PARAM].getValue()), 0.0f,30.0f);
	double mix = clamp(params[MIX_PARAM].getValue() + (inputs[MIX_CV_INPUT].getVoltage() * 0.1 * params[MIX_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);


	double inputL = inputs[SOURCE_L_INPUT].getVoltage();
	double inputR = inputL;
	if(inputs[SOURCE_R_INPUT].isConnected()) {
		inputR = inputs[SOURCE_R_INPUT].getVoltage();
	}

	double processedL = inputL;
	double processedR = inputR;


	compressor.setRatio(ratio);
	compressor.setThresh(50.0+threshold);
	compressor.setKnee(knee);
	compressor.setAttack(attack);
	compressor.setRelease(release);

	compressorRms.setRatio(ratio);
	compressorRms.setThresh(50.0+threshold);
	compressorRms.setKnee(knee);
	compressorRms.setAttack(attack);
	compressorRms.setRelease(release);
	compressorRms.setWindow(rmsWindow);

	
	double sidechain;
	bool usingSidechain = false;
	if(inputs[SIDECHAIN_INPUT].isConnected()) {
		sidechain = inputs[SIDECHAIN_INPUT].getVoltage();
		usingSidechain = true;
		if(filterMode) {
			sidechain = filterBank[2]->process(sidechain);
		}
	} else {
		if(filterMode) {
			processedL = filterBank[0]->process(inputL);
			processedR = filterBank[1]->process(inputR);
		}
	}

	if(rmsMode) {
		if(usingSidechain) {
			compressorRms.process(sidechain * sidechain);
		} else {
			double inSq1 = processedL * processedL;	// square input
			double inSq2 = processedR * processedR;
			double sum = inSq1 + inSq2;			// power summing
			compressorRms.process(sum);
		}
		gainReduction = compressorRms.getGainReduction();
	} else {
		if(usingSidechain) {
			compressor.process(sidechain);
		} else {
			double rect1 = fabs( processedL );	// rectify input
			double rect2 = fabs( processedR );
			double link = std::max( rect1, rect2 );	// link channels with greater of 2
			compressor.process(link);
		}
		gainReduction = compressor.getGainReduction();
	}		
	
	double finalGainLin = chunkware_simple::dB2lin(makeupGain-gainReduction);

	double outputL;
	double outputR;

	if(!bypassed) {
		outputL = lerp(inputL,inputL * finalGainLin,mix);
		outputR = lerp(inputR,inputR * finalGainLin,mix);
	} else {
		outputL = inputL;
		outputR = inputR;
	}



	outputs[OUTPUT_L].setVoltage(outputL);
	outputs[OUTPUT_R].setVoltage(outputR);
}

struct ManicCompressionDisplay : TransparentWidget {
	ManicCompression *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	

	ManicCompressionDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
	}

	void drawResponse(const DrawArgs &args, double threshold, double ratio, double knee) 
	{
		nvgStrokeColor(args.vg, nvgRGBA(0xe0, 0xe0, 0x10, 0xff));
		nvgStrokeWidth(args.vg, 2.0);
		nvgBeginPath(args.vg);

		threshold = 50.0 + threshold; // reverse it

		double kneeThreshold = threshold - (knee / 2.0);

		double lx = 8+(kneeThreshold*1.8);
		double ly = 119-(kneeThreshold*1.8);

		nvgMoveTo(args.vg,8,119);
		nvgLineTo(args.vg,lx,ly);
		for(int kx=0;kx<knee*1.8;kx++) {
			double kxdB = kx/1.8 - knee/2.0;	
			double a = kxdB + knee/2.0;
			double gain = (((1/ratio)-1.0) * a * a) / (2 * knee) + kxdB + threshold ; 

			// fprintf(stderr, "ratio: %f  knee: %f   db: %f  a: %f, ar: %f\n", ratio,knee,kx/3.0,a,gain);
			double ky = gain*1.8;
			// ly = oly-ky;
			nvgLineTo(args.vg,kx+lx,119.0-ky);
		}
		double finalGain = threshold + (50.0-threshold) / ratio;
		double ey = finalGain * 1.8;;
		nvgLineTo(args.vg,99,119.0-ey);
		nvgStroke(args.vg);
	}

	void drawGainReduction(const DrawArgs &args, Vec pos, float gainReduction) {
		gainReduction = 63-std::pow(std::min(gainReduction,20.0f),0.78); //scale meter
		
		double position = 2.0 * M_PI / 60.0 * gainReduction - M_PI / 2.0; // Rotate 90 degrees
		double cx= cos(position);
		double cy= sin(position);

		double sx= cx * 10.0 + 179.0;
		double sy= cy * 10.0 + 125.0;
		double ex= cx * 82.0 + 179.0;
		double ey= cy * 82.0 + 125.0;


		nvgStrokeColor(args.vg, nvgRGBA(0x10, 0x10, 0x10, 0xff));
		nvgStrokeWidth(args.vg, 1.0);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg,sx,sy);
		nvgLineTo(args.vg,ex,ey);
		nvgStroke(args.vg);

	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;

		drawResponse(args,module->threshold,module->ratio,module->knee);
		drawGainReduction(args, Vec(118, 37), module->gainReduction);
		// drawDivision(args, Vec(104, 47), module->division);
	}
};


struct ManicCompressionWidget : ModuleWidget {
	ManicCompressionWidget(ManicCompression *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ManicCompression.svg")));
		
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 14, 0)));	
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 14, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 14, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 14, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		{
			ManicCompressionDisplay *display = new ManicCompressionDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}


		addParam(createParam<RoundSmallFWKnob>(Vec(15, 140), module, ManicCompression::THRESHOLD_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(39, 158), module, ManicCompression::THRESHOLD_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(43, 143), module, ManicCompression::THRESHOLD_CV_INPUT));


		addParam(createParam<RoundSmallFWKnob>(Vec(75, 140), module, ManicCompression::RATIO_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(99, 158), module, ManicCompression::RATIO_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(103, 143), module, ManicCompression::RATIO_CV_INPUT));


		addParam(createParam<LEDButton>(Vec(10, 195), module, ManicCompression::RMS_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(11.5, 196.5), module, ManicCompression::RMS_MODE_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(13, 216), module, ManicCompression::RMS_MODE_INPUT));

		addParam(createParam<LEDButton>(Vec(10, 245), module, ManicCompression::FILTER_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(11.5, 246.5), module, ManicCompression::FILTER_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(31, 248), module, ManicCompression::FILTER_MODE_INPUT));


		addParam(createParam<RoundSmallFWKnob>(Vec(30, 195), module, ManicCompression::RMS_WINDOW_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(54, 213), module, ManicCompression::RMS_WINDOW_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(58, 198), module, ManicCompression::RMS_WINDOW_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(80, 195), module, ManicCompression::KNEE_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(104, 213), module, ManicCompression::KNEE_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(108, 198), module, ManicCompression::KNEE_CV_INPUT));


		addParam(createParam<RoundSmallFWKnob>(Vec(135, 140), module, ManicCompression::ATTACK_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(159, 158), module, ManicCompression::ATTACK_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(163, 143), module, ManicCompression::ATTACK_CV_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(195, 140), module, ManicCompression::RELEASE_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(219, 158), module, ManicCompression::RELEASE_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(223, 143), module, ManicCompression::RELEASE_CV_INPUT));


		addParam(createParam<RoundSmallFWKnob>(Vec(15, 280), module, ManicCompression::MAKEUP_GAIN_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(39, 298), module, ManicCompression::MAKEUP_CV_ATTENUVERTER_GAIN_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(43, 283), module, ManicCompression::MAKEUP_GAIN_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(105, 280), module, ManicCompression::MIX_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(129, 298), module, ManicCompression::MIX_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(133, 283), module, ManicCompression::MIX_CV_INPUT));

		addParam(createParam<LEDButton>(Vec(190, 280), module, ManicCompression::BYPASS_PARAM));
		addChild(createLight<LargeLight<RedLight>>(Vec(191.5, 281.5), module, ManicCompression::BYPASS_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(215, 283), module, ManicCompression::BYPASS_INPUT));


		addInput(createInput<PJ301MPort>(Vec(115, 335), module, ManicCompression::SIDECHAIN_INPUT));

		addInput(createInput<PJ301MPort>(Vec(10, 335), module, ManicCompression::SOURCE_L_INPUT));
		addInput(createInput<PJ301MPort>(Vec(40, 335), module, ManicCompression::SOURCE_R_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(180, 335), module, ManicCompression::OUTPUT_L));
		addOutput(createOutput<PJ301MPort>(Vec(210, 335), module, ManicCompression::OUTPUT_R));


		//addChild(createLight<LargeLight<BlueLight>>(Vec(69,58), module, ManicCompression::BLINK_LIGHT));
	}
};

Model *modelManicCompression = createModel<ManicCompression, ManicCompressionWidget>("ManicCompression");
