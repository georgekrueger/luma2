#ifndef MUSIC_H
#define MUSIC_H

#include <vector>
#include <iostream>
#include <fstream>
#include <map>
using namespace std;

///////////////////////////
// Scales
///////////////////////////
typedef enum
{
	CMAJ,
	CMIN,
	NO_SCALE,
} Scale;

const int NumScales = NO_SCALE;

struct ScaleInfo
{
	const char* Name;
	short startMidiPitch;
	short intervals[12];
	short numIntervals;
};

ScaleInfo scaleInfo[NumScales] = 
{
	{ "cmaj", 0, { 0, 2, 4, 5, 7, 9, 11 }, 7 },
	{ "cmin", 0, { 0, 2, 3, 5, 7, 9, 10 }, 7 },
};

const char* GetScaleName(Scale scale)
{
	return scaleInfo[scale].Name;
}

unsigned short GetMidiPitch(Scale scale, int octave, int degree)
{
	const ScaleInfo* info = &scaleInfo[scale];
	short startMidiPitch = info->startMidiPitch;
	short midiPitch = startMidiPitch * octave;
	if (degree >= 1 && degree <= info->numIntervals) {
		midiPitch += info->intervals[degree-1];
	}
	return midiPitch;
}

class Note
{
public:
	Note(Scale scale, int octave, int degree, short velocity, int length) :
		scale_(scale), octave_(octave), degree_(degree), velocity_(velocity), 
		length_(length), isRest_(false), on_(true)
	{
	}

	Note(int length) : length_(length), isRest_(true) {}
	~Note() {}

	short GetLength() { return length_; }
	short GetPitch() { return GetMidiPitch(scale_, octave_, degree_); }
	short GetVelocity() { return velocity_; }
	
	void SetRest(bool b) { isRest_ = b; }
	bool IsRest() { return isRest_; }

	void SetNoteOff() { on_ = false; }

	void Print()
	{
		if (isRest_) {
			cout << "rest " << length_;
		}
		else {
			cout << GetScaleName(scale_) << " " << octave_ << " " << degree_
				 << " " << velocity_ << " " << length_;
		}
	}

private:
	Scale scale_;
	int octave_;
	int degree_;
	short velocity_;
	int length_;
	bool isRest_;
	bool on_;
};

class Event
{
public:
	enum Type
	{ 
		NOTE 
	};
	Type type;
	Note* note;

	Event& Event::operator=(const Event &rhs) {
		// Only do assignment if RHS is a different object from this.
		if (this != &rhs) {
			this->type = rhs.type;
			this->note = new Note(*rhs.note);
		}
		return *this;
	}

	void Print()
	{
		switch(type)
		{
		case NOTE:
			note->Print();
			break;
		}
	}
};

class Pattern
{
public:
	Pattern() : repeatCount_(1) {}
	~Pattern() {}

public:
	void Add(Note* n)
	{
		Event e;
		e.type = Event::NOTE;
		e.note = n;
		events_.push_back(e);
	}

	void Add(Pattern* p)
	{
		int numEvents = p->GetNumEvents();
		int repeat = p->GetRepeatCount();
		for (int j=0; j<repeat; j++) {
			for (int i=0; i<numEvents; i++) {
				Event* e = p->GetEvent(i);
				events_.push_back(*e);
			}
		}
	}

	size_t GetNumEvents() { 
		return events_.size(); 
	}

	Event* GetEvent(int i) {
		return &events_[i];
	}
	
	void SetRepeatCount(int count) {
		repeatCount_ = count;
	}

	int GetRepeatCount() {
		return repeatCount_;
	}

	void Print()
	{
		cout << "[";
		for (unsigned long i=0; i<events_.size(); i++) {
			if (i != 0) {
				cout << ", ";
			}
			events_[i].Print();
		}
		cout << "]";
		if (repeatCount_ != 1) {
			cout << " # " << repeatCount_;
		}
	}

private:
	vector<Event> events_;
	int repeatCount_;
};

class Song
{
public:
	Song() {}

	void AddPattern(Pattern* p)
	{
		SongPattern sp(p);
		patterns_.push_back(sp);
	}

	void Update(int elapsedTime, vector<Event>& events, vector<int>& offsets)
	{
		int numPatterns = patterns_.size();
		for (int i=0; i<numPatterns; i++) {
			SongPattern* sp = &patterns_[i];
			int timeUsed = sp->offset_;
			while (timeUsed < elapsedTime && sp->pattern_->GetRepeatCount() > 0) {
				if (sp->pos_ >= sp->pattern_->GetNumEvents()) {
					sp->pattern_->SetRepeatCount(sp->pattern_->GetRepeatCount() - 1);
					continue;
				}

				Event* e = sp->pattern_->GetEvent(sp->pos_);
				switch(e->type) 
				{
					case Event::NOTE:
					{
						Note* note = e->note;
						if (e->note->IsRest())
						{
							timeUsed += note->GetLength();
							if (timeUsed > elapsedTime) {
								sp->offset_ = timeUsed - elapsedTime;
							}
							else {
								sp->pos_++;
							}
						}
						else {
							map<short, Note*>::iterator activeNote = activeNotes_.find(note->GetPitch());
							if (activeNote != activeNotes_.end()) {
								// stop note
								Event noteOffEvent = *e;
								noteOffEvent.note->SetNoteOff();
								events.push_back(noteOffEvent);
								offsets.push_back(timeUsed);
							}
							activeNotes_[note->GetPitch()] = note;
							events.push_back(*e);
							offsets.push_back(timeUsed);
							sp->pos_++;
						}
					}
					default:
						break;
				}
			}
		}
	}

private:
	class SongPattern
	{
	public:
		SongPattern(Pattern* pattern) : pos_(0), offset_(0), pattern_(pattern) {}

		unsigned int pos_;
		unsigned int offset_;
		Pattern* pattern_;
	};
	vector<SongPattern> patterns_;
	map<short, Note*> activeNotes_;
};

#endif