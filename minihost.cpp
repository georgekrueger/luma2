//-------------------------------------------------------------------------------------------------------
// VST Plug-Ins SDK
// Version 2.4		$Date: 2006/11/13 09:08:28 $
//
// Category     : VST 2.x SDK Samples
// Filename     : minihost.cpp
// Created by   : Steinberg
// Description  : VST Mini Host
//
// © 2006, Steinberg Media Technologies, All Rights Reserved
//-------------------------------------------------------------------------------------------------------

#include "pluginterfaces/vst2.x/aeffectx.h"
#include "portaudio.h"

#if _WIN32
#include <windows.h>
#elif TARGET_API_MAC_CARBON
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <stdio.h>
#include <iostream>
#include "lumagrammar.h"
#include "music.h"

extern Song song;

vector<Event> events;
vector<int> offsets;

using namespace std;

static const double AUDIO_SAMPLE_RATE = 44100;
static const int AUDIO_OUTPUT_CHANNELS = 2;
static const unsigned long AUDIO_FRAMES_PER_BUFFER = 512;
static const int VST_MAX_EVENTS = 512;

PaStream *stream = NULL;
AEffect* effect = NULL;
VstEvents* vstEvents = NULL;
bool audioStarted = false;

static const unsigned int VST_MAX_OUTPUT_CHANNELS_SUPPORTED = 2;
static float** vstOutputBuffer = NULL;

void PlayNoteOn(AEffect* effect, short pitch, short velocity, short length)
{
	VstMidiEvent e;
	e.type = kVstMidiType;
	e.byteSize = sizeof(VstMidiEvent);
	e.deltaFrames = 0;	///< sample frames related to the current block start sample position
	e.flags = 0;			///< @see VstMidiEventFlags
	e.noteLength = 0;	///< (in sample frames) of entire note, if available, else 0
	e.noteOffset = 0;	///< offset (in sample frames) into note from note start if available, else 0
	e.midiData[0] = (char)0x90;
	e.midiData[1] = (char)pitch;
	e.midiData[2] = (char)velocity;
	e.detune = 0;			///< -64 to +63 cents; for scales other than 'well-tempered' ('microtuning')
	e.noteOffVelocity = 0;	///< Note Off Velocity [0, 127]
	
	vstEvents->numEvents = 1;
	vstEvents->events[0] = (VstEvent*)&e;
	
	effect->dispatcher( effect, effProcessEvents, 0, 0, vstEvents, 0);

	vstEvents->numEvents = 0;

	//printf("play note on\n");
}

void PlayNoteOff(AEffect* effect, short pitch)
{
	VstMidiEvent e;
	e.type = kVstMidiType;
	e.byteSize = sizeof(VstMidiEvent);
	e.deltaFrames = 0;	///< sample frames related to the current block start sample position
	e.flags = 0;			///< @see VstMidiEventFlags
	e.noteLength = 0;	///< (in sample frames) of entire note, if available, else 0
	e.noteOffset = 0;	///< offset (in sample frames) into note from note start if available, else 0
	e.midiData[0] = (char)0x80;
	e.midiData[1] = (char)pitch;
	e.midiData[2] = (char)0;
	e.detune = 0;			///< -64 to +63 cents; for scales other than 'well-tempered' ('microtuning')
	e.noteOffVelocity = 0;	///< Note Off Velocity [0, 127]
	
	vstEvents->numEvents = 1;
	vstEvents->events[0] = (VstEvent*)&e;
	
	effect->dispatcher( effect, effProcessEvents, 0, 0, vstEvents, 0);

	vstEvents->numEvents = 0;

	//printf("play note off\n");
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int portaudioCallback( const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData )
{
    (void) inputBuffer; /* Prevent "unused variable" warnings. */

	static int counter = 0;
	static bool on = false;
	counter += framesPerBuffer;
	if (counter >= 88200) {
		counter -= 88200;
		if (on) {
			PlayNoteOff(effect, 60);
		}
		else {
			PlayNoteOn(effect, 60, 127, 500);
		}
		on = !on;
	}

	VstInt32 numOutputs = effect->numOutputs;
	
	float** vstOut = (float**)vstOutputBuffer;
	effect->processReplacing (effect, NULL, vstOut, AUDIO_FRAMES_PER_BUFFER);

	float *out = (float*)outputBuffer;
	for (unsigned long i=0; i<framesPerBuffer; i++) {
		*out++ = vstOutputBuffer[0][i];
		*out++ = vstOutputBuffer[1][i];
	}
	
    return 0;
}

void HandleAudioError(PaError err)
{
	// print error info here
	cout << "Audio failed to start. Error code: " << err << endl;
}

bool StartAudio()
{
	PaStreamParameters outputParameters;
    PaError err;
    
	err = Pa_Initialize();
	if( err != paNoError ) {
		HandleAudioError(err); 
		return false;
	}
    
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
      fprintf(stderr,"Error: No default output device.\n");
      HandleAudioError(err); return false;
    }

    outputParameters.channelCount = AUDIO_OUTPUT_CHANNELS;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    err = Pa_OpenStream(
              &stream,
              NULL, /* no input */
              &outputParameters,
              AUDIO_SAMPLE_RATE,
              AUDIO_FRAMES_PER_BUFFER,
              (paClipOff | paDitherOff),
              portaudioCallback,
              NULL );
    if( err != paNoError ) {
		HandleAudioError(err);
		return false;
	}

    err = Pa_StartStream( stream );
    if( err != paNoError ) {
		HandleAudioError(err); 
		return false;
	}

	audioStarted = true;

    return true;
}

bool StopAudio()
{
	PaError err = Pa_CloseStream( stream );
    if( err != paNoError ) {
		HandleAudioError(err); 
		return false;
	}
    Pa_Terminate();

	audioStarted = false;

	return true;
}

//-------------------------------------------------------------------------------------------------------
typedef AEffect* (*PluginEntryProc) (audioMasterCallback audioMaster);
static VstIntPtr VSTCALLBACK HostCallback (AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);

//-------------------------------------------------------------------------------------------------------
// PluginLoader
//-------------------------------------------------------------------------------------------------------
struct PluginLoader
{
//-------------------------------------------------------------------------------------------------------
	void* module;

	PluginLoader ()
	: module (0)
	{}

	~PluginLoader ()
	{
		if (module)
		{
		#if _WIN32
			FreeLibrary ((HMODULE)module);
		#elif TARGET_API_MAC_CARBON
			CFBundleUnloadExecutable ((CFBundleRef)module);
			CFRelease ((CFBundleRef)module);
		#endif
		}
	}

	bool loadLibrary (const char* fileName)
	{
	#if _WIN32
		module = LoadLibrary (fileName);
	#elif TARGET_API_MAC_CARBON
		CFStringRef fileNameString = CFStringCreateWithCString (NULL, fileName, kCFStringEncodingUTF8);
		if (fileNameString == 0)
			return false;
		CFURLRef url = CFURLCreateWithFileSystemPath (NULL, fileNameString, kCFURLPOSIXPathStyle, false);
		CFRelease (fileNameString);
		if (url == 0)
			return false;
		module = CFBundleCreate (NULL, url);
		CFRelease (url);
		if (module && CFBundleLoadExecutable ((CFBundleRef)module) == false)
			return false;
	#endif
		return module != 0;
	}

	PluginEntryProc getMainEntry ()
	{
		PluginEntryProc mainProc = 0;
	#if _WIN32
		mainProc = (PluginEntryProc)GetProcAddress ((HMODULE)module, "VSTPluginMain");
		if (!mainProc)
			mainProc = (PluginEntryProc)GetProcAddress ((HMODULE)module, "main");
	#elif TARGET_API_MAC_CARBON
		mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName ((CFBundleRef)module, CFSTR("VSTPluginMain"));
		if (!mainProc)
			mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName ((CFBundleRef)module, CFSTR("main_macho"));
	#endif
		return mainProc;
	}
//-------------------------------------------------------------------------------------------------------
};

//-------------------------------------------------------------------------------------------------------
static bool checkPlatform ()
{
#if VST_64BIT_PLATFORM
	printf ("*** This is a 64 Bit Build! ***\n");
#else
	printf ("*** This is a 32 Bit Build! ***\n");
#endif

	int sizeOfVstIntPtr = sizeof (VstIntPtr);
	int sizeOfVstInt32 = sizeof (VstInt32);
	int sizeOfPointer = sizeof (void*);
	int sizeOfAEffect = sizeof (AEffect);
	
	printf ("VstIntPtr = %d Bytes, VstInt32 = %d Bytes, Pointer = %d Bytes, AEffect = %d Bytes\n\n",
			sizeOfVstIntPtr, sizeOfVstInt32, sizeOfPointer, sizeOfAEffect);

	return sizeOfVstIntPtr == sizeOfPointer;
}

//-------------------------------------------------------------------------------------------------------
static void checkEffectProperties (AEffect* effect);
static void checkEffectProcessing (AEffect* effect);
extern bool checkEffectEditor (AEffect* effect); // minieditor.cpp

void Cleanup()
{
	if (audioStarted) {
		StopAudio();
	}

	printf ("HOST> Suspend effect...\n");
	effect->dispatcher (effect, effMainsChanged, 0, 0, 0, 0);

	printf ("HOST> Close effect...\n");
	effect->dispatcher (effect, effClose, 0, 0, 0, 0);

	delete vstEvents;
}

bool LoadPlugin()
{
	const char* fileName = "C:\\Program Files\\VSTPlugins\\Circle.dll";

	printf ("HOST> Load library...\n");
	PluginLoader loader;
	if (!loader.loadLibrary (fileName))
	{
		printf ("Failed to load VST Plugin library!\n");
		return false;
	}

	PluginEntryProc mainEntry = loader.getMainEntry ();
	if (!mainEntry)
	{
		printf ("VST Plugin main entry not found!\n");
		return false;
	}

	printf ("HOST> Create effect...\n");
	effect = mainEntry (HostCallback);
	if (!effect)
	{
		printf ("Failed to create effect instance!\n");
		return false;
	}

	if (effect->numOutputs > VST_MAX_OUTPUT_CHANNELS_SUPPORTED) {
		printf("Plugin has more outputs than are supported by this host. Max outputs support is: %d\n", effect->numOutputs);
		Cleanup();
		return false;
	}

	int nHdrLen = sizeof(VstEvents) + (100 * sizeof(VstMidiEvent *));
	BYTE * effEvData = new BYTE[nHdrLen];
	vstEvents = (VstEvents *) effEvData;
	vstEvents->numEvents = 0;
	
	printf ("HOST> Init sequence...\n");
	effect->dispatcher (effect, effOpen, 0, 0, 0, 0);
	effect->dispatcher (effect, effSetSampleRate, 0, 0, 0, (float)AUDIO_SAMPLE_RATE);
	effect->dispatcher (effect, effSetBlockSize, 0, AUDIO_FRAMES_PER_BUFFER, 0, 0);

	printf ("HOST> Resume effect...\n");
	effect->dispatcher (effect, effMainsChanged, 0, 1, 0, 0);

	checkEffectProperties (effect);

	vstOutputBuffer = new float*[VST_MAX_OUTPUT_CHANNELS_SUPPORTED];
	for (int i=0; i<VST_MAX_OUTPUT_CHANNELS_SUPPORTED; i++) {
		vstOutputBuffer[i] = new float[AUDIO_FRAMES_PER_BUFFER];
	}
	
	if (!StartAudio()) {
		cout << "Failed to start audio" << endl;
		Cleanup(); 
		return false;
	}

	//checkEffectProcessing (effect);
	checkEffectEditor (effect);

	Cleanup();

	return true;
}

//-------------------------------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
	if (!checkPlatform ())
	{
		printf ("Platform verification failed! Please check your Compiler Settings!\n");
		return -1;
	}

	events.reserve(100);
	offsets.reserve(100);

	// parse input file
	init_table();
	is.open("C:\\Documents and Settings\\George\\My Documents\\luma2\\input.txt", ifstream::in);
	int ret = yyparse();
	is.close();

	cout << endl;

	// test song class
	vector<Event> events; 
	vector<int> offsets;
	for (int i=0; i<30; i++) {
		cout << "--- 500 ms ---" << endl;
		song.Update(500, events, offsets);
		for (int j=0; j<events.size(); j++) {
			Event* e = &events[j];
			e->Print();
			cout << endl;
		}
		events.clear();
		offsets.clear();
	}
	
	LoadPlugin();

	return 0;
}

//-------------------------------------------------------------------------------------------------------
void checkEffectProperties (AEffect* effect)
{
	printf ("HOST> Gathering properties...\n");

	char effectName[256] = {0};
	char vendorString[256] = {0};
	char productString[256] = {0};

	effect->dispatcher (effect, effGetEffectName, 0, 0, effectName, 0);
	effect->dispatcher (effect, effGetVendorString, 0, 0, vendorString, 0);
	effect->dispatcher (effect, effGetProductString, 0, 0, productString, 0);

	printf ("Name = %s\nVendor = %s\nProduct = %s\n\n", effectName, vendorString, productString);

	printf ("numPrograms = %d\nnumParams = %d\nnumInputs = %d\nnumOutputs = %d\n\n", 
			effect->numPrograms, effect->numParams, effect->numInputs, effect->numOutputs);

	// Iterate programs...
	for (VstInt32 progIndex = 0; progIndex < effect->numPrograms; progIndex++)
	{
		char progName[256] = {0};
		if (!effect->dispatcher (effect, effGetProgramNameIndexed, progIndex, 0, progName, 0))
		{
			effect->dispatcher (effect, effSetProgram, 0, progIndex, 0, 0); // Note: old program not restored here!
			effect->dispatcher (effect, effGetProgramName, 0, 0, progName, 0);
		}
		printf ("Program %03d: %s\n", progIndex, progName);
	}

	printf ("\n");

	// Iterate parameters...
	for (VstInt32 paramIndex = 0; paramIndex < effect->numParams; paramIndex++)
	{
		char paramName[256] = {0};
		char paramLabel[256] = {0};
		char paramDisplay[256] = {0};

		effect->dispatcher (effect, effGetParamName, paramIndex, 0, paramName, 0);
		effect->dispatcher (effect, effGetParamLabel, paramIndex, 0, paramLabel, 0);
		effect->dispatcher (effect, effGetParamDisplay, paramIndex, 0, paramDisplay, 0);
		float value = effect->getParameter (effect, paramIndex);

		printf ("Param %03d: %s [%s %s] (normalized = %f)\n", paramIndex, paramName, paramDisplay, paramLabel, value);
	}

	printf ("\n");

	// Can-do nonsense...
	static const char* canDos[] =
	{
		"receiveVstEvents",
		"receiveVstMidiEvent",
		"midiProgramNames"
	};

	for (VstInt32 canDoIndex = 0; canDoIndex < sizeof (canDos) / sizeof (canDos[0]); canDoIndex++)
	{
		printf ("Can do %s... ", canDos[canDoIndex]);
		VstInt32 result = (VstInt32)effect->dispatcher (effect, effCanDo, 0, 0, (void*)canDos[canDoIndex], 0);
		switch (result)
		{
			case 0  : printf ("don't know"); break;
			case 1  : printf ("yes"); break;
			case -1 : printf ("definitely not!"); break;
			default : printf ("?????");
		}
		printf ("\n");
	}

	printf ("\n");
}

//-------------------------------------------------------------------------------------------------------
VstIntPtr VSTCALLBACK HostCallback (AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
{
	VstIntPtr result = 0;

	// Filter idle calls...
	bool filtered = false;
	if (opcode == audioMasterIdle)
	{
		static bool wasIdle = false;
		if (wasIdle)
			filtered = true;
		else
		{
			printf ("(Future idle calls will not be displayed!)\n");
			wasIdle = true;
		}
	}

	//if (!filtered)
	//	printf ("PLUG> HostCallback (opcode %d)\n index = %d, value = %p, ptr = %p, opt = %f\n", opcode, index, FromVstPtr<void> (value), ptr, opt);

	switch (opcode)
	{
		case audioMasterVersion :
			result = kVstVersion;
			break;
	}

	return result;
}
