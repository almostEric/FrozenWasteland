#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"


//Not sure why this is necessary, but SSLFO is running twice as fast as sample rate says it should
#define SampleRateCompensation 2



struct SeriouslySlowEG : Module {
	enum ParamIds {
		DELAY_TIME_BASE_PARAM,
		DELAY_TIME_PARAM = DELAY_TIME_BASE_PARAM + 6,
		ATTACK_TIME_BASE_PARAM,
		ATTACK_TIME_PARAM = ATTACK_TIME_BASE_PARAM + 6,
		ATTACK_CURVE_PARAM,
		DECAY_TIME_BASE_PARAM = ATTACK_CURVE_PARAM + 3,
		DECAY_TIME_PARAM = DECAY_TIME_BASE_PARAM + 6,
		DECAY_CURVE_PARAM,
		SUSTAIN_LEVEL_PARAM = DECAY_CURVE_PARAM + 3,
		RELEASE_TIME_BASE_PARAM,
		RELEASE_TIME_PARAM = RELEASE_TIME_BASE_PARAM + 6,
		RELEASE_CURVE_PARAM,
		HOLD_TIME_BASE_PARAM = RELEASE_CURVE_PARAM + 3,
		HOLD_TIME_PARAM = HOLD_TIME_BASE_PARAM + 6,
		TRIGGER_PARAM,
		GATE_MODE_PARAM,
		CYCLE_MODE_PARAM,
		RETRIGGER_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIGGER_INPUT,
		DELAY_TIME_INPUT,
		ATTACK_TIME_INPUT,
		DECAY_TIME_INPUT,
		SUSTAIN_LEVEL_INPUT,
		RELEASE_TIME_INPUT,
		HOLD_TIME_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENVELOPE_OUT,
		EOC_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		DELAY_SECONDS_LIGHT,
		DELAY_MINUTES_LIGHT,
		DELAY_HOURS_LIGHT,
		DELAY_DAYS_LIGHT,
		DELAY_WEEKS_LIGHT,
		DELAY_MONTHS_LIGHT,
		ATTACK_SECONDS_LIGHT,
		ATTACK_MINUTES_LIGHT,
		ATTACK_HOURS_LIGHT,
		ATTACK_DAYS_LIGHT,
		ATTACK_WEEKS_LIGHT,
		ATTACK_MONTHS_LIGHT,
		DECAY_SECONDS_LIGHT,
		DECAY_MINUTES_LIGHT,
		DECAY_HOURS_LIGHT,
		DECAY_DAYS_LIGHT,
		DECAY_WEEKS_LIGHT,
		DECAY_MONTHS_LIGHT,
		RELEASE_SECONDS_LIGHT,
		RELEASE_MINUTES_LIGHT,
		RELEASE_HOURS_LIGHT,
		RELEASE_DAYS_LIGHT,
		RELEASE_WEEKS_LIGHT,
		RELEASE_MONTHS_LIGHT,
		HOLD_SECONDS_LIGHT,
		HOLD_MINUTES_LIGHT,
		HOLD_HOURS_LIGHT,
		HOLD_DAYS_LIGHT,
		HOLD_WEEKS_LIGHT,
		HOLD_MONTHS_LIGHT,
		ATTACK_CURVE_LINEAR_LIGHT,
		ATTACK_CURVE_EXPONENTIAL_LIGHT,
		ATTACK_CURVE_LOGARITHMIC_LIGHT,
		DECAY_CURVE_LINEAR_LIGHT,
		DECAY_CURVE_EXPONENTIAL_LIGHT,
		DECAY_CURVE_LOGARITHMIC_LIGHT,
		RELEASE_CURVE_LINEAR_LIGHT,
		RELEASE_CURVE_EXPONENTIAL_LIGHT,
		RELEASE_CURVE_LOGARITHMIC_LIGHT,
		NUM_LIGHTS
	};

	enum Stage {
		STOPPED_STAGE,
		DELAY_STAGE,
		ATTACK_STAGE,
		DECAY_STAGE,
		SUSTAIN_STAGE,
		RELEASE_STAGE
	};

	enum CurveShape {
		LINEAR_SHAPE,
		EXPONENTIAL_SHAPE,
		LOGARITHMIC_SHAPE
	};



	dsp::SchmittTrigger gateTrigger, resetTrigger,delayTimeBaseTrigger[6],attackTimeBaseTrigger[6],decayTimeBaseTrigger[6],releaseTimeBaseTrigger[6],holdTimeBaseTrigger[6],attackCurveTrigger[3],decayCurveTrigger[3],releaseCurveTrigger[3];
	dsp::PulseGenerator eocPulse;
	
	double duration = 0.0;
	int delayTimeBase = 0;
	int attackTimeBase = 0;
	int decayTimeBase = 0;
	int releaseTimeBase = 0;
	int holdTimeBase = 0;
	float delayValue = 0;
	float attackValue = 0;
	float decayValue = 0;
	float releaseValue = 0;
	float holdValue = 0;
	int attackCurve = 1;
	int decayCurve = 1;
	int releaseCurve = 1;

	Stage stage = STOPPED_STAGE;
	double stageProgress, holdProgress;
	double envelope = 0;
	double releaseLevel = 0;
	float sustainLevel;


	bool firstStep = true;

	

	SeriouslySlowEG() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(DELAY_TIME_BASE_PARAM, 0.0, 1.0, 0.0);
		configParam(DELAY_TIME_PARAM, 0.0, 100.0, 1.0,"Delay Time");

		configParam(ATTACK_TIME_BASE_PARAM, 0.0, 1.0, 0.0);
		configParam(ATTACK_TIME_PARAM, 0.0, 100.0, 1.0,"Attack Time");

		configParam(DECAY_TIME_BASE_PARAM, 0.0, 1.0, 0.0);
		configParam(DECAY_TIME_PARAM, 0.0, 100.0, 1.0,"Decay Time");

		configParam(SUSTAIN_LEVEL_PARAM, 0.0, 1.0, 0.5,"Sustain Level","%",0,100);

		configParam(RELEASE_TIME_BASE_PARAM, 0.0, 1.0, 0.0);
		configParam(RELEASE_TIME_PARAM, 0.0, 100.0, 1.0,"Release Time");

		configParam(HOLD_TIME_BASE_PARAM, 0.0, 1.0, 0.0);
		configParam(HOLD_TIME_PARAM, 0.0, 100.0, 1.0,"Hold Time");

		configParam(GATE_MODE_PARAM, 0.0, 1.0, 0.0, "Gate Mode");
		configParam(CYCLE_MODE_PARAM, 0.0, 1.0, 0.0, "Cycle Mode");
		configParam(RETRIGGER_MODE_PARAM, 0.0, 1.0, 0.0, "Retrigger Mode");
	}

	double timeBase(uint8_t timeBase) {
		double numberOfSeconds = 1;
		switch(timeBase) {
			case 0 :
				numberOfSeconds = 1; // Seconds
				break;
			case 1 :
				numberOfSeconds = 60; // Minutes
				break;
			case 2 :
				numberOfSeconds = 3600; // Hours
				break;
			case 3 :
				numberOfSeconds = 86400; // Days
				break;
			case 4 :
				numberOfSeconds = 604800; // Weeks
				break;
			case 5 :
				numberOfSeconds = 2592000; // Months
				break;
		}
		return numberOfSeconds;
	}

	void process(const ProcessArgs &args) override {

		const float shapeExponent = 2.0;
		const float shapeInverseExponent = 0.5;

		for(int timeIndex = 0;timeIndex<6;timeIndex++)
		{	
			if (delayTimeBaseTrigger[timeIndex].process(params[DELAY_TIME_BASE_PARAM+timeIndex].getValue())) {
				delayTimeBase = timeIndex;
			}
			if (holdTimeBaseTrigger[timeIndex].process(params[HOLD_TIME_BASE_PARAM+timeIndex].getValue())) {
				holdTimeBase = timeIndex;
			}
			if (attackTimeBaseTrigger[timeIndex].process(params[ATTACK_TIME_BASE_PARAM+timeIndex].getValue())) {
				attackTimeBase = timeIndex;
			}
			if (decayTimeBaseTrigger[timeIndex].process(params[DECAY_TIME_BASE_PARAM+timeIndex].getValue())) {
				decayTimeBase = timeIndex;
			}
			if (releaseTimeBaseTrigger[timeIndex].process(params[RELEASE_TIME_BASE_PARAM+timeIndex].getValue())) {
				releaseTimeBase = timeIndex;
			}

			lights[DELAY_SECONDS_LIGHT + timeIndex].value = timeIndex != delayTimeBase ? 0.0 : 1.0;
			lights[HOLD_SECONDS_LIGHT + timeIndex].value = timeIndex != holdTimeBase ? 0.0 : 1.0;
			lights[ATTACK_SECONDS_LIGHT + timeIndex].value = timeIndex != attackTimeBase ? 0.0 : 1.0;
			lights[DECAY_SECONDS_LIGHT + timeIndex].value = timeIndex != decayTimeBase ? 0.0 : 1.0;
			lights[RELEASE_SECONDS_LIGHT + timeIndex].value = timeIndex != releaseTimeBase ? 0.0 : 1.0;
		}


		delayValue = clamp(params[DELAY_TIME_PARAM].getValue() + (inputs[DELAY_TIME_INPUT].getVoltage() * 10.0),0.0f,100.0f);
		double delayTime = delayValue * timeBase(delayTimeBase) * args.sampleRate;
		attackValue = clamp(params[ATTACK_TIME_PARAM].getValue() + (inputs[ATTACK_TIME_INPUT].getVoltage() * 10.0),0.0f,100.0f);
		double attackTime = attackValue * timeBase(attackTimeBase) * args.sampleRate;
		decayValue = clamp(params[DECAY_TIME_PARAM].getValue() + (inputs[DECAY_TIME_INPUT].getVoltage() * 10.0),0.0f,100.0f);
		double decayTime = decayValue * timeBase(decayTimeBase) * args.sampleRate;
		releaseValue = clamp(params[RELEASE_TIME_PARAM].getValue() + (inputs[DELAY_TIME_INPUT].getVoltage() * 10.0),0.0f,100.0f);
		double releaseTime = releaseValue * timeBase(releaseTimeBase) * args.sampleRate;
		holdValue = clamp(params[HOLD_TIME_PARAM].getValue() + (inputs[HOLD_TIME_INPUT].getVoltage() * 10.0),0.0f,100.0f);
		double holdTime = holdValue * timeBase(holdTimeBase) * args.sampleRate;


		for(int curveIndex = 0;curveIndex<3;curveIndex++)
		{
			if (attackCurveTrigger[curveIndex].process(params[ATTACK_CURVE_PARAM+curveIndex].getValue())) {
				attackCurve = curveIndex;
			}
			if (decayCurveTrigger[curveIndex].process(params[DECAY_CURVE_PARAM+curveIndex].getValue())) {
				decayCurve = curveIndex;
			}
			if (releaseCurveTrigger[curveIndex].process(params[RELEASE_CURVE_PARAM+curveIndex].getValue())) {
				releaseCurve = curveIndex;
			}
			lights[ATTACK_CURVE_LINEAR_LIGHT + curveIndex].value = curveIndex != attackCurve ? 0.0 : 1.0;
			lights[DECAY_CURVE_LINEAR_LIGHT + curveIndex].value = curveIndex != decayCurve ? 0.0 : 1.0;
			lights[RELEASE_CURVE_LINEAR_LIGHT + curveIndex].value = curveIndex != releaseCurve ? 0.0 : 1.0;
		}

		sustainLevel = clamp(params[SUSTAIN_LEVEL_PARAM].getValue() + (inputs[SUSTAIN_LEVEL_INPUT].getVoltage() / 10.0),0.0f,1.0f);

		if (gateTrigger.process(params[TRIGGER_PARAM].getValue() + inputs[TRIGGER_INPUT].getVoltage()))
		{
			if (stage == STOPPED_STAGE || params[RETRIGGER_MODE_PARAM].getValue() <= 0.5) {
				stage = DELAY_STAGE;
				holdProgress  = 0.0;
				stageProgress = 0.0;
				envelope = 0.0;
			}
			else {
				switch (stage) {
					case STOPPED_STAGE:
					case ATTACK_STAGE: {
						break;
					}

					case DELAY_STAGE: {
						stage = ATTACK_STAGE; 
						stageProgress = 0.0;
						envelope = 0.0;

						// we're skipping the delay; subtract the full delay time from hold time so that env has the same shape as if we'd waited out the delay.
						holdProgress = fminf(1.0, delayTime / holdTime);
						break;
					}

					case DECAY_STAGE:
					case SUSTAIN_STAGE:
					case RELEASE_STAGE: {
						stage = ATTACK_STAGE;
						switch (attackCurve) {
							case LINEAR_SHAPE: {
								stageProgress = envelope;
								break;
							}
							case EXPONENTIAL_SHAPE: {
								stageProgress = pow(envelope, shapeInverseExponent);
								break;
							}
							default: {
								stageProgress = pow(envelope, shapeExponent);
								break;
							}
						}

						// reset hold to what it would have been at this point in the attack the first time through.
						holdProgress = fminf(1.0, (delayTime + stageProgress * attackTime) / holdTime);
						break;
					}
				}
			}
		}
		else {
			switch (stage) {
				case STOPPED_STAGE:
				case RELEASE_STAGE: {
					break;
				}

				case DELAY_STAGE:
				case ATTACK_STAGE:
				case DECAY_STAGE:
				case SUSTAIN_STAGE: {
					bool gateMode = params[GATE_MODE_PARAM].getValue() > 0.5;
					bool holdComplete = holdProgress >= 1.0;
					if (!holdComplete) {
						// run the hold accumulation even if we're not in hold mode, in case we switch mid-cycle.
						holdProgress += 1.0/holdTime;
						holdComplete = holdProgress >= 1.0;
					}

					if (gateMode ? !gateTrigger.isHigh() : holdComplete) {
						stage = RELEASE_STAGE;
						stageProgress = 0.0;
						releaseLevel = envelope;
					}
					break;
				}
			}
		}

		bool complete = false;
		switch (stage) {
			case STOPPED_STAGE: {
				break;
			}

			case DELAY_STAGE: {
				stageProgress += 1.0 / delayTime;
				//fprintf(stderr, "%f %f\n", stageProgress, delayTime);
				if (stageProgress >= 1.0) {
					stage = ATTACK_STAGE;
					stageProgress = 0.0;
				}
				break;
			}

			case ATTACK_STAGE: {
				stageProgress += 1.0 / attackTime;
				switch (attackCurve) {
					case LINEAR_SHAPE: {
						envelope = stageProgress;
						break;
					}
					case EXPONENTIAL_SHAPE: {
						envelope = pow(stageProgress, shapeExponent);
						break;
					}
					default: {
						envelope = pow(stageProgress, shapeInverseExponent);
						break;
					}
				}
				if (envelope >= 1.0) {
					stage = DECAY_STAGE;
					stageProgress = 0.0;
				}
				break;
			}

			case DECAY_STAGE: {
				stageProgress += 1.0/decayTime;
				switch (decayCurve) {
					case LINEAR_SHAPE: {
						envelope = 1.0 - stageProgress;
						break;
					}
					case LOGARITHMIC_SHAPE: {
						envelope = stageProgress >= 1.0 ? 0.0 : pow(1.0 - stageProgress, shapeExponent);
						break;
					}
					default: {
						envelope = stageProgress >= 1.0 ? 0.0 : pow(1.0 - stageProgress, shapeInverseExponent);
						break;
					}
				}
				envelope *= 1.0 - sustainLevel;
				envelope += sustainLevel;
				if (envelope <= sustainLevel) {
					stage = SUSTAIN_STAGE;
				}
				break;
			}

			case SUSTAIN_STAGE: {
				envelope = sustainLevel;
				break;
			}

			case RELEASE_STAGE: {
				stageProgress += 1.0/releaseTime;
				switch (releaseCurve) {
					case LINEAR_SHAPE: {
						envelope = 1.0 - stageProgress;
						break;
					}
					case LOGARITHMIC_SHAPE: {
						envelope = stageProgress >= 1.0 ? 0.0 : pow(1.0 - stageProgress, shapeExponent);
						break;
					}
					default: {
						envelope = stageProgress >= 1.0 ? 0.0 : pow(1.0 - stageProgress, shapeInverseExponent);
						break;
					}
				}
				envelope *= releaseLevel;
				if (envelope <= 0.00001) {
					complete = true;
					envelope = 0.0;
					if (params[GATE_MODE_PARAM].getValue() < 0.5 && (params[CYCLE_MODE_PARAM].getValue() > 0.5 || gateTrigger.isHigh())) {
						stage = DELAY_STAGE;
						holdProgress =  0.0;
						stageProgress = 0.0;
					}
					else {
						stage = STOPPED_STAGE;
					}
				}
				break;
			}
		}

		float env = envelope * 10.0;
		outputs[ENVELOPE_OUT].setVoltage(env);

		if (complete) {
			eocPulse.trigger(1e-3);
		}
		float eocOutputValue = eocPulse.process(1.0 / args.sampleRate) ? 10.0 : 0;
		outputs[EOC_OUTPUT].setVoltage(eocOutputValue);

		firstStep = false;
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "delayTimeBase", json_integer((int) delayTimeBase));
		json_object_set_new(rootJ, "attackTimeBase", json_integer((int) attackTimeBase));
		json_object_set_new(rootJ, "decayTimeBase", json_integer((int) decayTimeBase));
		json_object_set_new(rootJ, "releaseTimeBase", json_integer((int) releaseTimeBase));
		json_object_set_new(rootJ, "holdTimeBase", json_integer((int) holdTimeBase));

		json_object_set_new(rootJ, "attackCurve", json_integer((int) attackCurve));
		json_object_set_new(rootJ, "decayCurve", json_integer((int) decayCurve));
		json_object_set_new(rootJ, "releaseCurve", json_integer((int) releaseCurve));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *dltbJ = json_object_get(rootJ, "delayTimeBase");
		if (dltbJ)
			delayTimeBase = json_integer_value(dltbJ);
		json_t *atbJ = json_object_get(rootJ, "attackTimeBase");
		if (atbJ)
			attackTimeBase = json_integer_value(atbJ);
		json_t *dtbJ = json_object_get(rootJ, "decayTimeBase");
		if (dtbJ)
			decayTimeBase = json_integer_value(dtbJ);
		json_t *rtbJ = json_object_get(rootJ, "releaseTimeBase");
		if (rtbJ)
			releaseTimeBase = json_integer_value(rtbJ);
		json_t *htbJ = json_object_get(rootJ, "holdTimeBase");
		if (htbJ)
			holdTimeBase = json_integer_value(htbJ);

		json_t *acJ = json_object_get(rootJ, "attackCurve");
		if (acJ)
			attackCurve = json_integer_value(acJ);
		json_t *dcJ = json_object_get(rootJ, "decayCurve");
		if (dcJ)
			decayCurve = json_integer_value(dcJ);
		json_t *rcJ = json_object_get(rootJ, "releaseCurve");
		if (rcJ)
			releaseCurve = json_integer_value(rcJ);

	}

	// void reset() override {
	// 	timeBase = 0;
	// }

	// For more advanced Module features, read Rack's engine.hpp header file
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

struct SSEGProgressDisplay : TransparentWidget {
	SeriouslySlowEG *module;
	int frame = 0;
	std::shared_ptr<Font> font;



	SSEGProgressDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/SUBWT___.ttf"));
	}

	void drawDelayProgress(const DrawArgs &args, float phase, bool active)
	{
		const float rotate90 = (M_PI) / 2.0;
		float startArc = 0 - rotate90;
		float endArc = (phase * M_PI * 2) - rotate90;

		if(active) {
			nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, 0xff));
		} else {
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
		}
		nvgBeginPath(args.vg);
		nvgArc(args.vg,49,62.5,35,startArc,endArc,NVG_CW);
		nvgLineTo(args.vg,49,62.5);
		nvgClosePath(args.vg);
		nvgFill(args.vg);
	}

	void drawEGProgress(const DrawArgs &args, int stage, double progress, double holdProgress, float sustainLevel)
	{
		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg,100,100);
		nvgLineTo(args.vg,150,25);
		nvgLineTo(args.vg,200,100-(75.0*sustainLevel));
		nvgLineTo(args.vg,250,100-(75.0*sustainLevel));
		nvgLineTo(args.vg,300,100);
		nvgStroke(args.vg);


		double attackProgress = progress;
		if(stage >= module->ATTACK_STAGE) {
			if(stage > module->ATTACK_STAGE)
				attackProgress = 1.0;			
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg,100,100);
			nvgLineTo(args.vg,100+(50.0*attackProgress),100-(75.0*attackProgress));
			nvgLineTo(args.vg,100+(50.0*attackProgress),100);
			nvgClosePath(args.vg);
			nvgFill(args.vg);
		}

		double decayProgress = progress;
		if(stage >= module->DECAY_STAGE) {
			if(stage > module->DECAY_STAGE)
				decayProgress = 1.0;			
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg,150,25);
			nvgLineTo(args.vg,150+(50.0*decayProgress),25+(75.0*decayProgress*(1-sustainLevel)));
			nvgLineTo(args.vg,150+(50.0*decayProgress),100);
			nvgLineTo(args.vg,150,100);
			nvgClosePath(args.vg);
			nvgFill(args.vg);
		}

		double sustainProgress = holdProgress;
		if(stage >= module->SUSTAIN_STAGE) {
			if(stage > module->SUSTAIN_STAGE)
				sustainProgress = 1.0;			
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg,200,100-(75.0*sustainLevel));
			nvgLineTo(args.vg,200+(50.0*sustainProgress),100-(75.0*sustainLevel));
			nvgLineTo(args.vg,200+(50.0*sustainProgress),100);
			nvgLineTo(args.vg,200,100);
			nvgClosePath(args.vg);
			nvgFill(args.vg);
		}

		double releaseProgress = progress;
		if(stage == module->RELEASE_STAGE) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg,250,100-(75.0*sustainLevel));
			nvgLineTo(args.vg,250+(50.0*releaseProgress),100-((75.0*sustainLevel)*(1.0-releaseProgress))); //This shoule be simplified
			nvgLineTo(args.vg,250+(50.0*releaseProgress),100);
			nvgLineTo(args.vg,250,100);
			nvgClosePath(args.vg);
			nvgFill(args.vg);
		}


	}

	void drawDuration(const DrawArgs &args, Vec pos, float duration) {
		nvgFontSize(args.vg, 12);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);

		//nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), " % #6.1f", duration);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if (!module)
			return;
		int stage = (int)module->stage;

		double delayProgress = module->stageProgress;
		float sustainLevel = module->sustainLevel;
		bool egActive = false;
		if(stage == module->STOPPED_STAGE) {
			delayProgress = 0.0;
		} else if (stage != module->DELAY_STAGE) {
			delayProgress = 1.0;
			egActive = true;
		}
		drawDelayProgress(args,delayProgress, egActive);  
		drawEGProgress(args,stage, module->stageProgress,module->holdProgress, sustainLevel);  

		drawDuration(args, Vec(44, 140), module->delayValue);
		drawDuration(args, Vec(138, 140), module->attackValue);
		drawDuration(args, Vec(213, 140), module->decayValue);
		drawDuration(args, Vec(288, 140), module->releaseValue);
		drawDuration(args, Vec(362, 140), module->holdValue);
		
	}
};

struct SeriouslySlowEGWidget : ModuleWidget {
	SeriouslySlowEGWidget(SeriouslySlowEG *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SeriouslySlowEG.svg")));
		

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		{
			SSEGProgressDisplay *display = new SSEGProgressDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, 220);
			addChild(display);
		}

		addParam(createParam<RoundFWKnob>(Vec(5, 150), module, SeriouslySlowEG::DELAY_TIME_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(97, 150), module, SeriouslySlowEG::ATTACK_TIME_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(172, 150), module, SeriouslySlowEG::DECAY_TIME_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(247, 150), module, SeriouslySlowEG::RELEASE_TIME_PARAM));
		addParam(createParam<RoundFWKnob>(Vec(323, 150), module, SeriouslySlowEG::HOLD_TIME_PARAM));

		addInput(createInput<FWPortInSmall>(Vec(37, 154), module, SeriouslySlowEG::DELAY_TIME_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(129, 154), module, SeriouslySlowEG::ATTACK_TIME_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(204, 154), module, SeriouslySlowEG::DECAY_TIME_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(279, 154), module, SeriouslySlowEG::RELEASE_TIME_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(353, 154), module, SeriouslySlowEG::HOLD_TIME_INPUT));


		addParam(createParam<RoundFWKnob>(Vec(315, 50), module, SeriouslySlowEG::SUSTAIN_LEVEL_PARAM));
		addInput(createInput<FWPortInSmall>(Vec(345, 54), module, SeriouslySlowEG::SUSTAIN_LEVEL_INPUT));

		
		addParam(createParam<CKSS>(Vec(12, 329.5), module, SeriouslySlowEG::GATE_MODE_PARAM));
		addParam(createParam<CKSS>(Vec(42, 329.5), module, SeriouslySlowEG::CYCLE_MODE_PARAM));
		addParam(createParam<CKSS>(Vec(72, 329.5), module, SeriouslySlowEG::RETRIGGER_MODE_PARAM));
		
		addParam(createParam<TL1105>(Vec(245, 350), module, SeriouslySlowEG::TRIGGER_PARAM));
		addInput(createInput<FWPortInSmall>(Vec(265, 350), module, SeriouslySlowEG::TRIGGER_INPUT));

		addOutput(createOutput<FWPortOutSmall>(Vec(300, 350), module, SeriouslySlowEG::ENVELOPE_OUT));
		addOutput(createOutput<FWPortOutSmall>(Vec(335, 350), module, SeriouslySlowEG::EOC_OUTPUT));

		addParam(createParam<LEDButton>(Vec(5, 183), module, SeriouslySlowEG::DELAY_TIME_BASE_PARAM + 0));
		addChild(createLight<LargeLight<BlueLight>>(Vec(6.5, 184.5), module, SeriouslySlowEG::DELAY_SECONDS_LIGHT));
		addParam(createParam<LEDButton>(Vec(5, 201), module, SeriouslySlowEG::DELAY_TIME_BASE_PARAM + 1));
		addChild(createLight<LargeLight<BlueLight>>(Vec(6.5, 202.5), module, SeriouslySlowEG::DELAY_MINUTES_LIGHT));
		addParam(createParam<LEDButton>(Vec(5, 219), module, SeriouslySlowEG::DELAY_TIME_BASE_PARAM + 2));
		addChild(createLight<LargeLight<BlueLight>>(Vec(6.5, 220.5), module, SeriouslySlowEG::DELAY_HOURS_LIGHT));
		addParam(createParam<LEDButton>(Vec(5, 237), module, SeriouslySlowEG::DELAY_TIME_BASE_PARAM + 3));
		addChild(createLight<LargeLight<BlueLight>>(Vec(6.5, 238.5), module, SeriouslySlowEG::DELAY_DAYS_LIGHT));
		addParam(createParam<LEDButton>(Vec(5, 255), module, SeriouslySlowEG::DELAY_TIME_BASE_PARAM + 4));
		addChild(createLight<LargeLight<BlueLight>>(Vec(6.5, 256.5), module, SeriouslySlowEG::DELAY_WEEKS_LIGHT));
		addParam(createParam<LEDButton>(Vec(5, 273), module, SeriouslySlowEG::DELAY_TIME_BASE_PARAM + 5));
		addChild(createLight<LargeLight<BlueLight>>(Vec(6.5, 274.5), module, SeriouslySlowEG::DELAY_MONTHS_LIGHT));

		addParam(createParam<LEDButton>(Vec(97, 183), module, SeriouslySlowEG::ATTACK_TIME_BASE_PARAM + 0));
		addChild(createLight<LargeLight<BlueLight>>(Vec(98.5, 184.5), module, SeriouslySlowEG::ATTACK_SECONDS_LIGHT));
		addParam(createParam<LEDButton>(Vec(97, 201), module, SeriouslySlowEG::ATTACK_TIME_BASE_PARAM + 1));
		addChild(createLight<LargeLight<BlueLight>>(Vec(98.5, 202.5), module, SeriouslySlowEG::ATTACK_MINUTES_LIGHT));
		addParam(createParam<LEDButton>(Vec(97, 219), module, SeriouslySlowEG::ATTACK_TIME_BASE_PARAM + 2));
		addChild(createLight<LargeLight<BlueLight>>(Vec(98.5, 220.5), module, SeriouslySlowEG::ATTACK_HOURS_LIGHT));
		addParam(createParam<LEDButton>(Vec(97, 2537), module, SeriouslySlowEG::ATTACK_TIME_BASE_PARAM + 3));
		addChild(createLight<LargeLight<BlueLight>>(Vec(98.5, 238.5), module, SeriouslySlowEG::ATTACK_DAYS_LIGHT));
		addParam(createParam<LEDButton>(Vec(97, 255), module, SeriouslySlowEG::ATTACK_TIME_BASE_PARAM + 4));
		addChild(createLight<LargeLight<BlueLight>>(Vec(98.5, 256.5), module, SeriouslySlowEG::ATTACK_WEEKS_LIGHT));
		addParam(createParam<LEDButton>(Vec(97, 273), module, SeriouslySlowEG::ATTACK_TIME_BASE_PARAM + 5));
		addChild(createLight<LargeLight<BlueLight>>(Vec(98.5, 274.5), module, SeriouslySlowEG::ATTACK_MONTHS_LIGHT));

		addParam(createParam<LEDButton>(Vec(172, 183), module, SeriouslySlowEG::DECAY_TIME_BASE_PARAM + 0));
		addChild(createLight<LargeLight<BlueLight>>(Vec(173.5, 184.5), module, SeriouslySlowEG::DECAY_SECONDS_LIGHT));
		addParam(createParam<LEDButton>(Vec(172, 201), module, SeriouslySlowEG::DECAY_TIME_BASE_PARAM + 1));
		addChild(createLight<LargeLight<BlueLight>>(Vec(173.5, 202.5), module, SeriouslySlowEG::DECAY_MINUTES_LIGHT));
		addParam(createParam<LEDButton>(Vec(172, 219), module, SeriouslySlowEG::DECAY_TIME_BASE_PARAM + 2));
		addChild(createLight<LargeLight<BlueLight>>(Vec(173.5, 220.5), module, SeriouslySlowEG::DECAY_HOURS_LIGHT));
		addParam(createParam<LEDButton>(Vec(172, 237), module, SeriouslySlowEG::DECAY_TIME_BASE_PARAM + 3));
		addChild(createLight<LargeLight<BlueLight>>(Vec(173.5, 238.5), module, SeriouslySlowEG::DECAY_DAYS_LIGHT));
		addParam(createParam<LEDButton>(Vec(172, 255), module, SeriouslySlowEG::DECAY_TIME_BASE_PARAM + 4));
		addChild(createLight<LargeLight<BlueLight>>(Vec(173.5, 256.5), module, SeriouslySlowEG::DECAY_WEEKS_LIGHT));
		addParam(createParam<LEDButton>(Vec(172, 273), module, SeriouslySlowEG::DECAY_TIME_BASE_PARAM + 5));
		addChild(createLight<LargeLight<BlueLight>>(Vec(173.5, 274.5), module, SeriouslySlowEG::DECAY_MONTHS_LIGHT));

		addParam(createParam<LEDButton>(Vec(247, 183), module, SeriouslySlowEG::RELEASE_TIME_BASE_PARAM + 0));
		addChild(createLight<LargeLight<BlueLight>>(Vec(248.5, 184.5), module, SeriouslySlowEG::RELEASE_SECONDS_LIGHT));
		addParam(createParam<LEDButton>(Vec(247, 201), module, SeriouslySlowEG::RELEASE_TIME_BASE_PARAM + 1));
		addChild(createLight<LargeLight<BlueLight>>(Vec(248.5, 202.5), module, SeriouslySlowEG::RELEASE_MINUTES_LIGHT));
		addParam(createParam<LEDButton>(Vec(247, 219), module, SeriouslySlowEG::RELEASE_TIME_BASE_PARAM + 2));
		addChild(createLight<LargeLight<BlueLight>>(Vec(248.5, 220.5), module, SeriouslySlowEG::RELEASE_HOURS_LIGHT));
		addParam(createParam<LEDButton>(Vec(247, 237), module, SeriouslySlowEG::RELEASE_TIME_BASE_PARAM + 3));
		addChild(createLight<LargeLight<BlueLight>>(Vec(248.5, 238.5), module, SeriouslySlowEG::RELEASE_DAYS_LIGHT));
		addParam(createParam<LEDButton>(Vec(247, 255), module, SeriouslySlowEG::RELEASE_TIME_BASE_PARAM + 4));
		addChild(createLight<LargeLight<BlueLight>>(Vec(248.5, 256.5), module, SeriouslySlowEG::RELEASE_WEEKS_LIGHT));
		addParam(createParam<LEDButton>(Vec(247, 273), module, SeriouslySlowEG::RELEASE_TIME_BASE_PARAM + 5));
		addChild(createLight<LargeLight<BlueLight>>(Vec(248.5, 274.5), module, SeriouslySlowEG::RELEASE_MONTHS_LIGHT));


		addParam(createParam<LEDButton>(Vec(325, 183), module, SeriouslySlowEG::HOLD_TIME_BASE_PARAM + 0));
		addChild(createLight<LargeLight<BlueLight>>(Vec(326.6, 184.5), module, SeriouslySlowEG::HOLD_SECONDS_LIGHT));
		addParam(createParam<LEDButton>(Vec(325, 201), module, SeriouslySlowEG::HOLD_TIME_BASE_PARAM + 1));
		addChild(createLight<LargeLight<BlueLight>>(Vec(326.6, 202.5), module, SeriouslySlowEG::HOLD_MINUTES_LIGHT));
		addParam(createParam<LEDButton>(Vec(325, 219), module, SeriouslySlowEG::HOLD_TIME_BASE_PARAM + 2));
		addChild(createLight<LargeLight<BlueLight>>(Vec(326.5, 220.5), module, SeriouslySlowEG::HOLD_HOURS_LIGHT));
		addParam(createParam<LEDButton>(Vec(325, 237), module, SeriouslySlowEG::HOLD_TIME_BASE_PARAM + 3));
		addChild(createLight<LargeLight<BlueLight>>(Vec(326.5, 238.5), module, SeriouslySlowEG::HOLD_DAYS_LIGHT));
		addParam(createParam<LEDButton>(Vec(325, 255), module, SeriouslySlowEG::HOLD_TIME_BASE_PARAM + 4));
		addChild(createLight<LargeLight<BlueLight>>(Vec(326.5, 256.5), module, SeriouslySlowEG::HOLD_WEEKS_LIGHT));
		addParam(createParam<LEDButton>(Vec(325, 273), module, SeriouslySlowEG::HOLD_TIME_BASE_PARAM + 5));
		addChild(createLight<LargeLight<BlueLight>>(Vec(326.5, 274.5), module, SeriouslySlowEG::HOLD_MONTHS_LIGHT));


		addParam(createParam<LEDButton>(Vec(97, 307), module, SeriouslySlowEG::ATTACK_CURVE_PARAM + 2));
		addChild(createLight<LargeLight<BlueLight>>(Vec(98.5, 308.5), module, SeriouslySlowEG::ATTACK_CURVE_LOGARITHMIC_LIGHT));
		addParam(createParam<LEDButton>(Vec(117, 307), module, SeriouslySlowEG::ATTACK_CURVE_PARAM + 0));
		addChild(createLight<LargeLight<BlueLight>>(Vec(118.5, 308.5), module, SeriouslySlowEG::ATTACK_CURVE_LINEAR_LIGHT));
		addParam(createParam<LEDButton>(Vec(137, 307), module, SeriouslySlowEG::ATTACK_CURVE_PARAM + 1));
		addChild(createLight<LargeLight<BlueLight>>(Vec(138.5, 308.5), module, SeriouslySlowEG::ATTACK_CURVE_EXPONENTIAL_LIGHT));


		addParam(createParam<LEDButton>(Vec(172, 307), module, SeriouslySlowEG::DECAY_CURVE_PARAM + 2));
		addChild(createLight<LargeLight<BlueLight>>(Vec(173.5, 308.5), module, SeriouslySlowEG::DECAY_CURVE_LOGARITHMIC_LIGHT));
		addParam(createParam<LEDButton>(Vec(192, 307), module, SeriouslySlowEG::DECAY_CURVE_PARAM + 0));
		addChild(createLight<LargeLight<BlueLight>>(Vec(193.5, 308.5), module, SeriouslySlowEG::DECAY_CURVE_LINEAR_LIGHT));
		addParam(createParam<LEDButton>(Vec(212, 307), module, SeriouslySlowEG::DECAY_CURVE_PARAM + 1));
		addChild(createLight<LargeLight<BlueLight>>(Vec(213.5, 308.5), module, SeriouslySlowEG::DECAY_CURVE_EXPONENTIAL_LIGHT));


		addParam(createParam<LEDButton>(Vec(247, 307), module, SeriouslySlowEG::RELEASE_CURVE_PARAM + 2));
		addChild(createLight<LargeLight<BlueLight>>(Vec(248.5, 308.5), module, SeriouslySlowEG::RELEASE_CURVE_LOGARITHMIC_LIGHT));
		addParam(createParam<LEDButton>(Vec(267, 307), module, SeriouslySlowEG::RELEASE_CURVE_PARAM + 0));
		addChild(createLight<LargeLight<BlueLight>>(Vec(268.5, 308.5), module, SeriouslySlowEG::RELEASE_CURVE_LINEAR_LIGHT));
		addParam(createParam<LEDButton>(Vec(287, 307), module, SeriouslySlowEG::RELEASE_CURVE_PARAM + 1));
		addChild(createLight<LargeLight<BlueLight>>(Vec(288.5, 308.5), module, SeriouslySlowEG::RELEASE_CURVE_EXPONENTIAL_LIGHT));



	}
};

Model *modelSeriouslySlowEG = createModel<SeriouslySlowEG, SeriouslySlowEGWidget>("SeriouslySlowEG");
