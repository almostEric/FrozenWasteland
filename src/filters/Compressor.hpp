#pragma once

#include <cmath>

struct EnvelopeDetection {
  EnvelopeDetection () {
    prev = 0;
  }

  void setCoefficients(float atttackTime, float releaseTime, float sampleRate) {
    this->sampleRate = sampleRate;
    this->attackTime = exp(-1 / ((sampleRate / 1000) * attackTime));
    this->releaseTime = exp(-1 / ((sampleRate / 1000) * releaseTime));
  }

  float process(float input) {
    input = fabs(input);

    if (input > prev) {
      prev *= attackTime;
      prev += (1 - attackTime) * input;
    } else{
      prev *= releaseTime;
      prev += (1 - releaseTime) * input;
    }

    return prev;
  }

  float attackTime;
  float releaseTime;
  float sampleRate;
  float prev;
};


struct Compressor {
  void setRatio(float ratio) {
    this->ratio = 1.0 / ratio;
  }
  void setThreshold(float threshold) {
    this->threshold = threshold;
  }

  void setCoefficients(float attackTime, float releaseTime, float threshold, float ratio, float sampleRate) {
    this->envelope.setCoefficients(attackTime, releaseTime, sampleRate);
    this->ratio = 1.0 / ratio;
    this->threshold = threshold;
  }

  float process(float input) {
    float level = envelope.process(input);

    if (level <= threshold) {
      return input;
    } else {
      //Calculate new level after compression then apply to input
      float compLevel = ratio * (level - threshold) + threshold;

      return input * compLevel / level;
    }
  }

  EnvelopeDetection envelope;
  float ratio;
  float threshold;
};
