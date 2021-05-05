#include "FrozenWasteland.hpp"
#include "dsp-compressor/SimpleComp.h"
#include "dsp-compressor/SimpleGain.h"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/menu.hpp"
#include "filters/biquad.h"

#define BANDS 5

struct ManicCompressionMB : Module {
	enum ParamIds {
        BAND_ACTIVE_PARAM,
        BAND_CUTOFF_PARAM = BAND_ACTIVE_PARAM + BANDS,
		BAND_CUTOFF_CV_ATTENUVERTER_PARAM = BAND_CUTOFF_PARAM + BANDS,
        BAND_WIDTH_PARAM = BAND_CUTOFF_CV_ATTENUVERTER_PARAM + BANDS,
		BAND_WIDTH_CV_ATTENUVERTER_PARAM = BAND_WIDTH_PARAM + BANDS,
		THRESHOLD_PARAM = BAND_WIDTH_CV_ATTENUVERTER_PARAM + BANDS,
		THRESHOLD_CV_ATTENUVERTER_PARAM = THRESHOLD_PARAM + BANDS,
		ATTACK_PARAM = THRESHOLD_CV_ATTENUVERTER_PARAM + BANDS,
		ATTACK_CV_ATTENUVERTER_PARAM = ATTACK_PARAM + BANDS,
		RELEASE_PARAM = ATTACK_CV_ATTENUVERTER_PARAM + BANDS,
		RELEASE_CV_ATTENUVERTER_PARAM = RELEASE_PARAM + BANDS,
		ATTACK_CURVE_PARAM = RELEASE_CV_ATTENUVERTER_PARAM + BANDS,
		ATTACK_CURVE_CV_ATTENUVERTER_PARAM = ATTACK_CURVE_PARAM + BANDS,
		RELEASE_CURVE_PARAM = ATTACK_CURVE_CV_ATTENUVERTER_PARAM + BANDS,
		RELEASE_CURVE_CV_ATTENUVERTER_PARAM = RELEASE_CURVE_PARAM + BANDS,
		RATIO_PARAM = RELEASE_CURVE_CV_ATTENUVERTER_PARAM + BANDS,
		RATIO_CV_ATTENUVERTER_PARAM = RATIO_PARAM + BANDS,
		KNEE_PARAM = RATIO_CV_ATTENUVERTER_PARAM + BANDS,
		KNEE_CV_ATTENUVERTER_PARAM = KNEE_PARAM + BANDS,
		MAKEUP_GAIN_PARAM = KNEE_CV_ATTENUVERTER_PARAM + BANDS,
		MAKEUP_GAIN_CV_ATTENUVERTER_PARAM = MAKEUP_GAIN_PARAM + BANDS,
		IN_GAIN_PARAM = MAKEUP_GAIN_CV_ATTENUVERTER_PARAM + BANDS,
		IN_GAIN_CV_ATTENUVERTER_PARAM = IN_GAIN_PARAM + BANDS,
		RMS_MODE_PARAM = IN_GAIN_CV_ATTENUVERTER_PARAM + BANDS,
		RMS_WINDOW_PARAM = RMS_MODE_PARAM + BANDS,
		RMS_WINDOW_CV_ATTENUVERTER_PARAM = RMS_WINDOW_PARAM + BANDS,
		// LP_FILTER_MODE_PARAM,
		// HP_FILTER_MODE_PARAM,
		MIX_PARAM = RMS_WINDOW_CV_ATTENUVERTER_PARAM + BANDS,
		MIX_CV_ATTENUVERTER_PARAM,
		BYPASS_PARAM,
		MS_MODE_PARAM,
		COMPRESS_M_PARAM,
		COMPRESS_S_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
        BAND_ACTIVE_INPUT,
        BAND_CUTOFF_INPUT = BAND_ACTIVE_INPUT + BANDS,
        BAND_WITDH_INPUT = BAND_CUTOFF_INPUT + BANDS,
		THRESHOLD_CV_INPUT = BAND_WITDH_INPUT + BANDS,
		ATTACK_CV_INPUT = THRESHOLD_CV_INPUT + BANDS,
		RELEASE_CV_INPUT = ATTACK_CV_INPUT + BANDS,
		ATTACK_CURVE_CV_INPUT = RELEASE_CV_INPUT + BANDS,
		RELEASE_CURVE_CV_INPUT = ATTACK_CURVE_CV_INPUT + BANDS,
		RATIO_CV_INPUT = RELEASE_CURVE_CV_INPUT + BANDS,
		KNEE_CV_INPUT = RATIO_CV_INPUT + BANDS,
		MAKEUP_GAIN_INPUT = KNEE_CV_INPUT + BANDS,
		IN_GAIN_INPUT = MAKEUP_GAIN_INPUT + BANDS,
		RMS_MODE_INPUT = IN_GAIN_INPUT + BANDS,
		RMS_WINDOW_INPUT = RMS_MODE_INPUT + BANDS,
        BAND_SIDECHAIN_INPUT = RMS_WINDOW_INPUT + BANDS,
		ENVELOPE_INPUT = BAND_SIDECHAIN_INPUT + BANDS,
		MIX_CV_INPUT = ENVELOPE_INPUT + BANDS,
		BYPASS_INPUT,
		SOURCE_L_INPUT,
		SOURCE_R_INPUT,
		SIDECHAIN_INPUT,
		// LP_FILTER_MODE_INPUT,
		// HP_FILTER_MODE_INPUT,
		MS_MODE_INPUT,
		COMPRESS_M_INPUT,
		COMPRESS_S_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_L,
		OUTPUT_R,
		ENVELOPE_OUT,
        BAND_OUTPUT_L = ENVELOPE_OUT + BANDS,
        BAND_OUTPUT_R = BAND_OUTPUT_L + BANDS,
        DETECTOR_OUTPUT = BAND_OUTPUT_R + BANDS,
		NUM_OUTPUTS = DETECTOR_OUTPUT + BANDS
	};
	enum LightIds {
        BAND_ACTIVE_LIGHT,
		RMS_MODE_LIGHT = BAND_ACTIVE_LIGHT + BANDS,
		// LP_FILTER_LIGHT,
		// HP_FILTER_LIGHT,
		BYPASS_LIGHT = RMS_MODE_LIGHT + BANDS,
		MS_MODE_LIGHT,
		COMPRESS_MID_LIGHT,
		COMPRESS_SIDE_LIGHT,
		NUM_LIGHTS
	};


	//Stuff for S&Hs
	dsp::SchmittTrigger bypassTrigger,	bypassInputTrigger,midSideModeTrigger,midSideModeInputTrigger,compressMidTrigger,compressMidInputTrigger,compressSideTrigger,compressSideInputTrigger, 
						rmsTrigger[BANDS], rmsInputTrigger[BANDS],bandEnabledTrigger[BANDS],bandEnabledInputTrigger[BANDS];
	

    double sampleRate;

    double lastCutoff[BANDS] = {0};
    double lastBandWidth[BANDS] = {0};

	chunkware_simple::SimpleComp compressor[BANDS];
	chunkware_simple::SimpleCompRms compressorRms[BANDS];
	float gainReduction[BANDS];
	double threshold[BANDS], ratio[BANDS], knee[BANDS];

	bool bypassed =  false;
	bool gateFlippedBypassed =  false;

    bool bandEnabled[BANDS] = {true};
    bool gateFlippedBandEnabled[BANDS]= {false};
	bool rmsMode[BANDS] = {false};
	bool gateFlippedRMS[BANDS] =  {false};

	bool midSideMode = false;
	bool gateFlippedMidSideMode =  false;
	bool compressMid = false;
	bool gateFlippedCompressMid =  false;
	bool compressSide = false;
	bool gateFlippedCompressSide =  false;

    int vuDisplayFadeTimer[BANDS] = {0};


	bool gateMode = false;
    int envelopeMode = 0;


	Biquad* lpFilterBank[6]; // 3 filters 2 for stereo inputs, one for sidechain x 2 to increase slope
	Biquad* hpFilterBank[6]; // 3 filters 2 for stereo inputs, one for sidechain x 2 to increase slope

	Biquad* bandFilterBank[BANDS * 2][2][3]; // 2 filters per band for increased slope (and phase correction), first index lowpass, 2nd highpass, 2nd index is channel [0] = L [1] = R, [2] = SC

    //percentages
	float bandFcPercentage[BANDS] = {0};
	float bandWidthPercentage[BANDS] = {0};
	float thresholdPercentage[BANDS] = {0};
	float ratioPercentage[BANDS] = {0};
	float kneePercentage[BANDS] = {0};
	float rmsWindowPercentage[BANDS] = {0};
	float attackPercentage[BANDS] = {0};
	float releasePercentage[BANDS] = {0};
	float attackCurvePercentage[BANDS] = {0};
	float releaseCurvePercentage[BANDS] = {0};
	float inGainPercentage[BANDS] = {0};
	float makeupGainPercentage[BANDS] = {0};
	float mixPercentage[BANDS] = {0};

	ManicCompressionMB() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        sampleRate = APP->engine->getSampleRate();

        for(int i=0;i<BANDS;i++) {
    		configParam(BAND_CUTOFF_PARAM + i, 0.f, 1.1f, 0.5f, "Band" + std::to_string(i+1) + " Frequency", " Hz", std::pow(2, 10.f), dsp::FREQ_C4 / std::pow(2, 5.f));
    		configParam(BAND_WIDTH_PARAM + i, 0.0f, 1.f, 0.0f, "Band" + std::to_string(i+1) + " Bandwidth"," %",0,100);

            configParam(THRESHOLD_PARAM + i, -50.0, 0.0, -30.0,"Band" + std::to_string(i+1) + " Threshold", " db");
            configParam(RATIO_PARAM + i, -0.2, 1.0, 0.1,"Band" + std::to_string(i+1) + " Ratio","",20,1.0);	
            configParam(ATTACK_PARAM + i, 0.0, 1.0, 0.1,"Band" + std::to_string(i+1) + " Attack", " ms",13.4975,8.0,-7.98);
            configParam(RELEASE_PARAM + i, 0.0, 1.0, 0.1,"Band" + std::to_string(i+1) + " Release"," ms",629.0,8.0,32.0);	
            configParam(ATTACK_CURVE_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Attack Curve");
            configParam(RELEASE_CURVE_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Release Curve");	
            configParam(KNEE_PARAM + i, 0.0, 10.0, 0.0,"Band" + std::to_string(i+1) + " Knee", " db");	
            configParam(RMS_WINDOW_PARAM + i, 0.02, 50.0, 5.0,"Band" + std::to_string(i+1) + " RMS Window", " ms");
            configParam(IN_GAIN_PARAM + i, 0.0, 30.0, 0.0,"Band" + std::to_string(i+1) + " Input Gain", " db");
            configParam(MAKEUP_GAIN_PARAM + i, 0.0, 30.0, 0.0,"Band" + std::to_string(i+1) + " Makeup Gain", " db");
            

            configParam(BAND_CUTOFF_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Frequency CV Attenuation","%",0,100);
            configParam(BAND_WIDTH_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Bandwidth CV Attenuation","%",0,100);
            configParam(THRESHOLD_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Threshold CV Attenuation","%",0,100);
            configParam(RATIO_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Ratio CV Attenuation","%",0,100);	
            configParam(ATTACK_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Attack CV Attenuation","%",0,100);
            configParam(RELEASE_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Release CV Attenuation","%",0,100);	
            configParam(ATTACK_CURVE_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Attack Curve CV Attenuation","%",0,100);
            configParam(RELEASE_CURVE_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Release Curve CV Attenuation","%",0,100);	
            configParam(KNEE_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Knee CV Attenuation","%",0,100);	
            configParam(RMS_WINDOW_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " RMS Windows CV Attenuation","%",0,100);	
            configParam(IN_GAIN_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Input Gain CV Attenuation","%",0,100);
            configParam(MAKEUP_GAIN_CV_ATTENUVERTER_PARAM + i, -1.0, 1.0, 0.0,"Band" + std::to_string(i+1) + " Makeup Gain CV Attenuation","%",0,100);

            double defaultCutoff = 0.25;
            for(int c = 0;c<3;c++) {
                double defaultLPCutoff = i == BANDS-1 ? 0.5 : defaultCutoff;    
                double defaultHPCutoff = i == 0 ? 1.0 / sampleRate : defaultCutoff;    

                bandFilterBank[i * 2][0][c] = new Biquad(bq_type_lowpass, defaultLPCutoff , 0.707, 0);
                bandFilterBank[i * 2 + 1][0][c] = new Biquad(bq_type_lowpass, defaultLPCutoff , 0.707, 0);
                bandFilterBank[i * 2][1][c] = new Biquad(bq_type_highpass, defaultHPCutoff , 0.707, 0);
                bandFilterBank[i * 2 + 1][1][c] = new Biquad(bq_type_highpass, defaultHPCutoff , 0.707, 0);
            }

        }
		
        configParam(MIX_PARAM, 0.0, 1.0, 1.0,"Mix","%",0,100);	
        configParam(MIX_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Mix CV Attenuation","%",0,100);	


		double lpCutoff = 4000/sampleRate;
		double hpCutoff = 240/sampleRate;

		for(int f=0;f<6;f++) {
			lpFilterBank[f] = new Biquad(bq_type_lowpass, lpCutoff , 0.707, 0);
			hpFilterBank[f] = new Biquad(bq_type_highpass, hpCutoff , 0.707, 0);
		}

        for(int b=0;b<BANDS;b++) {
            compressor[b].initRuntime();
            compressorRms[b].initRuntime();
        }

	}
	void process(const ProcessArgs &args) override;
    void dataFromJson(json_t *) override;
	void onSampleRateChange() override;
    double lerp(double v0, double v1, double t);
    double sgn(double val);
    json_t *dataToJson() override;

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

double ManicCompressionMB::lerp(double v0, double v1, double t) {
		return (1 - t) * v0 + t * v1;
}

double ManicCompressionMB::sgn(double val) {
    return (double(0) < val) - (val < double(0));
}	

json_t *ManicCompressionMB::dataToJson() {
	json_t *rootJ = json_object();
	
	json_object_set_new(rootJ, "bypassed", json_boolean(bypassed));
	json_object_set_new(rootJ, "midSideMode", json_boolean(midSideMode));
	json_object_set_new(rootJ, "compressMid", json_boolean(compressMid));
	json_object_set_new(rootJ, "compressSide", json_boolean(compressSide));
	json_object_set_new(rootJ, "gateMode", json_boolean(gateMode));
    json_object_set_new(rootJ, "envelopeMode", json_integer(envelopeMode));

    for(int i=0;i<BANDS;i++) {
        std::string buf = "bandEnabled-" + std::to_string(i) ;
    	json_object_set_new(rootJ, buf.c_str(), json_boolean(bandEnabled[i]));
    
        buf = "rmsMode-" + std::to_string(i) ;
    	json_object_set_new(rootJ, buf.c_str(), json_boolean(rmsMode[i]));
    }



	return rootJ;
}

void ManicCompressionMB::dataFromJson(json_t *rootJ) {

	json_t *bpJ = json_object_get(rootJ, "bypassed");
	if (bpJ)
		bypassed = json_boolean_value(bpJ);

	json_t *msmJ = json_object_get(rootJ, "midSideMode");
	if (msmJ)
		midSideMode = json_boolean_value(msmJ);

	json_t *cmJ = json_object_get(rootJ, "compressMid");
	if (cmJ)
		compressMid = json_boolean_value(cmJ);

	json_t *csJ = json_object_get(rootJ, "compressSide");
	if (csJ)
		compressSide = json_boolean_value(csJ);


	json_t *gmJ = json_object_get(rootJ, "gateMode");
	if (gmJ)
		gateMode = json_boolean_value(gmJ);

    json_t *emJ = json_object_get(rootJ, "envelopeMode");
	if (emJ)
		envelopeMode = json_integer_value(emJ);

    for(int i=0;i<BANDS;i++) {
        std::string buf = "bandEnabled-" + std::to_string(i) ;
        json_t *beJ = json_object_get(rootJ, buf.c_str());
        if (beJ) {
        	bandEnabled[i] = json_boolean_value(beJ);
        }

        buf = "rmsMode-" + std::to_string(i) ;
        json_t *rmsJ = json_object_get(rootJ, buf.c_str());
        if (rmsJ) {
        	rmsMode[i] = json_boolean_value(rmsJ);
        }
    }


}

void ManicCompressionMB::onSampleRateChange() {
	sampleRate = APP->engine->getSampleRate();
	double lpCutoff = 4000/sampleRate;
	double hpCutoff = 240/sampleRate;

	for(int f=0;f<6;f++) {
		lpFilterBank[f]->setFc(lpCutoff);
		hpFilterBank[f]->setFc(hpCutoff);
	}

    for(int c = 0;c<3;c++) {
        double defaultLPCutoff = 0.5;    
        double defaultHPCutoff = 1.0 / sampleRate;    

        bandFilterBank[(BANDS-1) * 2][0][c]->setFc(defaultLPCutoff);
        bandFilterBank[(BANDS-1) * 2 + 1][0][c]->setFc(defaultLPCutoff);
        bandFilterBank[0][1][c]->setFc(defaultHPCutoff);
        bandFilterBank[1][1][c]->setFc(defaultHPCutoff);
    }

    for(int b=0;b<BANDS;b++) { //Force bands to recalc
        lastCutoff[b] = -1;
    }
}

void ManicCompressionMB::process(const ProcessArgs &args) {

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

    for(int b=0;b<BANDS;b++) {
        compressor[b].setSampleRate(sampleRate);
        compressorRms[b].setSampleRate(sampleRate);


        double freqParam = clamp(params[BAND_CUTOFF_PARAM + b].getValue() + (inputs[BAND_CUTOFF_INPUT + b].getVoltage() * 0.1 * params[BAND_CUTOFF_CV_ATTENUVERTER_PARAM + b].getValue()),0.0f,1.1f);
        bandFcPercentage[b] = freqParam / 1.1;
		freqParam = freqParam * 10.f - 5.f;

        double bandwidthParam = clamp(params[BAND_WIDTH_PARAM + b].getValue() + (inputs[BAND_WITDH_INPUT + b].getVoltage() * 0.1 * params[BAND_WIDTH_CV_ATTENUVERTER_PARAM + b].getValue()),0.0f,1.f);
        bandWidthPercentage[b] = bandwidthParam;
		
        if(freqParam != lastCutoff[b] || bandwidthParam != lastBandWidth[b] ) {
            double cutoff = dsp::FREQ_C4 * pow(2.f, freqParam);

            double lpCutoff = clamp(cutoff + (cutoff / 1.0 * bandwidthParam),1.0f,20000.0f);
            double hpCutoff = clamp(cutoff - (cutoff / 1.0 * bandwidthParam),1.0f,20000.0f);

            for(int c=0;c<3;c++) {
                if(b < BANDS-1) {
                    bandFilterBank[b*2][0][c]->setFc(lpCutoff / sampleRate);
                    bandFilterBank[b*2 + 1][0][c]->setFc(lpCutoff / sampleRate);
                }
                if(b > 0) {
                    bandFilterBank[b*2][1][c]->setFc(hpCutoff / sampleRate);
                    bandFilterBank[b*2 + 1][1][c]->setFc(hpCutoff / sampleRate);
                }
            }
            // fprintf(stderr, "Recalculating Band:%i lFc:%f  fc:%f   lbw:%f   bw:%f  \n",b,freqParam,lastCutoff[b],bandwidthParam,lastBandWidth[b] );
            lastCutoff[b] = freqParam;
            lastBandWidth[b] = bandwidthParam;

        }


        float enableInput = inputs[BAND_ACTIVE_INPUT + b].getVoltage();
        if(gateMode) {
            if(bandEnabled[b] != (enableInput !=0) && gateFlippedBandEnabled[b] ) {
                bandEnabled[b] = enableInput != 0;
                gateFlippedBandEnabled[b] = true;
                if(!bandEnabled[b]) 
                    vuDisplayFadeTimer[b] = sampleRate;

            } else if (bandEnabled[b] == (enableInput !=0)) {
                gateFlippedBandEnabled[b] = true;
                if(!bandEnabled[b]) 
                    vuDisplayFadeTimer[b] = sampleRate;
            }
        } else {
            if(bandEnabledInputTrigger[b].process(enableInput)) {
                bandEnabled[b] = !bandEnabled[b];
                gateFlippedBandEnabled[b]= false;
                if(!bandEnabled[b]) 
                    vuDisplayFadeTimer[b] = sampleRate;
            }
        }
        if(bandEnabledTrigger[b].process(params[BAND_ACTIVE_PARAM + b].getValue())) {
            bandEnabled[b] = !bandEnabled[b];
            gateFlippedBandEnabled[b]= false;
            if(!bandEnabled[b]) 
                vuDisplayFadeTimer[b] = sampleRate;
        }
        lights[BAND_ACTIVE_LIGHT + b].value = bandEnabled[b] ? 1.0 : 0.0;
        if(vuDisplayFadeTimer[b] > 0 )
            vuDisplayFadeTimer[b]--;

        float rmsInput = inputs[RMS_MODE_INPUT + b].getVoltage();
        if(gateMode) {
            if(rmsMode[b] != (rmsInput !=0) && gateFlippedRMS[b] ) {
                rmsMode[b] = rmsInput != 0;
                gateFlippedRMS[b] = true;
            } else if (rmsMode[b] == (rmsInput !=0)) {
                gateFlippedRMS[b] = true;
            }
        } else {
            if(rmsInputTrigger[b].process(rmsInput)) {
                rmsMode[b] = !rmsMode[b];
                gateFlippedRMS[b]= false;
            }
        }
        if(rmsTrigger[b].process(params[RMS_MODE_PARAM + b].getValue())) {
            rmsMode[b] = !rmsMode[b];
            gateFlippedRMS[b]= false;
        }
        lights[RMS_MODE_LIGHT + b].value = rmsMode[b] ? 1.0 : 0.0;
    }

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


	double mix = clamp(params[MIX_PARAM].getValue() + (inputs[MIX_CV_INPUT].getVoltage() * 0.1 * params[MIX_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);


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

    double sidechain = 0;
    bool usingSidechain = false;
    if(inputs[SIDECHAIN_INPUT].isConnected()) {
        sidechain = inputs[SIDECHAIN_INPUT].getVoltage();
        usingSidechain = true;
    }

    double bandTotalL = 0;
    double bandTotalR = 0; 
    for(int b=0;b<BANDS;b++) {        
        if(bandEnabled[b]) {
            double processedBandL = bandFilterBank[b*2][1][0]->process(bandFilterBank[b*2+1][1][0]->process(bandFilterBank[b*2][0][0]->process(bandFilterBank[b*2+1][0][0]->process(processedL))));
            double processedBandR = bandFilterBank[b*2][1][1]->process(bandFilterBank[b*2+1][1][1]->process(bandFilterBank[b*2][0][1]->process(bandFilterBank[b*2+1][0][1]->process(processedR))));
            double sidechainBand = 0;
            if(usingSidechain) {
                sidechainBand = bandFilterBank[b*2][1][2]->process(bandFilterBank[b*2+1][1][2]->process(bandFilterBank[b*2][0][2]->process(bandFilterBank[b*2+1][0][2]->process(sidechain))));
            }

            float paramRatio = clamp(params[RATIO_PARAM + b].getValue() + (inputs[RATIO_CV_INPUT + b].getVoltage() * 0.1 * params[RATIO_CV_ATTENUVERTER_PARAM + b].getValue()),-0.2f,1.0f);
            ratioPercentage[b] = (paramRatio+0.2) / 1.2f;
            ratio[b] = pow(20.0,paramRatio);
            threshold[b] = clamp(params[THRESHOLD_PARAM + b].getValue() + (inputs[THRESHOLD_CV_INPUT + b].getVoltage() * 3.0 * params[THRESHOLD_CV_ATTENUVERTER_PARAM + b].getValue()),-50.0f,0.0f);
        	thresholdPercentage[b] = (threshold[b] + 50.0) / 50.0;
            float paramAttack = clamp(params[ATTACK_PARAM + b].getValue() + (inputs[ATTACK_CV_INPUT + b].getVoltage() * 10.0 * params[ATTACK_CV_ATTENUVERTER_PARAM + b].getValue()),0.0f,1.0f);
        	attackPercentage[b] = paramAttack;
            double attack = pow(13.4975,paramAttack) * 8.0 - 7.98;
            float paramRelease = clamp(params[RELEASE_PARAM + b].getValue() + (inputs[RELEASE_CV_INPUT + b].getVoltage() * 500.0 * params[RELEASE_CV_ATTENUVERTER_PARAM + b].getValue()),0.0f,1.0f);
        	releasePercentage[b] = paramRelease;
            double release = pow(629.0,paramRelease) * 8.0 + 32.0;

            double rmsWindow = clamp(params[RMS_WINDOW_PARAM + b].getValue() + (inputs[RMS_WINDOW_INPUT + b].getVoltage() * 500.0 * params[RMS_WINDOW_CV_ATTENUVERTER_PARAM + b].getValue()),0.02f,50.0f);
        	rmsWindowPercentage[b] = rmsWindow / 50.0;

            double attackCurve = clamp(params[ATTACK_CURVE_PARAM + b].getValue() + (inputs[ATTACK_CV_INPUT + b].getVoltage() * 0.1 * params[ATTACK_CURVE_CV_ATTENUVERTER_PARAM + b].getValue()),-1.0f,1.0f);
        	attackCurvePercentage[b] = attackCurve;
            double releaseCurve = clamp(params[RELEASE_CURVE_PARAM + b].getValue() + (inputs[RELEASE_CV_INPUT + b].getVoltage() * 0.1 * params[RELEASE_CV_ATTENUVERTER_PARAM + b].getValue()),-1.0f,1.0f);
        	releaseCurvePercentage[b] = releaseCurve;

            knee[b] = clamp(params[KNEE_PARAM + b].getValue() + (inputs[KNEE_CV_INPUT + b].getVoltage() * params[KNEE_CV_ATTENUVERTER_PARAM + b].getValue()),0.0f,10.0f);
        	kneePercentage[b] = knee[b] / 10.0;

            double inputGain = clamp(params[IN_GAIN_PARAM + b].getValue() + (inputs[IN_GAIN_INPUT + b].getVoltage() * 3.0 * params[IN_GAIN_CV_ATTENUVERTER_PARAM + b].getValue()), 0.0f,30.0f);
        	inGainPercentage[b] = inputGain / 30.0;
            inputGain = chunkware_simple::dB2lin(inputGain);
            double makeupGain = clamp(params[MAKEUP_GAIN_PARAM + b].getValue() + (inputs[MAKEUP_GAIN_INPUT + b].getVoltage() * 3.0 * params[MAKEUP_GAIN_CV_ATTENUVERTER_PARAM + b].getValue()), 0.0f,30.0f);
            makeupGainPercentage[b] = makeupGain / 30.0;

            double detectorInput;
            double calculatedGainReduction = 0;
            if(rmsMode[b]) {
                compressorRms[b].setRatio(ratio[b]);
                compressorRms[b].setThresh(50.0+threshold[b]);
                compressorRms[b].setKnee(knee[b]);
                compressorRms[b].setAttack(attack);
                compressorRms[b].setRelease(release);
                compressorRms[b].setAttackCurve(attackCurve);
                compressorRms[b].setReleaseCurve(releaseCurve);
                compressorRms[b].setWindow(rmsWindow);

                if(usingSidechain) {
                    detectorInput = sidechainBand * sidechainBand;
                } else {
                    double inSq1 = processedBandL * processedBandL;	// square input
                    double inSq2 = processedBandR * processedBandR;
                    double sum = inSq1 + inSq2;			// power summing
                    detectorInput = sum * inputGain;
                }
                outputs[DETECTOR_OUTPUT + b].setVoltage(detectorInput);
                compressorRms[b].process(inputs[BAND_SIDECHAIN_INPUT + b].isConnected() ? inputs[BAND_SIDECHAIN_INPUT + b].getVoltage() : detectorInput);
                calculatedGainReduction = compressorRms[b].getGainReduction();
            } else {
                compressor[b].setRatio(ratio[b]);
                compressor[b].setThresh(50.0+threshold[b]);
                compressor[b].setKnee(knee[b]);
                compressor[b].setAttack(attack);
                compressor[b].setRelease(release);
                compressor[b].setAttackCurve(attackCurve);
                compressor[b].setReleaseCurve(releaseCurve);
                
                if(usingSidechain) {
                    detectorInput = sidechainBand;
                } else {
                    double rect1 = fabs( processedBandL );	// rectify input
                    double rect2 = fabs( processedBandR );
                    double link = std::max( rect1, rect2 );	// link channels with greater of 2
                    detectorInput =  link * inputGain;
                }
                outputs[DETECTOR_OUTPUT + b].setVoltage(detectorInput);
                compressor[b].process(inputs[BAND_SIDECHAIN_INPUT + b].isConnected() ? inputs[BAND_SIDECHAIN_INPUT + b].getVoltage() : detectorInput);
                calculatedGainReduction = compressor[b].getGainReduction();
            }		
            switch(envelopeMode) {
                case 0 : // original gain reduced linear
                    outputs[ENVELOPE_OUT+b].setVoltage(clamp(chunkware_simple::dB2lin(calculatedGainReduction) / 3.0f,-10.0f,10.0f));
                    break;
                case 1 : // linear
                    outputs[ENVELOPE_OUT+b].setVoltage(chunkware_simple::dB2lin(calculatedGainReduction));
                    break;
                case 2 : // exponential
                    outputs[ENVELOPE_OUT+b].setVoltage(calculatedGainReduction);
                    break;
            }

            if(!inputs[ENVELOPE_INPUT+b].isConnected()) {
                gainReduction[b] = calculatedGainReduction;
            } else {
                switch(envelopeMode) {
                    case 0 : // original gain reduced linear
                        gainReduction[b] = chunkware_simple::lin2dB(inputs[ENVELOPE_INPUT+b].getVoltage() * 3.0);
                        break;
                    case 1 : // linear
                        gainReduction[b] = chunkware_simple::lin2dB(inputs[ENVELOPE_INPUT+b].getVoltage());
                        break;
                    case 2 : // exponential
                        gainReduction[b] = inputs[ENVELOPE_INPUT+b].getVoltage();
                        break;
                }                
            }
            
            double finalGainLin = chunkware_simple::dB2lin(makeupGain-gainReduction[b]);

            double bandOutputL, bandOutputR;
            if(!midSideMode) {
                bandOutputL = processedBandL * finalGainLin; 
                bandOutputR = processedBandR * finalGainLin;
            } else {
                bandOutputL =  compressMid ? processedBandL * finalGainLin : processedBandL;
                bandOutputR =  compressSide ? processedBandR * finalGainLin : processedBandR;			
            }
            outputs[BAND_OUTPUT_L + b].setVoltage(bandOutputL); 
            outputs[BAND_OUTPUT_R + b].setVoltage(bandOutputR); 
            bandTotalL += bandOutputL; 
            bandTotalR += bandOutputR;

        } else {
            ratioPercentage[b] = 0;
            thresholdPercentage[b] = 0;
            attackPercentage[b] = 0;
            attackCurvePercentage[b] = 0;
            releaseCurvePercentage[b] = 0;
            releasePercentage[b] = 0;
            rmsWindowPercentage[b] = 0;
            inGainPercentage[b] = 0;
            makeupGainPercentage[b] = 0;
        }
    }

	double outputL;
	double outputR;

	if(!bypassed) {
		
		double processedOutputL;
		double processedOutputR;
		if(!midSideMode) {
			processedOutputL = bandTotalL; 
			processedOutputR = bandTotalR;
		} else {
			//Decode
			processedOutputL = bandTotalL + bandTotalR;
			processedOutputR = bandTotalL - bandTotalR;			
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


struct MCDisplayFiltersResponse : FramebufferWidget {
  ManicCompressionMB *module;

  MCDisplayFiltersResponse() {
  }


  void draw(const DrawArgs &args) override {
    //background
    nvgFillColor(args.vg, nvgRGB(20, 30, 33));
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgStrokeColor(args.vg, nvgRGB(0x50,0x50,0x50));
    nvgStrokeWidth(args.vg, 0.5);
    for(int i=1;i<10;i+=1) {
        float xPosition = (std::log10(i) * 95.685);
        nvgMoveTo(args.vg,xPosition,0.5);
        nvgLineTo(args.vg,xPosition,100.5);
    }
    for(int i=10;i<100;i+=10) {
        float xPosition = (std::log10(i) * 95.685);
        nvgMoveTo(args.vg,xPosition,0.5);
        nvgLineTo(args.vg,xPosition,100.5);
    }
    float xPosition = (std::log10(150) * 95.685);
    nvgMoveTo(args.vg,xPosition,0.5);
    nvgLineTo(args.vg,xPosition,100.5);
    nvgStroke(args.vg);

    for(int i=1;i<6;i++) {
      nvgBeginPath(args.vg);
      nvgStrokeColor(args.vg, nvgRGB(0x50,0x50,0x50));
      nvgMoveTo(args.vg,0.5,i*16);
      nvgLineTo(args.vg,224.5,i*16);
      nvgStroke(args.vg);
    }

    if (!module) 
      return;

        //fprintf(stderr, "Point x:%i l:%i freq:%f response:%f  gain: %f  \n",x,l,frequency,response,levelResponse);

    for(int b=0;b<BANDS;b++) {

      if(module->bandEnabled[b]) {
        float r = 136 + b * 38;
        float g = 176 + b * 19;
        float bl = 255 - b * 19;

            // fprintf(stderr, "drive level: %f  r:%f g:%f b:%f \n",driveLevel,r,g,b);

        nvgBeginPath(args.vg);  
        nvgStrokeColor(args.vg, nvgRGB(r,g,bl));
        // nvgFillColor(args.vg, nvgRGBA(r,g,bl,40));

        nvgStrokeWidth(args.vg, 0.5);
        float maxY = 0;
        for(float x=0.0f; x<224.0f; x+=1.0f) {
            double frequency = std::pow(10.0f, x/95.685 + 2.0f) / module->sampleRate;
            double responseLP = module->bandFilterBank[b*2][0][0]->frequencyResponse(frequency); 
            double responseHP = module->bandFilterBank[b*2][1][0]->frequencyResponse(frequency); 
            // fprintf(stderr, "Point x:%i l:%i freq:%f response:%f  level response: %f  \n",x,l,frequency[0],response[0],levelResponse[0]);
                double responseDB = std::log10(std::max(responseLP * responseLP * responseHP * responseHP, 1.0e-4)) * 20;
                float responseYCoord = clamp(0.0f - (float) responseDB * 1.25, 0.0f, 100.0f);
                if(responseYCoord > maxY)
                    maxY = responseYCoord;
                // fprintf(stderr, "Point x:%i response:%f  \n",x,response[0]);            
                if(x < 1)
                nvgMoveTo(args.vg, 0.0f, responseYCoord);
                else  
                nvgLineTo(args.vg, x, responseYCoord);
        }
        //nvgStroke(args.vg);
        nvgLineTo(args.vg, 224, 100);
        nvgLineTo(args.vg, 0, 100);

        NVGpaint paint = nvgLinearGradient(args.vg,0,0,0,maxY,nvgRGBA(r,g,bl,140), nvgRGBA(r,g,bl,0));
        nvgFillPaint(args.vg, paint);

        nvgFill(args.vg);
      }
    }
  }
};  

struct ManicCompressionMBCompressionCurve : FramebufferWidget {
	ManicCompressionMB *module;
	int band = 0;
	std::shared_ptr<Font> font;


	ManicCompressionMBCompressionCurve() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
	}


	void drawResponse(const DrawArgs &args, int b, double threshold, double ratio, double knee) 
	{
		nvgStrokeColor(args.vg, module->inputs[module->ENVELOPE_INPUT+b].isConnected() ? nvgRGBA(0xe0, 0x80, 0x10, 0xcf) : nvgRGBA(0xe0, 0xe0, 0x10, 0xff));
		nvgStrokeWidth(args.vg, 2.0);
		nvgBeginPath(args.vg);

		threshold = 50.0 + threshold; // reverse it

		double kneeThreshold = threshold - (knee / 2.0);

		double lx = 1+(kneeThreshold*1.8);
		double ly = 99-(kneeThreshold*1.8);

		nvgMoveTo(args.vg,1+ (b*30),99);
		nvgLineTo(args.vg,lx+(b*30),ly);
		for(int kx=0;kx<knee*1.8;kx++) {
			double kxdB = kx/1.8 - knee/2.0;	
			double a = kxdB + knee/2.0;
			double gain = (((1/ratio)-1.0) * a * a) / (2 * knee) + kxdB + threshold ; 

			// fprintf(stderr, "ratio: %f  knee: %f   db: %f  a: %f, ar: %f\n", ratio,knee,kx/3.0,a,gain);
			double ky = gain*1.8;
			// ly = oly-ky;
			nvgLineTo(args.vg,kx+lx+(b*30),99.0-ky);
		}
        double endPoint = lx+(knee*1.8);
		double finalGain = threshold + (50.0-threshold) / ratio;
		double ey = std::min(finalGain * 1.8,99.0);
        double ex = std::min((99.0-endPoint) * ratio,99-endPoint) + endPoint + (b*30);
			// fprintf(stderr, "ratio: %f  fg: %f   ey: %f \n", ratio,finalGain,ey);
		nvgLineTo(args.vg,ex,99.0-ey);
		nvgStroke(args.vg);
	}

	

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
        for(int band=0;band<BANDS;band++) {
            if(module->bandEnabled[band]) {
                drawResponse(args,band,module->threshold[band],module->ratio[band],module->knee[band]);
            }
        }		
	}
};

struct ManicCompressionMBDisplay : FramebufferWidget {
	ManicCompressionMB *module;
	int band = 0;
	std::shared_ptr<Font> font;


	ManicCompressionMBDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
	}

    void drawMeterOffBox(const DrawArgs &args, Vec pos, int fadeLevel) 
	{
        int scaler = module->sampleRate / 150;
        //darken meter
        nvgFillColor(args.vg, nvgRGBA(20, 30, 33,150-(fadeLevel/scaler)));
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 125, 16.5, 119, 99);
        nvgFill(args.vg);
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
        if(!module->bandEnabled[band]) {
            drawMeterOffBox(args, Vec(118, 37), module->vuDisplayFadeTimer[band]);
        }
		drawGainReduction(args, Vec(118, 37), module->bandEnabled[band] ? module->gainReduction[band] : 0.0);
		
	}
};


struct ManicCompressionMBWidget : ModuleWidget {
	ManicCompressionMBWidget(ManicCompressionMB *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ManicCompressionMB.svg")));
		
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 14, 0)));	
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 14, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 14, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 14, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        {
            MCDisplayFiltersResponse *dfr = new MCDisplayFiltersResponse();    
            //if (module) {
                dfr->module = module;
            //}   
            dfr->box.pos = Vec(20.5, 23.5);
            dfr->box.size = Vec(224, 100);
            addChild(dfr);
        }

        {
            ManicCompressionMBCompressionCurve *dcc = new ManicCompressionMBCompressionCurve();    
            //if (module) {
                dcc->module = module;
            //}   
            dcc->box.pos = Vec(20.5, 23.5);
            dcc->box.size = Vec(224, 100);
            addChild(dcc);
        }


        for(int b=0;b<BANDS;b++) {
            {
                ManicCompressionMBDisplay *display = new ManicCompressionMBDisplay();
                display->module = module;
                display->band = b;
                display->box.pos = Vec(140+ (b * 140), 0);
                display->box.size = Vec(box.size.x, box.size.y);
                addChild(display);
            }

            addParam(createParam<LEDButton>(Vec(15 + (b * 50), 140), module, ManicCompressionMB::BAND_ACTIVE_PARAM + b));
            addChild(createLight<LargeLight<GreenLight>>(Vec(16.5  + (b * 50), 141.5), module, ManicCompressionMB::BAND_ACTIVE_LIGHT + b));
    		addInput(createInput<FWPortInReallySmall>(Vec(38 + (b * 50), 143), module, ManicCompressionMB::BAND_ACTIVE_INPUT + b));

            ParamWidget* bandCutoffParam = createParam<RoundSmallFWKnob>(Vec(10 + b*50, 170), module, ManicCompressionMB::BAND_CUTOFF_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(bandCutoffParam)->percentage = &module->bandFcPercentage[b];
            }
            addParam(bandCutoffParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(34 + b*50, 188), module, ManicCompressionMB::BAND_CUTOFF_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(38 + b*50, 173), module, ManicCompressionMB::BAND_CUTOFF_INPUT + b));

            if(b > 0 && b < BANDS-1) { // only middle bands have width
                ParamWidget* bandWidthParam = createParam<RoundSmallFWKnob>(Vec(10 + b*50, 225), module, ManicCompressionMB::BAND_WIDTH_PARAM + b);
                if (module) {
                    dynamic_cast<RoundSmallFWKnob*>(bandWidthParam)->percentage = &module->bandWidthPercentage[b];
                }
                addParam(bandWidthParam);							
                addParam(createParam<RoundReallySmallFWKnob>(Vec(34 + b*50, 243), module, ManicCompressionMB::BAND_WIDTH_CV_ATTENUVERTER_PARAM + b));
                addInput(createInput<FWPortInReallySmall>(Vec(38 + b*50, 228), module, ManicCompressionMB::BAND_WITDH_INPUT + b));
            }

            ParamWidget* thresholdParam = createParam<RoundSmallFWKnob>(Vec(15 + 255 + (b * 140), 135), module, ManicCompressionMB::THRESHOLD_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(thresholdParam)->percentage = &module->thresholdPercentage[b];
            }
            addParam(thresholdParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(40 + 255 + (b * 140), 153), module, ManicCompressionMB::THRESHOLD_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(44 + 255 + (b * 140), 138), module, ManicCompressionMB::THRESHOLD_CV_INPUT + b));


            ParamWidget* ratioParam = createParam<RoundSmallFWKnob>(Vec(75 + 255 + (b * 140), 135), module, ManicCompressionMB::RATIO_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(ratioParam)->percentage = &module->ratioPercentage[b];
            }
            addParam(ratioParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(100 + 255 + (b * 140), 153), module, ManicCompressionMB::RATIO_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(104 + 255 + (b * 140), 138), module, ManicCompressionMB::RATIO_CV_INPUT + b));


            addParam(createParam<LEDButton>(Vec(10 + 255 + (b * 140), 185), module, ManicCompressionMB::RMS_MODE_PARAM + b));
            addChild(createLight<LargeLight<BlueLight>>(Vec(11.5 + 255 + (b * 140), 186.5), module, ManicCompressionMB::RMS_MODE_LIGHT + b));
            addInput(createInput<FWPortInReallySmall>(Vec(13 + 255 + (b * 140), 206), module, ManicCompressionMB::RMS_MODE_INPUT + b));

            ParamWidget* rmsWindowParam = createParam<RoundSmallFWKnob>(Vec(30 + 255 + (b * 140), 185), module, ManicCompressionMB::RMS_WINDOW_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(rmsWindowParam)->percentage = &module->rmsWindowPercentage[b];
            }
            addParam(rmsWindowParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(55 + 255 + (b * 140), 203), module, ManicCompressionMB::RMS_WINDOW_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(59 + 255 + (b * 140), 188), module, ManicCompressionMB::RMS_WINDOW_INPUT + b));

            ParamWidget* kneeParam = createParam<RoundSmallFWKnob>(Vec(82 + 255 + (b * 140), 185), module, ManicCompressionMB::KNEE_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(kneeParam)->percentage = &module->kneePercentage[b];
            }
            addParam(kneeParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(107 + 255 + (b * 140), 203), module, ManicCompressionMB::KNEE_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(111 + 255 + (b * 140), 188), module, ManicCompressionMB::KNEE_CV_INPUT + b));


            ParamWidget* attackParam = createParam<RoundSmallFWKnob>(Vec(15 + 255 + (b * 140), 235), module, ManicCompressionMB::ATTACK_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(attackParam)->percentage = &module->attackPercentage[b];
            }
            addParam(attackParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(40 + 255 + (b * 140), 253), module, ManicCompressionMB::ATTACK_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(44 + 255 + (b * 140), 238), module, ManicCompressionMB::ATTACK_CV_INPUT + b));

            ParamWidget* releaseParam = createParam<RoundSmallFWKnob>(Vec(75 + 255 + (b * 140), 235), module, ManicCompressionMB::RELEASE_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(releaseParam)->percentage = &module->releasePercentage[b];
            }
            addParam(releaseParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(100 + 255 + (b * 140), 253), module, ManicCompressionMB::RELEASE_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(104 + 255 + (b * 140), 238), module, ManicCompressionMB::RELEASE_CV_INPUT + b));


            ParamWidget* attackCurveParam = createParam<RoundSmallFWKnob>(Vec(15 + 255 + (b * 140), 290), module, ManicCompressionMB::ATTACK_CURVE_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(attackCurveParam)->percentage = &module->attackCurvePercentage[b];
                dynamic_cast<RoundSmallFWKnob*>(attackCurveParam)->biDirectional = true;
            }
            addParam(attackCurveParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(40 + 255 + (b * 140), 303), module, ManicCompressionMB::ATTACK_CURVE_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(44 + 255 + (b * 140), 291), module, ManicCompressionMB::ATTACK_CURVE_CV_INPUT + b));

            ParamWidget* releaseCurveParam = createParam<RoundSmallFWKnob>(Vec(75 + 255 + (b * 140), 290), module, ManicCompressionMB::RELEASE_CURVE_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(releaseCurveParam)->percentage = &module->releaseCurvePercentage[b];
                dynamic_cast<RoundSmallFWKnob*>(releaseCurveParam)->biDirectional = true;
            }
            addParam(releaseCurveParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(40 + 255 + (b * 140), 303), module, ManicCompressionMB::RELEASE_CURVE_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(104 + 255 + (b * 140), 291), module, ManicCompressionMB::RELEASE_CURVE_CV_INPUT + b));


            ParamWidget* inGainParam = createParam<RoundSmallFWKnob>(Vec(15 + 255 + (b * 140), 341), module, ManicCompressionMB::IN_GAIN_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(inGainParam)->percentage = &module->inGainPercentage[b];
            }
            addParam(inGainParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(40 + 255 + (b * 140), 359), module, ManicCompressionMB::IN_GAIN_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(44 + 255 + (b * 140), 344), module, ManicCompressionMB::IN_GAIN_INPUT + b));

            ParamWidget* makeupGainParam = createParam<RoundSmallFWKnob>(Vec(75 + 255 + (b * 140), 341), module, ManicCompressionMB::MAKEUP_GAIN_PARAM + b);
            if (module) {
                dynamic_cast<RoundSmallFWKnob*>(makeupGainParam)->percentage = &module->makeupGainPercentage[b];
            }
            addParam(makeupGainParam);							
            addParam(createParam<RoundReallySmallFWKnob>(Vec(100 + 255 + (b * 140), 359), module, ManicCompressionMB::MAKEUP_GAIN_CV_ATTENUVERTER_PARAM + b));
            addInput(createInput<FWPortInReallySmall>(Vec(104 + 255 + (b * 140), 344), module, ManicCompressionMB::MAKEUP_GAIN_INPUT + b));


            addOutput(createOutput<PJ301MPort>(Vec(955 , 25 + b*32), module, ManicCompressionMB::ENVELOPE_OUT + b));
    		addInput(createInput<PJ301MPort>(Vec(995, 25 + b*32), module, ManicCompressionMB::ENVELOPE_INPUT + b));

    		addOutput(createOutput<PJ301MPort>(Vec(955, 210 + b*32), module, ManicCompressionMB::DETECTOR_OUTPUT + b));
    		addInput(createInput<PJ301MPort>(Vec(995, 210 + b*32), module, ManicCompressionMB::BAND_SIDECHAIN_INPUT + b));
            addOutput(createOutput<PJ301MPort>(Vec(1035 , 210 + b*32), module, ManicCompressionMB::BAND_OUTPUT_L + b));
            addOutput(createOutput<PJ301MPort>(Vec(1065 , 210 + b*32), module, ManicCompressionMB::BAND_OUTPUT_R + b));

        }


		addParam(createParam<LEDButton>(Vec(10, 280), module, ManicCompressionMB::MS_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(11.5, 281.5), module, ManicCompressionMB::MS_MODE_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(31, 283), module, ManicCompressionMB::MS_MODE_INPUT));


		addParam(createParam<LEDButton>(Vec(57, 280), module, ManicCompressionMB::COMPRESS_M_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(58.5, 281.5), module, ManicCompressionMB::COMPRESS_MID_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(78, 283), module, ManicCompressionMB::COMPRESS_M_INPUT));

		addParam(createParam<LEDButton>(Vec(101, 280), module, ManicCompressionMB::COMPRESS_S_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(102.5, 281.5), module, ManicCompressionMB::COMPRESS_SIDE_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(122, 283), module, ManicCompressionMB::COMPRESS_S_INPUT));


		addParam(createParam<RoundSmallFWKnob>(Vec(145, 280), module, ManicCompressionMB::MIX_PARAM));
		addParam(createParam<RoundReallySmallFWKnob>(Vec(170, 298), module, ManicCompressionMB::MIX_CV_ATTENUVERTER_PARAM));
		addInput(createInput<FWPortInReallySmall>(Vec(174, 283), module, ManicCompressionMB::MIX_CV_INPUT));

		addParam(createParam<LEDButton>(Vec(202, 280), module, ManicCompressionMB::BYPASS_PARAM));
		addChild(createLight<LargeLight<RedLight>>(Vec(203.5, 281.5), module, ManicCompressionMB::BYPASS_LIGHT));
		addInput(createInput<FWPortInReallySmall>(Vec(223, 283), module, ManicCompressionMB::BYPASS_INPUT));

		

		addInput(createInput<PJ301MPort>(Vec(10, 335), module, ManicCompressionMB::SOURCE_L_INPUT));
		addInput(createInput<PJ301MPort>(Vec(40, 335), module, ManicCompressionMB::SOURCE_R_INPUT));

		addInput(createInput<PJ301MPort>(Vec(110, 335), module, ManicCompressionMB::SIDECHAIN_INPUT));

		addOutput(createOutput<PJ301MPort>(Vec(186, 335), module, ManicCompressionMB::OUTPUT_L));
		addOutput(createOutput<PJ301MPort>(Vec(216, 335), module, ManicCompressionMB::OUTPUT_R));


		//addChild(createLight<LargeLight<BlueLight>>(Vec(69,58), module, ManicCompressionMB::BLINK_LIGHT));
	}

	struct TriggerGateItem : MenuItem {
		ManicCompressionMB *module;
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

		ManicCompressionMB *module = dynamic_cast<ManicCompressionMB*>(this->module);
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
};

Model *modelManicCompressionMB = createModel<ManicCompressionMB, ManicCompressionMBWidget>("ManicCompressionMB");
