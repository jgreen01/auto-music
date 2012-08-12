#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// constant definition
#define SAMPLING_RATE 44100
#define PI 3.14159

/* Note freqs for f(n) = 440 * (12th-root-of-2)^(n-49)
 * 
 * Finding 
 * C		note = (i-4)%12 == 0
 * C♯/D♭	note = (i-4)%12 == 1
 * D		note = (i-4)%12 == 2
 * D♯/E♭	note = (i-4)%12 == 3
 * E		note = (i-4)%12 == 4
 * F		note = (i-4)%12 == 5
 * F♯/G♭	note = (i-4)%12 == 6
 * G		note = (i-4)%12 == 7
 * G♯/A♭	note = (i-4)%12 == 8
 * A		note = (i-4)%12 == 9
 * A♯/B♭	note = (i-4)%12 == 10
 * B		note = (i-4)%12 == 11
 * 
 * Note generator algorithm
 * n = (octave * 10) + note + ((octave * 2) - 8)
 * 
 * based on info from wikipedia
 * http://en.wikipedia.org/wiki/Piano_key_frequencies
 */		
				
float noteLengths[] = { 1, 0.5, 0.25, 0.125, 0.0625 };

int locrianScale[] = {0,1,3,5,6,8,10};	// octave 7
int aeolianScale[] = {0,2,3,5,7,8,10};	// octave 6 or 7
int mixolydianScale[] = {0,2,4,5,7,9,10}; // octave 6 and 9_
int lydianScale[] = {0,2,4,6,7,9,11};	// octave 7 and 11#
int phrygianScale[] = {0,1,3,5,7,8,10};	// octave 7
int dorianScale[] = {0,2,3,5,7,9,10}; 	// octave 7 and 13_
int ionianScale[] = {0,2,4,5,7,9,11}; 	// octave 7 and 5_

int fadeIncounter = 0, fadeOutCounter = 0;

struct musicalMeasure{
	int tempo; //quarter-notes per minute
	int trackLength;
	int noteNumber; // whole notes
	int noteLength; // whole notes
	int *scale;
	int octave;
};

struct noteList{
	int noteLengthIndex;
		// index for noteLengths[]
		/* 0 = whole
		 * 1 = half
		 * 2 = quarter
		 * 3 = eighth
		 * 4 = sixteenth */
		 
	int scaleIndex;
		// 0 - 6
	
	float (*funPoint)(int,int);
		// my attempt at functionalizing the code
	
	struct noteList *prev; 
		// prev is not used but useful for reversing the track
		
	struct noteList	*next;
};

/* Creates general measure info */
void initMusicalMeasure(struct musicalMeasure *m, int tempo, int sec, 
							int scale[], int octave);

/* Used to create the note linked list */
void makeComposition(struct noteList *root, struct musicalMeasure *m);
void initNote(struct noteList *n, int scaleIndex, int noteLengthIndex, 
				struct noteList *prev);
int chooseNote(int prevNote, struct musicalMeasure *m);
float marsaglia(float x1, float x2);
float noteFreqGenerator(int note, int octave);
float freqGenerator(int n);

/* Used to write list to file */
void conductMusic(struct noteList *root, struct musicalMeasure *m);
int createNote(struct noteList *note, struct musicalMeasure *m, float rawMusic[]);
float noteData(int i, int scaleIndex, struct musicalMeasure *m);

/* prints list */
void printNoteList(struct noteList *root, struct musicalMeasure *m);

/* functions that can be applied to notes */
float cosEffect(int i, int speed);
float fadeIn(int i, int length);
float fadeOut(int i, int length);

/* envelops */
float sqrtEnvelop(int i, int enLength, int noteLength);
float linearEnvelop(int i, int enLength, int noteLength);

void pause(int time); // in nop's

float noteFreqGenerator(int note, int octave){
	int n = 0;

	n = (octave * 10) + note + ((octave * 2) - 8);

	return freqGenerator(n);
}	

float freqGenerator(int n){
	return 440*pow((double)2,(double)(n-49)/(double)12);
}

void initMusicalMeasure(struct musicalMeasure *m, int tempo, int sec, 
							int scale[], int octave){
	m->tempo = tempo; //quarter-notes per minute
	m->trackLength = SAMPLING_RATE * sec;
	m->noteNumber = (tempo*(sec/60))/4;
	m->noteLength = m->trackLength/m->noteNumber;
	m->scale = scale;
	m->octave = octave;
}

void initNote(struct noteList *n, int scaleIndex, int noteLengthIndex, 
					struct noteList *prev){
	
	n->noteLengthIndex = noteLengthIndex;
	n->scaleIndex = scaleIndex;
	n->prev = prev;
	
	//if(rand()%2)
	//	n->funPoint = cosEffect;
}

void conductMusic(struct noteList *root, struct musicalMeasure *m){
	int LengthOfNote;
	struct noteList *note;
	float rawMusic[m->noteLength];
	
	note = root;
	
	FILE *fp;
    fp = fopen("electricjazz.raw","wb");

	while(note != NULL){
		LengthOfNote = createNote(note ,m ,rawMusic);
		
		fwrite(rawMusic, sizeof(float), LengthOfNote, fp);
		
		note = note->next;
	}
	fclose(fp);
}

int createNote(struct noteList *note, struct musicalMeasure *m, float rawMusic[]){
	int i = 0;
	int lengthOfNote = (int)((float)m->noteLength * 
							noteLengths[note->noteLengthIndex]);
	int envelopLength = m->noteLength/48;

	for ( i = 0 ; i < lengthOfNote; i++){
		if(note->funPoint == &cosEffect){
			rawMusic[i] = note->funPoint(i,100) *
					linearEnvelop(i,envelopLength,lengthOfNote) *
					noteData(i, note->scaleIndex, m);
		} else if(note->funPoint == &fadeIn){
			rawMusic[i] = 
					note->funPoint(fadeIncounter,m->trackLength/6) *
					linearEnvelop(i,envelopLength,lengthOfNote) *
					noteData(i, note->scaleIndex, m);
			fadeIncounter++;
		} else if(note->funPoint == &fadeOut){
			rawMusic[i] = 
					note->funPoint(fadeOutCounter,m->trackLength/6) *
					linearEnvelop(i,envelopLength,lengthOfNote) *
					noteData(i, note->scaleIndex, m);
			fadeOutCounter++;
		} else {
			rawMusic[i] = linearEnvelop(i,envelopLength,lengthOfNote) *
					noteData(i, note->scaleIndex, m);
		}
	}

	return (lengthOfNote);
}

float noteData(int i, int scaleIndex, struct musicalMeasure *m){
	return cos( 2.0 * PI * 
			noteFreqGenerator(m->scale[scaleIndex],m->octave) / 
			(float)SAMPLING_RATE * (float)i);
}

void makeComposition(struct noteList *root, struct musicalMeasure *m){
	struct noteList *note, *prevNote;
	int i;
	float sum = 0;
	
	while(sum < m->trackLength){
		note = (struct noteList*) malloc(sizeof(struct noteList));
		
		if(i == 0){
			initNote(root, 0, 0, NULL);
			root->funPoint = fadeIn;
			root->next = note;
			sum += noteLengths[root->noteLengthIndex] * (float)m->noteLength;
			
			initNote(note, chooseNote(root->scaleIndex, m), rand()%3+2, root);
			note->funPoint = fadeIn;
			prevNote = note;
			sum += (noteLengths[prevNote->noteLengthIndex] * (float)m->noteLength);
		} else {
			initNote(note, chooseNote(prevNote->scaleIndex, m), rand()%3+2, prevNote);
			
			if(sum < (m->trackLength/6))
				note->funPoint = fadeIn;
			if(sum > (m->trackLength - (m->trackLength/6)))
				note->funPoint = fadeOut;
				
			prevNote->next = note;
			prevNote = note;
			sum += (noteLengths[prevNote->noteLengthIndex] * (float)m->noteLength);
		}
		i++;
	}
}

int chooseNote(int prevIndex, struct musicalMeasure *m){
	float gaussianRandNum;
	int currIndex = 0;
	int range = 6;
	
	gaussianRandNum = marsaglia(0.0, (float)range);
	
	while(gaussianRandNum > range || gaussianRandNum < 1)
		gaussianRandNum = marsaglia(0.0, (float)range);
	
	if(rand()%2)	
		currIndex = prevIndex - (int)gaussianRandNum;
	else
		currIndex = prevIndex + (int)gaussianRandNum;
	
	if(currIndex < 0)
		currIndex += range+1;
	else if (currIndex > range)
		currIndex -= range;
	
	return currIndex;
}

float marsaglia(float x1, float x2){
	float w;
	do {
		x1 = 2.0 * ((float)rand()/(float)RAND_MAX) - 1.0;
		x2 = 2.0 * ((float)rand()/(float)RAND_MAX) - 1.0;
		w = x1 * x1 + x2 * x2;
	} while ( w >= 1.0 );

	return(sqrt( (-2.0 * log( w ) ) / w ));
}

void printNoteList(struct noteList *root, struct musicalMeasure *m){
	struct noteList *note;
	int i = 0;
	
	note = root;
	
	while(note != NULL){
		printf("noteNumber: %d\tnoteLength: %f\tnoteValue: %f\n", 
				i,
				noteLengths[note->noteLengthIndex],
				noteFreqGenerator(note->scaleIndex, m->octave));
		printf("prev: %p\t next: %p\t",note->prev,note->next);
		if(note->funPoint == &cosEffect){
			printf("funPoint: %s\n","cosEffect");
		} else if(note->funPoint == &fadeIn){
			printf("funPoint: %s\n","fadeIn");
		} else if(note->funPoint == &fadeOut){
			printf("funPoint: %s\n","fadeOut");
		} else {
			printf("funPoint: %p\n",note->funPoint);
		}
		printf("\n");
		note = note->next;
		i++;
	}
}

float fadeIn(int i, int length){
	if(i < length)
		return (float)i/(float)length;
	else
		return 1;
}

float fadeOut(int i, int length){
	if(i < length)
		return (float)(length-i)/(float)length;
	else
		return 1;
}

float cosEffect(int i, int freq){
	return cos(i*PI*freq);
}

float sqrtEnvelop(int i, int enLength, int noteLength){
	float enValue = -1.0;
	if(i < enLength)
		enValue = sqrtf((float)i/enLength);
	else if((noteLength-i) < enLength)
		enValue = sqrtf((float)(noteLength-i)/enLength);
	
	if(enValue <= 1.0 && enValue != -1.0)
		return enValue;
	else 
		return 1.0;
}

float linearEnvelop(int i, int enLength, int noteLength){
	float enValue = -1.0;
	if(i < enLength)
		enValue = (float)i/enLength;
	else if((noteLength-i) < enLength)
		enValue = (float)(noteLength-i)/enLength;
	
	if(enValue <= 1.0 && enValue != -1.0)
		return enValue;
	else 
		return 1.0;
}

void pause(int time){
	int i;
	for(i = 0; i < time; i++)
		asm("nop");
}

int main(){
	printf("starting main program\n\n");
	
	srand(time(NULL));
	
	struct musicalMeasure *piece = (struct musicalMeasure*) 
							malloc(sizeof(struct musicalMeasure));
	
	struct noteList *root = (struct noteList*) 
							malloc(sizeof(struct noteList));
	
	initMusicalMeasure(piece, 120, 60, aeolianScale, 6);
	
	printf("compose jazz\n\n");
	
	makeComposition(root, piece);
	
	printf("finished composition\n\n");
	
	printNoteList(root, piece);
	
	printf("writing to file\n\n");
	
	conductMusic(root,piece);
	
	printf("electric jazz created\n");
	
	return(0);
}
