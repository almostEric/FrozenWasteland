/*
 *	Simple Compressor (header)
 *
 *  File		: SimpleComp.h
 *	Library		: SimpleSource
 *  Version		: 1.12
 *  Class		: SimpleComp, SimpleCompRms
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


#ifndef __SIMPLE_COMP_H__
#define __SIMPLE_COMP_H__

#include "SimpleHeader.h"		// common header
#include "SimpleEnvelope.h"		// for base class
#include "SimpleGain.h"			// for gain functions

#include <stdio.h> // Temporary
	
namespace chunkware_simple
{
	//-------------------------------------------------------------
	// simple compressor
	//-------------------------------------------------------------
	class SimpleComp : public AttRelEnvelope
	{
	public:
		SimpleComp();
		virtual ~SimpleComp() {}

		// parameters
		virtual void setThresh( double dB );
		virtual void setRatio( double dB );
		virtual void setKnee( double dB );

		virtual double getThresh( void ) const { return threshdB_; }
		virtual double getRatio( void )  const { return ratio_; }
		virtual double getKnee( void )  const { return kneedB_; }

		virtual double getGainReduction( void )  const { return gainReduction_; }

		// runtime
		virtual void initRuntime( void );			// call before runtime (in resume())
		//void process( double &in1, double &in2 );	// compressor runtime process
		//void process( double &in1, double &in2, double keyLinked );	// with stereo-linked key in
		void process( double keyLinked );	// only use analysis
		void processUpward( double keyLinked );	// only use analysis

	private:

		// transfer function
		double threshdB_;		// threshold (dB)
		double kneedB_;			// knee (db)
		double ratio_;			// ratio (compression: < 1 ; expansion: > 1)

		double gainReduction_;

		// runtime variables
		double envdB_;			// over-threshold envelope (dB)

	};	// end SimpleComp class

	//-------------------------------------------------------------
	// simple compressor with RMS detection
	//-------------------------------------------------------------
	class SimpleCompRms : public SimpleComp
	{
	public:
		SimpleCompRms();
		virtual ~SimpleCompRms() {}

		// sample rate
		virtual void setSampleRate( double sampleRate );

		// RMS window
		virtual void setWindow( double ms );
		virtual double getWindow( void ) const { return ave_.getTc(); }

		// runtime process
		virtual void initRuntime( void );			// call before runtime (in resume())
		// void process( double &in1, double &in2 );	// compressor runtime process
		// void process( double &in1, double &in2, double &sidechain );	// with stereo-linked key in
		void process( double sidechain );	// only use analysis
		void processUpward( double keyLinked );	// only use analysis

	private:

		EnvelopeDetector ave_;	// averager
		double aveOfSqrs_;		// average of squares

	};	// end SimpleCompRms class

}	// end namespace chunkware_simple

// include inlined process function
#include "SimpleCompProcess.inl"

#endif	// end __SIMPLE_COMP_H__
