/*
 *	Simple Compressor (runtime function)
 *
 *  File		: SimpleCompProcess.inl
 *	Library		: SimpleSource
 *  Version		: 1.12
 *  Implements	: void SimpleComp::process( double &in1, double &in2 )
 *				  void SimpleComp::process( double &in1, double &in2, double keyLinked )
 *				  void SimpleCompRms::process( double &in1, double &in2 )
 *
 *	� 2006, ChunkWare Music Software, OPEN-SOURCE
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a
 *	copy of this software and associated documentation files (the "Software"),
 *	to deal in the Software without restriction, including without limitation
 *	the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *	and/or sell copies of the Software, and to permit persons to whom the
 *	Software is furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in
 *	all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *	THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *	DEALINGS IN THE SOFTWARE.
 */


#ifndef __SIMPLE_COMP_PROCESS_INL__
#define __SIMPLE_COMP_PROCESS_INL__

namespace chunkware_simple
{
	
	//-------------------------------------------------------------
	INLINE void SimpleComp::process( double keyLinked )
	{
		keyLinked = fabs( keyLinked );		// rectify (just in case)

		// convert key to dB
		keyLinked += DC_OFFSET;				// add DC offset to avoid log( 0 )
		double keydB = lin2dB( keyLinked );	// convert linear -> dB

		// threshold
		double overdB = keydB - threshdB_ - (kneedB_ / 2.0);	// delta over threshold
		if ( overdB < 0.0 ) 
			overdB = 0.0;
			

		// attack/release

		overdB += DC_OFFSET;					// add DC offset to avoid denormal
		AttRelEnvelope::run( overdB, envdB_);	// run attack/release envelope
		envdB_ -= DC_OFFSET;					// subtract DC offset

		/* REGARDING THE DC OFFSET: In this case, since the offset is added before 
		 * the attack/release processes, the envelope will never fall below the offset,
		 * thereby avoiding denormals. However, to prevent the offset from causing
		 * constant gain reduction, we must subtract it from the envelope, yielding
		 * a minimum value of 0dB.
		 */
		if(envdB_ > 0 && envdB_ < kneedB_) {
			double a = envdB_ + kneedB_/2.0;
			double gain = (((1.0/ratio_)-1.0) * a * a) / (2 * kneedB_);
			gainReduction_ = -gain;
			// fprintf(stderr, "ratio: %f  knee: %f  odb:%f   a: %f, gr: %f   gr. %f\n", ratio_,kneedB_,overdB, a,gainReduction_,dB2lin( -gainReduction_ ));
		} else {
			gainReduction_ = envdB_ * ( ratio_ - 1.0 );	// gain reduction (dB)
			// fprintf(stderr, "odb:%f gr: %f   gr. %f\n", overdB, gainReduction_,dB2lin( -gainReduction_ ));
		}

		overdB = envdB_;			// save state


	}

	//-------------------------------------------------------------
	INLINE void SimpleComp::processUpward( double keyLinked )
	{
		keyLinked = fabs( keyLinked );		// rectify (just in case)

		// convert key to dB
		keyLinked += DC_OFFSET;				// add DC offset to avoid log( 0 )
		double keydB = lin2dB( keyLinked );	// convert linear -> dB
		if ( keydB < 0.0 ) 
			keydB = 0.0;

		// threshold
		double underdB = threshdB_ - (kneedB_ / 2.0) - keydB;	// delta under threshold
		if ( underdB < 0.0 ) 
			underdB = 0.0;

			

		// attack/release

		underdB += DC_OFFSET;					// add DC offset to avoid denormal
		AttRelEnvelope::run( underdB, envdB_);	// run attack/release envelope
		envdB_ -= DC_OFFSET;					// subtract DC offset

		/* REGARDING THE DC OFFSET: In this case, since the offset is added before 
		 * the attack/release processes, the envelope will never fall below the offset,
		 * thereby avoiding denormals. However, to prevent the offset from causing
		 * constant gain reduction, we must subtract it from the envelope, yielding
		 * a minimum value of 0dB.
		 */
		if(envdB_ > 0 && envdB_ < kneedB_) {
			double a = envdB_ + kneedB_/2.0;
			double gain = ((1.0 - (1.0/ratio_)) * a * a) / (2 * kneedB_);
			gainReduction_ = -gain;
			// fprintf(stderr, "ratio: %f  knee: %f  udb:%f   a: %f, gr: %f   gr. %f\n", ratio_,kneedB_,underdB, a,gainReduction_,dB2lin( -gainReduction_ ));
		} else {
			gainReduction_ = -(envdB_ * (1.0 - (1.0/ratio_)));	// gain increase (dB)
		// if ( gainReduction_ > 30.0 ) 
		// 	gainReduction_ = 30.0;
			// fprintf(stderr, "indB:%f thdB:%f  udb:%f gr: %f   gr. %f\n",keydB, threshdB_, underdB, gainReduction_,dB2lin( -gainReduction_ ));
		}

		underdB = envdB_;			// save state


	}


	INLINE void SimpleCompRms::process(double sidechain )
	{
		double sum = sidechain;				// power summing
		sum += DC_OFFSET;					// DC offset, to prevent denormal
		ave_.run( sum, aveOfSqrs_ );		// average of squares
		double rms = sqrt( aveOfSqrs_ );	// rms (sort of ...)

		/* REGARDING THE RMS AVERAGER: Ok, so this isn't a REAL RMS
		 * calculation. A true RMS is an FIR moving average. This
		 * approximation is a 1-pole IIR. Nonetheless, in practice,
		 * and in the interest of simplicity, this method will suffice,
		 * giving comparable results.
		 */

		SimpleComp::process( rms );	// rest of process
	}

	INLINE void SimpleCompRms::processUpward(double sidechain )
	{
		double sum = sidechain;				// power summing
		sum += DC_OFFSET;					// DC offset, to prevent denormal
		ave_.run( sum, aveOfSqrs_ );		// average of squares
		double rms = sqrt( aveOfSqrs_ );	// rms (sort of ...)

		/* REGARDING THE RMS AVERAGER: Ok, so this isn't a REAL RMS
		 * calculation. A true RMS is an FIR moving average. This
		 * approximation is a 1-pole IIR. Nonetheless, in practice,
		 * and in the interest of simplicity, this method will suffice,
		 * giving comparable results.
		 */

		SimpleComp::processUpward( rms );	// rest of process
	}

}	// end namespace chunkware_simple

#endif	// end __SIMPLE_COMP_PROCESS_INL__
