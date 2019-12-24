// soundSynthesizer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//the first letter of each variable name indicates the data type of that variable
//project->linker->additional dependancies, winmm.lib is used by olcNoiseMaker.h


#include <iostream>
using namespace std;

#include "olcNoiseMaker.h"

//this variable should be atomic because the while loop is changing it, and this function is also passed on to 
//olcNoiseMaker.h which constantly runs a thread in the background which may also change it, so to avoid race condition
atomic<double> dFrequencyOutput = 0.0;

namespace synth {

	//convert frequency(Hz) to angular frequency
	double w(double dHertz) {

		return dHertz * 2.0 * PI;
	}
		
	//oscillators
	const int SINE = 0;
	const int SQUARE = 1;
	const int TRIANGLE = 2;
	const int SAW_ANA = 3;
	const int SAW_DIG = 4;
	const int NOISE = 5;

	double osc(double dHertz, double dTime, const int nType = SINE, double dLFOHertz = 0.0, double dLFOAmplitude = 0.0) {
		double dOutput = 0.0;
		double dFreq = w(dHertz) * dTime + dLFOAmplitude * dHertz * sin(w(dLFOHertz) * dTime);
		switch (nType) {

			//sine wave
		case SINE:
			return sin(dFreq);
			//square wave
		case SQUARE:
			return (sin(dFreq)) > 0.0 ? 1.0 : -1.0;

			//triangle wave
		case TRIANGLE:
			return asin(sin(dFreq)) * (2.0 / PI);

			//saw wave (analogue, warm, computationally slow)
		case SAW_ANA:

			for (double n = 1.0; n <= 50.0; n++)
				dOutput += (sin(n * dFreq) / n);
			return dOutput * (2.0 / PI);

			//saw wave (computationally optimised, fast, harsh)
		case SAW_DIG:
			return (2.0 / PI) * (dHertz * PI * fmod(dTime, 1.0 / dHertz) - PI / 2.0);

			//noise
		case NOISE:
			return 2.0 * ((double)rand() / (double)RAND_MAX) - 1.0;

		default:
			return 0.0;
		}
	}

	//duration of a note is usually considered the time between the press and release of the key by player
	//but in reality, the duration maybe be longer, and amplitude may not be constant in this duration, this is the envelope
	//for attack decay sustain and release 
	struct sEnvelopeADSR {
		double dAttackTime; //time needed to reach the peak amplitude(start amplitude) after the player has pressed the key
		double dDecayTime;	//time needed to reach the sustained amplitude, from the start amplitude
		double dReleaseTime; //time needed to reach 0 amplitude after the player has released the key

		double dSustainAmplitude; //amplitude in the sustained phase
		double dStartAmplitude; //initially amplitude maybe higher(peak) before it settles down to a constant amplitude(sustain)

		double dTriggerOnTime; //the instant when the key is pressed
		double dTriggerOffTime; //the instant when the key is released

		//because the release phase happens when the key is released, we need to know the whether key is pressed or not
		//i.e., we need to maintain the state of the key
		bool bNoteOn;

		sEnvelopeADSR() {
			dAttackTime = 0.001; //everything is in seconds
			dDecayTime = 1.0;
			dReleaseTime = 1.0;
			dStartAmplitude = 1.0;
			dSustainAmplitude = 0.0;
			dTriggerOnTime = 0.0;
			dTriggerOffTime = 0.0;
			bNoteOn = false;
		}

		//when the note is pressed we'll call the NoteOn function
		void NoteOn(double dTimeOn) {
			dTriggerOnTime = dTimeOn;
			bNoteOn = true;
		}

		void NoteOff(double dTimeOff) {
			dTriggerOffTime = dTimeOff;
			bNoteOn = false;
		}

		double GetAmplitude(double dTime) {
			double dAmplitude = 0.0;

			//dTime is the "real" time, the wall time, but we need to maintain time for the lifetime of the envelope
			//the envelope time starts from the instant when the key is pressed
			// **envelope time begins when the key is pressed but doesn't end the key is released
			double dEnvelopeTime = 0.0;


			//Attack-decay-sustain
			if (bNoteOn) {

				dEnvelopeTime = dTime - dTriggerOnTime;

				//Attack phase
				if (dEnvelopeTime <= dAttackTime) {
					dAmplitude = (dStartAmplitude / dAttackTime) * dEnvelopeTime;

				}

				//Decay phase
				if ((dEnvelopeTime > dAttackTime) && (dEnvelopeTime <= (dAttackTime + dDecayTime))) {
					dAmplitude = ((dSustainAmplitude - dStartAmplitude) / dDecayTime) * (dEnvelopeTime - dAttackTime) + dStartAmplitude;

				}

				//Sustain phase
				if (dEnvelopeTime > (dAttackTime + dDecayTime)) {
					dAmplitude = dSustainAmplitude;

				}

			}

			//Release
			else {
				//release phase starts from the moment when the key is released
				dEnvelopeTime = dTime - dTriggerOffTime;


				dAmplitude = ((0.0 - dSustainAmplitude) / dReleaseTime) * (dEnvelopeTime)+dSustainAmplitude;


			}

			//if amplitude is too low, set it to 0, otherwise it may cause problems
			if (dAmplitude <= 0.0001) {
				dAmplitude = 0.0;
			}

			return dAmplitude;
		}

	};

	struct instrument {
		double dVolume;
		sEnvelopeADSR env;
		virtual double sound(double dTime, double dFrequency) = 0;
	};

	struct bell :public instrument {
		bell() {
			env.dAttackTime = 0.01;
			env.dDecayTime = 1.0;
			env.dStartAmplitude = 1.0;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 1.0;
		}

		virtual double sound(double dTime, double dFrequency) {
			double dOutput = env.GetAmplitude(dTime) *
				(
					1.0 * osc(dFrequencyOutput * 2.0, dTime, 0, 5, 0.001)
					+ 0.5 * osc(dFrequencyOutput * 3.0, dTime, 0)
					+ 0.25 * osc(dFrequencyOutput * 4.0, dTime, 0)
					);
			return dOutput;
		}
	};

	struct harmonica :public instrument {
		harmonica() {
			env.dAttackTime = 0.01;
			env.dDecayTime = 1.0;
			env.dStartAmplitude = 1.0;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 1.0;
		}

		double sound(double dTime, double dFrequency) {
			double dOutput = env.GetAmplitude(dTime) *
				(
					1.0 * osc(dFrequencyOutput * 1.0, dTime, 1, 5, 0.001)
					+ 0.5 * osc(dFrequencyOutput * 1.5, dTime, 1)
					+ 0.25 * osc(dFrequencyOutput * 2.0, dTime, 1)
					+ 0.05 * osc(0.0, dTime, 5)
					);
			return dOutput;
		}
	};
}

synth::bell voice;

//the olcNoiseMaker class uses double data type, but real sound software uses int?
//dTime is the time that has passed since the beginning of the program
double MakeNoise(double dTime) {
	
	double dOutput = voice.sound(dTime, dFrequencyOutput);

	//master volume
	return 0.2*dOutput;
}

//we pick A4 = 440Hz
double dOctaveBaseFrequency = 220.0; //A3 note
double dSemitoneRatio = pow(2.0, 1.0 / 12.0);
string keys = "ZSXCFVGBNJMK\xbcL\xbe\xbf"; //\xbc.. are for period, comma and slash

//using wide characters wchar_t. why? not portable?
int wmain() {

	wcout <<"single sine wave oscillator, no polyphony"<< endl;
	// Display a keyboard
	wcout << endl <<
		"|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |" << endl <<
		"|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |" << endl <<
		"|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__" << endl <<
		"|     |     |     |     |     |     |     |     |     |     |" << endl <<
		"|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |" << endl <<
		"|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|" << endl << endl;

	//get a list of all sound hardware devices
	vector<wstring> devices = olcNoiseMaker<short>::Enumerate();

	//display findings
	for (auto d : devices) wcout << "found output device: " << d << endl;

	//create an instance of olcNoiseMaker, short is 16 bits which corresponds to the bit depth
	//use the first device, usually the system's default sound device
	//sample rate = 44.1 Khz, No. of channels = 1
	//8 and 512 are used for latency management between pressing a key on the keyboard and the sound produced
	olcNoiseMaker<short> sound(devices[0], 44100, 1, 8, 512);

	//link MakeNoise with sound machine
	sound.SetUserFunction(MakeNoise);

	int nCurrentKey = -1;
	bool bKeyPressed = false;
	while (1)
	{
		bKeyPressed = false;
		for (int k = 0; k < 16; k++)
		{	//we try to mimic piano keys through the keyboard starting from A2 to 
			//GetAsyncKeyState will return short int(16 bits) value with the highest bit set if the key is pressed down when the function is called	
			if (GetAsyncKeyState((unsigned char)(keys[k])) & 0x8000)
			{
				if (nCurrentKey != k)
				{
					dFrequencyOutput = dOctaveBaseFrequency * pow(dSemitoneRatio, k);
					voice.env.NoteOn(sound.GetTime());
					wcout << "\rNote On : " << sound.GetTime() << "s " << dFrequencyOutput << "Hz";
					nCurrentKey = k;
				}

				bKeyPressed = true;
			}
		}

		if (!bKeyPressed)
		{
			if (nCurrentKey != -1)
			{
				wcout << "\rNote Off: " << sound.GetTime() << "s                        ";
				voice.env.NoteOff(sound.GetTime());
				nCurrentKey = -1;
			}
		}
	}
	
	return 0;
}
