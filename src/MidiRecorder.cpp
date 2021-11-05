#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/menu.hpp"
#include "xml/tinyxml2.h"

#include <sstream>
#include <iomanip>
#include <time.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "osdialog.h"


#define CHANNEL_COUNT 16
#define MAX_NOTES 12
#define MAX_MIDI_NOTES 128

using namespace tinyxml2;

struct MidiEvent {
  long timeStamp;
  int eventType;
  uint8_t value;
  uint8_t velocity;
};


struct MidiRecorder : Module {

    enum ParamIds {
        RECORD_PARAM,
        NOTE_VALUE_PARAM,
        NOTE_VELOCITY_PARAM = NOTE_VALUE_PARAM + CHANNEL_COUNT,
        NOTE_VELOCITY_RANGE_PARAM = NOTE_VELOCITY_PARAM + CHANNEL_COUNT,
        ACCENT_NOTE_VALUE_PARAM = NOTE_VELOCITY_RANGE_PARAM + CHANNEL_COUNT,
        ACCENT_VELOCITY_PARAM = ACCENT_NOTE_VALUE_PARAM + CHANNEL_COUNT,
        ACCENT_VELOCITY_RANGE_PARAM = ACCENT_VELOCITY_PARAM + CHANNEL_COUNT,
		NUM_PARAMS = ACCENT_VELOCITY_RANGE_PARAM + CHANNEL_COUNT
	}; 

	enum InputIds {
        BPM_INPUT,
        RUN_INPUT,
        GATE_INPUT,
        ACCENT_INPUT,
        NOTE_VALUE_INPUT,
        NOTE_VELOCITY_INPUT = NOTE_VALUE_INPUT + CHANNEL_COUNT,
        NOTE_VELOCITY_RANGE_INPUT = NOTE_VELOCITY_INPUT + CHANNEL_COUNT,
        ACCENT_NOTE_VALUE_INPUT = NOTE_VELOCITY_RANGE_INPUT + CHANNEL_COUNT,
        ACCENT_VELOCITY_INPUT = ACCENT_NOTE_VALUE_INPUT + CHANNEL_COUNT,
        ACCENT_VELOCITY_RANGE_INPUT = ACCENT_VELOCITY_INPUT + CHANNEL_COUNT,
		NUM_INPUTS = ACCENT_VELOCITY_RANGE_INPUT + CHANNEL_COUNT
	};

	enum OutputIds {        
		NUM_OUTPUTS
	};

	enum LightIds {
        RECORDING_LIGHT,
		NUM_LIGHTS 
	};

	const char* noteNames[MAX_NOTES] = {"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"};

    bool midiNoteDisplayMode = false;
    bool recording = false;
    bool firstEventReceived = false;
    uint16_t ticksPerQN = 960;
    float bpm;
    dsp::SchmittTrigger recordingTrigger;
    std::string fileName;
    std::ofstream midiFile;
    std::string drumMapFile = "";
    bool useDrumMap = false;

    std::string drumNoteNames[MAX_MIDI_NOTES];

    long totalTime = 0;
    int tickCount = 0;
    long ticksSinceLastEvent;
    double stepCount;

    uint8_t notes[CHANNEL_COUNT] = {0};
    uint8_t velocity[CHANNEL_COUNT] = {0};
    uint8_t velocityRandom[CHANNEL_COUNT] = {0};
    uint8_t accentNotes[CHANNEL_COUNT] = {0};
    uint8_t accentVelocity[CHANNEL_COUNT] = {0};
    uint8_t accentVelocityRandom[CHANNEL_COUNT] = {0};

    int currentNoteInputValue[CHANNEL_COUNT];
    int currentAccentInputValue[CHANNEL_COUNT];

    std::vector<MidiEvent> midiEvents;
    long eventCount = 0;

	MidiRecorder() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
				
        for(int c=0;c<CHANNEL_COUNT;c++) {
            configParam(MidiRecorder::NOTE_VALUE_PARAM + c, 0, 127, 36.0+c,"MIDI Note");		
            configParam(MidiRecorder::NOTE_VELOCITY_PARAM + c, 0.0, 127.0, 64.0,"Note Velocity");		
            configParam(MidiRecorder::NOTE_VELOCITY_RANGE_PARAM + c, 0.0, 127.0, 0.0,"Velocity Random Range");		
            configParam(MidiRecorder::ACCENT_NOTE_VALUE_PARAM + c, 0.0, 127.0, 36.0+c,"Accent Note");		
            configParam(MidiRecorder::ACCENT_VELOCITY_PARAM + c, 0.0, 127.0, 110.0,"Accent Velocity");		
            configParam(MidiRecorder::ACCENT_VELOCITY_RANGE_PARAM + c, 0, 127.0, 0.0,"Accent Velocity Random Range");	

            configInput(NOTE_VALUE_INPUT+c, "Note " + std::to_string(c+1) +" Value");
            configInput(NOTE_VELOCITY_INPUT+c, "Note " + std::to_string(c+1) +" Velocity");
            configInput(NOTE_VELOCITY_RANGE_INPUT+c, "Note " + std::to_string(c+1) +" Velocity Range");
            configInput(ACCENT_NOTE_VALUE_INPUT+c, "Note " + std::to_string(c+1) +" Accent Value");
            configInput(ACCENT_VELOCITY_INPUT+c, "Note " + std::to_string(c+1) +" Accent Velocity");
            configInput(ACCENT_VELOCITY_RANGE_INPUT+c, "Note " + std::to_string(c+1) +" Accent Velocity Range");
        }

        configInput(BPM_INPUT, "BPM");
        configInput(RUN_INPUT, "Run");
        configInput(GATE_INPUT, "Gate");
        configInput(ACCENT_INPUT, "Accent");

        onReset();
	}

    std::string HexStringToByteString(std::string hex ) {

        std::basic_string<uint8_t> bytes;

        // Iterate over every pair of hex values in the input string (e.g. "18", "0f", ...)
        for (size_t i = 0; i < hex.length(); i += 2)
        {
            uint16_t byte;

            // Get current pair and store in nextbyte
            std::string nextbyte = hex.substr(i, 2);

            // Put the pair into an istringstream and stream it through std::hex for
            // conversion into an integer value.
            // This will calculate the byte value of your string-represented hex value.
            std::istringstream(nextbyte) >> std::hex >> byte;

            // As the stream above does not work with uint8 directly,
            // we have to cast it now.
            // As every pair can have a maximum value of "ff",
            // which is "11111111" (8 bits), we will not lose any information during this cast.
            // This line adds the current byte value to our final byte "array".
            bytes.push_back(static_cast<uint8_t>(byte));
        }

        // we are now generating a string obj from our bytes-"array"
        // this string object contains the non-human-readable binary byte values
        // therefore, simply reading it would yield a String like ".0n..:j..}p...?*8...3..x"
        // however, this is very useful to output it directly into a binary file like shown below
        std::string result(begin(bytes), end(bytes));
        return result;
    }

    void loadDrumMap(std::string path) {
        XMLDocument doc;
	    doc.LoadFile( path.c_str() );        
        
        XMLElement*  listNode = doc.FirstChildElement( "DrumMap" )->FirstChildElement( "list" );
        bool foundMap = false;
        int elementCount = 0;
        do {
            if(listNode->Attribute("name","Map")) {
                foundMap = true;
            } else {
                elementCount++;
                listNode = listNode->NextSiblingElement();
            }
        } while(elementCount < 3 && !foundMap);
                //fprintf(stderr, "Found Map %i\n", foundMap);
        if(foundMap) {
            useDrumMap = true;
            int id = 0; //need to make this more sophisticated, but this xml parsing sucks    
            XMLElement* itemNode = listNode->FirstChildElement("item");
            do {
                XMLElement* stringNode = itemNode->FirstChildElement("string");
                std::string itemName = stringNode->Attribute("value");
                drumNoteNames[id] = itemName;
                //fprintf(stderr, "%s\n", itemName.c_str());

                itemNode = itemNode->NextSiblingElement();
                id++;
            } while(itemNode);
        }
	    //title = textNode->Value();
    }

    void clearDrumMap() {
        drumMapFile = "";
    }

    void CreateMidiFile(std::string fileName) {	
		// midiFile.open (fileName);
        std::ofstream midiFile(fileName, std::ios::binary | std::ios::out);

        //Create Header
		midiFile << "MThd";
        midiFile << HexStringToByteString("00000006");
        midiFile << HexStringToByteString("0000"); // going for type 0 right now, will switch to 1
        midiFile << HexStringToByteString("0001"); // see if we can get away with 1 chunk
        midiFile << (unsigned char) (ticksPerQN >> 8);
        midiFile << (unsigned char) (ticksPerQN % 256);



        std::vector<unsigned char> timeVector;
        int chunklen = 0;
        for(int x=0;x<2;x++) {  // Do it twice, once to count bytes, the other to write.
            if(x==1) {
                midiFile << "MTrk";
                for(int i=3;i>=0;i--) {
                    int divisor = 1 << (i*8);
                    uint8_t byteValue = chunklen / divisor;
                    midiFile << byteValue;
                    chunklen = chunklen % divisor;
                }

            }
            for(int e=0;e<eventCount;e++) {
                MidiEvent thisEvent = midiEvents[e];
                long delta = thisEvent.timeStamp;

                timeVector.resize(0);
                do {
                    unsigned char byteValue = delta & 0x7f;
                    timeVector.push_back(byteValue);
                    delta = delta >> 7;
                } while(delta > 0);
                for(int i=timeVector.size()-1;i>=0;i--) {
                    unsigned char byteValue = timeVector[i];
                    if(i>0) 
                        byteValue |= 0x80;
                    if(x==0)
                        chunklen++;
                    else
                        midiFile << byteValue;
                }
                if(x==0) {
                    chunklen+=3;
                } else {
                    midiFile << (unsigned char)(thisEvent.eventType == 1 ? 0x99 : 0x89);
                    midiFile << (unsigned char)thisEvent.value;
                    midiFile << (unsigned char)thisEvent.velocity;
                }
            }
        }

		midiFile.close();

	}

    json_t *dataToJson() override {
		json_t *rootJ = json_object();

        json_object_set_new(rootJ, "drumMapFile", json_string(drumMapFile.c_str()));	


		json_object_set_new(rootJ, "midiNoteDisplayMode", json_boolean(midiNoteDisplayMode));
		json_object_set_new(rootJ, "ticksPerQN", json_integer((uint16_t) ticksPerQN));
		
		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {

        // drumMapFile
        json_t *drumMapFileJ = json_object_get(rootJ, "drumMapFile");
        if (drumMapFileJ) {
            drumMapFile = json_string_value(drumMapFileJ);
            if(strcmp(drumMapFile.c_str(),"") != 0) {
                loadDrumMap(drumMapFile);
            }
        }

		json_t *mndmJ = json_object_get(rootJ, "midiNoteDisplayMode");
		if (mndmJ)
			midiNoteDisplayMode = json_boolean_value(mndmJ);

		json_t *tpqJ = json_object_get(rootJ, "ticksPerQN");
		if (tpqJ) {
			ticksPerQN = json_integer_value(tpqJ);			
		}	
	}


	void process(const ProcessArgs &args) override {

        float sampleRate = args.sampleRate;
        bpm = powf(2.0,clamp(inputs[BPM_INPUT].getVoltage(),-3.0,3.0)) * 120.0;
        float tickLength = (sampleRate * 60) / (bpm * ticksPerQN);

        for(int c=0;c<CHANNEL_COUNT;c++) {
            notes[c] = clamp(params[NOTE_VALUE_PARAM + c].getValue() + (inputs[NOTE_VALUE_INPUT].getVoltage() / 12.7),0,127);
            velocity[c] = clamp(params[NOTE_VELOCITY_PARAM + c].getValue() + (inputs[NOTE_VELOCITY_INPUT].getVoltage() / 12.7),0,127);
            velocityRandom[c] = clamp(params[NOTE_VELOCITY_RANGE_PARAM + c].getValue() + (inputs[NOTE_VELOCITY_RANGE_INPUT].getVoltage() / 12.7),0,127);
            accentNotes[c] = clamp(params[ACCENT_NOTE_VALUE_PARAM + c].getValue() + (inputs[ACCENT_NOTE_VALUE_INPUT].getVoltage() / 12.7),0,127);
            accentVelocity[c] = clamp(params[ACCENT_VELOCITY_PARAM + c].getValue() + (inputs[ACCENT_VELOCITY_INPUT].getVoltage() / 12.7),0,127);
            accentVelocityRandom[c] = clamp(params[ACCENT_VELOCITY_RANGE_PARAM + c].getValue() + (inputs[ACCENT_VELOCITY_RANGE_INPUT].getVoltage() / 12.7),0,127);
        }


        if (recordingTrigger.process(params[RECORD_PARAM].getValue() +  inputs[RUN_INPUT].getVoltage())) {
            recording = !recording;
            if(recording) {
                ticksSinceLastEvent = 0;
                totalTime = 0;
                tickCount = 0;
                stepCount = 0;
                midiEvents.resize(0);
                eventCount = 0;
                firstEventReceived = false;
            }
        }
        lights[RECORDING_LIGHT].value = recording;

        uint8_t nbrChannels = inputs[GATE_INPUT].getChannels();
        
        if(recording) {
            for(int i=0;i<nbrChannels;i++) {
                int noteIn = inputs[GATE_INPUT].getVoltage(i);  
                float accentIn = inputs[ACCENT_INPUT].getVoltage(i);
                if(noteIn ^ currentNoteInputValue[i]) {
                    firstEventReceived = true;
                    currentNoteInputValue[i] = noteIn;
                    float rnd = ((float) rand()/RAND_MAX) - 0.5f;

                    MidiEvent newEvent;
                    newEvent.timeStamp = ticksSinceLastEvent;
                    newEvent.eventType = noteIn > 0.0 ? 1 : 0;
                    newEvent.value = accentIn > 0 ? accentNotes[i] : notes[i];
                    newEvent.velocity = clamp(accentIn > 0 ? accentVelocity[i] + (rnd * accentVelocityRandom[i]) : velocity[i] + (rnd * velocityRandom[i]),0.0,127.0);
                    midiEvents.push_back(newEvent);
                    eventCount = midiEvents.size();

                    ticksSinceLastEvent = 0;
                }
            }

            if(firstEventReceived) {//Don't start counting until we get our first event, so there isn't a bunch of empty space
                stepCount++;
                if(stepCount >= tickLength) {
                    stepCount -= tickLength;
                    ticksSinceLastEvent++;
                    tickCount++;
                    if(tickCount >= ticksPerQN) {
                        totalTime++;
                        tickCount=0;
                    }                
                }
            }
        }
	}

    
	
	
	// For more advanced Module features, read Rack's engine.hpp header file
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

};


struct MidiRecorderDisplay : TransparentWidget {
	MidiRecorder *module;
	std::shared_ptr<Font> font;
    std::string fontPath;

	MidiRecorderDisplay() {
        fontPath = asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf");
	}


    void drawNotes(const DrawArgs &args, Vec pos) {
        nvgFontSize(args.vg, 10);
        nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);


		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));

        for(int c=0;c<CHANNEL_COUNT;c++) {
            //snprintf(text, sizeof(text), "%s", module->noteNames[c]);
            if(module->midiNoteDisplayMode) {
                
                if(module->useDrumMap) {
            		nvgFontSize(args.vg, 7);

                    snprintf(text, sizeof(text), "%s", module->drumNoteNames[module->notes[c]].c_str());          
                    nvgText(args.vg, pos.x + 3 + c*55, pos.y+1, text, NULL);

                    snprintf(text, sizeof(text), "%s", module->drumNoteNames[module->accentNotes[c]].c_str());          
                    nvgText(args.vg, pos.x + 3 + c*55, pos.y+131, text, NULL);

                } else {            
                    int octave = (module->notes[c] / 12) - 1.0;
                    int note = module->notes[c] % 12;

                    snprintf(text, sizeof(text), "%s%i", module->noteNames[note],octave);          
                    nvgText(args.vg, pos.x + c*55, pos.y+1, text, NULL);

                    octave = (module->accentNotes[c] / 12) - 1.0;
                    note = module->accentNotes[c] % 12;

                    snprintf(text, sizeof(text), "%s%i", module->noteNames[note],octave);          
                    nvgText(args.vg, pos.x + c*55, pos.y+131, text, NULL);
                }
            } else {
                snprintf(text, sizeof(text), "%i", module->notes[c]);          
                nvgText(args.vg, pos.x + c*55, pos.y+1, text, NULL);

                snprintf(text, sizeof(text), "%i", module->accentNotes[c]);          
                nvgText(args.vg, pos.x + c*55, pos.y+131, text, NULL);
            }	    
        }
	}

    void drawBPM(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 20);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);


		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));

        snprintf(text, sizeof(text), "%3.0f", module->bpm);          
        nvgText(args.vg, pos.x, pos.y, text, NULL);

	}



    void drawTime(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 20);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);


		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));

        snprintf(text, sizeof(text), "%li", module->totalTime);          
        nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawEventCount(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 20);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -1);
		nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);


		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));

        snprintf(text, sizeof(text), "%li", module->eventCount);          
        nvgText(args.vg, pos.x, pos.y, text, NULL);
	}


	void draw(const DrawArgs &args) override {
        font = APP->window->loadFont(fontPath);

		if (!module)
			return;     

		drawNotes(args, Vec(85,40));
		drawBPM(args, Vec(175,345));    
		drawTime(args, Vec(385,330));
		drawEventCount(args, Vec(485,330));
	}
};

struct DrumMapItem : MenuItem {
	MidiRecorder *mrm ;
	void onAction(const event::Action &e) override {
		
		char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, NULL);        //////////dir.c_str(),
		if (path) {
			mrm->loadDrumMap(path);
			mrm->drumMapFile = std::string(path);
			free(path);
		}
	}
};

struct ClearDrumMapItem : MenuItem {
	MidiRecorder *mrm ;
	void onAction(const event::Action &e) override {
        mrm->clearDrumMap();
        mrm->useDrumMap = false;
	}
};


struct MidiRecorderWidget : ModuleWidget {
	MidiRecorderWidget(MidiRecorder *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MidiRecorder.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	   
       {
			MidiRecorderDisplay *display = new MidiRecorderDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addParam(createParam<LEDButton>(Vec(20, 328), module, MidiRecorder::RECORD_PARAM));
		addChild(createLight<LargeLight<RedLight>>(Vec(21.5, 329.5), module, MidiRecorder::RECORDING_LIGHT));


		addInput(createInput<FWPortInSmall>(Vec(40, 328), module, MidiRecorder::RUN_INPUT));
        addInput(createInput<FWPortInSmall>(Vec(100, 328), module, MidiRecorder::BPM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(200, 328), module, MidiRecorder::GATE_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(254, 328), module, MidiRecorder::ACCENT_INPUT));

        for(int c=0;c<CHANNEL_COUNT;c++) {
            addParam(createParam<RoundSmallFWSnapKnob>(Vec(40 + c*55,55), module, MidiRecorder::NOTE_VALUE_PARAM + c));			
            addParam(createParam<RoundSmallFWSnapKnob>(Vec(40 + c*55,90), module, MidiRecorder::NOTE_VELOCITY_PARAM + c));			
            addParam(createParam<RoundSmallFWSnapKnob>(Vec(40 + c*55,125), module, MidiRecorder::NOTE_VELOCITY_RANGE_PARAM + c));			
            addParam(createParam<RoundSmallFWSnapKnob>(Vec(40 + c*55,185), module, MidiRecorder::ACCENT_NOTE_VALUE_PARAM + c));			
            addParam(createParam<RoundSmallFWSnapKnob>(Vec(40 + c*55,220), module, MidiRecorder::ACCENT_VELOCITY_PARAM + c));			
            addParam(createParam<RoundSmallFWSnapKnob>(Vec(40 + c*55,255), module, MidiRecorder::ACCENT_VELOCITY_RANGE_PARAM + c));			

            addInput(createInput<FWPortInSmall>(Vec(67 + c*55, 55), module, MidiRecorder::NOTE_VALUE_INPUT + c));
            addInput(createInput<FWPortInSmall>(Vec(67 + c*55, 90), module, MidiRecorder::NOTE_VELOCITY_INPUT + c));
            addInput(createInput<FWPortInSmall>(Vec(67 + c*55, 125), module, MidiRecorder::NOTE_VELOCITY_RANGE_INPUT + c));
            addInput(createInput<FWPortInSmall>(Vec(67 + c*55, 185), module, MidiRecorder::ACCENT_NOTE_VALUE_INPUT + c));
            addInput(createInput<FWPortInSmall>(Vec(67 + c*55, 220), module, MidiRecorder::ACCENT_VELOCITY_INPUT + c));
            addInput(createInput<FWPortInSmall>(Vec(67 + c*55, 255), module, MidiRecorder::ACCENT_VELOCITY_RANGE_INPUT + c));
        }


        

		//addChild(createLight<LargeLight<GreenLight>>(Vec(190, 284), module, QuadGrooveExpander::CONNECTED_LIGHT));

		
	}

    void loadSample(std::string path) {

    }

    struct MRSaveMidiFile : MenuItem {
		MidiRecorder *module ;
		void onAction(const event::Action &e) override {
			
			osdialog_filters* filters = osdialog_filters_parse("MIDI File:mid");
			char *filename  = osdialog_file(OSDIALOG_SAVE, NULL, NULL, filters);        //////////dir.c_str(),
			if (filename) {

				char *dot = strrchr(filename,'.');
				if(dot == 0 || strcmp(dot,".mid") != 0)
				strcat(filename,".mid");

                module->CreateMidiFile(filename);
				free(filename);
			}
			osdialog_filters_free(filters);
		}
	};

    struct NoteDisplayNameItem : MenuItem {
		MidiRecorder *module;
		void onAction(const event::Action &e) override {
			module->midiNoteDisplayMode = !module->midiNoteDisplayMode;
		}
		void step() override {
			text = "Use Note Names";
			rightText = (module->midiNoteDisplayMode) ? "✔" : "";
		}
	};


    void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		MidiRecorder *module = dynamic_cast<MidiRecorder*>(this->module);
		assert(module);

		MRSaveMidiFile *saveMidiFile = new MRSaveMidiFile();
		saveMidiFile->module = module;
		saveMidiFile->text = "Save MIDI File";
		menu->addChild(saveMidiFile);


		NoteDisplayNameItem *noteDisplayNameItem = new NoteDisplayNameItem();
		noteDisplayNameItem->module = module;
		menu->addChild(noteDisplayNameItem);


		{
			OptionsMenuItem* mi = new OptionsMenuItem("Ticks per Quarter Note");
			mi->addItem(OptionMenuItem("96", [module]() { return module->ticksPerQN == 96; }, [module]() { module->ticksPerQN = 96; }));
			mi->addItem(OptionMenuItem("240", [module]() { return module->ticksPerQN == 240; }, [module]() { module->ticksPerQN = 240; }));
			mi->addItem(OptionMenuItem("441", [module]() { return module->ticksPerQN == 441; }, [module]() { module->ticksPerQN = 441; }));
			mi->addItem(OptionMenuItem("480", [module]() { return module->ticksPerQN == 480; }, [module]() { module->ticksPerQN = 480; }));
			mi->addItem(OptionMenuItem("882", [module]() { return module->ticksPerQN == 882; }, [module]() { module->ticksPerQN = 882; }));
			mi->addItem(OptionMenuItem("960", [module]() { return module->ticksPerQN == 960; }, [module]() { module->ticksPerQN = 960; }));
			mi->addItem(OptionMenuItem("15360 (Cubase)", [module]() { return module->ticksPerQN == 15360; }, [module]() { module->ticksPerQN = 15360; }));
			mi->addItem(OptionMenuItem("16384 (Bitwig)", [module]() { return module->ticksPerQN == 16384; }, [module]() { module->ticksPerQN = 16384; }));
			//OptionsMenuItem::addToMenu(mi, menu);
			menu->addChild(mi);
		}

        spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

        DrumMapItem *drumMapItem = new DrumMapItem;
		drumMapItem->text = "Load Drum Map";
		drumMapItem->mrm = module;
		menu->addChild(drumMapItem);

        ClearDrumMapItem *clearDrumMapItem = new ClearDrumMapItem;
		clearDrumMapItem->text = "Clear Drum Map";
		clearDrumMapItem->mrm = module;
		menu->addChild(clearDrumMapItem);


	}
};

Model *modelMidiRecorder = createModel<MidiRecorder, MidiRecorderWidget>("MidiRecorder");
 