
# Frozen Wasteland VCV plugins

A collection of unusual plugins that will add a certain coolness to your patches.

## BPM LFO
![BPM LFO](./doc/bpmlfo.png)

- Tempo Sync'd LFO.
- Can be either clock driven, or use the BPM output standard of Clockd
- CV Control of Time Multiplication/Division to create LFOs sync'd to some ratio of clock
- Phase Control allows LFOs to be offset (cv controllable). 90° button limits offset to 0, 90, 180, 270 degrees.
- Pairs well with the Quad Algorithmic Rhythm
- When Holding is active, the Outputs stay at last value
- Hold input can either be a gate (which switches each time) or momentary (active while signal is positve)
- If set to Free, LFO still runs while being held (even if outputs don't change), Pause causes LFO to pause.

## BPM LFO 2
![BPM LFO](./doc/bpmlfo2.png)

- Variation of original BPM LFO.
- Can be either clock driven, or use the BPM output standard of Clockd
- CV Control of Time Multiplication/Division and Wave Shape
- Slope control interpolates between selected waveform and a sin wave
- Skew either moves between sawtooth and triangle waves or controls duty cycle of square wave
- Phase Control allows LFOs to be offset (cv controllable). 90° button limits offset to 0, 90, 180, 270 degrees.
- Can either be variable sawtooth/triangle or variable duty cycle square wave

## BPM LFO PhaseX
![BPM LFO PhaseX](./doc/bpx.png)

- Expander for both BPM LFO and BPM LFO 2 offering multiple phase outputs.
- Phase Division specifies the equal phase spacing of the outpus (CV Controllable)
- Integer button forces dision to be integer value, turning it off allows smooth slewing between divisions


## Damian Lillard
![Damian Lillard](./doc/damianlillard.png)
- Voltage Controlled Quad Crossover
- Use Sends/Returns to apply FX to different frequency domains of input signal
- Basic example, distort only the mid lows and/or mid highs so your distortion doesn't remove bottom end
- or, apply different delays to create interesting resonances and other FX
- Video of a snare drum being fed into four delay lines: https://www.youtube.com/watch?v=EB7A_hzMpNI

## Drunken Rampage
![Drunken Rampage](./doc/dr.png)
- Based on Befaco's Rampage module
- New BAC % control makes DR randomly overshoot input voltage (depending on how drunk you have set the channel, it may take a couple tries to reach it)

## Everlasting Glottal Stopper
![Everlasting Glottal Stopper](./doc/egs.png)
- Based on the Rosenburg model of human's larynx
- Pairs well with the Vox Inhumana
- Can be a bit fussy to create harmonically "rich" waves, but playing with the "Closed" setting can find some sweet spots
- Use the Noise parameter to add a "breathy" quality to wave

## Filling Station
![Filling Station](./doc/fs.png)
- Allows pattern based mapping of 4 inputs to 16 outputs
- This is ideal for creating complex sounding drum fills using multiple sound sources (but not limited to that)
- Click on a box and then drag up/down to set the output - you need to start with the left most box and then work right
- Each time an input is triggered, the matched output on that step is triggered, then the step for that input advances
- If repeat mode is "None", once the pattern is finished, nothing will trigger until it is reset (with the RESET button or input)
- In "Highest Step" repeat mode, the pattern will auto-reset when the track with the most steps reaches its end
- In "Last Step" repeat mode, the pattern will auto-reset when all tracks have completed
- In "Independent" repeat mode, each track will reset when it reaches its end.

## The Gardener
![The Gardener](./doc/tg.png)
- Master controller for Seeds of Change (and other sequencers)
- Allows nested sequences of repeated patterns.
- Connect Reseed output to SoC's reseed input to get sequence to repeat every "Reseed Div" clock count
- Connect New Seed Trigger to a source of seed values (say a sequencer or LFO)
- Delay Compensation delays output of Clock Out and Reseed Out triggers by n samples, so that a seed generated by new seed trigger is ready at output in time
- Seed In and Out are polyphonic and there is a delay compensation for the split and merge needed
- Gardener's can be chained together for complex orchestrations of controlled randomness. Check out Dieter Stubler's work.

## Hair Pick
![Hair Pick](./doc/hp.png)
- Based on the comb filter section of Intellijel/Cylonix Rainmaker
- Best bet is to refer to this: https://intellijel.com/wp-content/uploads/2016/01/rainmaker_manual-v109.pdf
- Comb filter size is either based on Clock & Division or Size Parameter - patching a clock in disables Size knob
- V/Oct works using either method
- The Size CV can change size +/- 10% using both Clock or Size
- There are 16 comb patterns, the last 8 are randomly perturbed versions of the first 8
- The Tap count can vary the number of taps between 1 and 64. They are deactivated in a pattern shown in the rainmaker manual
- The Edge Level, Tent Level and Tent Tap control the overall volume of the taps.
- Feedback Type can add non-linearity and exponential decay. Clarinet mode is same as guitar for now.
- The size out allows the comb's length to control other modules (say the feedback delay time in Portland Weather)

## Just a Phase(r)
![Just a Phase(r)](./doc/jp.png)
- Classic Phaser effect with control over nearly everything
- Mod Type controls how the notches are modulated - either logarithmic, constant, logarithmic with notches alternating directions and constant with notches alternating directions
- Span controls the range of the notches, at 100% it is about 4 octaves

## Lissajou LFO.

![Lissajou LFO](./doc/llfo.png)

- Loosely based on ADDAC Systems now discontinued 502 module https://www.modulargrid.net/e/addac-system-addac502-ultra-lissajous
- Each LFO is actually a pair of LFOs (x and y)
- Adjusting them will show harmonic relationship between the two
- Phase of the X oscillators can be adjusted in relation to its Y oscillator
- Shape control allows interpolation between tri/sawooth and sin waveshape
- Skew control moves waveshape between sawtooth - triangle - reverse sawtooth
- Yellow is LFO 1, Blue is LFO 2
- Output 1: (x1 + x2) / 2
- Output 2: (y1 + y2) / 2
- Output 3: (x1 + y1 + x2 + y2) / 4
- Output 4: x1/x2
- Output 5: y1/y2
- Output 6: x1 * x2 (essentially ring modulating them)
- Output 7: y1 * y2
- Output 8: x1 * x2 * y1 * y2

## Manic Compression

![Manic Compression](./doc/mc.png)

- This is a fully CV controllable compressor
- Controls are standard for most compressors (threshold, ratio, attack, release)
- RMS mode uses a window (whose length is controlled by the RMS window controls) to find peaks, otherwise is an instantaneous peak detector
- Attack and Release curves can make curves range from exponential to linear to logarithmic depending on input. 
- Old Opto compressors were famous for having non-linear (exponential in their case) release times
- LP Filter applies a Low pass filter onto the input before it is sent to level detector. This can allow transients to go through before compressor kicks in (great for drums)
- HP Filter applies a Hi pass filter onto the input before it is sent to level detector. This prevents detector reacting to harder to hear low frequencies before compressor kicks in (great for bass)
- Mid-Side mode encodes the inputs into mid-side format and allows compression of either the mid or side portion, then the signal is converted back to stereo
- In the menu, there is an option for "Gate Mode". all of the CV inputs for switches (Bypass, RMS, LP Filter, etc.) are normally triggers. Gate Mode turns them into gate inputs
- In the menu there are three options for the Envelope out - gain reduced linear, linear and exponential. If the last two are used, the output voltage very high with large gain reduction amounts

## Meglomanic Compression

![Megalomanic Compression](./doc/mmc.png)

- Basically 5 band version of Manic Compression
- Each Band has a Filter Cutoff, band 1 is low pass, band 5 is high pass, the rest are band pass with controllable bandwidth
- Each Band compressor's features are the same as above 
- By using Bands' Envelope outs and ins, you can have the detector of one band control another. 
- Band Sidechain allows the filtered signal (whether source or sidechain) to be fed into the detector of another band (or something else)

## Midichlorian

![Midichlorian](./doc/midichlorian.png)
- Allows VSTs in Host to play microtonal scales
- Polyphonic CV input gets broken into a Polyphonic CV out for Host's CV input and a polyphonc Pitch Bend output for Host
- Set Pitch Bend in VST to +/- 1 half step

## Midi Recorder

![Midi Recorder](./doc/midirecorder.png)
- Records Gate events and allows saving them to a MIDI file for use elsewhere.
- Gate and Accent inputs are polyphonic
- Record button starts recording - recommend using something like the Run output of Impomptu Modular's Clocked module to syncronize recording
- BPM input uses Impromptu Modular's BPM output standard (0V = 120BPM)

## Mr. Blue Sky

![Mr. Blue Sky](./doc/mrbluesky.png)

- Yes, I love ELO
- This is shamelessly based on Sebastien Bouffier (bid°°)'s fantastic zINC vocoder
- Generally you patch something "synthy" to the carrier input
- The voice sample (or I like to think "harmonically interesting" source) gets patched into the Mod input
- Each modulator band is normalled to its respective carrier input, but the patch points allow you to have different bands modulate different carrier bands
- You can patch in effects (a delay, perhaps?) between the mod out and carrier in.
- CV Control of over almost everything. I highly recommend playing with the band offset.
- You can either CV the value of the band offset, or send triggers to the + and - Inputs to increment/decrement the offset

## The One Ring (modulator)

![The One Ring Modulator](./doc/oneringmod.png)

- Ring modulation essentially multiplies the two frequencies together, creating non-harmonic sidebands
- Doing RM digitally is easy - just multiply the two signals. But it was tougher to do with an analog circuit.  The traditional design used 4 diodes.
- This module is based on this paper: http://recherche.ircam.fr/pub/dafx11/Papers/66_e.pdf which describes how to model an analog ring modulator with all its quirks
- Let's you simulate different types of diode response as used in analog ring modulators
- Bias is the input voltage level where the diode responds - below that the diode outputs 0V
- Linear is the input voltage level where the diode outputs a linear response. Between the Bias and Linar levels, the diode acts 'weird' or let's say 'harmonically interesting'
- Slope is the strength of the diode output
- These controls can lower the overall output, Gain Compensation keeps output level constant
- The graph display is useful for seeing the diodes' voltage responses
- CV control over everything
- Feedback allows adding the mixed output to the input - this can create some added upper harmonics
### Ring Modulation
- Ring Modulation has always appealed to the math nerd in me, because while the process is pretty straightfoward, the results are really cool.
- example. If you feed into a RM a 200hz sine wave and a 500hz sine wave, the output is the sum and difference of those two frequencies, so 300hz and 700hz.
- Where it gets crazy is when harmonics are involved since the output is the sum and difference of "all" the frequencies. example, let's say you had the 200 and 500hz sine waves, but they both each had a 2nd harmonic as well, so a 400hz and 1000hz. The output would be: 500+200=700,1000+200=1200,500+400=900,1000+400=1400,500-200=300,500-400=100,1000-300=700(again),1000-400=600. So a whole slew of new frequencies got created.
- Most waveforms have *way* more harmonics than this, so the results are really crazy.
- The Dalek Voices in the original Dr. Who were done using ring modulators. Exterminate!!!

## Phased Locked Loop (PLL)

![Phased Lock Loop](./doc/pll.png)

- Inspired by Doepfer's A-196 PLL module
- This is a very weird module and can be kind of "fussy". Recommend reading http://www.doepfer.de/A196.htm
- Added CV control of the LPF that the 196 did not have
- Added Pulse Width and Pulse Width Modulation of the internal VCO
- Generally you want to "listen" to the **SQR OUT**
- You'll want to feed a Square-ish wave into Signal In
- Low **LPF FREQ** settings create a warbling effect, high = craziness
- **EXTERNAL IN** overrides interal VCO
- The **LPF OUT** is normalled to the **VCO CV**, try patching something in between the two
- Five comparator modes: XOR, D type Flip Flop use different boolean logic. Coincidence is based on which signal is "ahead". Fuzzy XOR and Fuzzy Hyperbolic XOR use logic operators based on work by Lofti Zadeh and hyperbolic parabolas respectively
- Does not make pretty sounds, but can be a lot of fun.

## Portland Weather

![Portland Weather](./doc/pw.png)
- Based on the rhythmic tap delay section of Intellijel/Cylonix Rainmaker

### This is a crazy deep module and is best just to play with. 
### Since it is a "rhythmic" delay, short duration or percussive sounds generally work best

- Best bet is to refer to this: https://intellijel.com/downloads/manuals/cylonix-rainmaker_manual.pdf
- Delay time is either based on Clock & Division or Size Parameter - patching a clock in disables Size knob
- There are 16 delay groove patterns. The groove amount balances the delay time between the groove or straight time
- Stacking a tap makes the tap use the delay time of its neighbor to right. If multiple taps are stacked the right most tap controls time
- Stacking is useful for making chords
- Each tap has its own level, pan, filter type, cutoff, Q, pitch shift and pitch detune control
- The left and right channels can independently use a tap # to determine where feedback comes from (the feedback bypasses the filter and pitchshifting)
- The feedback slip controls allow the feedback to be ahead/behind the tap time for dragging effects, etc.
- If feedback tap is set to All, the mix of all 16 taps is fed back (use cautiously)
- If feedback tap is set to Ext, the feedback time input is used. Useful for sycing with say, the Hair Pick
- Reverse mode reverses direction of input buffer - time is based on feedback time
- Ping Pong feeds the L feedback into the right channel and vise versa
- FB Send and Returns allow you to insert FX into the feedback loop
- If engaged, the compressor can limit the feedback.  Red = hard clip, Yellow = -6db compression, Green = adaptive compression
- Ducking allows the input signal to control the volume of the delay, ducking is also available in the feedback loop
- The θ buttons next to the outputs allow the delay signal in that channel to be phase reversed. In headphones, this can make the delay sound like it is behind you 
- In Context Menu, the Grain Number and Grain Size control how the all the pitch shifters work. "RAW" is uses one grain sample but without the a triangle window

## PW Algorithm Expander

![Portland Weather - Algorithm Expander](./doc/pwae.png)

- Create your own grooves for Portland weather's 16 delay taps.
- Can use either Euclidean (yellow) or Golumb Ruler (blue) alogorithms
- Chain the QAR Groove Expander, QAR Warped Space and the QAR Probability Expanders to create dynamic, shifting patterns
- Can be chained after a Tap Breakout expander (but not before)

## PW Grid Control Expander

![Portland Weather - Grid Control Expander](./doc/pwgc.png)

- Modulate parameters of all 16 taps at once.
- Right click on grid for built in patterns
- The grid can be shifted in up/down and left/right using CV
- The Pin Xs control has five modes that change how X values greater than/less than the blue axis line respond to CV
- The X values can be rotated around the axis
- Multiple Grid Controls can be chained
- Can be chained with Tap Breakout Expander
- Can be chained before a Algorithmic expander (but not after)

## PW Tap Breakout Expander

![Portland Weather - Tap Breakout Expander](./doc/pwtb.png)

- Individual Send/Returns for each Tap of Portland Weather.
- Can be chained with Grid Control Expander
- Algorithmic Expander can be chained after (but not before)


## Probably Not(e)

![Probably Not(e)](./doc/pn.png)

- This is a probabilistic quantizer that will use weighted probabilities to quantize note to a scale
- This is based on the ideas of Dieter Stubler
- Input voltage is first quantized to nearest note
- Spread Control allows other notes to be possibly chosen
- Slant Control controls the direction of notes to choose (either below, above, or both)
- Focus Control controls initial probability, at 100% all notes have equal probability, at 0% furthest notes have a probability of 10%, with a linear increase towards center note
- No Repeat controls the probablilty that a note can repeat - at 100% a note will never repeat
- Then within the context of key/scale an additional probability is used - by default, the root and fifth (V) of the scale have the highest proability of being selected.
- If a note is off (red) it will not be selected by the Quantizer
- These probabilities can be changed by the knobs or CV.
- The Save button will overwrite the default weights for a scale. The initialize option from the context menu will restore.
- If Octave Wrap is enabled, notes will stay within the Note In's octave (so a B could would be 11 half-steps above a C), if Octave Wrap is off, the notes will be near the original value
- Octave control allows shifting of the output
- The Shift control moves the weightings of the active notes up or down - this can be used to move between modes within a scale (ie c major to a minor)
- If v/Oct is enabled the Key inputs use a 1v/Oct scaling so the key can follow other quanitizers or MIDI keyboards
- Shift has 3 modes, with the light off, the shift is 1V - per note shift (+ or -). Blue is a v/Oct of scaling where the shift is relative to the key. 
  Green is v/Oct where the note is absolute (so 0V will shift to C)
- Intonation: off = Equal Temperment, Green = Just Intonation, Blue = Pythagorean tuning
- Intonation has a trigger input that switches between equal and whatever intonation was selected
- Weight outputs the current notes probability weight so it can be used to affect other modules
- Change outputs a trigger anytime the quantized note changes
- Weighting changes the overall weighting from linear (0%) to logarithmic (100%). 
- The trigger is monophoic for all voices unless the polyphonic mode button next to it is enabled. Then each polyphonic voice is triggered by a corresponding polyphonic trigger

## Probably Not(e) - Bohlen Pierce
![Probably Not(e) - Bohlen Pierce](./doc/pnbp.png)

- Same basic features a Probable Not(e)
- Based on the Bohelen Pierce tuning system which uses a 3:1 "tritave" divided into 13 notes instead of the traditional 2:1 octave with 12 notes
- For reference: https://en.wikipedia.org/wiki/Bohlen%E2%80%93Pierce_scale

## Probably Not(e) - Arabic
![Probably Not(e) - Arabic](./doc/pna.png)
- Another probabilistic quantizer based on Arabic scales (or more accurately, Arabic jins and maqams)
- This module is based on the work of Johnny Farraj and Sami Abu Shumays. Their book Inside Arabic Music is very highly recommended (and will help understanding how this module works)
- In Maqam Scale mode, the PN-A acts like other quantizers in the Probably Note family, but uses Arabic maqams (which are made up of one or more jins)
- Much more powerful is turning off the scale mode which will then display the complete sayr - or possible modulation routes for a maqam. Since maqams in this mode do not have octave equivalence, the input is limited to -1 to 1v to determine which note to play (negative voltages choose notes below the tonic)
- Use the octave control to change octaves if needed (multi octave melodies are rare in Arabic music)
- use the tonic control to change which key the Maqam is played in (the tonic is limited to realistic values for the Maqam)
- Either use the Jins control to modulate, or even better use the modulate trigger input to use the probability to pick the next jins

## Probably Not(e) - Math Nerd
![Probably Not(e) - Math Nerd](./doc/pnmn.png)

- Familiarity with original Probably Not(e) will help :)
- Probablistic Quantizer that uses generated scales based on any combination of three methods.
- First method: Prime Factors
- - Up to 10 Prime numbers (and the transcendtal/irrational numbers, pi, e and phi) can be used as either numerators or denominators to create pitch ratios
- -  If the value in numerator or denominator is > 0, that factor is used (0s are ignored). 
- - If a value > 1, then powers of the factor up to that value are used (ie if factor is 5 and numerator/denominator value is 3, then combinations 5, 25 and 125 are used)
- - Example: If 2 and 3 are used for numerators and 5 and 7 are used for denominators, pitch ratios generated are 2/5,2/7,2/35,3/5,3/7,3/35,6/5,6/7 and 6/35.
- - ratios < 1 and > 2 are scaled to fit within octave.
- - Tempering allows ratios using a factor to be stretched or shrunk
- - This can potentially generate a *LOT* of pitch ratios
- Second Method Equal Divisions of Octaves (EDOs)
- - Divides the octave into an equal number of pitches - 12 is the standard western equal temperment scale
- - Skip allows only certain divisions to be used. For instance a 113edo with skip set to 31, would take the 1st pitch, the 32nd, the 63rd, etc.
- - The skip does not fit evenly into the # of divisions, the wrap control allows the divisions to "wrap" around from the end and start over, including more pitches
- - Third method is Moments of Symmetry.
- -  Basically there are allowable combinations of small and large steps (the ratio controls the difference in size between the two)
- -  This is based on Erv Wilson's work and I'm probably doing it wrong :) look for more updates to this.
- Next, the number of pitches can be reduced by using an alogorithm is to select from available pitch ratios.
- -  Eucldean tries to find the most balanced distribution of pitches, Golumb Ruler te most unbalanced. Perfect Balance is just strange :)
- Optionally the EDOs can be used to "temper" the notes generated by the other methods. 
- - You can either choose the pitch that is closest to the EDO (best fit) or temper all. 
- - Threshold controls how close the note has to be to the EDO pitch to get adjusted
- - At 0% strength the note is left alone, at 100% it is the EDO pitch.
- - A standard practice is to generate a bunch of notes using prime factors, then use Best Fit at 0% strength to choose notes, but leave strength at 0% to retain the scale's character
- Finally the number of pitches can be further reduced by mapping the pattern of a "standard" scale on to the selected pitches.
- Scale Mapping can either be: 
- - Off - the pitches are not mapped to any standard scale,
- - Spread (green) which tries to match the pattern of the source "standard scale" across the number of pitches in the generated scale. 
- - Repeat (blue) repeats the standard scale pattern for as many notes are in the generated scale.
- - Nearest Neighbor (purple) tries to find pitch closest to the mapped scale (using equal temperment).
- The size of the Octave can be adjusted from (2x - standard to 4x)
- The v/O button controls how input cv is mapped to the generated octave. If octave mapping is off, then it will take 2v to cover all the notes in an octave that is twice the normal size. If octave mapping is enabled, the 1V/Octave input standard will hold true no matter what size the octave of generated scale is (so conceivably 1v in could generate 3v out at highest setting)
- Since it would be difficult to individually control the weighting of so many pitches, a "dissonance" control chooses the overall weighting within the spread. To the left more consonant notes have a higher weighting, to the right more dissonant pitches do. 
- The scale display shows dissonance by height - more dissonant notes are shorter. 
- The weighting of a pitch is shown by its brightness
- If scale mapping is enabled, unused notes become red
- If Scale Weighting is enabled, the individual note weighting of the standard scale also be applied. (ie the 5th has a greater weighting than the 7th in the minor scale)
- From the context menu, the current scale can be exported to a scala file.

## Probably Not(e) - Chord Expander
![PN - Chord Expander](./doc/pnce.png)
- Expander for Probably Note (original version only for now)
- The Quant output of Probably Not(e) becomes polyphonic when expander added
- This will not generate every chord that is possible, but it can create very musical sounding ones.
- Generates 4 note chords based on the root of the current PN note and using notes within the key/scale.
- Dissonance probablilty controls whether notes are randomly diminished or augmented.
- Suspension controls whether the III of scale gets changed to suspended II or IV.

## Probably Not(e) - Octave Probability Expander
![PN - Octave Probability Expander](./doc/pnope.png)
- Expander for Probably Note (original version only for now)
- Allows selected notes generated by PN to be probabalistally octave shifted
- The display shows the overall probability based on the ratio of all the octave shift knobs.
- Probability and note selection is CV controllable
- Multiple expanders can be chained so different notes have different probabilities

## Quad Algorithmic Rhythm

![Quad Algorithmic Rhythm](./doc/qar.png)

- 4 Algorithmic rhythm based triggers
- 5 Algorithms: Euclidean (yellow), Golumb Ruler (blue), Well Formed (green), Perfect Balance (orange), Manual (white), and Boolean (purple). 
Each track can use its own Algorithm (Boolean only available on tracks 3 & 4).
- Accents can have their own algorithm.
- CV Control of Alogorithm. Try hooking EOC output to Algorithm trigger to get alternating rhythms
- CV control of Steps, Divisions and Offset, Padding, Accents and Accent Rotation
- EOC (t) triggers when the track reaches is end. EOC (g) triggers at then end of a groove pattern if a Groove Expander is being used
- QARs can be chained together to create arbitrarily long sequences. This can either be done manually by patching the appropriate outputs and input triggers together, or just by having multiple QARs be adjacent.
When they are connected this way, the Clock, Mute and Reset signals, and Eoc outputs and Start inputs are normallized, this means they are automatically patched together, but you can still override this by patching something in. Additionally the master QAR will output all of the triggers from the attached modules 
- If Chain Mode is Boss, the QAR runs on start up, then stops if the track's Start input is patched, until a Start trigger is received - basically the first QAR should be set to this
- If Chain Mode is Employee, the QAR track will be idle until a Start trigger is received.
- Patch EoC (End of Cycle) outputs to next QAR's Start Inputs
- Last QAR's EoC should be patched back to first QAR's Start Input to create a complete loop.
- May want to consider using an OR module (Qwelk has a nice one) so that mutiple QAR's outs and accent outs can gate a single unit
- Mute Input keeps rhythm running just no output
- https://www.youtube.com/watch?v=ARMxz11z9FU is an example of how to patch a couple QARs together and drive some drum synths
- Normally each track advances one step every clock beat and are independent. If Time Sync is enabled, the selected track becomes the master and the other tracks will fit their patterns to the timing of the master. This allows for complex polyrhythms (ie. 15 on 13 on 11 on 4, etc.)
- If X sync is selected, all tracks sync to the number of steps set by the X Sync control
- If a track's Run Free is enabled, it will not sync to master track - this allows combinations of polyrhythms and polymeters
- https://www.youtube.com/watch?v=eCErJMKAlVY is an example of the QAR with Time Sync enabled
- Reset input and trigger now just reset beat to 0, allowing ratcheting. Holding SHIFT key while doing a reset, performs a hard reset of all timing info like previously
### Euclidean Rhythms (Yellow)
- Euclidean are based upon attempting to equally distribute the divisions among the steps available
- Basic example, with a step count of 16, and 2 divisions, the divisions will be on the 1 and the 9.
- A division of 4 would give you a basic 4 on the floor rhythm
- The Offset control lets you move the starting point of the rhythm to something other than the 1
### Golomb Ruler Rhythms (Turquoise)
- Unlike Euclidean Rhythms which seek to evenly distribute the divisions, Golomb Rulers try to ensure unequal distribution
- Basic example, with a step count of 16, and 4 divisions, the divisions will be on the 1,4,9 and 13.
### Well Formed Rulers (Green)
- Based on the work of A. J. Milne
- https://www.researchgate.net/publication/302345911_XronoMorph_Algorithmic_generation_of_perfectly_balanced_and_well-formed_rhythms
- Needs Well Formed Expander to take full advantage, as there are some additional parameters
- Works best with multiple WF rhythms in a hierarchy
### Perfect Balanced Rhythms (Orange)
- Based on the work of A. J. Milne
- https://www.researchgate.net/publication/302345911_XronoMorph_Algorithmic_generation_of_perfectly_balanced_and_well-formed_rhythms
### Manual Mode (White)
- Click on steps to turn beat on/off.
- Hold SHIFT key while clicking to turn Accents on/off
- Offset is still CV controllable
### Fibonacci Mode (Blue)
- Steps based on the Fibonacci sequence.
### Boolean Logic (Purple)
- Uses a boolean operator to combine the previous two tracks (so 3 = 1 & 2, 4 = 2 & 3)
- Length can't be longer than shortest of the 2 tracks combined
- Division controls the logical operator.
-- 1 = AND
-- 2 = OR
-- 3 = XOR
-- 4 = NAND
-- 5 = NOR
-- 6 = NXOR
- Operators repeat if Division is past 6

## QAR Conditional Expander

![QAR Conditional Expander](./doc/QARC.png)

- Expander Module for Quad Algorithmic Rhythm
- Basically a CV controllable per-step/beat clock divider
- If Conditional Mode is off, beats will fire when clock counts down to 0
- If Conditional Mode on on (purple light),  beats will fire unless clock is 0
- Conditional expanders can be chained so different tracks can have different counts
- Can also be chained with QAR Probability

## QAR Groove Expander

![QAR Groove Expander](./doc/QARG.png)

- Expander Module for Quad Algorithmic Rhythm
- Allows creating patterns that move steps before and after the beat.
- Each step can be shifted -50% to +50% of beat
- Amount interpolates between straignt time and the selected pattern
- Length sets the length of the pattern. It will repeat independently of the length of the track(s) it is applied to
- L=T sets the length of the groove to match the track(s) it is applied to
- Normally pattern is set on a step by step basis. Turning DIV on will make pattern follow the beats it is applied to.
- Random allows the beat to shift up to -50% to +50%.
- Random will work in conjunction with groove patterns
- Gaussian mode uses a distribution pattern so that most beats are close to the original
- Expanders can be chained, so putting another Groove Expander before can allow each track to have its own groove 
- Can also be chained with QAR Probability

## QAR Irrational Rhythm Expander

![QAR Irrational Rhythm Expander](./doc/QARIR.png)

- Expander Module for Quad Algorithmic Rhythm
- Allows for creating irrational rhythms within a track (triplets are the most common).
- Length and Ratio controls create the rhythm - ex: a triplet would be 3:2, a septuplet 7:6.
- If DIVS mode is on, then the IR start on the selected beat, no matter which step it fall on
- Expanders can be chained so tracks can have their own IRs, or multiple IRs on a track
- Right expander(s) have priority over expander on left
- Can also be chained with QAR Groove and any other QAR expander

## QAR Probability Expander

![QAR Probability Expander](./doc/QARP.png)

- Expander Module for Quad Algorithmic Rhythm
- Allows controlling probability on a step by step basis.
- If DIVS mode is on, then the probabilities will change the 1st beat, 2nd beat, etc no matter which step they fall on
- Buttons for each step control grouping. Each track's group is independent
- Group Mode enabled means that if the first step of group is not triggered, then other group members will not, otherwise their probability is used
- Expanders can be chained so tracks can have their own probability patterns
- Left expander has priority over expander(s) on right
- Can also be chained with QAR Groove

## QAR Warped Space Expander
![QAR Warped Space Expander](./doc/qarws.png)

- Expander Module for Quad Algorithmic Rhythm
- Allows warping the duration of beats
- The time of the track will stay constant, but some beats will be faster and others slower.
- The position control allows moving where the warped beats around

## QAR Well Formed Rhythm Expander
![QAR Warped Space Expander](./doc/qarwf.png)

- Expander Module for Quad Algorithmic Rhythm
- Provides extra parameters for Well Formed Rhythms
- Ratio controls the length of long vs. short steps.
- Left Button is hierarchy - if enabled, the steps and ratio of the track are based on the well formed track above it
- Right Button is Complmeent - if green, only plays beats where there is not also a beat at same time in the parent track above it. Yellow uses all tracks above.

## Quantussy Cell

![Quantussy Cell](./doc/qc.png)

- This is based on work by Peter Blasser and Richard Brewster.
- Instantiate any number of cells (odd numbers work best - say 5 or 7)
- This is what is called a **Quantussy Ring**

![Quantussy Ring](./doc/qring.png)

- The Freq control sets the baseline value of the internal LFO
- The Castle output should be connected to the next Cell's CV Input.
- One of the outputs (usually triangle or saw) should be connected to the next Cell's Castle input
- Repeat for each cell. The last cell is connected back to the first cell
- Use any of the wav outputs from any cell to provide semi-random bordering on chaotic CV
- Check out http://pugix.com/synth/eurorack-quantussy-cells/

## Roulette LFO

![Roulette LFO](./doc/rlfo.png)

- Based on the concept of a couple different types of Roulettes: https://en.wikipedia.org/wiki/Roulette_(curve)
- A circle/ellipse rolling either inside a bigger circle/ellipse (Hypotrochoid) or circle/ellipse rolling outside a bigger circle/ellipse (Epitrochoid)
- Ratio controls the size of the big fixed shape in relation to the generating (rolling) shape
- eF and eG control the eccentricity of the fixed shape and generating (rolling) shape respectively. An eccentricity of 1 = circle
- d is the distance of the 'pen' from the center of the generating circle.
- if d = 1 the shapes become a special form of the shapes - either a Hypocycloid or Epicycloid
- θF and θG allow control of the phase of the Fixed and Generating shape respectively

## Seeds of Change

![Seeds of Change](./doc/soc.png)

- Generates 4 pseudo-Random values and 4 pseudo-Random Gates.
- Since initial random seed can be specified by knob or CV, sequences are repeatable.
- Seed Value does not take effect until the reset button is pressed or reset trigger received.
- ![Full Documentation](./doc/SeedsofChange.pdf) 

## Seeds of Change - CV Expander

![Seeds of Change - CV Expander](./doc/soccv.png)
- Adds 12 Additional probabilistic CV outputs to a master SoC.

## Seeds of Change - Gate Expander

![Seeds of Change - CV Expander](./doc/socg.png)
- Adds 12 Additional probabilistic gate outputs to a master SoC.

## Seriously Slow LFO

![Seriously Slow LFO](./doc/sslfo.png)

- Waiting for the next Ice Age? Tidal Modulator too fast paced? This is the LFO for you.
- Generate oscillating CVs with range from 1 minute to 100 months
- Phase Control allows LFOs to be offset (cv controllable). 90° button limits offset to 0, 90, 180, 270 degrees.
- NOTE: Pretty sure my math is correct, but 100 month LFOs have not been unit tested

## Seriously Envelope Generator

![Seriously Slow LFO](./doc/sseg.png)
- The Seriously Slow LFO's new buddy
- Basic Envelope Generator but on looooonnnnnnggggg time scales.

## Slice of Life

![Slice of Life](./doc/sol.png)
- FW's interpretation of Future Sound System's OSC Recombination Engine Eurorack module

- Best bet is to refer to this: https://www.futuresoundsystems.co.uk/returnosc2.html
- PW Skew now has two modes - one like the original, the other where the frequency change is inverted

## String Theory VCO

![String Theory VCO](./doc/st.png)

- Based on the Karplus-Strong plucked string algorithm.
- Triggering "Pluck" causes either a burst of noise or external input to be fed into a short delay line
- Source of noise is internal, but there are several noise types available (white, pink, Gaussian), unless external input is used
- Controlling the length of the delay line will change perceived pitch - longer delays = lower pitches
- Direct control of delay line length or using v/oct pitch control is possible
- Grains control allows multiple 'strings' to be plucked. Phase Offset delays each string, Spread allows each successive string to be detuned
- Feedback is internally processed through a low/higpass FILTER (12 o'clock in no filtering), but can be externally processed through FB send/return
- Feedback can be negative (inverted)
- Feedback Shift allows the feedback of one grain to be fed into another grain
- The initial burst of noise or external input can go through a Windowing function. Green = Hanning, Blue = Blackman
- Grains can be ring modulated either against the internal noise source or an external input. RM Grains controls # of grains that are ring modulated (starting with first)
- If engaged, the compressor can limit the Feedback.  Red = hard clip, Yellow = -6db compression, Green = adaptive compression


## Vox Inhumana

![Vox Inhumana](./doc/voxinhumana.png)

- Generates vowel-ish sounds when given harmonically rich sources
- Pairs well with the Everlasting Glottal Stopper, but sawtooth and pulse waves work well
- Vowel/Voice formants are based on https://www.classes.cs.uchicago.edu/archive/1999/spring/CS295/Computing_Resources/Csound/CsManual3.48b1.HTML/Appendices/table3.html
- Fc allows changing the frequency of all formants at once, over a large range
- The CV of a formant allows a +/- 50% change of base vowel frequency
- The CV of amplitude allows the base level of the vowel/voice to be modified by about 2x.
- Changing the Fc of formants 1 & 2 can make the vowel sound more long or short
- The expander allows CV control of the Q (resonance) of each formant, and to choose 12db/oct slope for the filters

## Contributing

I welcome Issues and Pull Requests to this repository if you have suggestions for improvement.

These plugins are released into the public domain ([CC0](https://creativecommons.org/publicdomain/zero/1.0/)).
