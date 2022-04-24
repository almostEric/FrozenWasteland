#include "FrozenWasteland.hpp"
#include "dsp-compressor/SimpleComp.h"
#include "dsp-compressor/SimpleGain.h"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/menu.hpp"
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
		COMPRESS_DIRECTION_PARAM,
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
		COMPRESS_DIRECTION_INPUT,
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
		COMPRESS_DIRECTION_LIGHT,
		NUM_LIGHTS
	};


	//Stuff for S&Hs
	dsp::SchmittTrigger bypassTrigger,	rmsTrigger, lpFilterTrigger, hpFilterTrigger, bypassInputTrigger, rmsInputTrigger, lpFilterInputTrigger, hpFilterInputTrigger,
						midSideModeTrigger,midSideModeInputTrigger,compressMidTrigger,compressMidInputTrigger,compressSideTrigger,compressSideInputTrigger,
						compressDirectionTrigger,compressDirectionInputTrigger;
	
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
	bool compressDirection = false;
	bool gateFlippedCompressDirection = false;

	bool gateMode = false;
	int envelopeMode = 0;

	Biquad* lpFilterBank[6]; // 3 filters 2 for stereo inputs, one for sidechain x 2 to increase slope
	Biquad* hpFilterBank[6]; // 3 filters 2 for stereo inputs, one for sidechain x 2 to increase slope

	//percentages
	float thresholdPercentage = 0;
	float ratioPercentage = 0;
	float kneePercentage = 0;
	float rmsWindowPercentage = 0;
	float attackPercentage = 0;
	float releasePercentage = 0;
	float attackCurvePercentage = 0;
	float releaseCurvePercentage = 0;
	float inGainPercentage = 0;
	float makeupGainPercentage = 0;
	float mixPercentage = 0;

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

	
		configButton(RMS_MODE_PARAM,"RMS Mode");
		configButton(LP_FILTER_MODE_PARAM,"LP Filter");
		configButton(HP_FILTER_MODE_PARAM,"HP FIlter");
		configButton(MS_MODE_PARAM,"Mid-Side Mode");
		configButton(COMPRESS_M_PARAM,"Compress Mid");
		configButton(COMPRESS_S_PARAM,"Compress Side");
		configButton(BYPASS_PARAM,"Bypass");
		configButton(COMPRESS_DIRECTION_PARAM,"Compression Direction");


		configInput(THRESHOLD_CV_INPUT, "Threshold");
		configInput(ATTACK_CV_INPUT, "Attack");
		configInput(RELEASE_CV_INPUT, "Release");
		configInput(ATTACK_CURVE_CV_INPUT, "Attack Curve");
		configInput(RELEASE_CURVE_CV_INPUT, "Release Curve");
		configInput(RATIO_CV_INPUT, "Ratio");
		configInput(KNEE_CV_INPUT, "Knee");
		configInput(MAKEUP_GAIN_INPUT, "Makeup Gain");
		configInput(MIX_CV_INPUT, "Mix Level");
		configInput(RMS_MODE_INPUT, "RMS Mode");
		configInput(RMS_WINDOW_INPUT, "RMS Window");
		configInput(BYPASS_INPUT, "Bypass Control");
		configInput(SOURCE_L_INPUT, "Left Source");
		configInput(SOURCE_R_INPUT, "Right Source");
		configInput(SIDECHAIN_INPUT, "Sidechain");
		configInput(LP_FILTER_MODE_INPUT, "LP Filter Mode");
		configInput(HP_FILTER_MODE_INPUT, "HP Filter Mode");
		configInput(MS_MODE_INPUT, "Mid/Side Mode");
		configInput(COMPRESS_M_INPUT, "Compress Mid");
		configInput(COMPRESS_S_INPUT, "Compress Side");
		configInput(IN_GAIN_INPUT, "Input Gain");
		configInput(COMPRESS_DIRECTION_INPUT, "Compression Direction");

		configOutput(OUTPUT_L, "Left");
		configOutput(OUTPUT_R, "Right");
		configOutput(ENVELOPE_OUT, "Envelope");


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
	json_object_set_new(rootJ, "compressDirection", json_integer((bool) compressDirection));


	json_object_set_new(rootJ, "gateMode", json_integer((bool) gateMode));
	json_object_set_new(rootJ, "envelopeMode", json_integer(envelopeMode));

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

	json_t *emJ = json_object_get(rootJ, "envelopeMode");
	if (emJ)
		envelopeMode = json_integer_value(emJ);

	json_t *cdJ = json_object_get(rootJ, "compressDirection");
	if (cdJ)
		compressDirection = json_integer_value(cdJ);

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

	float compressDirectionInput = inputs[COMPRESS_DIRECTION_INPUT].getVoltage();
	if(gateMode) {
		if(compressDirection != (compressDirection !=0) && gateFlippedCompressDirection ) {
			compressDirection = compressDirectionInput != 0;
			gateFlippedCompressDirection = true;
		} else if (compressDirection == (compressDirection !=0)) {
			gateFlippedCompressDirection = true;
		}
	} else {
		if(compressDirectionInputTrigger.process(compressDirectionInput)) {
			compressDirection = !compressDirection;
			gateFlippedCompressDirection = false;
		}
	}
	if(compressDirectionTrigger.process(params[COMPRESS_DIRECTION_PARAM].getValue())) {
		compressDirection = !compressDirection;
		gateFlippedCompressDirection = false;
	}
	lights[COMPRESS_DIRECTION_LIGHT].value = compressDirection ? 1.0 : 0.0;



	float paramRatio = clamp(params[RATIO_PARAM].getValue() + (inputs[RATIO_CV_INPUT].getVoltage() * 0.1 * params[RATIO_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
	ratioPercentage = paramRatio;
	ratio = pow(20.0,paramRatio);
	threshold = clamp(params[THRESHOLD_PARAM].getValue() + (inputs[THRESHOLD_CV_INPUT].getVoltage() * 3.0 * params[THRESHOLD_CV_ATTENUVERTER_PARAM].getValue()),-50.0f,0.0f);
	thresholdPercentage = (threshold + 50.0) / 50.0;
	float paramAttack = clamp(params[ATTACK_PARAM].getValue() + (inputs[ATTACK_CV_INPUT].getVoltage() * 10.0 * params[ATTACK_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
	attackPercentage = paramAttack;
	double attack = pow(13.4975,paramAttack) * 8.0 - 7.98;
	float paramRelease = clamp(params[RELEASE_PARAM].getValue() + (inputs[RELEASE_CV_INPUT].getVoltage() * 500.0 * params[RELEASE_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
	releasePercentage = paramRelease;
	double release = pow(629.0,paramRelease) * 8.0 + 32.0;

	double rmsWindow = clamp(params[RMS_WINDOW_PARAM].getValue() + (inputs[RMS_WINDOW_INPUT].getVoltage() * 500.0 * params[RMS_WINDOW_CV_ATTENUVERTER_PARAM].getValue()),0.02f,50.0f);
	rmsWindowPercentage = rmsWindow / 50.0;

	double attackCurve = clamp(params[ATTACK_CURVE_PARAM].getValue() + (inputs[ATTACK_CV_INPUT].getVoltage() * 0.1 * params[ATTACK_CURVE_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);
	attackCurvePercentage = attackCurve;
	double releaseCurve = clamp(params[RELEASE_CURVE_PARAM].getValue() + (inputs[RELEASE_CV_INPUT].getVoltage() * 0.1 * params[RELEASE_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);
	releaseCurvePercentage = releaseCurve;

	knee = clamp(params[KNEE_PARAM].getValue() + (inputs[KNEE_CV_INPUT].getVoltage() * params[KNEE_CV_ATTENUVERTER_PARAM].getValue()),0.0f,10.0f);
	kneePercentage = knee / 10.0;

	double inputGain = clamp(params[IN_GAIN_PARAM].getValue() + (inputs[IN_GAIN_INPUT].getVoltage() * 3.0 * params[IN_GAIN_CV_ATTENUVERTER_PARAM].getValue()), 0.0f,30.0f);
	inGainPercentage = inputGain / 30.0;
	inputGain = chunkware_simple::dB2lin(inputGain);
	double makeupGain = clamp(params[MAKEUP_GAIN_PARAM].getValue() + (inputs[MAKEUP_GAIN_INPUT].getVoltage() * 3.0 * params[MAKEUP_GAIN_CV_ATTENUVERTER_PARAM].getValue()), 0.0f,30.0f);
	makeupGainPercentage = makeupGain / 30.0;
	double mix = clamp(params[MIX_PARAM].getValue() + (inputs[MIX_CV_INPUT].getVoltage() * 0.1 * params[MIX_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
	mixPercentage = mix;


	double inputL = inputs[SOURCE_L_INPUT].getVoltage();
	double inputR = inputL;
	if(inputs[SOURCE_R_INPUT].isConnected()) {
		inputR = inputs[SOURCE_R_INPUT].getVoltage();
	}
	double originalInputL = inputL;
	double originalInputR = inputR;

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
		compressorRms.setRatio(ratio);
		compressorRms.setThresh(50.0+threshold);
		compressorRms.setKnee(knee);
		compressorRms.setAttack(attack);
		compressorRms.setRelease(release);
		compressorRms.setAttackCurve(attackCurve);
		compressorRms.setReleaseCurve(releaseCurve);
		compressorRms.setWindow(rmsWindow);
		

		if(usingSidechain) {
			if(!compressDirection)
				compressorRms.process(sidechain * sidechain);
			else
				compressorRms.processUpward(sidechain * sidechain);
		} else {
			double inSq1 = processedL * processedL;	// square input
			double inSq2 = processedR * processedR;
			double sum = inSq1 + inSq2;			// power summing
			if(!compressDirection)
				compressorRms.process(sum * inputGain);
			else
				compressorRms.processUpward(sum * inputGain);
		}
		gainReduction = compressorRms.getGainReduction();
	} else {
		compressor.setRatio(ratio);
		compressor.setThresh(50.0+threshold);
		compressor.setKnee(knee);
		compressor.setAttack(attack);
		compressor.setRelease(release);
		compressor.setAttackCurve(attackCurve);
		compressor.setReleaseCurve(releaseCurve);
		

		if(usingSidechain) {
			if(!compressDirection)
				compressor.process(sidechain);
			else
				compressor.processUpward(sidechain);
		} else {
			double rect1 = fabs( processedL );	// rectify input
			double rect2 = fabs( processedR );
			double link = std::max( rect1, rect2 );	// link channels with greater of 2
			if(!compressDirection)
				compressor.process(link * inputGain);
			else
				compressor.processUpward(link * inputGain);
		}
		gainReduction = compressor.getGainReduction();
	}		
	
	switch(envelopeMode) {
		case 0 : // original gain reduced linear
			outputs[ENVELOPE_OUT].setVoltage(clamp(chunkware_simple::dB2lin(gainReduction) / 3.0f,-10.0f,10.0f));
			break;
		case 1 : // linear
			outputs[ENVELOPE_OUT].setVoltage(chunkware_simple::dB2lin(gainReduction));
			break;
		case 2 : // exponential
			outputs[ENVELOPE_OUT].setVoltage(gainReduction);
			break;
	}
	if(compressDirection)
		makeupGain = -makeupGain;
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

		outputL = lerp(originalInputL,processedOutputL,mix);
		outputR = lerp(originalInputR,processedOutputR,mix);
	} else {
		outputL = originalInputL;
		outputR = originalInputR;
	}



	outputs[OUTPUT_L].setVoltage(outputL);
	outputs[OUTPUT_R].setVoltage(outputR);
}

struct ManicCompressionDisplay : TransparentWidget {
	ManicCompression *module;
	int frame = 0;
	std::shared_ptr<Font> font;
	std::string fontPath;

	

	ManicCompressionDisplay() {
		fontPath =asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf");
	}

	void drawResponse(const DrawArgs &args, double threshold, double ratio, double knee, bool direction) 
	{
		nvgStrokeColor(args.vg, nvgRGBA(0xe0, 0xe0, 0x10, 0xff));
		nvgStrokeWidth(args.vg, 2.0);
		nvgBeginPath(args.vg);

		threshold = 50.0 + threshold; // reverse it


		nvgMoveTo(args.vg,8,119);
		if (!direction) { //downward compression
			float kneeThreshold = threshold - (knee / 2.0);
			float lx = 8+std::max(kneeThreshold*1.8f,0.0f);
			float ly = 119-std::max(kneeThreshold*1.8f,0.0f);

			nvgLineTo(args.vg,lx,ly);
			for(int kx=0;kx<knee*1.8;kx++) {
				float kxdB = kx/1.8f - knee/2.0f;	
				float a = kxdB + knee/2.0;
				float gain = (((1/ratio)-1.0) * a * a) / (2 * knee) + kxdB + threshold ; 

				// fprintf(stderr, "threshold: %f  ratio: %f  knee: %f kx:%i  db: %f  a: %f, gain: %f\n", threshold,ratio,knee,kx,kxdB,a,gain);
				float ky = std::max(std::min(gain*1.8f,90.0f),0.0f);
				nvgLineTo(args.vg,std::min(kx+lx,99.f),119.0-ky);
			}
			double finalGain = threshold + (50.0-threshold) / ratio;
			double ey = finalGain * 1.8;;
			nvgLineTo(args.vg,99,119.0-ey);
		} else {			
			float ey = threshold * 1.8f;;
			float ex = 8 + (threshold / ratio * 1.8f);
			nvgLineTo(args.vg,ex,119.0-ey);

			// double kneeThreshold = threshold - (knee / 2.0);
			// double lx = 8+(kneeThreshold*1.8);
			// double ly = 119-(kneeThreshold*1.8);

			// nvgLineTo(args.vg,lx,ly);

			// for(int kx=0;kx<knee*1.8;kx++) {
			// 	double kxdB = kx/1.8 - knee/2.0;	
			// 	double a = kxdB + knee/2.0;
			// 	double gain = ((ratio-1.0) * a * a) / (2 * knee) + kxdB; 

			// 	fprintf(stderr, "threshold: %f  ratio: %f  knee: %f kx:%i  db: %f  a: %f, gain: %f\n", threshold,ratio,knee,kx,kxdB,a,gain);
			// 	double ky = gain*1.8;
			// 	// ly = oly-ky;
			// 	nvgLineTo(args.vg,kx+ex,119.0-(ey+ky));
			// }

			 nvgLineTo(args.vg,ex+(90.0-ey),29.0);
		}
		nvgStroke(args.vg);
	}

	void drawGainReduction(const DrawArgs &args, Vec pos, float gainReduction, bool compressDirection) {

		double gainDirection = sgn(gainReduction);
		double gainReductionDegree;
		float scaling = 0.88;
		if(!compressDirection) { //False is normal, downward compression
			if (gainReduction >=10) 
				scaling =0.76;
			gainReductionDegree = 63.25-std::pow(std::min(std::abs(gainReduction),20.0f),scaling) * gainDirection; //scale meter
		} else {
			float adjustedGR = 0;
			gainReduction = std::max(gainReduction,-30.0f); // limit of meter
			scaling = 1.26 - ((gainReduction / -15.0) * 0.32);
			if (gainReduction <-10.0) {
				adjustedGR = std::abs(gainReduction+10.0f);
				scaling = 1.037 - (std::max(std::log(adjustedGR),0.0f) * 0.076);
			}
				
			gainReductionDegree = 52.15-std::pow(std::abs(gainReduction),scaling) * gainDirection; //scale meter
			// fprintf(stderr, "gain: %f  ag: %f  log:%f scaling:%f\n", gainReduction,adjustedGR,std::log(adjustedGR),scaling);

		}
		double position = 2.0 * M_PI / 60.0 * gainReductionDegree - M_PI / 2.0; // Rotate 90 degrees
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
		font = APP->window->loadFont(fontPath);

		if (!module)
			return;

		drawResponse(args,module->threshold,module->ratio,module->knee,module->compressDirection);
		drawGainReduction(args, Vec(118, 37), module->gainReduction,module->compressDirection);
		// drawDivision(args, Vec(104, 47), module->division);
	}
};


struct ManicCompressionWidget : ModuleWidget {
	SvgWidget* gainAdditionMeter;

	ManicCompressionWidget(ManicCompression *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ManicCompression.svg")));
		
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 14, 0)));	
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 14, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 14, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 14, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			gainAdditionMeter = new SvgWidget();
			gainAdditionMeter->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/GainAdditionMeter.svg")));
			gainAdditionMeter->box.pos = Vec(119, 23.5);
			gainAdditionMeter->visible = false;
			addChild(gainAdditionMeter);
		}


		{
			ManicCompressionDisplay *display = new ManicCompressionDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}



		ParamWidget* thresholdParam = createParam<RoundSmallFWKnob>(Vec(15, 140), module, ManicCompression::THRESHOLD_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(thresholdParam)->percentage = &module->thresholdPercentage;
		}
		addParam(thresholdParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(40, 158), module, ManicCompression::THRESHOLD_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(44, 143), module, ManicCompression::THRESHOLD_CV_INPUT));


		ParamWidget* ratioParam = createParam<RoundSmallFWKnob>(Vec(75, 140), module, ManicCompression::RATIO_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(ratioParam)->percentage = &module->ratioPercentage;
		}
		addParam(ratioParam);							
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


		ParamWidget* rmsWindowParam = createParam<RoundSmallFWKnob>(Vec(30, 195), module, ManicCompression::RMS_WINDOW_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(rmsWindowParam)->percentage = &module->rmsWindowPercentage;
		}
		addParam(rmsWindowParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(55, 213), module, ManicCompression::RMS_WINDOW_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(59, 198), module, ManicCompression::RMS_WINDOW_INPUT));

		ParamWidget* kneeParam = createParam<RoundSmallFWKnob>(Vec(82, 195), module, ManicCompression::KNEE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(kneeParam)->percentage = &module->kneePercentage;
		}
		addParam(kneeParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(107, 213), module, ManicCompression::KNEE_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(111, 198), module, ManicCompression::KNEE_CV_INPUT));


		ParamWidget* attackParam = createParam<RoundSmallFWKnob>(Vec(135, 140), module, ManicCompression::ATTACK_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(attackParam)->percentage = &module->attackPercentage;
		}
		addParam(attackParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(160, 158), module, ManicCompression::ATTACK_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(164, 143), module, ManicCompression::ATTACK_CV_INPUT));

		ParamWidget* releaseParam = createParam<RoundSmallFWKnob>(Vec(195, 140), module, ManicCompression::RELEASE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(releaseParam)->percentage = &module->releasePercentage;
		}
		addParam(releaseParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(220, 158), module, ManicCompression::RELEASE_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(224, 143), module, ManicCompression::RELEASE_CV_INPUT));


		ParamWidget* attackCurveParam = createParam<RoundSmallFWKnob>(Vec(135, 200), module, ManicCompression::ATTACK_CURVE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(attackCurveParam)->percentage = &module->attackCurvePercentage;
			dynamic_cast<RoundSmallFWKnob*>(attackCurveParam)->biDirectional = true;
		}
		addParam(attackCurveParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(160, 213), module, ManicCompression::ATTACK_CURVE_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(164, 198), module, ManicCompression::ATTACK_CURVE_CV_INPUT));

		ParamWidget* releaseCurveParam = createParam<RoundSmallFWKnob>(Vec(195, 200), module, ManicCompression::RELEASE_CURVE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(releaseCurveParam)->percentage = &module->releaseCurvePercentage;
			dynamic_cast<RoundSmallFWKnob*>(releaseCurveParam)->biDirectional = true;
		}
		addParam(releaseCurveParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(220, 213), module, ManicCompression::RELEASE_CURVE_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(224, 198), module, ManicCompression::RELEASE_CURVE_CV_INPUT));


		ParamWidget* inGainParam = createParam<RoundSmallFWKnob>(Vec(15, 280), module, ManicCompression::IN_GAIN_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(inGainParam)->percentage = &module->inGainPercentage;
		}
		addParam(inGainParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(40, 298), module, ManicCompression::IN_GAIN_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(44, 283), module, ManicCompression::IN_GAIN_INPUT));

		ParamWidget* makeupGainParam = createParam<RoundSmallFWKnob>(Vec(75, 280), module, ManicCompression::MAKEUP_GAIN_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(makeupGainParam)->percentage = &module->makeupGainPercentage;
		}
		addParam(makeupGainParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(100, 298), module, ManicCompression::MAKEUP_GAIN_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(104, 283), module, ManicCompression::MAKEUP_GAIN_INPUT));

		ParamWidget* mixParam = createParam<RoundSmallFWKnob>(Vec(135, 280), module, ManicCompression::MIX_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(mixParam)->percentage = &module->mixPercentage;
		}
		addParam(mixParam);							
		addParam(createParam<RoundReallySmallFWKnob>(Vec(160, 298), module, ManicCompression::MIX_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(164, 283), module, ManicCompression::MIX_CV_INPUT));

		addParam(createParam<LEDButton>(Vec(192, 304), module, ManicCompression::BYPASS_PARAM));
		addChild(createLight<LargeLight<RedLight>>(Vec(193.5, 305.5), module, ManicCompression::BYPASS_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(213, 307), module, ManicCompression::BYPASS_INPUT));

		
		addParam(createParam<LEDButton>(Vec(110, 247), module, ManicCompression::MS_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(111.5, 248.5), module, ManicCompression::MS_MODE_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(131, 250), module, ManicCompression::MS_MODE_INPUT));


		addParam(createParam<LEDButton>(Vec(157, 247), module, ManicCompression::COMPRESS_M_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(158.5, 248.5), module, ManicCompression::COMPRESS_MID_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(178, 250), module, ManicCompression::COMPRESS_M_INPUT));

		addParam(createParam<LEDButton>(Vec(201, 247), module, ManicCompression::COMPRESS_S_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(202.5, 248.5), module, ManicCompression::COMPRESS_SIDE_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(222, 250), module, ManicCompression::COMPRESS_S_INPUT));

		addParam(createParam<LEDButton>(Vec(192, 278), module, ManicCompression::COMPRESS_DIRECTION_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(193.5, 279.5), module, ManicCompression::COMPRESS_DIRECTION_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(213, 281), module, ManicCompression::COMPRESS_DIRECTION_INPUT));

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
			text = "CV Gate Mode";
			rightText = (module->gateMode) ? "✔" : "";
		}
	};

	
	
	void appendContextMenu(Menu* menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		ManicCompression *module = dynamic_cast<ManicCompression*>(this->module);
		assert(module);

		TriggerGateItem *triggerGateItem = new TriggerGateItem();
		triggerGateItem->module = module;
		menu->addChild(triggerGateItem);
		{
			OptionsMenuItem* mi = new OptionsMenuItem("Envelope Mode");
			mi->addItem(OptionMenuItem("Linear - Gain Reduced", [module]() { return module->envelopeMode == 0; }, [module]() { module->envelopeMode = 0; }));
			mi->addItem(OptionMenuItem("Linear", [module]() { return module->envelopeMode == 1; }, [module]() { module->envelopeMode = 1; }));
			mi->addItem(OptionMenuItem("Exponential", [module]() { return module->envelopeMode == 2; }, [module]() { module->envelopeMode = 2; }));
			//OptionsMenuItem::addToMenu(mi, menu);
			menu->addChild(mi);
		}
	}

	void step() override {
		if (module) {
			gainAdditionMeter->visible  = ((ManicCompression*)module)->compressDirection;
		}
		Widget::step();
	}
};

Model *modelManicCompression = createModel<ManicCompression, ManicCompressionWidget>("ManicCompression");
