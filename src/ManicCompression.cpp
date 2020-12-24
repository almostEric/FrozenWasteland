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
		MAKEUP_GAIN_CV_ATTENUVERTER_PARAM,
		MIX_PARAM,
		MIX_CV_ATTENUVERTER_PARAM,
		BYPASS_PARAM,
		RMS_MODE_PARAM,
		RMS_WINDOW_PARAM,
		RMS_WINDOW_CV_ATTENUVERTER_PARAM,
		LP_FILTER_MODE_PARAM,
		HP_FILTER_MODE_PARAM,
		MS_MODE_PARAM,
		COMPRESS_M_PARAM,
		COMPRESS_S_PARAM,
		IN_GAIN_PARAM,
		IN_GAIN_CV_ATTENUVERTER_PARAM,
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
		LP_FILTER_MODE_INPUT,
		HP_FILTER_MODE_INPUT,
		MS_MODE_INPUT,
		COMPRESS_M_INPUT,
		COMPRESS_S_INPUT,
		IN_GAIN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_L,
		OUTPUT_R,
		ENVELOPE_OUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BYPASS_LIGHT,
		RMS_MODE_LIGHT,
		LP_FILTER_LIGHT,
		HP_FILTER_LIGHT,
		MS_MODE_LIGHT,
		COMPRESS_MID_LIGHT,
		COMPRESS_SIDE_LIGHT,
		NUM_LIGHTS
	};


	//Stuff for S&Hs
	dsp::SchmittTrigger bypassTrigger,	rmsTrigger, lpFilterTrigger, hpFilterTrigger, bypassInputTrigger, rmsInputTrigger, lpFilterInputTrigger, hpFilterInputTrigger,
						midSideModeTrigger,midSideModeInputTrigger,compressMidTrigger,compressMidInputTrigger,compressSideTrigger,compressSideInputTrigger;
	
	chunkware_simple::SimpleComp compressor;
	chunkware_simple::SimpleCompRms compressorRms;
	float gainReduction;
	double threshold, ratio, knee;

	bool bypassed =  false;
	bool gateFlippedBypassed =  false;
	bool rmsMode = false;
	bool gateFlippedRMS =  false;
	bool lpFilterMode = false;
	bool hpFilterMode = false;
	bool gateFlippedLPFilter =  false;
	bool gateFlippedHPFilter =  false;

	bool midSideMode = false;
	bool gateFlippedMidSideMode =  false;
	bool compressMid = false;
	bool gateFlippedCompressMid =  false;
	bool compressSide = false;
	bool gateFlippedCompressSide =  false;


	bool gateMode = false;

	Biquad* lpFilterBank[6]; // 3 filters 2 for stereo inputs, one for sidechain x 2 to increase slope
	Biquad* hpFilterBank[6]; // 3 filters 2 for stereo inputs, one for sidechain x 2 to increase slope

	ManicCompression() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(THRESHOLD_PARAM, -50.0, 0.0, -30.0,"Threshold", " db");
		configParam(RATIO_PARAM, 0.0, 1.0, 0.1,"Ratio","",20,1.0);	
		configParam(ATTACK_PARAM, 0.0, 1.0, 0.1,"Attack", " ms",13.4975,8.0,-7.98);
		configParam(RELEASE_PARAM, 0.0, 1.0, 0.1,"Release"," ms",629.0,8.0,32.0);	
		configParam(ATTACK_CURVE_PARAM, -1.0, 1.0, 0.0,"Attack Curve");
		configParam(RELEASE_CURVE_PARAM, -1.0, 1.0, 0.0,"Release Curve");	
		configParam(KNEE_PARAM, 0.0, 10.0, 0.0,"Knee", " db");	
		configParam(RMS_WINDOW_PARAM, 0.02, 50.0, 5.0,"RMS Window", " ms");
		configParam(IN_GAIN_PARAM, 0.0, 30.0, 0.0,"Input Gain", " db");
		configParam(MAKEUP_GAIN_PARAM, 0.0, 30.0, 0.0,"Makeup Gain", " db");
		configParam(MIX_PARAM, 0.0, 1.0, 1.0,"Mix","%",0,100);	

		configParam(THRESHOLD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Threshold CV Attenuation","%",0,100);
		configParam(RATIO_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Ratio CV Attenuation","%",0,100);	
		configParam(ATTACK_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Attack CV Attenuation","%",0,100);
		configParam(RELEASE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Release CV Attenuation","%",0,100);	
		configParam(ATTACK_CURVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Attack Curve CV Attenuation","%",0,100);
		configParam(RELEASE_CURVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Release Curve CV Attenuation","%",0,100);	
		configParam(KNEE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Knee CV Attenuation","%",0,100);	
		configParam(RMS_WINDOW_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"RMS Windows CV Attenuation","%",0,100);	
		configParam(IN_GAIN_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Input Gain CV Attenuation","%",0,100);
		configParam(MAKEUP_GAIN_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Makeup Gain CV Attenuation","%",0,100);
		configParam(MIX_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Mix CV Attenuation","%",0,100);	


		float sampleRate = APP->engine->getSampleRate();
		double lpCutoff = 4000/sampleRate;
		double hpCutoff = 240/sampleRate;

		for(int f=0;f<6;f++) {
			lpFilterBank[f] = new Biquad(bq_type_lowpass, lpCutoff , 0.707, 0);
			hpFilterBank[f] = new Biquad(bq_type_highpass, hpCutoff , 0.707, 0);
		}

		compressor.initRuntime();
		compressorRms.initRuntime();

	}
	void process(const ProcessArgs &args) override;
    void dataFromJson(json_t *) override;
	void onSampleRateChange() override;
    json_t *dataToJson() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

double lerp(double v0, double v1, double t) {
		return (1 - t) * v0 + t * v1;
}

double sgn(double val) {
    return (double(0) < val) - (val < double(0));
}	

json_t *ManicCompression::dataToJson() {
	json_t *rootJ = json_object();
	
	json_object_set_new(rootJ, "bypassed", json_integer((bool) bypassed));
	json_object_set_new(rootJ, "rmsMode", json_integer((bool) rmsMode));
	json_object_set_new(rootJ, "lpFilterMode", json_integer((bool) lpFilterMode));
	json_object_set_new(rootJ, "hpFilterMode", json_integer((bool) hpFilterMode));

	json_object_set_new(rootJ, "midSideMode", json_integer((bool) midSideMode));
	json_object_set_new(rootJ, "compressMid", json_integer((bool) compressMid));
	json_object_set_new(rootJ, "compressSide", json_integer((bool) compressSide));


	json_object_set_new(rootJ, "gateMode", json_integer((bool) gateMode));

	return rootJ;
}

void ManicCompression::dataFromJson(json_t *rootJ) {

	json_t *bpJ = json_object_get(rootJ, "bypassed");
	if (bpJ)
		bypassed = json_integer_value(bpJ);

	json_t *rmsJ = json_object_get(rootJ, "rmsMode");
	if (rmsJ)
		rmsMode = json_integer_value(rmsJ);

	json_t *lfmJ = json_object_get(rootJ, "lpFilterMode");
	if (lfmJ)
		lpFilterMode = json_integer_value(lfmJ);

	json_t *hfmJ = json_object_get(rootJ, "hpFilterMode");
	if (hfmJ)
		hpFilterMode = json_integer_value(hfmJ);

	json_t *msmJ = json_object_get(rootJ, "midSideMode");
	if (msmJ)
		midSideMode = json_integer_value(msmJ);

	json_t *cmJ = json_object_get(rootJ, "compressMid");
	if (cmJ)
		compressMid = json_integer_value(cmJ);

	json_t *csJ = json_object_get(rootJ, "compressSide");
	if (csJ)
		compressSide = json_integer_value(csJ);


	json_t *gmJ = json_object_get(rootJ, "gateMode");
	if (gmJ)
		gateMode = json_integer_value(gmJ);

}

void ManicCompression::onSampleRateChange() {
	float sampleRate = APP->engine->getSampleRate();
	double lpCutoff = 4000/sampleRate;
	double hpCutoff = 240/sampleRate;

	for(int f=0;f<6;f++) {
		lpFilterBank[f]->setFc(lpCutoff);
		hpFilterBank[f]->setFc(hpCutoff);
	}
}

void ManicCompression::process(const ProcessArgs &args) {
	compressor.setSampleRate(double(args.sampleRate));
	compressorRms.setSampleRate(double(args.sampleRate));

	float bypassInput = inputs[BYPASS_INPUT].getVoltage();
	if(gateMode) {
		if(bypassed != (bypassInput !=0) && gateFlippedBypassed ) {
			bypassed = bypassInput != 0;
			gateFlippedBypassed = true;
		} else if (bypassed == (bypassInput !=0)) {
			gateFlippedBypassed = true;
		}
	} else {
		if(bypassInputTrigger.process(bypassInput)) {
			bypassed = !bypassed;
			gateFlippedBypassed= false;
		}
	}
	if(bypassTrigger.process(params[BYPASS_PARAM].getValue())) {
		bypassed = !bypassed;
		gateFlippedBypassed= false;
	}
	lights[BYPASS_LIGHT].value = bypassed ? 1.0 : 0.0;

	float rmsInput = inputs[RMS_MODE_INPUT].getVoltage();
	if(gateMode) {
		if(rmsMode != (rmsInput !=0) && gateFlippedRMS ) {
			rmsMode = rmsInput != 0;
			gateFlippedRMS = true;
		} else if (rmsMode == (rmsInput !=0)) {
			gateFlippedRMS = true;
		}
	} else {
		if(rmsInputTrigger.process(rmsInput)) {
			rmsMode = !rmsMode;
			gateFlippedRMS= false;
		}
	}
	if(rmsTrigger.process(params[RMS_MODE_PARAM].getValue())) {
		rmsMode = !rmsMode;
		gateFlippedRMS= false;
	}
	lights[RMS_MODE_LIGHT].value = rmsMode ? 1.0 : 0.0;

	float lpFilterInput = inputs[LP_FILTER_MODE_INPUT].getVoltage();
	if(gateMode) {
		if(lpFilterMode != (lpFilterInput !=0) && gateFlippedLPFilter ) {
			lpFilterMode = lpFilterInput != 0;
			gateFlippedLPFilter = true;
		} else if (lpFilterMode == (lpFilterInput !=0)) {
			gateFlippedLPFilter = true;
		}
	} else {
		if(lpFilterInputTrigger.process(lpFilterInput)) {
			lpFilterMode = !lpFilterMode;
			gateFlippedLPFilter= false;
		}
	}
	if(lpFilterTrigger.process(params[LP_FILTER_MODE_PARAM].getValue())) {
		lpFilterMode = !lpFilterMode;
		gateFlippedLPFilter= false;
	}
	lights[LP_FILTER_LIGHT].value = lpFilterMode ? 1.0 : 0.0;

	float hpFilterInput = inputs[HP_FILTER_MODE_INPUT].getVoltage();
	if(gateMode) {
		if(hpFilterMode != (hpFilterInput !=0) && gateFlippedHPFilter ) {
			hpFilterMode = hpFilterInput != 0;
			gateFlippedHPFilter = true;
		} else if (hpFilterMode == (hpFilterInput !=0)) {
			gateFlippedHPFilter = true;
		}
	} else {
		if(hpFilterInputTrigger.process(hpFilterInput)) {
			hpFilterMode = !hpFilterMode;
			gateFlippedHPFilter= false;
		}
	}
	if(hpFilterTrigger.process(params[HP_FILTER_MODE_PARAM].getValue())) {
		hpFilterMode = !hpFilterMode;
		gateFlippedHPFilter= false;
	}
	lights[HP_FILTER_LIGHT].value = hpFilterMode ? 1.0 : 0.0;

	float msModeInput = inputs[MS_MODE_INPUT].getVoltage();
	if(gateMode) {
		if(midSideMode != (msModeInput !=0) && gateFlippedMidSideMode ) {
			midSideMode = msModeInput != 0;
			gateFlippedMidSideMode = true;
		} else if (midSideMode == (msModeInput !=0)) {
			gateFlippedMidSideMode = true;
		}
	} else {
		if(midSideModeInputTrigger.process(msModeInput)) {
			midSideMode = !midSideMode;
			gateFlippedMidSideMode= false;
		}
	}
	if(midSideModeTrigger.process(params[MS_MODE_PARAM].getValue())) {
		midSideMode = !midSideMode;
		gateFlippedMidSideMode= false;
	}
	lights[MS_MODE_LIGHT].value = midSideMode ? 1.0 : 0.0;

	float compressMidInput = inputs[COMPRESS_M_INPUT].getVoltage();
	if(gateMode) {
		if(compressMid != (compressMidInput !=0) && gateFlippedCompressMid ) {
			compressMid = compressMidInput != 0;
			gateFlippedCompressMid = true;
		} else if (compressMid == (compressMidInput !=0)) {
			gateFlippedCompressMid = true;
		}
	} else {
		if(compressMidInputTrigger.process(compressMidInput)) {
			compressMid = !compressMid;
			gateFlippedCompressMid= false;
		}
	}
	if(compressMidTrigger.process(params[COMPRESS_M_PARAM].getValue())) {
		compressMid = !compressMid;
		gateFlippedCompressMid= false;
	}
	lights[COMPRESS_MID_LIGHT].value = compressMid ? (midSideMode ? 1.0 : 0.1) : 0.0;

	float compressSideInput = inputs[COMPRESS_S_INPUT].getVoltage();
	if(gateMode) {
		if(compressSide != (compressSideInput !=0) && gateFlippedCompressSide ) {
			compressSide = compressSideInput != 0;
			gateFlippedCompressSide = true;
		} else if (compressSide == (compressSideInput !=0)) {
			gateFlippedCompressSide = true;
		}
	} else {
		if(compressSideInputTrigger.process(compressSideInput)) {
			compressSide = !compressSide;
			gateFlippedCompressSide= false;
		}
	}
	if(compressSideTrigger.process(params[COMPRESS_S_PARAM].getValue())) {
		compressSide = !compressSide;
		gateFlippedCompressSide= false;
	}
	lights[COMPRESS_SIDE_LIGHT].value = compressSide ? (midSideMode ? 1.0 : 0.1) : 0.0;


	float paramRatio = clamp(params[RATIO_PARAM].getValue() + (inputs[RATIO_CV_INPUT].getVoltage() * 0.1 * params[RATIO_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
	ratio = pow(20.0,paramRatio);
	threshold = clamp(params[THRESHOLD_PARAM].getValue() + (inputs[THRESHOLD_CV_INPUT].getVoltage() * 3.0 * params[THRESHOLD_CV_ATTENUVERTER_PARAM].getValue()),-50.0f,0.0f);
	float paramAttack = clamp(params[ATTACK_PARAM].getValue() + (inputs[ATTACK_CV_INPUT].getVoltage() * 10.0 * params[ATTACK_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
	double attack = pow(13.4975,paramAttack) * 8.0 - 7.98;
	float paramRelease = clamp(params[RELEASE_PARAM].getValue() + (inputs[RELEASE_CV_INPUT].getVoltage() * 500.0 * params[RELEASE_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
	double release = pow(629.0,paramRelease) * 8.0 + 32.0;

	double rmsWindow = clamp(params[RMS_WINDOW_PARAM].getValue() + (inputs[RMS_WINDOW_INPUT].getVoltage() * 500.0 * params[RMS_WINDOW_CV_ATTENUVERTER_PARAM].getValue()),0.02f,50.0f);

	double attackCurve = clamp(params[ATTACK_CURVE_PARAM].getValue() + (inputs[ATTACK_CV_INPUT].getVoltage() * 0.1 * params[ATTACK_CURVE_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);
	double releaseCurve = clamp(params[RELEASE_CURVE_PARAM].getValue() + (inputs[RELEASE_CV_INPUT].getVoltage() * 0.1 * params[RELEASE_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);

	knee = clamp(params[KNEE_PARAM].getValue() + (inputs[KNEE_CV_INPUT].getVoltage() * params[KNEE_CV_ATTENUVERTER_PARAM].getValue()),0.0f,10.0f);

	double inputGain = clamp(params[IN_GAIN_PARAM].getValue() + (inputs[IN_GAIN_INPUT].getVoltage() * 3.0 * params[IN_GAIN_CV_ATTENUVERTER_PARAM].getValue()), 0.0f,30.0f);
	inputGain = chunkware_simple::dB2lin(inputGain);
	double makeupGain = clamp(params[MAKEUP_GAIN_PARAM].getValue() + (inputs[MAKEUP_GAIN_INPUT].getVoltage() * 3.0 * params[MAKEUP_GAIN_CV_ATTENUVERTER_PARAM].getValue()), 0.0f,30.0f);
	double mix = clamp(params[MIX_PARAM].getValue() + (inputs[MIX_CV_INPUT].getVoltage() * 0.1 * params[MIX_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);


	double inputL = inputs[SOURCE_L_INPUT].getVoltage();
	double inputR = inputL;
	if(inputs[SOURCE_R_INPUT].isConnected()) {
		inputR = inputs[SOURCE_R_INPUT].getVoltage();
	}

	//In Mid-Side Mode, L=mid, R=side
	if(midSideMode) {
		//Encode
		double mid = inputL + inputR;
		double side = inputL - inputR;
		inputL = mid;
		inputR = side;
	}

	double processedL = inputL;
	double processedR = inputR;


	compressor.setRatio(ratio);
	compressor.setThresh(50.0+threshold);
	compressor.setKnee(knee);
	compressor.setAttack(attack);
	compressor.setRelease(release);
	compressor.setAttackCurve(attackCurve);
	compressor.setReleaseCurve(releaseCurve);

	compressorRms.setRatio(ratio);
	compressorRms.setThresh(50.0+threshold);
	compressorRms.setKnee(knee);
	compressorRms.setAttack(attack);
	compressorRms.setRelease(release);
	compressorRms.setAttackCurve(attackCurve);
	compressorRms.setReleaseCurve(releaseCurve);
	compressorRms.setWindow(rmsWindow);

	
	double sidechain;
	bool usingSidechain = false;
	if(inputs[SIDECHAIN_INPUT].isConnected()) {
		sidechain = inputs[SIDECHAIN_INPUT].getVoltage();
		usingSidechain = true;
		if(lpFilterMode) {
			sidechain = lpFilterBank[5]->process(lpFilterBank[4]->process(sidechain));
		}
		if(hpFilterMode) {
			sidechain = hpFilterBank[5]->process(hpFilterBank[4]->process(sidechain));
		}
	} else {
		if(lpFilterMode) {
			processedL = lpFilterBank[1]->process(lpFilterBank[0]->process(processedL));
			processedR = lpFilterBank[3]->process(lpFilterBank[2]->process(processedR));
		}
		if(hpFilterMode) {
			processedL = hpFilterBank[1]->process(hpFilterBank[0]->process(processedL));
			processedR = hpFilterBank[3]->process(hpFilterBank[2]->process(processedR));
		}
	}

	if(rmsMode) {
		if(usingSidechain) {
			compressorRms.process(sidechain * sidechain);
		} else {
			double inSq1 = processedL * processedL;	// square input
			double inSq2 = processedR * processedR;
			double sum = inSq1 + inSq2;			// power summing
			compressorRms.process(sum * inputGain);
		}
		gainReduction = compressorRms.getGainReduction();
	} else {
		if(usingSidechain) {
			compressor.process(sidechain);
		} else {
			double rect1 = fabs( processedL );	// rectify input
			double rect2 = fabs( processedR );
			double link = std::max( rect1, rect2 );	// link channels with greater of 2
			compressor.process(link * inputGain);
		}
		gainReduction = compressor.getGainReduction();
	}		
	
	outputs[ENVELOPE_OUT].setVoltage(clamp(chunkware_simple::dB2lin(gainReduction) / 3.0f,-10.0f,10.0f));
	double finalGainLin = chunkware_simple::dB2lin(makeupGain-gainReduction);

	double outputL;
	double outputR;

	if(!bypassed) {
		
		double processedOutputL;
		double processedOutputR;
		if(!midSideMode) {
			processedOutputL = inputL * finalGainLin; 
			processedOutputR = inputR * finalGainLin;
		} else {
			double processedMid =  compressMid ? inputL * finalGainLin : inputL;
			double processedSide =  compressSide ? inputR * finalGainLin : inputR;
			//Decode
			processedOutputL = processedMid + processedSide;
			processedOutputR = processedMid - processedSide;
			
		}

		outputL = lerp(inputL,processedOutputL,mix);
		outputR = lerp(inputR,processedOutputR,mix);
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

		double gainDirection = sgn(gainReduction);
		float scaling = 0.88;
		if (gainReduction >=10) 
			scaling =0.76;
		gainReduction = 63.25-std::pow(std::min(std::abs(gainReduction),20.0f),scaling) * gainDirection; //scale meter
		
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
		addParam(createParam<RoundReallySmallFWKnob>(Vec(40, 158), module, ManicCompression::THRESHOLD_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(44, 143), module, ManicCompression::THRESHOLD_CV_INPUT));


		addParam(createParam<RoundSmallFWKnob>(Vec(75, 140), module, ManicCompression::RATIO_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(100, 158), module, ManicCompression::RATIO_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(104, 143), module, ManicCompression::RATIO_CV_INPUT));


		addParam(createParam<LEDButton>(Vec(10, 195), module, ManicCompression::RMS_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(11.5, 196.5), module, ManicCompression::RMS_MODE_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(13, 216), module, ManicCompression::RMS_MODE_INPUT));

		addParam(createParam<LEDButton>(Vec(10, 247), module, ManicCompression::LP_FILTER_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(11.5, 248.5), module, ManicCompression::LP_FILTER_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(31, 250), module, ManicCompression::LP_FILTER_MODE_INPUT));

		addParam(createParam<LEDButton>(Vec(60, 247), module, ManicCompression::HP_FILTER_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(61.5, 248.5), module, ManicCompression::HP_FILTER_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(81, 250), module, ManicCompression::HP_FILTER_MODE_INPUT));


		addParam(createParam<RoundSmallFWKnob>(Vec(30, 195), module, ManicCompression::RMS_WINDOW_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(55, 213), module, ManicCompression::RMS_WINDOW_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(59, 198), module, ManicCompression::RMS_WINDOW_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(82, 195), module, ManicCompression::KNEE_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(107, 213), module, ManicCompression::KNEE_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(111, 198), module, ManicCompression::KNEE_CV_INPUT));


		addParam(createParam<RoundSmallFWKnob>(Vec(135, 140), module, ManicCompression::ATTACK_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(160, 158), module, ManicCompression::ATTACK_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(164, 143), module, ManicCompression::ATTACK_CV_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(195, 140), module, ManicCompression::RELEASE_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(220, 158), module, ManicCompression::RELEASE_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(224, 143), module, ManicCompression::RELEASE_CV_INPUT));


		addParam(createParam<RoundSmallFWKnob>(Vec(135, 200), module, ManicCompression::ATTACK_CURVE_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(160, 213), module, ManicCompression::ATTACK_CURVE_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(164, 198), module, ManicCompression::ATTACK_CURVE_CV_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(195, 200), module, ManicCompression::RELEASE_CURVE_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(220, 213), module, ManicCompression::RELEASE_CURVE_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(224, 198), module, ManicCompression::RELEASE_CURVE_CV_INPUT));


		addParam(createParam<RoundSmallFWKnob>(Vec(15, 280), module, ManicCompression::IN_GAIN_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(40, 298), module, ManicCompression::IN_GAIN_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(44, 283), module, ManicCompression::IN_GAIN_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(75, 280), module, ManicCompression::MAKEUP_GAIN_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(100, 298), module, ManicCompression::MAKEUP_GAIN_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(104, 283), module, ManicCompression::MAKEUP_GAIN_INPUT));

		addParam(createParam<RoundSmallFWKnob>(Vec(135, 280), module, ManicCompression::MIX_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(160, 298), module, ManicCompression::MIX_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(164, 283), module, ManicCompression::MIX_CV_INPUT));

		addParam(createParam<LEDButton>(Vec(192, 280), module, ManicCompression::BYPASS_PARAM));
		addChild(createLight<LargeLight<RedLight>>(Vec(193.5, 281.5), module, ManicCompression::BYPASS_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(213, 283), module, ManicCompression::BYPASS_INPUT));

		
		addParam(createParam<LEDButton>(Vec(110, 247), module, ManicCompression::MS_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(111.5, 248.5), module, ManicCompression::MS_MODE_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(131, 250), module, ManicCompression::MS_MODE_INPUT));


		addParam(createParam<LEDButton>(Vec(157, 247), module, ManicCompression::COMPRESS_M_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(158.5, 248.5), module, ManicCompression::COMPRESS_MID_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(178, 250), module, ManicCompression::COMPRESS_M_INPUT));

		addParam(createParam<LEDButton>(Vec(201, 247), module, ManicCompression::COMPRESS_S_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(202.5, 248.5), module, ManicCompression::COMPRESS_SIDE_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(222, 250), module, ManicCompression::COMPRESS_S_INPUT));

		addInput(createInput<PJ301MPort>(Vec(10, 335), module, ManicCompression::SOURCE_L_INPUT));
		addInput(createInput<PJ301MPort>(Vec(40, 335), module, ManicCompression::SOURCE_R_INPUT));

		addInput(createInput<PJ301MPort>(Vec(85, 335), module, ManicCompression::SIDECHAIN_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(140, 335), module, ManicCompression::ENVELOPE_OUT));

		addOutput(createOutput<PJ301MPort>(Vec(186, 335), module, ManicCompression::OUTPUT_L));
		addOutput(createOutput<PJ301MPort>(Vec(216, 335), module, ManicCompression::OUTPUT_R));


		//addChild(createLight<LargeLight<BlueLight>>(Vec(69,58), module, ManicCompression::BLINK_LIGHT));
	}

	struct TriggerGateItem : MenuItem {
		ManicCompression *module;
		void onAction(const event::Action &e) override {
			module->gateMode = !module->gateMode;
		}
		void step() override {
			text = "Gate Mode";
			rightText = (module->gateMode) ? "✔" : "";
		}
	};
	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		ManicCompression *module = dynamic_cast<ManicCompression*>(this->module);
		assert(module);

		TriggerGateItem *triggerGateItem = new TriggerGateItem();
		triggerGateItem->module = module;
		menu->addChild(triggerGateItem);
			
	}
};

Model *modelManicCompression = createModel<ManicCompression, ManicCompressionWidget>("ManicCompression");
