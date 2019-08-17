/*
 * This software is released under GPL. If you don't know what that is
 * immediately point your webbrowser to  http://www.gnu.org and read
 * all about it.
 *
 * The short version: You can modify the sources for your own purposes
 * but you must include the new source if you distribute a derived  work.
 *
 * (C) 2002, 2003, 2004, 2005, 2006 Jens M Andreasen <ja@linux.nu>
 *
 *
 *
 *    (
 *     )
 *   c[]
 * 
 */ 



#ifndef MX44_H
#define MX44_H

#include "mmx.h"
#include <alsa/asoundlib.h>  
//#define OMNIMODE 


#ifdef OMNIMODE
// Tim suggested a smaller footprint
#define MIDICHANNELS 1 
#define CHANNELMASK 0x0   
#else
// Elsewise we are multitimbral
#define MIDICHANNELS 16
#define CHANNELMASK 0x0F
#endif

#define SAMPLES 32 // number of samples to produce before polling midi
#define ENVLOOP 2  // number of envelope updates during sample production

#define OPLOOP SAMPLES/ENVLOOP
#define OUTBUF SAMPLES * 2 // stereo == 2

/* 
 * Data for a running envelope
 * 
 */
typedef struct 
{
  // Each envelope has a current value ..
  mmx_t state02;  // d 
  mmx_t state13;  // d
 
  // .. and a precalculated +/- value to be added .. 
  mmx_t delta02; // d
  mmx_t delta13; // d
  // .. for exactly x samples ..
  int time[4][8];     // [op][step]
  int time_count[4];  // [op]

  // .. before being replaced by the next precalculated delta value
  int delta_value[4][9]; // [op][step]
  int step[4]; // op

  int sustain[4]; // middle level in the sustain loop
  int release[4]; // the release deltas cannot be precalculated

  int running; // bit 0 - 3 set while env still running
  int audible; // bit 0 - 3 set if this voice is in the mix
}Mx44env;

/*
 * Data for a single running voice
 *
 */
typedef struct _mx44voice
{

  // mmx_t samples[SAMPLES];
  //    mmx_t delay[1024]; // Delay on am/pm feedback;

  // intonation parameters
  int intonation[4][2]; // w (op) [amount | decay]

  // state of oscillators
  mmx_t state02;  // d (op 0, op 2) 
  mmx_t state13;  // d (op 1, op3)
  // frequency of oscillators
  mmx_t delta02;  // d 
  mmx_t delta13;  // d


  // Each oscillator is phase- and amplitude modulated
  // by every other oscillators previous output

  // Modulaton arguments are skewed diagonally as in:

  // x[3](op2,op1,op0,op3)
  // x[2](op1,op0,op3,op2)
  // x[1](op0,op3,op2,op1)
  // x[0](op3,op2,op1,op0)

  mmx_t pm[4]; // w
  mmx_t am[4]; // w

  // pseudo sin is averaged from two bumpy approximations
  // running 60 degrees apart at the same frequency. 
  // Optionally one part can be "octave doubled" and
  // crossmodulated to a second set of waweshapes.
  mmx_t od; // w (op)
  mmx_t complexwawe;

  // Magic Number Modulation
  mmx_t magicnumber;
  mmx_t magicmodulation; // outer loop
  mmx_t magicmodulation2; //inner loop

  // some switches
  mmx_t lowpass;
  mmx_t waweshape;

  // Stereo mix
  mmx_t mixL;     // w
  mmx_t mixR;     // w

  mmx_t wheel;    // w 

  // Final utput of oscillator 0,1,2,3
  mmx_t out;      // w


  short delay_index;
  short delay_time;
  int delay_osc_state;
  int delay_osc_delta;
  int delay_osc_amount;
  // Next voice in an asignment que (running, hold, released, silent)
  struct _mx44voice *next; 
  struct _mx44voice *previous; 
  
  // envelopes
  Mx44env env;
  

  unsigned char channel; // channel number
  unsigned char key;
  short exp;
  short padding[2];
}Mx44voice;



// The patch sets up the initial state according to
// key and velocity etc
#define WAWEBUTTON 1
#define WHEELBUTTON 2
#define INVERTWHEELBUTTON 4
#define LOWPASSBUTTON 8
#define WAWESHAPEBUTTON 16
#define PHASEFOLLOWKEYBUTTON 32
#define MAGICBUTTON 64
#define EXPRESSIONBUTTON 128



typedef struct // Mx44patch
{
  short  pm[4][4]; // [destination] [source]
  short  am[4][4];

  short mix[4][2]; // [op] [left | right]
  
  short  env_level[4][8]; // [op] [stage]
  short  env_time[4][8];

  short harmonic[4];

  short intonation[4][2]; // [op] [amount | decay]

  short detune[4];

  short velocityfollow[4];

  unsigned char od[4]; // octave doubling
  unsigned char button[4]; // WAWE, WHEEL, LOWPASS and WAWESHAPE

  // key bias
  short breakpoint[4];   // [op]
  short keybias[4][2];   // [op] [hi | lo value]
  // env keyfollow
  short keyfollow[4][2]; // [op] [attack | sustain-loop]
  // key velocity
  short velocity[4]; // [op]

  short phase[4][2]; // [op] [offset | velocityfollow]
  char name[32];

  short fx_delay[5]; // delay time, spread, feedback, modulation and outlevel
  short fx_overdrive; // distortion

  char lfo_button[2]; // loop, aftertouch .
  short lfo[6]; // lfo rate 1,  rate 2,  level 1, level 2, time 1 and time 2

  unsigned char GS_atck[2];
  unsigned char GS_dec[2];
  unsigned char GS_rel[2];

  signed char GS_vari[2];
  unsigned char GS_timb[2];

  unsigned char GS_ptmt[2];
  unsigned char GS_clst[2];

  unsigned char GS_vol[2];
  signed char GS_pan[2];


  // unused //----------
  short mod_delay;
  short unused[41];
} Mx44patch;




typedef struct { // Mx44state
  mmx_t mmxtmp[3];
  mmx_t volume;
  mmx_t wheel;


  Mx44voice *voices;
  Mx44voice *que[5]; // assigned, held, released, silent, excess held
  Mx44voice *last[5]; 
  Mx44voice *keyboard[MIDICHANNELS][128][4];
  int keyTable[8][128];
  unsigned char temperament;

  // some controllers
  char holdpedal[MIDICHANNELS];
  short pitchbend[MIDICHANNELS];
  char modulation[MIDICHANNELS];
  char mod_now[MIDICHANNELS];
  unsigned char exp[MIDICHANNELS];
  unsigned char exp_now[MIDICHANNELS];

  //... add some more control for plutek:
  signed char vari[MIDICHANNELS]; // GS sound variation
  //signed char vari_now[MIDICHANNELS];
  unsigned char timb[MIDICHANNELS]; // GS timbre/harmonic intensity
  unsigned char timb_now[MIDICHANNELS];
  signed char ctff[MIDICHANNELS]; // GS cutoff freq

  unsigned char ptmt[MIDICHANNELS]; // GS portamento
  unsigned char vol[MIDICHANNELS]; // GS channel volume
  unsigned char vol_now[MIDICHANNELS];
  signed char pan[MIDICHANNELS];  // GS pan
  unsigned char atck[MIDICHANNELS]; // GS env attack
  unsigned char dec[MIDICHANNELS];  // GS env decay
  unsigned char loop[MIDICHANNELS]; // GS undefined, here env sustain loop
  unsigned char rel[MIDICHANNELS];  // GS env release


  unsigned char clst[MIDICHANNELS]; // GS celeste/detune
  snd_seq_t *seq_handle;                 ////
  struct pollfd *pfd;                    ////
  int npfd;                              ////


  int lfo_env_state[MIDICHANNELS];
  int lfo_rate_state[MIDICHANNELS];


  int current; // number of prerendered samples already delivered

  int isGraphic;
  int maxpoly;
  int samplerate;

  Mx44patch patch[128]; // preset patches
  Mx44patch tmpPatch[MIDICHANNELS]; // actual editable and sounding patch
  char patchNo[MIDICHANNELS]; // chosen preset number 
  char monomode[MIDICHANNELS];

  char shruti[12];

}Mx44state;


Mx44state* mx44_new(int maxpolyphony);
 // middle A == 440.0
void mx44_reset(Mx44state* mx44,double tuningfork,int samplerate);
void mx44_pgmchange(Mx44state* mx44,int channel,int patchnumber);
void mx44_noteon(Mx44state* mx44,int channel,int key,int velocity);
void mx44_noteoff(Mx44state* mx44,int channel,int key,int velocity);
void mx44_pitchbend(Mx44state* mx44,int channel,int data1,int data2);
void mx44_control(Mx44state* mx44,int channel,int control,int data);
void mx44_chanpress(Mx44state* mx44,int channel,int data);

void mx44_dispose(Mx44state * mx44);

#ifdef OMNIMODE
// syntax checking only ... 
// use with debugger at your own risk ..
void omni(Mx4state* mx4, int request,float* port_left, float* port_right)
#endif

int mx44_play(Mx44state* mx44,short * stereo_out);

#endif

