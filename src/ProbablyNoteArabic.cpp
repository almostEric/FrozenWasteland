#include "FrozenWasteland.hpp"
#include "ui/knobs.hpp"
#include "ui/ports.hpp"
#include "ui/menu.hpp"
#include "dsp-noise/noise.hpp"
#include "osdialog.h"
#include <sstream>
#include <iomanip>
#include <time.h>

#include <iostream>
#include <fstream>
#include <string>

#define MAX_NOTES 12
#define MAX_JINS 25
#define MAX_FAMILIES 16
#define MAX_AJNAS_IN_SAYR 13
#define MAX_AJNAS_IN_SCALE 3
#define MAX_MAQAM 8
#define MAX_JINS_NOTES 9
#define SCALE_SIZE 7
#define TRIGGER_DELAY_SAMPLES 5

using namespace frozenwasteland::dsp;

struct Jins {
	const char* Name;
	int NumberMainTones;
	int NumberLowerTones;
	int NumberExtendedTones;
	int TonicPosition;
	int GhammazPosition;
	int NumberTonics;
	int AvailableTonics[MAX_NOTES];
	float Intonation[MAX_JINS_NOTES];
	float Weighting[MAX_JINS_NOTES];	
	int NumberMaqams;
};


struct Maqam {
	const char* Name;
	int NumberAjnasInSayr;
	int AjnasInSayr[MAX_AJNAS_IN_SAYR];
	int SayrOverlapPoint[MAX_AJNAS_IN_SAYR];
	float AjnasWeighting[MAX_AJNAS_IN_SAYR+1];
	bool AjnasUsed[MAX_AJNAS_IN_SAYR+1][MAX_AJNAS_IN_SAYR+1];
	int NumberAjnasInScale;
	int AjnasInScale[MAX_AJNAS_IN_SCALE];
	int ScaleOverlapPoint[MAX_AJNAS_IN_SCALE];
	int AscendingDescending[MAX_AJNAS_IN_SCALE];
};

struct ProbablyNoteArabic : Module {
	enum ParamIds {
		SPREAD_PARAM,
		SPREAD_CV_ATTENUVERTER_PARAM,
		SLANT_PARAM,
		SLANT_CV_ATTENUVERTER_PARAM,
		FOCUS_PARAM,
		FOCUS_CV_ATTENUVERTER_PARAM,
        MAQAM_PARAM,
        FAMILY_PARAM,
		MAQAM_SCALE_MODE_PARAM,
		MODULATE_JINS_PARAM,
        MODULATION_CHANCE_PARAM,
		MODULATION_CHANCE_CV_ATTENUVERTER_PARAM,
        TONIC_PARAM,
        OCTAVE_PARAM,
		OCTAVE_CV_ATTENUVERTER_PARAM,	
		PITCH_RANDOMNESS_PARAM,
		PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM,
		PITCH_RANDOMNESS_GAUSSIAN_PARAM,
        RESET_MAQAM_PARAM,
		RESET_JINS_PARAM,
		JINS_WEIGHT_SCALING_PARAM,
		NOTE_WEIGHT_SCALING_PARAM,
		CURRENT_JINS_PARAM,
		AJNAS_ACTIVE_PARAM,
		AJNAS_WEIGHT_PARAM = AJNAS_ACTIVE_PARAM + MAX_AJNAS_IN_SAYR+1,
        NOTE_ACTIVE_PARAM = AJNAS_WEIGHT_PARAM + MAX_AJNAS_IN_SAYR+1,
        NOTE_WEIGHT_PARAM = NOTE_ACTIVE_PARAM + MAX_JINS_NOTES,
		NON_REPEATABILITY_PARAM = NOTE_WEIGHT_PARAM + MAX_JINS_NOTES,
		NON_REPEATABILITY_CV_ATTENUVERTER_PARAM,
		NUM_PARAMS		
	};
	enum InputIds {
		NOTE_INPUT,
		SPREAD_INPUT,
		SLANT_INPUT,
		FOCUS_INPUT,
		MAQAM_INPUT,
        FAMILY_INPUT,
		MODULATION_CHANCE_INPUT,
        TONIC_INPUT,
        OCTAVE_INPUT,
		PITCH_RANDOMNESS_INPUT,
		RESET_SCALE_INPUT,
		TRIGGER_INPUT,
        EXTERNAL_RANDOM_INPUT,
        EXTERNAL_RANDOM_JINS_INPUT,
        EXTERNAL_RANDOM_OCTAVE_JINS_INPUT,
		MODULATE_JINS_TRIGGER_INPUT,
		JINS_SELECT_INPUT,
        AJNAS_WEIGHT_INPUT,
        NOTE_WEIGHT_INPUT = AJNAS_WEIGHT_INPUT + MAX_AJNAS_IN_SAYR+1,
		NON_REPEATABILITY_INPUT = NOTE_WEIGHT_INPUT + MAX_JINS_NOTES,
		NUM_INPUTS
	};
	enum OutputIds {
		QUANT_OUTPUT,
		WEIGHT_OUTPUT,
		NOTE_CHANGE_OUTPUT,
		CURRENT_JINS_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		MAQAM_SCALE_MODE_LIGHT,
		PITCH_RANDOMNESS_GAUSSIAN_LIGHT,
		AJNAS_ACTIVE_LIGHT = PITCH_RANDOMNESS_GAUSSIAN_LIGHT + 2,
        NOTE_ACTIVE_LIGHT = AJNAS_ACTIVE_LIGHT + (MAX_AJNAS_IN_SAYR+1) * 2 ,
		NUM_LIGHTS = NOTE_ACTIVE_LIGHT + MAX_JINS_NOTES * 2
	};
	enum QuantizeModes {
		QUANTIZE_CLOSEST,
		QUANTIZE_LOWER,
		QUANTIZE_UPPER,
	};


	const char* noteNames[MAX_NOTES] = {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};

// c c#     d       d#     e     f       f#      g       g#      a     a#     b
//{0,111.73,203.91,315.64,386.61,498.04,582.51,701.955,813.69,884.36,996.09,1088.27},
//0         200           400    500           700            900           1100

	float whiteKeys[SCALE_SIZE] = {0,200,400,500,700,900,1100};

//pythagorean minor 3d 294.13, maj 407.82
	Jins defaultJins[MAX_JINS] = {
		{"Rast", 5,2,2,1,5,3, {0,5,7}, {-315.64,-161.73,0,203.91,344.13,498.04,701.955,813.69,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1,0.05}, 7}, 
		{"Nahawand", 5,3,1,1,5,4, {0,2,5,7}, {-386.61,-315.64,-111.78,0,203.91,294.13,498.04,701.955,813.69}, {0.03,0.05,0.1,1,0.5,0.5,0.5,0.8,0.1}, 2}, 
		{"Nikriz", 5,2,2,1,5,4, {0,2,5,7}, {-386.61,-111.73,0,203.91,315.64,582.51,701.955,813.69,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1,0.05}, 2}, 
		{"Ajam", 5,2,1,1,5,3, {0,5,7}, {-315.64,-111.73,0,203.91,407.82,498.04,701.955,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1}, 3}, 
		{"Bayati", 4,2,2,1,4,4, {1,4,9,0}, {-336.61,-203.91,0,161.73,386.61,498.04,701.955,813.69}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 3}, 
		{"Hijaz", 4,2,2,1,4,4, {1,7,9,0}, {-336.61,-203.91,0,111.73,294.13,498.04,701.955,813.69}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 3}, 
		{"Kurd", 4,1,2,1,4,4, {1,7,9,0}, {-203.91,0,121.73,315.64,498.04,701.955,813.69}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 3}, 
		{"Saba", 3,2,3,1,3,3, {1,7,9}, {-336.61,-203.91,0,161.73,294.13,386.61,498.04,582.51}, {0.05,0.1,1,0.5,0.8,0.1,0.05,0.05}, 1}, 
		{"Sikah", 3,3,1,1,3,3, {4,9,11}, {-386.61,-203.91,-111.73,50,203.91,386.61,498.04}, {0.05,0.05,0.1,1,0.5,0.8,0.1}, 5}, 
		{"Jiharkah", 5,2,1,1,5,3, {5,7,11}, {-315.64,-161.73,0,203.91,382.33,473.04,701.955,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1}, 1}, //Extended Jins
		{"Sazkar", 5,2,2,1,5,4, {0,2,5,7}, {-315.64,-161.73,0,244.91,344.13,498.04,701.955,813.69,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1,0.05}, 1}, 
		{"Musta'ar", 3,3,1,1,3,3, {4,9,11}, {-386.61,-203.91,-111.73,50,203.91,386.61,498.04}, {0.03,0.05,0.1,1,0.5,0.8,0.1}, 1}, 
		{"Nahawand Murassa", 5,2,1,1,5,4, {0,2,5,7}, {-315.64,-111.78,0,203.91,294.13,498.04,582.51,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1}, 1}, 
		{"Athar Kurd", 5,2,1,1,5,5, {0,2,5,7,9}, {-315.64,-111.78,0,111.73,315.64,582.51,701.955,884.36}, {0.05,0.1,1,0.5,0.5,0.5,0.8,0.1}, 1}, 
		{"Saba Zamzam", 3,1,3,1,3,3, {1,7,9}, {-203.91,0,111.73,294.13,386.61,498.04,582.51}, {0.05,0.1,1,0.5,0.5,0.8,0.1,0.05}, 1}, 
		{"Lami", 4,0,2,1,4,1, {1}, {0,111.73,315.64,498.04,582.51,813.69}, {1,0.5,0.5,0.8,0.1,0.05}, 1}, 
		{"Upper Rast", 4,1,1,4,1,1, {0}, {-203.91,0,203.91,344.13,498.04,701.955}, {0.1,0.8,0.5,0.5,1.0,0.1}, 0}, //Auxillary Jins for modulations #16
		{"Upper Ajam", 4,1,1,4,1,1, {0}, {-203.91,0,203.91,407.82,498.04,701.955}, {0.1,0.8,0.5,0.5,1.0,0.1}, 0}, 
		{"Saba Dalanshin", 3,1,2,3,1,1, {0}, {-203.91,0,161.73,294.13,498.04,701.955,813.69}, {0.1,0.8,0.5,1,0.1,0.1,0.05}, 0}, 
		{"Hijazkar", 4,2,0,1,1,1, {0}, {-405.86,-111.73,0,111.73,405.86,498.04}, {0.05,0.1,1,0.5,0.5,0.5}, 0}, 
		{"Sikah Baladi", 4,2,0,1,1,1, {7}, {-501.61,-188.91,0,228.91,461.61,548.04}, {0.5,0.5,1,0.5,0.5,0.5}, 0}, 
		{"Mukhalif Sharqi", 3,0,0,1,1,1, {4}, {50,111.73,203.91}, {1,0.5,0.5}, 0}, 
		{"Hijaz Murassa", 4,2,1,1,4,1, {1}, {-203.91,-111.73,0,111.73,407.82,498.04,582.51}, {0.05,0.1,1,0.5,0.5,0.8,0.1}, 0}, 
		{"Ajam Murassa", 5,1,1,1,5,1, {5}, {-111.73,0,203.91,407.82,548.04,701.955,884.36}, {0.1,1,0.5,0.5,0.5,0.8,0.1}, 0}, 	
		{"Husayni", 5,0,0,1,5,1, {0}, {-111.73,0,203.91,407.82,498.04,701.955,884.36}, {0.1,1,0.5,0.5,0.8,0.1,0.05}, 0}, //Not Done
	};

	Maqam defaultMaqams[MAX_FAMILIES][MAX_MAQAM] = {
		{ 
			{"Rast", 13, {16,1,5,0,4,7,1,9,20,18,8,11,2}, {5,5,5,8,5,5,8,8,6,6,3,3,1}, 
				{1.0f,1.0f,0.6,0.6,0.6,0.5,0.5,0.3,0.3,0.4,0.3,0.2,0.2,0.1}, 
				//0                            1                              2                             3                             4                            5                              6                             7                             8                             9                             10                            11                           12                             13                      
				{{1,1,1,1,1,1,1,0,0,1,0,1,1,1},{1,1,1,1,1,0,0,1,1,1,0,0,0,1},{1,1,1,1,1,0,0,1,1,1,1,1,1,0},{1,0,1,1,1,1,0,1,1,0,1,1,1,0},{1,1,1,1,1,0,0,0,0,1,0,0,0,1},{1,0,0,1,1,1,1,1,1,0,0,1,0,0},{1,0,0,0,0,1,1,0,0,0,0,1,0,0},{1,1,0,0,0,0,0,1,0,1,0,0,0,0},{1,1,0,0,0,0,0,0,1,1,1,0,0,0},{1,1,0,0,0,0,0,1,1,1,0,0,0,0},{0,1,1,0,0,0,0,0,0,1,0,1,0,0},{1,1,1,1,1,1,1,0,0,0,0,1,1,0},{1,0,1,1,1,0,0,0,0,0,0,1,1,0},{1,1,0,0,0,0,0,0,0,0,0,0,0,1}},
				3,{0,16,1},{1,5,5},{2,0,1}
			},
			{"Kirdan", 5, {16,1,5,8,0}, {5,5,5,3,8}, 
				{1.0f,1.0f,0.5,0.4,0.3,0.3}, 
				{{1,1,1,1,1,1},{1,1,1,1,0,1},{1,1,1,1,1,0},{1,1,1,1,1,0},{1,0,1,1,1,0},{1,1,0,0,0,1}},
				2,{0,16},{1,5},{2,2}
			}, 
			{"Suznak", 7, {5,4,7,1,1,8,0}, {5,5,5,8,5,3,8}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5,0.4}, 
				{{1,1,1,0,0,0,1,1},{1,1,1,1,1,1,1,1},{1,1,1,1,0,1,1,1},{0,1,1,1,0,1,1,0},{0,1,1,0,1,0,0,0},{1,1,1,1,0,1,1,0},{1,1,1,1,0,1,1,1},{1,1,0,0,0,0,1,1}},
				2,{0,5},{1,5},{2,2}
			},
			{"Nairuz", 5, {4,7,1,5,1}, {5,5,8,5,5}, 
				{1.0f,1.0f,0.5,0.5,0.4,0.3}, 
				{{1,1,1,0,1,1},{1,1,1,1,1,1},{1,1,1,0,1,1},{0,1,0,1,1,0},{1,1,1,1,1,1},{1,1,1,0,1,1}},
				2,{0,4},{1,5},{2,2}
			}, 
			{"Dalanshin", 5, {18,16,1,0,9}, {6,5,5,8,8}, 
				{1.0f,1.0f,0.5,0.4,0.4,0.4}, 
				{{1,1,1,1,1,0},{1,1,1,1,1,1},{1,1,1,1,0,1},{1,1,1,1,0,0},{1,1,1,0,1,1},{0,1,1,0,1,1}},
				2,{0,16},{1,5},{2,2}
			},
			{"Suzdalara", 3, {5,8,11}, {5,3,3}, 
				{1.0f,1.0f,0.5,0.4}, 
				{{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}},
				2,{0,1},{1,5},{2,2}
			},
			{"Mahur", 3, {17,1,5}, {5,5,5}, 
				{1.0f,1.0f,0.5,0.4}, 
				{{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}},
				2,{0,17},{1,5},{2,2}
			}
		},
		{
			{"Nahawand", 8, {6,5,4,17,1,12,3,2}, {5,5,5,5,8,1,3,1}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.4,0.4,0.4,0.4}, 
				//0                   1                   2                   3                   4                   5                   6                   7                    8
				{{1,1,1,1,1,1,1,1,1},{1,1,1,1,0,1,0,1,0},{1,1,1,1,1,1,0,1,0},{1,1,1,1,0,1,0,1,0},{1,0,1,0,1,1,0,0,0},{1,1,1,1,1,1,0,0,0},{1,0,0,0,0,0,1,0,0},{1,1,0,1,0,0,0,1,0},{1,0,0,0,0,0,0,0,0}},
				3,{1,5,6},{1,5,5},{2,0,1}
			},
			{"Ushshaq Masri", 6, {4,1,3,4,0,3}, {5,8,10,12,8,3}, 
				{1.0f,1.0f,0.7,0.7,0.6,0.5,0.4}, 
				{{1,1,1,0,0,0,1},{1,1,1,0,1,1,1},{1,1,1,1,1,1,0},{0,0,1,1,1,0,0},{0,1,1,1,1,1,0},{0,1,1,0,1,1,0},{1,1,0,0,0,0,1}},
				2,{1,4},{1,5},{2,2}
			}
		},
		{
			{"Nikriz", 10, {1,3,17,6,4,12,0,2,19,19}, {5,7,4,9,9,5,5,5,9,5}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5,0.4,0.4,0.4,0.4,0.3}, 
				//0                       1                       2                       3                       4                       5                        6                        7                 
				{{1,1,1,0,0,0,0,0,0,0,1},{1,1,1,0,1,1,0,1,1,1,1},{1,1,1,1,1,0,0,0,0,0,0},{0,0,1,1,0,0,0,0,0,0,0},{0,1,1,0,1,0,0,0,0,0,0},{0,1,0,0,0,1,0,0,1,0,0,0},{0,1,0,0,0,0,1,0,0,0,0},{0,1,0,0,0,1,0,1,1,1,0},{0,1,0,0,0,0,0,1,1,1,0},{0,1,0,0,0,0,0,1,1,1,0},{1,1,0,0,0,0,0,0,0,0,1}},
				2,{2,1},{1,5},{2,2}
			},
			{"Nawa Athar", 8, {19,1,2,1,1,6,4,5}, {5,8,8,5,1,5,5,5}, 
				{1.0f,1.0f,0.8,0.5,0.5,0.5,0.5,0.5}, 
				//0                   1                   2                   3                  4                5                   6                   7
				{{1,1,1,1,1,1,0,0,0},{1,1,1,1,1,0,0,0,1},{1,1,1,1,0,0,1,1,1},{1,1,1,1,0,0,0,0,0},{1,1,1,0,0,0,0},{1,0,0,0,0,1,1,1,1},{0,0,1,0,0,1,1,1,0},{0,0,1,0,0,1,1,1,0},{0,1,0,1,0,1,0,0,1}},
				2,{2,19},{1,5},{2,2}
			}
		},
		{ 
			{"Ajam", 7, {17,6,1,5,18,5,19}, {5,8,5,5,3,1,1}, 
				{1.0f,1.0f,0.8,0.4,0.4,0.4,0.2,0.2}, 
				{{1,1,1,1,1,1,1,1},{1,1,1,1,1,0,0,0},{1,1,1,1,1,0,0,0},{1,1,1,1,1,0,0,0},{1,1,1,1,1,1,0,0},{1,0,0,0,1,1,0,0},{1,0,0,0,0,0,1,0},{1,0,0,0,0,0,0,1}},
				3,{3,17,1},{1,5,5},{2,0,1}
			}, 
			{"Shawq Afza", 5, {5,18,17,1,4}, {5,3,5,5,5}, 
				{1.0f,1.0f,0.6,0.5,0.4,0.4}, 
				{{1,1,1,1,1,0},{1,1,1,1,1,1},{1,1,1,0,0,0},{1,1,0,1,1,0},{1,1,0,1,1,1},{0,1,0,0,1,1}},
				2,{3,5},{1,5},{2,2}
			},
			{"Ajam Ushayran", 11, {3,1,6,1,19,4,18,17,1,4,6}, {8,5,10,9,10,10,5,5,5,3,3}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5,0.4,0.4,0.4,0.4,0.3,0.3}, 
				//0                       1                       2                       3                       4                       5                        6                        7                 
				{{1,1,1,0,0,0,0,0,1,1,0,1},{1,1,1,1,1,1,1,1,1,1,0,1},{1,1,1,1,0,0,1,1,1,0,1,1},{0,1,1,1,0,0,0,0,0,0,0,0},{0,1,0,0,1,0,0,0,0,0,0,0},{0,1,0,0,0,1,0,0,0,0,0,0},{0,1,1,0,0,0,1,0,0,0,0,0},{0,1,1,0,0,0,0,1,1,0,0,0},{1,1,1,0,0,0,0,1,1,1,0,0},{1,0,0,0,0,0,0,0,1,1,0,0},{1,0,1,0,0,0,0,0,0,0,1,1},{1,0,1,0,0,0,0,0,0,0,1,1}},
				3,{3,6,1},{1,3,6},{2,2,2}
			},
		},
		{
			{"Bayati", 13, {1,0,19,3,17,4,5,9,0,3,9,5,7}, {4,4,5,6,5,8,4,7,7,3,3,1,1}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5,0.5,0.5,0.4,0.4,0.4,0.2,0.2,0.1}, 
				//0                             1                             2                             3                             4                             5                            6                             7                             8                                9                             10                            11                                             
				{{1,1,1,1,1,0,1,1,0,0,1,1,1,1},{1,1,0,1,1,0,1,1,0,0,1,1,0,0},{1,0,1,1,0,0,1,1,0,1,0,0,1,0},{1,1,1,1,0,0,0,0,0,0,0,0,0,0},{1,1,0,1,1,1,1,0,0,0,0,0,0,1},{0,0,0,0,1,1,0,0,0,0,1,0,0,0},{1,1,1,0,1,0,1,1,0,0,0,0,0,0},{1,1,1,0,0,0,1,1,1,1,0,0,0,0},{0,0,0,0,0,0,1,1,0,0,0,0,0,0},{0,0,1,0,0,0,0,1,0,1,0,0,0,0},{1,1,0,0,0,1,0,0,0,0,1,1,0,0},{1,1,0,0,0,0,0,0,0,0,1,1,0,0},{1,1,1,0,0,0,0,0,0,0,0,0,1,1},{1,1,1,0,1,0,0,0,0,0,0,0,1,1}},
				2,{4,1},{1,4},{2,2}
			},
			{"Bayati Shuri", 5, {5,4,1,0,7}, {4,8,7,7,4}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5}, 
				{{1,1,1,0,0,0},{1,1,1,1,1,1},{1,1,1,0,0,0},{0,1,0,1,1,0},{0,1,0,1,1,0},{0,1,0,0,0,0}},
				2,{4,5},{1,4},{2,2}
			},
			{"Husayni", 4, {24,0,1,3}, {5,4,4,6}, 
				{1.0f,1.0f,0.5,0.4,0.4}, 
				{{1,1,1,1,1},{1,1,1,1,1},{1,1,1,0,0},{1,1,0,1,1},{1,1,0,1,1}},
				2,{4,24},{1,5},{2,2}
			}
		},
		{
			{"Hijaz", 6, {1,0,5,2,19,4}, {4,4,8,4,6,1}, 
				{1.0f,1.0f,1.0,0.5,0.5,0.5,0.5}, 
				{{1,1,1,1,1,1,1},{1,1,1,1,1,0,1},{1,1,1,1,1,0,1},{1,1,1,1,0,1,0},{1,1,1,0,1,1,0},{1,0,0,1,1,1,0},{1,1,1,0,0,0,1}},
				2,{5,1},{1,4},{2,2}
			},
			{"Hijazkar", 7, {22,19,1,2,1,0,25}, {1,8,8,4,4,4,4}, 
				{1.0f,1.0f,1.0,0.3,0.6,0.6,0.4,0.4}, 
				//0                 1                 2                 3                 4                 5                 6                 7 
				{{1,0,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{1,1,1,0,1,0,0,0},{0,0,0,1,1,0,0,1},{1,1,1,1,1,1,1,0},{1,1,0,0,1,1,1,0},{0,0,0,0,1,1,1,0},{0,0,0,1,1,0,1,1}},
				2,{5,2},{1,4},{2,2}
			},
			{"Zanjaran", 6, {3,5,18,1,2,19}, {4,8,6,4,4,8}, 
				{1.0f,1.0f,1.0,0.5,0.5,0.5,0.5}, 
				{{1,1,1,0,1,1,1},{1,1,1,1,1,1,0},{1,1,1,1,0,0,1},{0,1,1,1,0,0,0},{1,1,0,0,1,1,0},{1,1,0,0,1,1,1},{1,0,1,0,0,1,1}},
				2,{5,3},{1,4},{2,2}
			}
		},
		{
			{"Kurd", 6, {1,0,3,4,6,5}, {4,4,6,8,8,1}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5,0.5}, 
				{{1,1,1,1,0,1,1},{1,1,1,1,0,1,1},{1,1,1,0,1,0,1},{1,1,0,1,0,1,0},{0,1,1,0,1,1,0},{1,1,0,1,1,1,0},{1,1,1,0,0,0,1}},
				2,{6,1},{1,4},{2,2}
			},
			{"Hijazkar Kurd", 5, {1,0,2,6,19}, {4,4,4,8,8}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5}, 
				{{1,1,0,0,0,0},{1,1,1,1,1,0},{0,1,1,1,0,0},{0,1,1,1,0,1},{0,1,0,0,1,1},{0,0,0,1,1,1}},
				2,{6,1},{1,4},{2,2}
			},
			{"Kurd 1950s exp", 10, {1,6,1,13,14,15,5,3,0,4}, {4,6,1,1,1,1,1,4,4,5},
				{1.0f,1.0f,0.5,0.5,0.5,0.4,0.4,0.3,0.2,0.2,0.1}, 
				//0                       1                       2                       3                       4                       5                       6                       7                       8                       9                       10                                                                         
				{{1,1,1,1,1,1,1,1,0,1,0},{1,1,1,0,0,0,0,1,1,0,0},{1,1,1,0,0,1,0,0,0,0,0},{1,0,0,1,0,0,0,0,0,0,1},{1,0,0,0,1,1,0,0,0,0,0},{1,0,1,0,1,1,0,0,0,0,0},{1,0,0,0,0,0,1,0,0,0,0},{0,1,1,0,0,0,0,0,0,0,0},{1,1,1,1,1,1,0,0,1,1,0},{1,1,0,0,0,0,0,0,0,1,1},{0,0,0,1,0,0,0,0,0,1,1}},
				2,{6,1},{1,4},{2,2}
			}
		},
		{
			{"Saba", 10, {5,3,17,2,7,4,3,9,1,0}, {3,6,3,6,8,1,3,3,4,4}, 
				{1.0f,1.0f,1.0,0.5,0.5,0.4,0.4,0.3,0.2,0.2,0.2}, 
				//0                       1                     2                       3                       4                       5                       6                       7                       8                       9                       10                                                                         
				{{1,1,1,0,1,1,1,1,1,0,0},{1,1,1,1,1,0,0,0,0,0},{1,1,1,1,1,0,1,0,0,1,0},{0,1,1,1,0,0,1,0,0,0,0},{0,1,1,0,1,1,0,0,0,0,0},{1,1,0,0,1,1,0,0,0,0,0},{1,0,1,0,0,0,1,1,1,1,1},{1,0,0,1,0,0,1,1,1,0,0},{1,0,0,0,0,0,1,1,1,1,0},{0,0,1,0,0,0,1,0,1,1,1},{0,0,0,0,0,1,1,0,0,1,1}},
				3,{7,5,3},{1,3,6},{2,2,2}
			}
		},
		{
			{"Huzam", 8, {5,0,8,4,16,1,11,21}, {3,6,8,3,3,3,1,1}, 
				{1.0f,1.0f,1.0,0.5,0.5,0.5,0.4,0.4,0.4}, 
				//0                   1                   2                   3                   4                   5                   6                   7                 
				{{1,1,1,1,1,1,1,1,1},{1,1,1,0,1,1,1,0,0},{1,1,1,1,0,1,0,0,0},{1,0,1,1,0,0,0,0,0},{1,1,0,0,1,0,0,0,0},{1,1,1,0,0,1,1,1,0},{1,1,0,0,0,1,1,0,0},{1,0,0,0,0,0,1,1,0},{1,0,0,0,0,0,0,0,0}},
				3,{8,5,0},{1,3,6},{2,2,2}
			},
			{"Sikah", 5, {0,16,1,5,8}, {6,3,3,3,8},
				{1.0f,1.0f,1.0,0.5,0.5,0.5}, 
				{{1,0,1,1,1,1},{0,1,1,1,0,1},{1,1,1,1,0,0},{1,0,1,1,0,0},{1,0,0,0,0,0},{1,1,0,0,0,0}},
				3,{8,16,0},{1,3,6},{2,2,2}
			},
			{"'Iraq", 7, {4,1,5,7,0,8,4}, {3,6,3,3,6,8,10}, 
				{1.0f,1.0f,0.8,0.4,0.4,0.4,0.4,0.2}, 
				{{1,1,0,1,0,0,1,0},{1,1,1,1,1,1,0,1},{0,1,1,1,0,1,0,1},{0,1,0,1,0,1,0,0},{0,1,0,0,1,0,0,0},{0,0,0,1,0,1,1,1},{1,0,0,0,0,1,1,1},{0,1,1,0,0,1,1,1}},
				3,{8,4,0},{1,3,6},{2,2,2}
  			},
			{"Bastanikar", 8, {7,5,3,2,5,4,0,8}, {3,5,8,8,3,3,6,8}, 
				{1.0f,1.0f,0.8,0.5,0.5,0.5,0.5,0.5}, 
				//0                   1                   2                   3                  4                5                   6                   7
				{{1,1,0,0,0,1,1,1,1},{1,1,1,1,0,1,1,0,0},{0,1,1,1,1,0,0,0,0},{0,0,1,1,1,0,0,0,0},{1,1,0,0,0,1,1,1,0},{1,1,0,0,0,1,1,0,0},{1,0,0,0,0,0,0,1,1},{1,0,0,0,0,0,0,1,1}},
				3,{8,7,5},{1,3,5},{2,2,2}
			},
			{"Awj 'Iraq", 5, {8,4,0,5,4}, {8,10,6,3,3}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5}, 
				{{1,1,0,0,1,1},{1,1,1,1,0,0},{0,1,1,1,0,0},{0,1,1,1,1,0},{1,0,0,1,1,1},{1,0,0,0,1,1}},
				3,{8,5,8},{1,3,6},{2,2,2}
			}
		},
		{
			{"Jiharkah", 6, {5,1,16,23,1,20}, {5,5,5,1,2,1}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5}, 
				{{1,1,1,1,1,1,1},{1,1,1,0,0,0,0},{1,1,1,1,1,0,0},{1,0,1,1,1,1,0,0},{1,0,1,1,1,0,0},{1,0,0,0,0,1,0},{1,0,0,0,0,0,1}},
				2,{9,16},{1,5},{2,2}
			}			
		},
		{
			{"Sazkar", 5, {16,1,5,8,0}, {5,5,5,3,8}, 
				{1.0f,1.0f,0.5,0.4,0.3,0.3}, 
				{{1,1,1,1,1,1},{1,1,1,1,0,1},{1,1,1,1,1,0},{1,1,1,1,1,0},{1,0,1,1,1,0},{1,1,0,0,0,1}},
				2,{10,16},{1,5},{2,2}
			}
		},
		{
			{"Musta'ar", 6, {1,0,16,8,5,8}, {3,6,3,8,3,1}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5,0.5}, 
				{{1,1,1,0,0,0,1},{1,1,0,1,0,1,0},{1,1,1,1,1,0,0},{0,1,1,1,0,1,0},{0,0,1,0,1,0,1},{0,1,0,1,0,1,1},{1,0,0,0,1,1,1}},
				3,{11,1,0},{1,3,6},{2,2,2}
			}
		},
		{
			{"Nahawand Murassa", 6, {24,5,4,6,17,24}, {1,4,5,5,5,8}, 
				{1.0f,1.0f,0.5,0.5,0.5,0.5}, 
				{{1,1,1,0,0,0,1},{1,1,0,1,1,0,0},{1,0,1,0,0,0,1},{0,1,0,0,1,1,0,0},{0,1,0,1,1,0,0},{0,0,0,0,0,1,1},{1,0,1,0,0,1,1}},
				2,{24,5},{1,4},{2,2}
			}
		},
		{
			{"Athar Kurd", 7, {19,14,2,1,14,6,6}, {5,1,1,1,5,5,1}, 
				{1.0f,1.0f,0.5,0.5,0.4,0.4,0.4}, 
				//0                 1                 2                 3                 4                 5                 6                 7 
				{{1,1,1,1,1,1,1,1},{1,1,0,1,0,1,1,0},{1,0,1,0,0,1,0,1},{1,0,0,1,1,0,0,0},{1,0,0,1,1,1,1,0},{1,1,1,0,0,1,1,0},{1,1,0,0,0,1,1,0},{1,0,1,0,0,0,0,1}},
				2,{13,19},{1,5},{2,2}
			}
		},
		{
			{"Saba Zamzam", 7, {5,2,3,17,6,13,1}, {3,6,6,3,1,1,4}, 
				{1.0f,1.0f,0.5,0.5,0.4,0.4,0.4}, 
				//0                 1                 2                 3                 4                 5                 6                 7 
				{{1,1,1,0,0,1,1,0},{1,1,1,1,1,0,0,0},{1,1,1,1,0,0,0,0},{1,1,1,1,0,1,0,1},{0,1,0,1,1,0,0,0},{1,0,0,1,0,1,1,1},{1,0,0,0,0,1,1,0},{0,0,0,1,0,0,0,1}},
				3,{14,5,3},{1,3,6},{2,2,2}
			}
		},
		{
			{"Lami", 3, {6,1,2}, {4,4,4}, 
				{1.0f,1.0f,0.5,0.5}, 
				{{1,1,1,1},{1,1,1,0},{1,1,1,1},{1,0,1,1}},
				2,{15,6},{1,4},{2,2}
			}
		}
	};

	Jins jins[MAX_JINS];
	Maqam maqams[MAX_FAMILIES][MAX_MAQAM];
   
	
	//const char* arabicScaleNames[MAX_JINS] = {"عجم","بياتي","حجاز","كرد","نهاوند","نكريز","راست","صبا","سيكاه"};
	
	dsp::SchmittTrigger clockTrigger,modulateJinsTrigger,resetMaqamTrigger,resetJinsTrigger,maqamScaleModeTrigger,pitchRandomnessGaussianTrigger,noteActiveTrigger[MAX_JINS_NOTES],ajnasActiveTrigger[MAX_AJNAS_IN_SAYR+1]; 
	dsp::PulseGenerator noteChangePulse,jinsChangedPulse;
	GaussianNoiseGenerator _gauss;
    
 
    bool noteActive[MAX_JINS_NOTES] = {false};
    float noteInitialProbability[MAX_JINS_NOTES] = {0.0f};
	float actualNoteProbability[MAX_JINS_NOTES] = {0.0};
	
	bool maqamScaleMode = false; 
	bool maqamScaleModeChanged = false;


	int currentAnjas[MAX_AJNAS_IN_SAYR+1] = {0};
	int currentAnjasOverlap[MAX_AJNAS_IN_SAYR+1] = {0};
    bool currentAjnasInUse[MAX_AJNAS_IN_SAYR+1] = {0};
    bool currentAjnasActive[MAX_AJNAS_IN_SAYR+1] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    float currentAjnasWeighting[MAX_AJNAS_IN_SAYR+1] = {0};
	float actualAjnasProbability[MAX_AJNAS_IN_SAYR+1] = {0};
	int numberOfScaleAjnas;
	int currentScaleAjnas[MAX_AJNAS_IN_SCALE];
	int currentScaleAjnasOverlap[MAX_AJNAS_IN_SCALE];
	int currentScaleAjnasDirection[MAX_AJNAS_IN_SCALE];

    int octave = 0;
    int spread = 0;
	float upperSpread = 0.0;
	float lowerSpread = 0.0;
	float slant = 0;
	float focus = 0; 
	float nonRepeat = 0;
	int lastRandomNote = -1; 
	int currentNote = 0;
	int probabilityNote = 0;
	float lastQuantizedCV = 0;
	bool resetTriggered = false;
	bool resetMaqamTriggered = false;
	int lastNote = -1;
	int lastSpread = -1;
	int lastSlant = -1;
	float lastFocus = -1;
	bool pitchRandomGaussian = false;

	int quantizeMode = QUANTIZE_CLOSEST;

	int family = 0;
	int lastFamily = -1;

    int tonic = 0;
    int lastTonic = -1;

	int maqamIndex = 0;
	int lastMaqamIndex = -1;

	int jinsIndex = 0;
	int lastJinsIndex = -1;

	Jins rootJins;
	Jins currentJins;
	Maqam currentMaqam;

	bool ajnasActive[MAX_AJNAS_IN_SAYR+1];
	int numberActiveAjnas = 0;


	int numberMainTones;
	int numberLowerTones;
	int numberExtendedTones;
	int numberAllTones;
	int tonicPosition;
	int ghammazPosition;
	float currentIntonation[MAX_JINS_NOTES];
	float currentWeighting[MAX_JINS_NOTES];	
	int numberMaqams;
	int numberTonics;
	int availableTonics[MAX_NOTES];


	bool triggerDelayEnabled = false;
	float triggerDelay[TRIGGER_DELAY_SAMPLES] = {0};
	int triggerDelayIndex = 0;

	//percentages
	float spreadPercentage = 0;
	float slantPercentage = 0;
	float distributionPercentage = 0;
	float nonRepeatPercentage = 0;
	float octavePercentage = 0;
	float familyPercentage = 0;
	float maqamPercentage = 0;
	float currentJinsPercentage = 0;
	float tonicPercentage = 0;
	float pitchRandomnessPercentage = 0;
	float noteWeightPercentage[MAX_NOTES] = {0};
	float anjasWeightPercentage[MAX_AJNAS_IN_SAYR] = {0};
    

	ProbablyNoteArabic() {
		// Configure the module
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ProbablyNoteArabic::SPREAD_PARAM, 0.0, 9.0, 0.0,"Spread", " Notes");
        configParam(ProbablyNoteArabic::SPREAD_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Spread CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteArabic::SLANT_PARAM, -1.0, 1.0, 0.0,"Slant","%",0,100);
        configParam(ProbablyNoteArabic::SLANT_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Slant CV Attenuation" ,"%",0,100);
		configParam(ProbablyNoteArabic::FOCUS_PARAM, 0.0, 1.0, 0.0,"Focus","%",0,100);
		configParam(ProbablyNoteArabic::FOCUS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Focus CV Attenuation");
		configParam(ProbablyNoteArabic::NON_REPEATABILITY_PARAM, 0.0, 1.0, 0.0,"Non Repeat Probability"," %",0,100);
        configParam(ProbablyNoteArabic::NON_REPEATABILITY_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Non Repeat Probability CV Attenuation","%",0,100);

		
		configParam(ProbablyNoteArabic::FAMILY_PARAM, 0.0, (float)MAX_FAMILIES-1, 0.0,"Family (Root Jins)");
		configParam(ProbablyNoteArabic::MAQAM_PARAM, 0.0, 1.0, 0.0,"Maqam");
		
        
		configParam(ProbablyNoteArabic::TONIC_PARAM, 0.0, 2.0, 0.0,"Tonic");
		configParam(ProbablyNoteArabic::OCTAVE_PARAM, -4.0, 4.0, 0.0,"Octave");
        configParam(ProbablyNoteArabic::OCTAVE_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Octave CV Attenuation","%",0,100);
		configParam(ProbablyNoteArabic::PITCH_RANDOMNESS_PARAM, 0.0, 10.0, 0.0,"Randomize Pitch Amount"," Cents");
        configParam(ProbablyNoteArabic::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM, -1.0, 1.0, 0.0,"Randomize Pitch Amount CV Attenuation","%",0,100);
		

		configParam(ProbablyNoteArabic::NOTE_WEIGHT_SCALING_PARAM, 0.0, 1.0, 0.0,"Note Weight Scaling");
		configParam(ProbablyNoteArabic::JINS_WEIGHT_SCALING_PARAM, 0.0, 1.0, 0.0,"Jins Weight Scaling");


        srand(time(NULL));

        for(int i=0;i<MAX_AJNAS_IN_SAYR+1;i++) { //Should probably be max + 1
			configParam(ProbablyNoteArabic::AJNAS_ACTIVE_PARAM + i, 0.0, 1.0, 0.0,"Jins Active");
			configParam(ProbablyNoteArabic::AJNAS_WEIGHT_PARAM + i, 0.0, 1.0, 0.0,"Jins Weight");
		}

        for(int i=0;i<MAX_JINS_NOTES;i++) {
            configParam(ProbablyNoteArabic::NOTE_ACTIVE_PARAM + i, 0.0, 1.0, 0.0,"Note Active");		
            configParam(ProbablyNoteArabic::NOTE_WEIGHT_PARAM + i, 0.0, 1.0, 0.0,"Note Weight");		
        }

		onReset();
	}

	void reConfigParam (int paramId, float minValue, float maxValue, float defaultValue) {
		ParamQuantity *pq = paramQuantities[paramId];
		pq->minValue = minValue;
		pq->maxValue = maxValue;
		pq->defaultValue = defaultValue;		
	}

    double lerp(double v0, double v1, float t) {
        return (1 - t) * v0 + t * v1;
    }

    int weightedProbability(float *weights,int weightCount, float scaling,float randomIn) {
        double weightTotal = 0.0f;
		double linearWeight, logWeight, weight;
            
        for(int i = 0;i < weightCount; i++) {
			linearWeight = weights[i];
			logWeight = (std::pow(10,weights[i]) - 1) / 10.0;
			weight = lerp(linearWeight,logWeight,scaling);
            weightTotal += weight;
        }

		//Bail out if no weights
			if(weightTotal == 0.0)
			return 0;

        int chosenWeight = -1;        
        double rnd = randomIn * weightTotal;
        for(int i = 0;i < weightCount;i++ ) {
			linearWeight = weights[i];
			logWeight = (std::pow(10,weights[i]) - 1) / 10.0;
			weight = lerp(linearWeight,logWeight,scaling);

            if(rnd < weight) {
                chosenWeight = i;
                break;
            }
            rnd -= weight;
        }
        return chosenWeight;
    }


	int nextActiveNote(int note,int offset) {
		if(offset == 0)
			return note;
		int offsetNote = note;
		bool noteOk = false;
		int noteCount = 0;
		int notesSearched = 0;
		do {
			offsetNote = (offsetNote + 1) % MAX_JINS_NOTES;
			notesSearched +=1;
			if(noteActive[offsetNote]) {
				noteCount +=1;
			}
			noteOk = noteCount == offset;
		} while(!noteOk && notesSearched < MAX_JINS_NOTES);
		return offsetNote;
	}

	int ensureActiveJins(int jins, bool *anjasUsed, bool *anjasStatus,int anjasCount ) {
		if(anjasStatus[jins] && anjasUsed[jins])
			return jins;
		int offsetJins=jins;
		bool jinsOk = false;
		int jinsSearched = 0;
		do {
			offsetJins = (offsetJins + 1) % anjasCount;
			jinsSearched +=1;
			if(anjasStatus[offsetJins] && anjasUsed[offsetJins] ) {
				jinsOk = true;
			}
		} while(!jinsOk && jinsSearched < anjasCount);
		return jinsOk ? offsetJins : -1;
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "triggerDelayEnabled", json_boolean(triggerDelayEnabled));
		json_object_set_new(rootJ, "maqamScaleMode", json_boolean(maqamScaleMode));
		json_object_set_new(rootJ, "pitchRandomGaussian", json_boolean(pitchRandomGaussian));
		json_object_set_new(rootJ, "quantizeMode", json_integer((int) quantizeMode)); 

		

		for(int i=0;i<MAX_JINS;i++) {
			for(int j=0;j<MAX_JINS_NOTES;j++) {
				char buf[100];
				char notebuf[100];
				strcpy(buf, "jinsNoteWeight-");
				strcat(buf, jins[i].Name);
				strcat(buf, ".");
				sprintf(notebuf, "%i", j);
				strcat(buf, notebuf);
				json_object_set_new(rootJ, buf, json_real((float) jins[i].Weighting[j]));
			}
		}


		for(int i=0;i<MAX_FAMILIES;i++) {			
			for(int j=0;j<jins[i].NumberMaqams;j++) {
				for(int k=0;k<=maqams[i][j].NumberAjnasInSayr;k++) {
					char buf[100];
					char notebuf[100];
					strcpy(buf, "ajnasWeight-"); 
					strcat(buf, maqams[i][j].Name);
					strcat(buf, ".");
					sprintf(notebuf, "%i", k);
					strcat(buf, notebuf);
					json_object_set_new(rootJ, buf, json_real((float) maqams[i][j].AjnasWeighting[k]));
				}
			}
		}

		for(int i=0;i<MAX_AJNAS_IN_SAYR+1;i++) {
			char buf[100];
			char notebuf[100];
			strcpy(buf, "ajnasActive-"); 
			sprintf(notebuf, "%i", i);
			strcat(buf, notebuf);
			json_object_set_new(rootJ, buf, json_boolean(currentAjnasActive[i]));			
		}

		return rootJ;
	};

	void dataFromJson(json_t *rootJ) override {


		json_t *ctTd = json_object_get(rootJ, "triggerDelayEnabled");
		if (ctTd) {
			triggerDelayEnabled = json_boolean_value(ctTd);
		}

		json_t *ctMs = json_object_get(rootJ, "maqamScaleMode");
		if (ctMs)
			maqamScaleMode = json_boolean_value(ctMs);
		json_t *ctRg = json_object_get(rootJ, "pitchRandomGaussian");
		if (ctRg)
			pitchRandomGaussian = json_boolean_value(ctRg);

		json_t *sumQm = json_object_get(rootJ, "quantizeMode");
		if (sumQm) {
			quantizeMode = json_integer_value(sumQm);			
		}




		for(int i=0;i<MAX_JINS;i++) {
			for(int j=0;j<MAX_JINS_NOTES;j++) {
				char buf[100];
				char notebuf[100];
				strcpy(buf, "jinsNoteWeight-");
				strcat(buf, jins[i].Name);
				strcat(buf, ".");
				sprintf(notebuf, "%i", j);
				strcat(buf, notebuf);
				json_t *sumJ = json_object_get(rootJ, buf);
				if (sumJ) {
					jins[i].Weighting[j] = json_real_value(sumJ);
				}
			}
		}

		for(int i=0;i<MAX_FAMILIES;i++) {
			for(int j=0;j<jins[i].NumberMaqams;j++) {
				for(int k=0;k<=maqams[i][j].NumberAjnasInSayr;k++) {
					char buf[100];
					char notebuf[100];
					strcpy(buf, "ajnasWeight-"); 
					strcat(buf, maqams[i][j].Name);
					strcat(buf, ".");
					sprintf(notebuf, "%i", k);
					strcat(buf, notebuf);
					json_t *sumJ = json_object_get(rootJ, buf);
					if (sumJ) {
						maqams[i][j].AjnasWeighting[k] = json_real_value(sumJ);
					}
				}
			}
		}

		for(int i=0;i<MAX_AJNAS_IN_SAYR+1;i++) {
			char buf[100];
			char notebuf[100];
			strcpy(buf, "ajnasActive-"); 
			sprintf(notebuf, "%i", i);
			strcat(buf, notebuf);
			json_t *sumJ = json_object_get(rootJ, buf);
			if (sumJ) {
				currentAjnasActive[i] = json_boolean_value(sumJ);
			}
		}

		resetTriggered = true;
	}
	

	void process(const ProcessArgs &args) override {
	
		if (resetMaqamTrigger.process(params[RESET_MAQAM_PARAM].getValue())) {
			resetTriggered = true;	
			resetMaqamTriggered = true;		
			maqams[family][maqamIndex] = defaultMaqams[family][maqamIndex];
			for(int i=0;i<MAX_AJNAS_IN_SAYR+1;i++) {
				currentAjnasActive[i] = true;
			}
		}

		if (resetJinsTrigger.process(params[RESET_JINS_PARAM].getValue())) {
			resetTriggered = true;			
			if(!maqamScaleMode) {
				int jinsId = currentAnjas[jinsIndex]; 
				jins[jinsId] = defaultJins[jinsId];
			} else {
				for(int i=0;i<numberOfScaleAjnas;i++) {
					jins[currentScaleAjnas[i]] = defaultJins[currentScaleAjnas[i]];	
				}
			}
		}

		if (pitchRandomnessGaussianTrigger.process(params[PITCH_RANDOMNESS_GAUSSIAN_PARAM].getValue())) {
			pitchRandomGaussian = !pitchRandomGaussian;
		}		
		lights[PITCH_RANDOMNESS_GAUSSIAN_LIGHT].value = pitchRandomGaussian;

		if (maqamScaleModeTrigger.process(params[MAQAM_SCALE_MODE_PARAM].getValue())) {
			maqamScaleMode = !maqamScaleMode;
			maqamScaleModeChanged = true;
		}		
		lights[MAQAM_SCALE_MODE_LIGHT].value = maqamScaleMode;


        spread = clamp(params[SPREAD_PARAM].getValue() + (inputs[SPREAD_INPUT].getVoltage() * params[SPREAD_CV_ATTENUVERTER_PARAM].getValue()),0.0f,9.0f);
		spreadPercentage = spread / 9.0f;

		slant = clamp(params[SLANT_PARAM].getValue() + (inputs[SLANT_INPUT].getVoltage() / 10.0f * params[SLANT_CV_ATTENUVERTER_PARAM].getValue()),-1.0f,1.0f);
		slantPercentage = slant;

        focus = clamp(params[FOCUS_PARAM].getValue() + (inputs[FOCUS_INPUT].getVoltage() / 10.0f * params[FOCUS_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
		distributionPercentage = focus;

		nonRepeat = clamp(params[NON_REPEATABILITY_PARAM].getValue() + (inputs[NON_REPEATABILITY_INPUT].getVoltage() / 10.0f * params[NON_REPEATABILITY_CV_ATTENUVERTER_PARAM].getValue()),0.0f,1.0f);
		nonRepeatPercentage = nonRepeat;

		family = clamp(params[FAMILY_PARAM].getValue() + (inputs[FAMILY_INPUT].getVoltage() * (MAX_FAMILIES-1) / 10.0),0.0,MAX_FAMILIES-1);
		familyPercentage = family/(MAX_FAMILIES - 1.0f);
		if(family != lastFamily) {

			rootJins = jins[family]; 
			numberTonics = rootJins.NumberTonics;
			numberMaqams = rootJins.NumberMaqams;

			std::copy(rootJins.AvailableTonics,rootJins.AvailableTonics+numberTonics,availableTonics);

			reConfigParam(MAQAM_PARAM,0,std::max((float)(numberMaqams-1 + .01),0.0f),0);
			maqamIndex = 0;
			lastMaqamIndex = -1; //Force update;

			reConfigParam(TONIC_PARAM,0,(float)(numberTonics-1),0);
			tonic = 0;
			lastTonic = -1; //Force Update

			//Reset
			if(lastFamily != -1) {
				params[MAQAM_PARAM].setValue(0);
				params[TONIC_PARAM].setValue(0);
			}


			lastFamily = family;
		}

		maqamIndex = clamp(params[MAQAM_PARAM].getValue() + (inputs[MAQAM_INPUT].getVoltage() * (MAX_MAQAM-1.0f) / 10.0),0.0f,numberMaqams-1.0f);
		maqamPercentage = maqamIndex / (numberMaqams -1.0f);
		if(maqamIndex != lastMaqamIndex || maqamScaleModeChanged || resetMaqamTriggered) {
			currentMaqam = maqams[family][maqamIndex];

			numberActiveAjnas = currentMaqam.NumberAjnasInSayr;
			std::copy(currentMaqam.AjnasInSayr,currentMaqam.AjnasInSayr+MAX_AJNAS_IN_SAYR,currentAnjas+1);
			std::copy(currentMaqam.SayrOverlapPoint,currentMaqam.SayrOverlapPoint+MAX_AJNAS_IN_SAYR,currentAnjasOverlap+1);
			std::copy(currentMaqam.AjnasWeighting,currentMaqam.AjnasWeighting+MAX_AJNAS_IN_SAYR+1,currentAjnasWeighting);

			currentAnjas[0] = family;
			currentAnjasOverlap[0] = 1;

			numberOfScaleAjnas = currentMaqam.NumberAjnasInScale;
			std::copy(currentMaqam.AjnasInScale,currentMaqam.AjnasInScale+MAX_AJNAS_IN_SAYR,currentScaleAjnas);
			std::copy(currentMaqam.ScaleOverlapPoint,currentMaqam.ScaleOverlapPoint+MAX_AJNAS_IN_SAYR,currentScaleAjnasOverlap);
			std::copy(currentMaqam.AscendingDescending,currentMaqam.AscendingDescending+MAX_AJNAS_IN_SAYR,currentScaleAjnasDirection);
			
			if(!maqamScaleMode) {
				reConfigParam(CURRENT_JINS_PARAM,0,(float)(numberActiveAjnas),0);        
				reConfigParam(SPREAD_PARAM,0.0,4.0,0);
			} else {
				reConfigParam(CURRENT_JINS_PARAM,0,0.1,0);
				reConfigParam(SPREAD_PARAM,0.0,3.0,0);
			}

			jinsIndex = 0;
			lastJinsIndex = -1; // Force reload of jins status and weighting
			params[CURRENT_JINS_PARAM].setValue(jinsIndex);

			lastMaqamIndex = maqamIndex;
		}

		//Process Jins Modulations
		bool jinsSeected = false;
		if(modulateJinsTrigger.process(params[MODULATE_JINS_PARAM].getValue() + inputs[MODULATE_JINS_TRIGGER_INPUT].getVoltage()) && !maqamScaleMode) {
			for(int i=0;i<numberActiveAjnas+1;i++) {
				if(ajnasActive[i]) {						
					actualAjnasProbability[i] = clamp(params[AJNAS_WEIGHT_PARAM+i].getValue() + (inputs[AJNAS_WEIGHT_INPUT+i].getVoltage() / 10.0f),0.0f,1.0f);
				} else {
					actualAjnasProbability[i] = 0;
				}
			}

			bool jinsChanged = false;

			do {
				float rnd = ((float) rand()/RAND_MAX);
				if(inputs[EXTERNAL_RANDOM_JINS_INPUT].isConnected()) {
					rnd = inputs[EXTERNAL_RANDOM_JINS_INPUT].getVoltage() / 10.0f;
				}	
				jinsIndex = weightedProbability(actualAjnasProbability, numberActiveAjnas+1, params[JINS_WEIGHT_SCALING_PARAM].getValue(),rnd);				
				jinsChanged = jinsIndex != lastJinsIndex;				
			} while(!jinsChanged);			

			//params[CURRENT_JINS_PARAM].setValue(jinsIndex);	
			currentJinsPercentage = jinsIndex / float(numberActiveAjnas);	
			jinsSeected = true;					
		}

				

		if(inputs[JINS_SELECT_INPUT].isConnected()) {
			//jinsIndex = clamp((int)inputs[JINS_SELECT_INPUT].getVoltage(0),0,numberActiveAjnas+1); 
			jinsIndex = clamp((int)inputs[JINS_SELECT_INPUT].getVoltage(0),0,numberActiveAjnas); 
			currentJinsPercentage = jinsIndex / float(numberActiveAjnas);	
			jinsSeected = true;					
			// params[CURRENT_JINS_PARAM].setValue(jinsIndex);							
		}

		if(!jinsSeected) {
			jinsIndex = params[CURRENT_JINS_PARAM].getValue();	
			currentJinsPercentage = jinsIndex / float(numberActiveAjnas);	
		}					
								
		//Add code to check if parameter got set to inactive jins

		if(jinsIndex != lastJinsIndex || maqamScaleModeChanged || resetTriggered) {
			currentJins = jins[currentAnjas[jinsIndex]];

			tonicPosition = currentJins.TonicPosition;			
			ghammazPosition = currentJins.GhammazPosition;			

			numberMainTones = currentJins.NumberMainTones;
			numberLowerTones = currentJins.NumberLowerTones;
			numberExtendedTones = currentJins.NumberExtendedTones;
			numberAllTones = numberLowerTones + numberMainTones + numberExtendedTones;

			if(!maqamScaleMode) {
				std::copy(currentMaqam.AjnasUsed[jinsIndex],currentMaqam.AjnasUsed[jinsIndex]+MAX_AJNAS_IN_SAYR+1,currentAjnasInUse);

				for(int i=0;i<numberActiveAjnas+1;i++) {
					ajnasActive[i] = currentAjnasInUse[i] && currentAjnasActive[i];	
					params[AJNAS_WEIGHT_PARAM+i].setValue(currentAjnasWeighting[i]);
				}
				for(int i=numberActiveAjnas+1;i<MAX_AJNAS_IN_SAYR+1;i++) {
					ajnasActive[i] = false;	
					params[AJNAS_WEIGHT_PARAM+i].setValue(0.0);
				}

				float overlapIntonationAdjustment = 0.0;
				int overlap = currentAnjasOverlap[jinsIndex] - 1;	
				if(overlap >= SCALE_SIZE) { // If an octave jins
					overlap -=SCALE_SIZE;
					overlapIntonationAdjustment = 1200.0;
				}	  
				overlapIntonationAdjustment += rootJins.Intonation[rootJins.NumberLowerTones + overlap];

				int buttonOffset = MAX_JINS_NOTES - numberAllTones;
				for(int i=0;i<MAX_JINS_NOTES;i++) {
					if(i >= buttonOffset) {			
						noteActive[i] = 1;	
						currentWeighting[i] = currentJins.Weighting[i - buttonOffset];	
						currentIntonation[i] = currentJins.Intonation[i - buttonOffset] + overlapIntonationAdjustment;					
					} else {
						noteActive[i] = 0;
						currentWeighting[i] = 0.0f;
					} 
				}
			} else {				
				//Set lower 2 notes to inactive as they are not in 7 note scale
				for(int j=0;j<2;j++) {						
					noteActive[j] = false;	
					currentWeighting[j] = 0;	
				}
				float overlapIntonationAdjustment = 0.0f;	
				for(int i=0;i<numberOfScaleAjnas;i++) {
					ajnasActive[i] = true;
					params[AJNAS_WEIGHT_PARAM+i].setValue(1);
					if(currentScaleAjnasDirection[i] !=1) {//1 is descending
						Jins scaleJins = jins[currentScaleAjnas[i]];	
						int overlap = currentScaleAjnasOverlap[i] - 1; 				
						if(i > 0) {
							Jins previousScaleJins = jins[currentScaleAjnas[i-1]];	
							int lastOverlap = currentScaleAjnasOverlap[i-1] - 1; 				
							overlapIntonationAdjustment += previousScaleJins.Intonation[previousScaleJins.NumberLowerTones + overlap - lastOverlap];
						}				
						for(int j=overlap;j<SCALE_SIZE;j++) {						
							noteActive[j+2] = true;	
							currentWeighting[j+2] = scaleJins.Weighting[j - overlap + scaleJins.NumberLowerTones];	
							currentIntonation[j+2] = scaleJins.Intonation[j - overlap + scaleJins.NumberLowerTones] + overlapIntonationAdjustment;
						}
					}
				}
				for(int i=numberOfScaleAjnas;i<MAX_AJNAS_IN_SAYR+1;i++) {
					ajnasActive[i] = false;
					params[AJNAS_WEIGHT_PARAM+i].setValue(0);					
				}
			}			
		}

        int tonicIndex = clamp(params[TONIC_PARAM].getValue() + (inputs[TONIC_INPUT].getVoltage() * ((float)numberTonics-1) / 10.0),0.0f,(float)numberTonics-1.0);
		tonicPercentage = tonicIndex / (numberTonics - 1.0f);
		tonic = availableTonics[tonicIndex];
		
        octave = clamp(params[OCTAVE_PARAM].getValue() + (inputs[OCTAVE_INPUT].getVoltage() * 0.4 * params[OCTAVE_CV_ATTENUVERTER_PARAM].getValue()),-4.0f,4.0f);
		octavePercentage = (octave + 4.0f) / 8.0f;
	
        double noteIn = inputs[NOTE_INPUT].getVoltage();
		float octaveAdjust = 0.0;
		double octaveIn = 0.0;

		if(!maqamScaleMode) {
			noteIn = clamp(noteIn,-1.0,0.9999f);

			octaveIn = std::floor(noteIn);
			// double fractionalValue = noteIn - octaveIn;
			// double lastDif = 1.0f;    
			// for(int i = 0;i<SCALE_SIZE;i++) {            
			// 	double currentDif = std::abs( - fractionalValue);
			// 	if(currentDif < lastDif) {
			// 		lastDif = currentDif;
			// 		currentNote = i;
			// 	}            
			// }
			double fractionalValue = std::abs(noteIn - octaveIn);
			double lastDif = 99.0f;    
			for(int i = 0;i<SCALE_SIZE;i++) {
				double lowNote = whiteKeys[i] / 1200.0;
				double highNote = whiteKeys[(i+1) % SCALE_SIZE] / 1200.0;
				if(i == SCALE_SIZE-1) {
					highNote +=1200;
				}
				double median = (lowNote + highNote) / 2.0;
				            
				double lowNoteDif = std::abs(lowNote - fractionalValue);
				double highNoteDif = std::abs(highNote - fractionalValue);
				double medianDif = std::abs(median - fractionalValue);

				double currentDif;
				bool direction = lowNoteDif < highNoteDif;
				int note;
				switch(quantizeMode) {
					case QUANTIZE_CLOSEST :
						currentDif = medianDif;
						note = direction ? i : (i + 1) % MAX_NOTES;
						break;
					case QUANTIZE_LOWER :
						currentDif = lowNoteDif;
						note = i;
						break;
					case QUANTIZE_UPPER :
						currentDif = highNoteDif;
						note = (i + 1) % MAX_NOTES;
						break;
				}

				if(currentDif < lastDif) {
					lastDif = currentDif;
					currentNote = note;
				}            
			}

			

			switch ((int)octaveIn) {
				case -1 : 
					currentNote = clamp(currentNote-4,std::min(3-numberLowerTones,2),2) + (MAX_JINS_NOTES - numberAllTones) - (3-numberLowerTones);
					octaveAdjust = 1;
					break;
				case 0 :
					currentNote = clamp(currentNote,0, numberMainTones + numberExtendedTones - 1) + numberLowerTones + (MAX_JINS_NOTES - numberAllTones);
					break;
				break;				
			}
			
		} else {
			octaveIn = std::floor(noteIn);
			double fractionalValue = noteIn - octaveIn;
			double lastDif = 1.0f;    
			for(int i = 0;i<SCALE_SIZE;i++) {            
				double currentDif = std::abs((whiteKeys[i] / 1200.0) - fractionalValue);
				if(currentDif < lastDif) {
					lastDif = currentDif;
					currentNote = i+2;
				}            
			}
		}


		if(lastNote != currentNote || lastSpread != spread || lastSlant != slant || lastFocus != focus || maqamScaleModeChanged) {
			for(int i = 0; i<MAX_JINS_NOTES;i++) {
				noteInitialProbability[i] = 0.0;
			}
			noteInitialProbability[currentNote] = 1.0;
			upperSpread = std::ceil((float)spread * std::min(slant+1.0,1.0));
			lowerSpread = std::ceil((float)spread * std::min(1.0-slant,1.0));

			for(int i=1;i<=spread;i++) {
				int noteAbove = (currentNote + i);
				int noteBelow = (currentNote - i);
				if(maqamScaleMode) {
					noteAbove = (currentNote + i) % MAX_JINS_NOTES;
					if(noteAbove < 2)
						noteAbove += 2;					
					noteBelow = currentNote - i;
					if(noteBelow < 2)
						noteBelow += SCALE_SIZE;					
				}

				if(noteAbove < MAX_JINS_NOTES) {
					float upperInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/(float)spread); 
					noteInitialProbability[noteAbove] = i <= upperSpread ? upperInitialProbability : 0.0f;	
				}			
				if(noteBelow >= 0) {
					float lowerInitialProbability = lerp(1.0,lerp(0.1,1.0,focus),(float)i/(float)spread); 
					noteInitialProbability[noteBelow] = i <= lowerSpread ? lowerInitialProbability : 0.0f;		
				}		
			}
			lastNote = currentNote;
			lastSpread = spread;
			lastSlant = slant;
			lastFocus = focus;
		}

		
	
	
		//Process weights
		if(jinsIndex != lastJinsIndex || maqamScaleModeChanged || resetTriggered) {

				
			for(int i=0;i<MAX_JINS_NOTES;i++) {				
				params[NOTE_WEIGHT_PARAM + i].setValue(currentWeighting[i]);
			}	

			lastTonic = tonic;
			lastJinsIndex = jinsIndex;		
			maqamScaleModeChanged = false;	
			resetTriggered = false;
			resetMaqamTriggered = false;
		}

		int jinsId = !maqamScaleMode ? currentAnjas[jinsIndex] : currentScaleAjnas[jinsIndex]; 
        for(int i=0;i<MAX_AJNAS_IN_SAYR+1;i++) {
			if(i < numberActiveAjnas+1) {
				if (ajnasActiveTrigger[i].process(params[AJNAS_ACTIVE_PARAM+i].getValue())) {
					ajnasActive[i] = !ajnasActive[i];
					currentAjnasActive[i] = !currentAjnasActive[i];
				}
			}

			float anjasWeighting = clamp(params[AJNAS_WEIGHT_PARAM+i].getValue() + (inputs[AJNAS_WEIGHT_INPUT+i].getVoltage() / 10.0f),0.0f,1.0f);
			maqams[family][maqamIndex].AjnasWeighting[i] = anjasWeighting;
			maqams[family][maqamIndex].AjnasUsed[jinsIndex][i] = ajnasActive[i];
			if(ajnasActive[i]) {
				anjasWeightPercentage[i] = anjasWeighting;
	            lights[AJNAS_ACTIVE_LIGHT+i*2].value = anjasWeighting;  
				lights[AJNAS_ACTIVE_LIGHT+(i*2)+1].value = 0;    
			}
			else { 
				anjasWeightPercentage[i] = 0.0;
				lights[AJNAS_ACTIVE_LIGHT+(i*2)].value = 0;    
				lights[AJNAS_ACTIVE_LIGHT+(i*2)+1].value = 1;    
			}
		}

		int buttonOffset = MAX_JINS_NOTES - numberAllTones; 
        for(int i=0;i<MAX_JINS_NOTES;i++) {
            if (i >= buttonOffset && noteActiveTrigger[i].process(params[NOTE_ACTIVE_PARAM+i].getValue())) {
                noteActive[i] = !noteActive[i];
            }			
			float userProbability;
			if(noteActive[i]) {
	            userProbability = clamp(params[NOTE_WEIGHT_PARAM+i].getValue() + (inputs[NOTE_WEIGHT_INPUT+i].getVoltage() / 10.0f),0.0f,1.0f);    
				noteWeightPercentage[i] = userProbability;
				lights[NOTE_ACTIVE_LIGHT+(i*2)].value = userProbability;    
				lights[NOTE_ACTIVE_LIGHT+(i*2)+1].value = 0;    
				jins[jinsId].Weighting[i-buttonOffset] = params[NOTE_WEIGHT_PARAM+i].getValue();
			}
			else { 
				userProbability = 0.0;
				noteWeightPercentage[i] = 0.0;
				lights[NOTE_ACTIVE_LIGHT+(i*2)].value = 0;    
				lights[NOTE_ACTIVE_LIGHT+(i*2)+1].value = 1;    
				jins[jinsId].Weighting[i-buttonOffset] = 0.0;
			}
			
			actualNoteProbability[i] = noteInitialProbability[i] * userProbability; 
			outputs[CURRENT_JINS_OUTPUT].setVoltage(jinsIndex);
        }


		float randomRange = clamp(params[PITCH_RANDOMNESS_PARAM].getValue() + (inputs[PITCH_RANDOMNESS_INPUT].getVoltage() * params[PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM].getValue()),0.0,10.0);
		pitchRandomnessPercentage = randomRange / 10.0;

		if(inputs[TRIGGER_INPUT].active) {
				// fprintf(stderr, "P(N) A Triggered Input Connected %i \n",triggerDelayIndex);

			float currentTriggerInput = inputs[TRIGGER_INPUT].getVoltage();
			triggerDelay[triggerDelayIndex] = currentTriggerInput;
			int delayedIndex = (triggerDelayIndex + 1) % TRIGGER_DELAY_SAMPLES;
			float triggerInputValue = triggerDelayEnabled ? triggerDelay[delayedIndex] : currentTriggerInput;
			triggerDelayIndex = delayedIndex;
			

			if (clockTrigger.process(triggerInputValue) ) {		

				float rnd = ((float) rand()/RAND_MAX);
				if(inputs[EXTERNAL_RANDOM_INPUT].isConnected()) {
					rnd = inputs[EXTERNAL_RANDOM_INPUT].getVoltage() / 10.0f;
				}

				float repeatProbability = ((double) rand()/RAND_MAX);
				if (spread > 0 && nonRepeat > 0.0 && repeatProbability < nonRepeat && lastRandomNote >=0 ) {
					actualNoteProbability[lastRandomNote] = 0; //Last note has no chance of repeating 						
				}


				int randomNote = weightedProbability(actualNoteProbability,MAX_JINS_NOTES,params[NOTE_WEIGHT_SCALING_PARAM].getValue(),rnd);
				if(randomNote == -1) { //Couldn't find a note, so find first active
					bool noteOk = false;
					int notesSearched = 0;
					randomNote = currentNote; 
					do {
						randomNote = (randomNote + 1) % MAX_JINS_NOTES;
						notesSearched +=1;
						noteOk = noteActive[randomNote] || notesSearched >= MAX_JINS_NOTES;
					} while(!noteOk);
				}
				lastRandomNote = randomNote; // for repeatability


				probabilityNote = randomNote;

				double quantitizedNoteCV;
				int notePosition = randomNote;


				float pitchRandomness = 0;
				if(pitchRandomGaussian) {
					bool gaussOk = false; // don't want values that are beyond our mean
					float gaussian;
					do {
						gaussian= _gauss.next();
						gaussOk = gaussian >= -1 && gaussian <= 1;
					} while (!gaussOk);
					pitchRandomness = (2.0 - gaussian) * randomRange / 1200.0;
				} else {
					pitchRandomness = (1.0 - ((double) rand()/RAND_MAX)) * randomRange / 600.0;
				}

				
				//quantitizedNoteCV =(currentIntonation[notePosition] / 1200.0) + (maqamScaleMode ? 0.0 : (tonic /12.0)); 
				quantitizedNoteCV =(currentIntonation[notePosition] / 1200.0) + (tonic /12.0); 
				quantitizedNoteCV += octaveIn + octave + octaveAdjust; 
				outputs[QUANT_OUTPUT].setVoltage(quantitizedNoteCV + pitchRandomness);
				outputs[WEIGHT_OUTPUT].setVoltage(clamp((params[NOTE_WEIGHT_PARAM+randomNote].getValue() + (inputs[NOTE_WEIGHT_INPUT+randomNote].getVoltage() / 10.0f) * 10.0f),0.0f,10.0f));

				if(lastQuantizedCV != quantitizedNoteCV) {
					noteChangePulse.trigger(1e-3);	
					lastQuantizedCV = quantitizedNoteCV;
				}        
			}
			outputs[NOTE_CHANGE_OUTPUT].setVoltage(noteChangePulse.process(1.0 / args.sampleRate) ? 10.0 : 0);
		}

	}

	// For more advanced Module features, see engine/Module.hpp in the Rack API.
	// - dataToJson, dataFromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize: implements custom behavior requested by the user
	void onReset() override;
};

void ProbablyNoteArabic::onReset() {
	clockTrigger.reset();

	triggerDelayEnabled = false;
	for(int i=0;i<TRIGGER_DELAY_SAMPLES;i++) {
		triggerDelay[i] = 0.0f;
	}

	for(int i=0;i<MAX_JINS;i++) {
		jins[i] = defaultJins[i];
	}
	for(int i = 0;i<MAX_FAMILIES;i++) {
		for(int j=0;j<MAX_MAQAM;j++) {
			maqams[i][j] = defaultMaqams[i][j];
		}
	}

	resetTriggered = true;
}




struct ProbablyNoteArabicDisplay : TransparentWidget {
	ProbablyNoteArabic *module;
	int frame = 0;
	std::shared_ptr<Font> font;
	std::string fontPath;

	ProbablyNoteArabicDisplay() {
		fontPath = asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf");
	}


    void drawTonic(const DrawArgs &args, Vec pos, int tonic) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -0.5);

		char text[128];
		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		snprintf(text, sizeof(text), "%s", module->noteNames[tonic]);
		nvgTextAlign(args.vg,NVG_ALIGN_LEFT);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawMaqam(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -0.5);

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->currentMaqam.Name);
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawFamily(const DrawArgs &args, Vec pos) {
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -0.5);

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", module->rootJins.Name);
	
		nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    void drawAjnas(const DrawArgs &args, bool scaleMode, int numberUpperAnjas, int *upperAjnas,int *overlapPoint, int numberScaleAnjas, int *scaleAjnas,int *scaleOverlapPoint) {

		//Draw Boxes first
		for(int i = 0;i<MAX_AJNAS_IN_SAYR+1;i++) {
			nvgStrokeWidth(args.vg, 1.0);
			nvgStrokeColor(args.vg, nvgRGBA(0x99, 0x99, 0x99, 0xff));
			nvgFillColor(args.vg, nvgRGBA(0x10, 0x10, 0x15, 0xff));

			//Jins Name
			nvgBeginPath(args.vg);
			nvgRect(args.vg,66,i*17+129,75,11);
			nvgClosePath(args.vg);		
			nvgStroke(args.vg);
			nvgFill(args.vg);

			//Overlap
			nvgBeginPath(args.vg);
			nvgRect(args.vg,145,i*17+129,15,11);
			nvgClosePath(args.vg);		
			nvgStroke(args.vg);
			nvgFill(args.vg);
		}

		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -0.5);

		nvgFillColor(args.vg, nvgRGBA(0x4a, 0xc3, 0x27, 0xff));
		char text[128];

		if(!scaleMode) { 
			for(int i=0;i<numberUpperAnjas+1;i++) {
				snprintf(text, sizeof(text), "%s", module->jins[upperAjnas[i]].Name);
				float x= 68;
				float y= i*17+137;

				nvgTextAlign(args.vg,NVG_ALIGN_LEFT);
				nvgText(args.vg, x, y, text, NULL);
				
				x= 157;
				snprintf(text, sizeof(text), "%i", overlapPoint[i]);
				nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
				nvgText(args.vg, x, y, text, NULL);
			} 
		} else {
			for(int i=0;i<numberScaleAnjas;i++) {
				snprintf(text, sizeof(text), "%s", module->jins[scaleAjnas[i]].Name);
				float x= 68;
				float y= i*17+137;

				nvgTextAlign(args.vg,NVG_ALIGN_LEFT);
				nvgText(args.vg, x, y, text, NULL);
				
				x= 157;
				snprintf(text, sizeof(text), "%i", scaleOverlapPoint[i]);
				nvgTextAlign(args.vg,NVG_ALIGN_RIGHT);
				nvgText(args.vg, x, y, text, NULL);
			} 
		}
	}


	void drawNoteRange(const DrawArgs &args, bool scaleMode, float *noteInitialProbability, int numberLowerTones,int numberMainTones,int numberExtendedTones, int tonicPosition, int ghammazPosition) 
	{		
		
		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -0.5);

		
		int buttonOffset = 9 - (numberLowerTones + numberMainTones + numberExtendedTones);
		if(scaleMode) {
			buttonOffset = 2;
			numberLowerTones = 0;
		}
		int tonicIndex = numberLowerTones + buttonOffset + tonicPosition - 1;
		int ghammazIndex = numberLowerTones + buttonOffset + ghammazPosition +  - 1;


		// Draw indicator
		for(int i = 0; i<MAX_JINS_NOTES;i++) {
		
			int actualTarget = (i ) % MAX_JINS_NOTES;

			float opacity = noteInitialProbability[actualTarget] * 255;
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, (int)opacity));
			if(actualTarget == module->probabilityNote) {
				nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, (int)opacity));
			}

			float y = 310 - (i*23);
			float x = 173;
			float width = 21;

			nvgBeginPath(args.vg);
			nvgRect(args.vg,x,y,width,23);
			nvgFill(args.vg);
			nvgClosePath(args.vg);

			if(i == tonicIndex) {
				nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
				nvgStrokeWidth(args.vg,0.5);
				nvgBeginPath(args.vg);
				nvgRect(args.vg,x-10,y+8,8,8);
				nvgStroke(args.vg);

				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
				char text[128];
				snprintf(text, sizeof(text), "%s", "T");
				nvgText(args.vg, x-8.5, y+15, text, NULL);
			}

			if(i == ghammazIndex && ghammazIndex != tonicIndex) {
				nvgStrokeWidth(args.vg,0.5);
				nvgBeginPath(args.vg);
				nvgCircle(args.vg,x-6,y+12,4);
				nvgStroke(args.vg);

				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
				char text[128];
				snprintf(text, sizeof(text), "%s", "G");
				nvgText(args.vg, x-8.5, y+15, text, NULL);
			}

		}
	}

	void drawCurrentJins(const DrawArgs &args, int jinsIndex,int numberScaleAjnas, int *scaleDirection, bool scaleMode) 
	{					
		if(!scaleMode) {
			// Draw indicator for current Jins
			float y = 125 + (jinsIndex*17);
			float x = 4;
			float width = 20;
			nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, 0xff));
			nvgBeginPath(args.vg);
			nvgRect(args.vg,x,y,width,17);
			nvgFill(args.vg);			
		} else {
			//Draw indicator for Ajnas in scale
			for(int i=0;i<numberScaleAjnas;i++) {
				float y = 125 + (i*17);
				float x = 4;
				float width = 20;
				switch(scaleDirection[i]) {
					case 0: //Ascending
						nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0x20, 0xff));
						break;
					case 1: //Descending
						nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0xff, 0xff));
						break;
					case 2: //Both
						nvgFillColor(args.vg, nvgRGBA(0x20, 0xff, 0x20, 0xff));
						break;
				}
				nvgBeginPath(args.vg);
				nvgRect(args.vg,x,y,width,17);
				nvgFill(args.vg);		
			}	
		}

	}


	void draw(const DrawArgs &args) override {
		font = APP->window->loadFont(fontPath);


		if (!module)
			return; 

		if(module->lastFamily == -1  || module->lastMaqamIndex == -1)
			return;

		drawFamily(args, Vec(9,82));
		drawMaqam(args, Vec(90,82));
		drawTonic(args, Vec(245,82), module->tonic);
		drawCurrentJins(args,module->jinsIndex,module->numberOfScaleAjnas,module->currentScaleAjnasDirection,module->maqamScaleMode);
		drawNoteRange(args,module->maqamScaleMode, module->noteInitialProbability, module->currentJins.NumberLowerTones,module->currentJins.NumberMainTones,module->currentJins.NumberExtendedTones,module->currentJins.TonicPosition,  module->currentJins.GhammazPosition);
		drawAjnas(args,module->maqamScaleMode,module->numberActiveAjnas, module->currentAnjas,module->currentAnjasOverlap,module->numberOfScaleAjnas,module->currentScaleAjnas,module->currentScaleAjnasOverlap);
	}
};

struct ProbablyNoteArabicWidget : ModuleWidget {
	PortWidget* inputs[MAX_JINS_NOTES];
	ParamWidget* weightParams[MAX_JINS_NOTES];
	ParamWidget* noteOnParams[MAX_JINS_NOTES];
	LightWidget* lights[MAX_JINS_NOTES];  


	ProbablyNoteArabicWidget(ProbablyNoteArabic *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ProbablyNoteArabic.svg")));


		{
			ProbablyNoteArabicDisplay *display = new ProbablyNoteArabicDisplay();
			display->module = module;
			display->box.pos = Vec(0, 0);
			display->box.size = Vec(box.size.x, box.size.y);
			addChild(display);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 12, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		ParamWidget* spreadParam = createParam<RoundSmallFWSnapKnob>(Vec(8,25), module, ProbablyNoteArabic::SPREAD_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(spreadParam)->percentage = &module->spreadPercentage;
		}
		addParam(spreadParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(34,50), module, ProbablyNoteArabic::SPREAD_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(36, 28), module, ProbablyNoteArabic::SPREAD_INPUT));

		ParamWidget* slantParam = createParam<RoundSmallFWKnob>(Vec(65,25), module, ProbablyNoteArabic::SLANT_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(slantParam)->percentage = &module->slantPercentage;
			dynamic_cast<RoundSmallFWKnob*>(slantParam)->biDirectional = true;
		}
		addParam(slantParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(91,49), module, ProbablyNoteArabic::SLANT_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(93, 29), module, ProbablyNoteArabic::SLANT_INPUT));

		ParamWidget* distributionParam = createParam<RoundSmallFWKnob>(Vec(122, 25), module, ProbablyNoteArabic::FOCUS_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(distributionParam)->percentage = &module->distributionPercentage;
		}
		addParam(distributionParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(148,49), module, ProbablyNoteArabic::FOCUS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(150, 29), module, ProbablyNoteArabic::FOCUS_INPUT));

		ParamWidget* nonRepeatParam = createParam<RoundSmallFWKnob>(Vec(179, 25), module, ProbablyNoteArabic::NON_REPEATABILITY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(nonRepeatParam)->percentage = &module->nonRepeatPercentage;
		}
		addParam(nonRepeatParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(205,51), module, ProbablyNoteArabic::NON_REPEATABILITY_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(207, 29), module, ProbablyNoteArabic::NON_REPEATABILITY_INPUT));


		ParamWidget* octaveParam = createParam<RoundSmallFWSnapKnob>(Vec(243,25), module, ProbablyNoteArabic::OCTAVE_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(octaveParam)->percentage = &module->octavePercentage;
		}
		addParam(octaveParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(269,49), module, ProbablyNoteArabic::OCTAVE_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(271, 29), module, ProbablyNoteArabic::OCTAVE_INPUT));


		ParamWidget* familyParam = createParam<RoundSmallFWSnapKnob>(Vec(8,86), module, ProbablyNoteArabic::FAMILY_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(familyParam)->percentage = &module->familyPercentage;
		}
		addParam(familyParam);							
		addInput(createInput<FWPortInSmall>(Vec(36, 90), module, ProbablyNoteArabic::FAMILY_INPUT));

		ParamWidget* maqamParam = createParam<RoundSmallFWSnapKnob>(Vec(88,86), module, ProbablyNoteArabic::MAQAM_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(maqamParam)->percentage = &module->maqamPercentage;
		}
		addParam(maqamParam);							
		addInput(createInput<FWPortInSmall>(Vec(116, 90), module, ProbablyNoteArabic::MAQAM_INPUT));
		addParam(createParam<TL1105>(Vec(139, 97), module, ProbablyNoteArabic::RESET_MAQAM_PARAM));

		ParamWidget* currentJinsParam = createParam<RoundSmallFWSnapKnob>(Vec(168,86), module, ProbablyNoteArabic::CURRENT_JINS_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(currentJinsParam)->percentage = &module->currentJinsPercentage;
		}
		addParam(currentJinsParam);							
		addInput(createInput<FWPortInSmall>(Vec(196, 90), module, ProbablyNoteArabic::JINS_SELECT_INPUT));
		addParam(createParam<TL1105>(Vec(219, 97), module, ProbablyNoteArabic::RESET_JINS_PARAM));


		ParamWidget* tonicParam = createParam<RoundSmallFWSnapKnob>(Vec(243,86), module, ProbablyNoteArabic::TONIC_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWSnapKnob*>(tonicParam)->percentage = &module->tonicPercentage;
		}
		addParam(tonicParam);							
		addInput(createInput<FWPortInSmall>(Vec(271, 90), module, ProbablyNoteArabic::TONIC_INPUT));

		        	


		//Upper Ajnas
		for(int i=0;i<MAX_AJNAS_IN_SAYR+1;i++) {
			float x = 5;
			float y = i * 17 + 125;
			addParam(createParam<LEDButton>(Vec(x, y), module, ProbablyNoteArabic::AJNAS_ACTIVE_PARAM+i));
			addChild(createLight<LargeLight<GreenRedLight>>(Vec(x+1.5, y+1.5), module, ProbablyNoteArabic::AJNAS_ACTIVE_LIGHT+i*2));
			ParamWidget* weightParam = createParam<RoundExtremelySmallFWKnob>(Vec(x + 20,y+2), module, ProbablyNoteArabic::AJNAS_WEIGHT_PARAM+i);
			if (module) {
				dynamic_cast<RoundExtremelySmallFWKnob*>(weightParam)->percentage = &module->anjasWeightPercentage[i];
			}
			addParam(weightParam);							
			addInput(createInput<FWPortInReallySmall>(Vec(x + 40, y+4), module, ProbablyNoteArabic::AJNAS_WEIGHT_INPUT+i));

		}

		for(int i=0;i<MAX_JINS_NOTES;i++) {
			addParam(createParam<LEDButton>(Vec(175, 312 - i*23), module, ProbablyNoteArabic::NOTE_ACTIVE_PARAM+i));
			addChild(createLight<LargeLight<GreenRedLight>>(Vec(176.5, 313.5 - i*23), module, ProbablyNoteArabic::NOTE_ACTIVE_LIGHT+i*2));
			ParamWidget* weightParam = createParam<RoundReallySmallFWKnob>(Vec(195,310 - i*23), module, ProbablyNoteArabic::NOTE_WEIGHT_PARAM+i);
			if (module) {
				dynamic_cast<RoundReallySmallFWKnob*>(weightParam)->percentage = &module->noteWeightPercentage[i];
			}
			addParam(weightParam);							
			addInput(createInput<FWPortInReallySmall>(Vec(219, 314 - i*23), module, ProbablyNoteArabic::NOTE_WEIGHT_INPUT+i));
		}

		addParam(createParam<LEDButton>(Vec(264, 126), module, ProbablyNoteArabic::MAQAM_SCALE_MODE_PARAM));
		addChild(createLight<LargeLight<BlueLight>>(Vec(265.5, 127.5), module, ProbablyNoteArabic::MAQAM_SCALE_MODE_LIGHT));


		addParam(createParam<RoundReallySmallFWKnob>(Vec(245,166), module, ProbablyNoteArabic::JINS_WEIGHT_SCALING_PARAM));		
		addParam(createParam<RoundReallySmallFWKnob>(Vec(285,166), module, ProbablyNoteArabic::NOTE_WEIGHT_SCALING_PARAM));		

		ParamWidget* pitchRandomnessParam = createParam<RoundSmallFWKnob>(Vec(250,205), module, ProbablyNoteArabic::PITCH_RANDOMNESS_PARAM);
		if (module) {
			dynamic_cast<RoundSmallFWKnob*>(pitchRandomnessParam)->percentage = &module->pitchRandomnessPercentage;
		}
		addParam(pitchRandomnessParam);							
        addParam(createParam<RoundReallySmallFWKnob>(Vec(275,230), module, ProbablyNoteArabic::PITCH_RANDOMNESS_CV_ATTENUVERTER_PARAM));			
		addInput(createInput<FWPortInSmall>(Vec(278, 209), module, ProbablyNoteArabic::PITCH_RANDOMNESS_INPUT));
		addParam(createParam<LEDButton>(Vec(252, 237), module, ProbablyNoteArabic::PITCH_RANDOMNESS_GAUSSIAN_PARAM));
		addChild(createLight<LargeLight<GreenLight>>(Vec(253.5, 238.5), module, ProbablyNoteArabic::PITCH_RANDOMNESS_GAUSSIAN_LIGHT));

		addInput(createInput<FWPortInSmall>(Vec(240, 275), module, ProbablyNoteArabic::EXTERNAL_RANDOM_JINS_INPUT));
		addParam(createParam<TL1105>(Vec(270, 275), module, ProbablyNoteArabic::MODULATE_JINS_PARAM));
		addInput(createInput<FWPortInSmall>(Vec(290, 275), module, ProbablyNoteArabic::MODULATE_JINS_TRIGGER_INPUT));


		addInput(createInput<FWPortInSmall>(Vec(240, 313), module, ProbablyNoteArabic::EXTERNAL_RANDOM_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(265, 313), module, ProbablyNoteArabic::TRIGGER_INPUT));
		addInput(createInput<FWPortInSmall>(Vec(290, 313), module, ProbablyNoteArabic::NOTE_INPUT));



		addOutput(createOutput<FWPortInSmall>(Vec(290, 346),  module, ProbablyNoteArabic::QUANT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(263, 346),  module, ProbablyNoteArabic::WEIGHT_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(237, 346),  module, ProbablyNoteArabic::NOTE_CHANGE_OUTPUT));
		addOutput(createOutput<FWPortInSmall>(Vec(210, 346),  module, ProbablyNoteArabic::CURRENT_JINS_OUTPUT));

	}

	struct TriggerDelayItem : MenuItem {
		ProbablyNoteArabic *module;
		void onAction(const event::Action &e) override {
			module->triggerDelayEnabled = !module->triggerDelayEnabled;
			fprintf(stderr, "P(N) A Triggered Delay Changed %i \n",module->triggerDelayEnabled);
		}
		void step() override {
			text = "Trigger Delay";
			rightText = (module->triggerDelayEnabled) ? "✔" : "";
		}
	};

	
	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		ProbablyNoteArabic *module = dynamic_cast<ProbablyNoteArabic*>(this->module);
		assert(module);

		menu->addChild(new MenuLabel());
		{
      		OptionsMenuItem* mi = new OptionsMenuItem("Quantize Mode");
			mi->addItem(OptionMenuItem("Closet", [module]() { return module->quantizeMode == 0; }, [module]() { module->quantizeMode = 0; }));
			mi->addItem(OptionMenuItem("Round Lower", [module]() { return module->quantizeMode == 1; }, [module]() { module->quantizeMode = 1; }));
			mi->addItem(OptionMenuItem("Round Upper", [module]() { return module->quantizeMode == 2; }, [module]() { module->quantizeMode = 2; }));
			menu->addChild(mi);
		}

		TriggerDelayItem *triggerDelayItem = new TriggerDelayItem();
		triggerDelayItem->module = module;
		menu->addChild(triggerDelayItem);

	}
		
};



// Define the Model with the Module type, ModuleWidget type, and module slug
Model *modelProbablyNoteArabic = createModel<ProbablyNoteArabic, ProbablyNoteArabicWidget>("ProbablyNoteArabic");
    