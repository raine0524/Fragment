#ifndef	MIDIFILE_H_
#define	MIDIFILE_H_

#include <string>
#include <vector>
#include <list>

//if evt=0xB0(Control change), nn is:
#define GM_Control_Modulation_Wheel	1
#define GM_Control_Volumel						7
#define GM_Control_Pan								10
#define GM_Control_Expression					11
#define GM_Control_Pedal							64
#define GM_Control_ResetAllControl			121
#define GM_Control_AllNotesOff				123

class BaseEvent
{
public:
	BaseEvent() : tick(0) {}
	~BaseEvent() {}

	int tick;
};

class Event : public BaseEvent
{
public:
	Event() : evt(0), nn(0), vv(0) {}
	~Event() {}

	unsigned char evt, nn, vv;
};

class TextEvent : public BaseEvent
{
public:
	std::string text;
};

class SpecificInfoEvent : public BaseEvent
{
public:
	std::vector<unsigned char> infos;
};

class TempoEvent : public BaseEvent
{
public:
	TempoEvent() : tempo(0) {}
	~TempoEvent() {}

	int tempo;		//usec/ед to  ед/min
};

class TimeSignatureEvent : public BaseEvent
{
public:
	TimeSignatureEvent() : numerator(0), denominator(0), number_ticks(0), number_32nd_notes(0) {}
	~TimeSignatureEvent() {}

	int numerator, denominator, number_ticks, number_32nd_notes;
};

class KeySignatureEvent : public BaseEvent
{
public:
	KeySignatureEvent() : sf(0), mi(0) {}
	~KeySignatureEvent() {}

	int sf, mi;
};

class SysExclusiveEvent : public BaseEvent
{
public:
	std::vector<unsigned char> event;
};

class ITrack
{
public:
	ITrack() : number(0) {}
	~ITrack() {}

	int number;
	std::string name;
	std::string instrument;
	std::list<Event> events;
	std::list<TextEvent> lyrics, texts;
	std::list<SpecificInfoEvent> specificEvents;
};

class MidiFile
{
public:
	MidiFile() : quarter(0), format(0) {}
	~MidiFile() 
	{
		format = 1;
		quarter = 480;
	}

	std::list<TextEvent> markers, cuePoints;
	std::list<TempoEvent> tempos;
	std::list<TimeSignatureEvent> timeSignatures;
	std::list<KeySignatureEvent> keySignatures;
	std::list<SysExclusiveEvent> sysExclusives;
	std::list<ITrack> tracks;

	int quarter, format;
	std::string author, name, copyright;

	ITrack* getTrack(int idx);
	ITrack* getTrackPianoTrack();
	double secPerTick();
};

#endif		//MIDIFILE_H_