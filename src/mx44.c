/* mx44.c
 *
 * This software is released under GPL. If you don't know what that is
 * immediately point your webbrowser to  http://www.gnu.org and read
 * all about it.
 *
 * The short version: You can modify the sources for your own purposes
 * but you must include the new source if you distribute a derived work.
 *
 * (C) 2002, 2003, 2004, 2005, 2006 Jens M Andreasen <ja@linux.nu>
 *
 *
 *
 *   (
 *    )
 *  c[]
 *
 *
 */

#include "mx44.h"
#include "interface2.h"
#include "cmdline_opts.h"

extern opts * options;

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

/* high resolution processor clock count
 */
static inline unsigned long long
rdtsc()
   {
     unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
   }

//////////////////////////////////////////////////////////////////
//
//   JACK interface adapted from jack_simple_client.c
//   ------------------------------------------------
//
#include <jack/jack.h>
//
jack_port_t *port_L;
jack_port_t *port_R;



static void mx44_midi_in( Mx44state *mx44, snd_seq_t *seq_handle)
{
  snd_seq_event_t *ev;

  do {
    snd_seq_event_input(seq_handle, &ev);

    switch (ev->type)
      {
      case SND_SEQ_EVENT_CONTROLLER:
	mx44_control( mx44,
		      ev->data.control.channel,
		      ev->data.control.param,
		      ev->data.control.value );
	break;
      case SND_SEQ_EVENT_PITCHBEND:
	mx44_pitchbend( mx44,
			ev->data.control.channel,
			ev->data.control.value,
			0x3F );
	break;
      case SND_SEQ_EVENT_NOTEON:
	mx44_noteon( mx44,
		     ev->data.note.channel,
		     ev->data.note.note,
		     ev->data.note.velocity );
	break;
      case SND_SEQ_EVENT_NOTEOFF:
	mx44_noteoff( mx44,
		      ev->data.note.channel,
		      ev->data.note.note,
		      ev->data.note.velocity );
	break;
      case SND_SEQ_EVENT_PORT_SUBSCRIBED:

	fprintf(stderr,"Port subscribed: sender: %d:%d dest: %d:%d\n",
		ev->data.connect.sender.client,
		ev->data.connect.sender.port,
		ev->data.connect.dest.client,
		ev->data.connect.dest.port);
	break;
      case SND_SEQ_EVENT_PGMCHANGE:
	mx44_pgmchange( mx44,
			ev->data.control.channel,
			ev->data.control.value );
	if(mx44->isGraphic)
	  setwidgets(mx44->tmpPatch,
		     ev->data.control.channel,
		     ev->data.control.value);

	break;
      case SND_SEQ_EVENT_SENSING:
	// FIXME:
	//
	// This is apparently active sensing.
	// If we get it once and then loose it
	// we should shut up!
	//
	break;
      case SND_SEQ_EVENT_CHANPRESS:
	mx44_chanpress( mx44,
			ev->data.control.channel,
			ev->data.control.value );
	break;
      default:
	fprintf(stderr,"Unknown SND_SEQ_EVENT_#%d\n",ev->type);
      }

    snd_seq_free_event(ev);

  } while (snd_seq_event_input_pending(seq_handle, 0) > 0);

}

static int mx44_jack_process (jack_nframes_t nframes, void *arg)
{
  //
  //  The process callback for this JACK application.
  //  It is called by JACK at the appropriate times.
  //


  static short mx44buf[SAMPLES * 2];
  static int current = SAMPLES * 2;


  static int called = 0;
  Mx44state *mx44 = arg;

  if (poll(mx44->pfd, mx44->npfd, 0) > 0)
    mx44_midi_in( mx44, mx44->seq_handle);


  float *left
    = (float *) jack_port_get_buffer (port_L, nframes);

  float *right
    = (float *) jack_port_get_buffer (port_R, nframes);

  float ftmp;

  int n = 0;
  while(n < nframes)
    {
      if(current == SAMPLES * 2)
	{
	  memset(mx44buf, 0, sizeof(mx44buf));
	  mx44_play(mx44,mx44buf);
	  current = 0;
	}


      ftmp = mx44buf[current++];
      *right = ldexpf(ftmp,-16);
      ++right;

      ftmp = mx44buf[current++];
      *left = ldexpf(ftmp,-16);
      ++left;


      ++n;
    }


  if(!called)
    printf("\n  Jack nframes: %d\n\n",(int)nframes);
  called = 1;
  return 0;
}

static void
mx44_jack_shutdown (void *arg)
{
  //
  //  This is the shutdown callback for this JACK application.
  //  It is called by JACK if the server ever shuts down or
  //  decides to disconnect the client.
  //

  exit (1);
}
static int
mx44_jack_init(Mx44state *mx44)
{
  const char *client_name = "Mx44.2";


  if (snd_seq_open(&mx44->seq_handle, "default", SND_SEQ_OPEN_INPUT, 0) < 0)
    {
      fprintf(stderr, "Error opening ALSA sequencer.\n");
      //return NULL;
    }
   snd_seq_set_client_name(mx44->seq_handle, client_name);
   int portid;
   if ((portid =
	snd_seq_create_simple_port(mx44->seq_handle, client_name,
				   SND_SEQ_PORT_CAP_WRITE
				   | SND_SEQ_PORT_CAP_SUBS_WRITE,

				   SND_SEQ_PORT_TYPE_MIDI_GENERIC
				   | SND_SEQ_PORT_TYPE_SOFTWARE
				   | SND_SEQ_PORT_TYPE_SYNTHESIZER)) < 0)
    {
      fprintf(stderr, "Error creating sequencer port.\n");
      //return NULL;
    }


  mx44->npfd = snd_seq_poll_descriptors_count(mx44->seq_handle, POLLIN);////
  mx44->pfd = malloc(mx44->npfd * sizeof(struct pollfd));////

  snd_seq_poll_descriptors(mx44->seq_handle, mx44->pfd, mx44->npfd, POLLIN);////


  //
  //  This is the intializer that will set up ports
  //  and callbacks. Returns samplerate on success.
  //

  const char **ports;
  const char *server_name = NULL;

  jack_client_t *client;
  jack_status_t status;

  int samplerate;
  client = jack_client_open(client_name, JackNullOption, &status, server_name);

  if (!client)
    {
      fprintf (stderr, "jack_client_open() failed, "
                       "status = 0x%2.0x\n", status);
      if (status & JackServerFailed)
        {
          fprintf (stderr, "Unable to connect to JACK server\n");
        }
      fprintf (stderr, "JACK server not running.\n");
      return 0;
    }
  if (status & JackServerStarted)
    {
      fprintf (stderr, "JACK server started\n");
    }
  if (status & JackNameNotUnique)
    {
      client_name = jack_get_client_name(client);
      fprintf (stderr, "unique name `%s' assigned\n", client_name);
    }

  // tell the JACK server to call `process()' whenever
  // there is work to be done.

  jack_set_process_callback (client, mx44_jack_process, mx44);

  // tell the JACK server to call `jack_shutdown()' if
  // it ever shuts down, either entirely, or if it
  //  just decides to stop calling us.

  jack_on_shutdown (client, mx44_jack_shutdown, mx44);

  // display the current sample rate.

  printf ("engine sample rate: %" PRIu32 "\n",
	  samplerate = jack_get_sample_rate (client));

  // create two ports

  port_L = jack_port_register (client, "out_L", JACK_DEFAULT_AUDIO_TYPE,
                                JackPortIsOutput|JackPortIsTerminal, 0);
  port_R = jack_port_register (client, "out_R", JACK_DEFAULT_AUDIO_TYPE,
                                JackPortIsOutput|JackPortIsTerminal, 0);

  // tell the JACK server that we are ready to roll

  if (jack_activate (client))
    {
      fprintf (stderr, "cannot activate client \n");
      return 0;
    }
  else
    fprintf (stderr, "client activated \n");

  if (!options->autoconnect)
    return samplerate;

  ports = jack_get_ports (client, NULL, NULL, JackPortIsInput);
  if (!ports)
    {
      fprintf(stderr, "Cannot find any physical playback ports\n");
    }
  else
    {
      int i;
      for(i=0;ports[i];++i)
        ;
      if(i > 0) {
          if (jack_connect(client, jack_port_name (port_L), ports[0]))
              fprintf(stderr, "cannot connect output port_L\n");
          else
              printf("connected %s:out_L --> %s\n", client_name, ports[0]);

      }
      if(i > 1) {
          if (jack_connect (client, jack_port_name (port_R), ports[1]))
            fprintf (stderr, "cannot connect output port_R\n");
          else
            printf("connected %s:out_R --> %s\n", client_name, ports[1]);

      }
      free (ports);
    }

  return samplerate;
}
//
//
// EOF JACK interface
////////////////////////////////////////////////////////////////////////




#define PHASEALIGN ((0x0940<<14))
#define FIXMUL(x,y) ((x * y) >> 15)
// static mmx_t dummy;




static /*const*/ mmx_t high_mask = (mmx_t)0xFFFF0000FFFF0000LL;
static /*const*/ mmx_t shrt_min  = (mmx_t)0x8000800080008000LL;
//static mmx_t mmxtmp;



/* 16bit integer pseudo sin for the Mx44 synthesizer
-----------------------------------------------------

 * Asserts 16bit short and 32bit (default) int
 * PI == 2^15
 *
 * Use the sum of two mx44_psin in tandem shifted by
 * approx 60 degrees to cancel out most of the odd
 * overtones. (I use an offset of 0x14d4 in Mx44)
 */
inline static
short mx44_psin(short n)
{
  short x = n ^ 0x8000; // x is n shifted 180 degrees
  n = (n * x) >> 13;    // take the halfwawe and ...
  return n ^ (x >> 15); // negate the expression if x is negative
}

// #define S(n) (((n)*((n)^0x8000))>>13)^(((short)((n)^0x8000))>>15)


// Better alternative: S(x) := (x² - 2) (x³ - x)
// .. or perhaps:      S(x) := (x² - 1) (x³/2 - 1.0078125 x ) * 3.015625
//-------------------------------------------------------------------


Mx44state * mx44_new(int maxpoly)
{
  Mx44state* mx44 = malloc(sizeof(Mx44state));
  memset(mx44, 0, sizeof(Mx44state));

  mx44->voices = malloc(sizeof(Mx44voice) * maxpoly + sizeof(mmx_t));
  memset(mx44->voices, 0, sizeof(Mx44voice) * maxpoly + sizeof(mmx_t));

  mx44->maxpoly = maxpoly;

  int channel;
  for (channel = 0;channel < MIDICHANNELS;++channel)
    {

      mx44->ptmt[channel] = 127;
      mx44->vol[channel] = 127;

      mx44->vari[channel] = 63;
      mx44->timb[channel] = 127;
      mx44->ctff[channel] = 127;

      mx44->atck[channel] = 127;
      mx44->dec[channel] = 127;
      mx44->loop[channel] = 127;
      mx44->rel[channel] = 127;

      mx44->clst[channel] = 127;
      mx44->exp[channel] = 127;
    }

# ifdef GUI_ONLY
  mx44->samplerate = 12345;
  return mx44;
# endif

  //thread_mutex_init(&mx44->busy,NULL);

  //mx44_alsa_init(mx44);

  mx44->samplerate = mx44_jack_init(mx44);

  return mx44;

}

void mx44_dispose(Mx44state * mx44)
{
  if(mx44)
    {
      if(mx44->voices)
        free(mx44->voices);
      free(mx44);
    }
}


inline static
void append(Mx44state * mx44,int q,Mx44voice *voice)
{
  if(mx44->que[q])
    {
      mx44->last[q]->next = voice;
      voice->previous = mx44->last[q];
      voice->next = NULL;
      mx44->last[q] = voice;
    }
  else
    {
      mx44->que[q] = voice;
      mx44->last[q] = voice;
      voice->previous = NULL;
      voice->next = NULL;
    }
}


inline static
void un_link(Mx44state * mx44,Mx44voice *voice)
{

  int q;
  if(voice->next)
    voice->next->previous = voice->previous;

  if(voice->previous)
    voice->previous->next = voice->next;

  for(q = 0;q < 5; ++q)
    {
      if(mx44->que[q] == voice)
	      mx44->que[q] = voice->next;
      if(mx44->last[q] == voice)
	      mx44->last[q] = voice->previous;
    }

}


inline static
Mx44voice *pop(Mx44state * mx44,int q)
{
  Mx44voice *voice = mx44->que[q];
  if(voice)
    un_link(mx44,voice);
  return voice;
}

/*
void mx44_reset(Mx44state * mx44,double tuningfork,int samplerate)
{
  int i;
  if(mx44->samplerate)
    samplerate = mx44->samplerate;
  tuningfork /= 128.0;

  tuningfork  *=  1.05946309436  *  1.05946309436  *  1.05946309436;

  for(i = 0; i < 128; tuningfork *= 1.05946309436, ++i)
    mx44->keyTable[i] = (tuningfork)*((double)0x10000000/(double)samplerate);

  mx44->que[0] = NULL;
  mx44->que[1] = NULL;
  mx44->que[2] = NULL;
  mx44->que[3] = NULL;
  mx44->que[4] = NULL;
  for(i = 0; i < mx44->maxpoly; ++i )
    append(mx44,3,&(mx44->voices[i]));


  for(i = 0 ; i < MIDICHANNELS;++i)
    {
      mx44->pitchbend[i]  = 512;
      mx44->modulation[i] = mx44->mod_now[i] = 8;
    }
}
*/
 /*
static double c_aaron[] =
  {
    0,
    76.049,
    193.157,
    310.265,//
    386.314,
    503.422,
    579.471,
    696.579,
    772.628,
    889.736,
    1006.843,//
    1082.892
  };
*/
// French/Italian cembalo? Modified meantone.
static double c_ordinaire[] =
  {
    0,
    77,
    193,
    290,//
    386,
    504,
    580,
    696,
    775,
    890,
    997,//
    1083
  };
/*
// Werckmeister II (4), meantone (1691)
double c_werckmeister_II[] =
  {
    0,
    82.405,
    196.090,
    294.135,
    392.180,
    498.045,
    588.270,
    694.135,
    784.360,
    890.225,
    1003.910,
    1086.315
  };



// Werckmeister III (5), even tempered (1691)
double c_werckmeister_III[] =
  {
    0,
    96.09,
    203.09,
    300,
    396.09,
    503.910,
    600.0,
    701.955,
    792.18,
    900.0,
    1001.955,
    1098.045
  };
*/
// Werckmeister IV (6), well tempered (1691)
double c_werckmeister_IV[] =
  {
    0,
    90.225,

    //187.154,
    195.435,

    297.487,
    394.416,
    498.045,
    594.974,
    698.551,
    792.180,
    892.461,
    999.442,
    1096.371
  };

/*
// Herbert Kelletat's "Bach Temperament" (1966)
static double c_kelletat[] =
  {
    0.0,
    90.225,
    195.435,
    294.135,
    386.313,
    498.045,
    588.270,
    700.002,
    792.180,
    890.876,
    996.090,
    1086.315
  };


// Herbert Anton Kellner's "Bach Temperament" (1977)
static double c_kellner[] =
  {
    0,
    90.225,
    194.526,
    294.135,
    389.052,
    498.045,
    588.270,
    697.263,
    792.180,
    891.789,
    996.090,
    1091.007
  };

// Temperament of the Gabler-Organ in Weingarten
// after the restoration of 1983
static double c_weingarten83[] =
  {
    0,
    95.1125,
    196.09,
    301.46625,
    394.135,
    499.0225,
    591.2025,
    698.45,
    798.045,
    895.1125,
    1001.46625,
    1091.2025
  };

// Temperament of the Silberman-Organ in Freiberger Dom
// after the restoration of 1985
static double c_freiberg85[] =
  {
    0,
    90.225,
    196.090,
    298.045,
    394.135,
    500.000,
    590.225,
    698.045,
    790.225,
    896.090,
    1000,
    1092
  };
*/
/***********************************************
 * The Intervals of the Ten Thats of Indian
 * Classical Indian Music relative to C-Major
 *
 * (Note:  - means Flat, # means Sharp)
 *
 * Name              Intervals
 * --------          ---------------------
 * Bilawal:          1, 2, 3, 4,5, 6, 7
 * Khamaj:           1, 2, 3, 4,5, 6,-7
 * Kafi:             1, 2,-3, 4,5, 6,-7
 *
 * Asawari:          1, 2,-3, 4,5,-6,-7
 * Bhairavi:         1,-2,-3, 4,5,-6,-7
 * Bhairav:          1,-2, 3, 4,5,-6, 7
 *
 * Kalyan:           1, 2, 3,#4,5, 6, 7
 * Todi:             1,-2,-3,#4,5,-6, 7
 * Purbi:            1,-2, 3,#4,5,-6, 7
 * Marwa:            1,-2, 3,#4,5, 6, 7
 *
 *************************************************/


static double c_shruti_A[] =
  {
    0.0, // unison

    90.22504, // 256/243 Pythagorean limma
    182.4038, //  10/9   minor whole tone
    294.1351, //  32/27  Pythagorean minor third
    386.3139, //   5/4   just major third
    498.0452, //   4/3   just perfect fourth
    590.2239, //  45/32  just tritonus

    701.9953, //   3/2   perfect fifth

    792.1803, // 128/81  Pythagorean minor sixth
    884.357,  //   5/3   just major sixth
    996.0905, //  16/9   Pythagorean minor seventh
    1088.269, //  15/8   just major seventh
  };
static double c_shruti_B[] =
  {
    0.0, // unison

    111.7313, //  16/15  minor diatonic semitone
    203.9100, //   9/8   major whole tone
    315.6414, //   6/5   just minor third
    407.8201, //  81/64  Pythagorean major third
    519.5515, //  27/20  acute fourth
    611.7302, // 729/512 Pythagorean tritonus

    701.9953, //   3/2   perfect fifth

    813.6866, //   8/5   just minor sixth
    905.8654, //  27/16  Pythagorean major sixth
    1017.596, //   9/5   just major seventh
    1109.8    // 243/128 Pythagorean major seventh
  };


/*
// South Indian?
static double c_bhatkhande[] =
  {
    0,
    99,
    204,
    316,
    394,
    498,
    597,
    702,
    801,
    906,
    1018,
    1096
  };

*/

// Hammond Tonewheel, rational even tempered
static double c_hammond[] =
  {
    0.0,
    99.89267627,
    200.7760963,
    300.488157,
    400.180858,
    499.8955969,
    600.6025772,
    700.5966375,
    799.8695005,
    900.5764808,
    1000.29122,
    1099.983921,
  };

/*
static double c_even_tempered[] =
  {
    0,
    100.0,
    200.0,
    300.0,
    400.0,
    500.0,
    600.0,
    700.0,
    800.0,
    900.0,
    1000.0,
    1100.0
  };
*/
//tuning in cents from C
static void c_tune(int *keytable,double *scale,double low_C, int samplerate)
{
  int oct,n;

  for (oct = 0;oct < 10;++oct)
    {
      for(n= 0; n < 12;++n)
	{
	  double note = low_C * pow(1.0005777895,scale[n]+(oct/2.0)+(n/24.0));
	  keytable[(oct*12)+n] = (note)*((double)0x10000000/(double)samplerate);
	}
      low_C += low_C;
    }
}

//tuning in cents from D
static void d_tune(int *keyTable,double *scale,double low_C, int samplerate)
{
    low_C  *=  1.05946309436  *  1.05946309436;
    c_tune(keyTable+2,scale,low_C,samplerate);
}
void mx44_reset(Mx44state * mx44,double tuningfork,int samplerate)
{
  int i;
  if(mx44->samplerate)
    samplerate = mx44->samplerate;
  tuningfork /= 128.0;

  // apparently we should start at C
  tuningfork  *=  1.05946309436  *  1.05946309436  *  1.05946309436;

  c_tune(mx44->keyTable[0],c_hammond,tuningfork,samplerate);
  c_tune(mx44->keyTable[1],c_werckmeister_IV,tuningfork,samplerate);
  c_tune(mx44->keyTable[2],c_ordinaire,tuningfork,samplerate);  ///

  // IF option "shruti tuned in D"
  d_tune(mx44->keyTable[3],c_shruti_A,tuningfork,samplerate); ///
  d_tune(mx44->keyTable[4],c_shruti_B,tuningfork,samplerate);
  // ELSE
  // c_tune(mx44->keyTable[3],c_shruti_A,tuningfork,samplerate); ///
  // c_tune(mx44->keyTable[4],c_shruti_B,tuningfork,samplerate);

  mx44->que[0] = NULL;
  mx44->que[1] = NULL;
  mx44->que[2] = NULL;
  mx44->que[3] = NULL;
  mx44->que[4] = NULL;



  for(i = 0; i < mx44->maxpoly; ++i )
    append(mx44,3,&(mx44->voices[i]));


  for(i = 0 ; i < MIDICHANNELS;++i)
    {
      mx44->pitchbend[i]  = 512;
      mx44->modulation[i] = mx44->mod_now[i] = 8;
    }
}
/*
inline static
void w( Mx44state * mx44, Mx44voice *voice, int dst_op, int key, int harmonic, int detune)
{
  detune = (mx44->clst[voice->channel] *detune)>>7;
  detune = (((FIXMUL(detune,detune)*detune)) + (detune<<5))<<1;
  switch(dst_op)
    {
    case 0:
      voice->delta02.d[0] = ((((long long)mx44->keyTable[key]*harmonic)>>5)+detune);
      break;
    case 1:
      voice->delta13.d[0] = ((((long long)mx44->keyTable[key]*harmonic)>>5)+detune);
      break;
    case 2:
      voice->delta02.d[1] = ((((long long)mx44->keyTable[key]*harmonic)>>5)+detune);
      break;
    case 3:
      voice->delta13.d[1] = ((((long long)mx44->keyTable[key]*harmonic)>>5)+detune);
      break;
    }
}
*/

inline static
void w( Mx44state * mx44, Mx44voice *voice, int dst_op, int key, int harmonic, int detune)
{
  detune = (mx44->clst[voice->channel] *detune)>>7;
  detune = (((FIXMUL(detune,detune)*detune)) + (detune<<5))<<1;

  if(mx44->temperament != 3)
    key = mx44->keyTable[mx44->temperament][key];
  else
    {
      int mod = (key+10)%12;
      int off = mx44->shruti[mod];
      key = mx44->keyTable[mx44->temperament+off][key];
    }

  key =  ((((long long) key * harmonic)>>5)+detune);

  switch(dst_op)
    {
    case 0:
      voice->delta02.d[0] = key;
      break;
    case 1:
      voice->delta13.d[0] = key;
      break;
    case 2:
      voice->delta02.d[1] = key;
      break;
    case 3:
      voice->delta13.d[1] = key;
      break;
    }
}


inline static
void pm( Mx44voice *voice, int dst_op, int src_op, int amount )
{
  voice->pm[(src_op - dst_op)&0x03].w[dst_op] = amount;
}


inline static
void am( Mx44voice *voice, int dst_op, int src_op, int amount )
{
  voice->am[(src_op - dst_op)&0x03].w[dst_op] = amount;
}


inline static
void env( Mx44voice *voice,
	  int op    , int start_level,
	  int time_0, int level_0, // attack
	  int time_1, int level_1,
	  int time_2, int level_2, // decay
	  int time_3, int level_3,
	  int time_4, int level_4, // sustain
	  int time_5, int level_5,
	  int time_6, int level_6, // release
	  int time_7, int level_7 )
{

  Mx44env *env =&(voice->env);
  int *time = env->time[op];

  int *delta_value = env->delta_value[op];

  //  Warning to self: Do not attemt simplifying this section ever again!
  //  >:-!

  time_0 |= OPLOOP-1, time_0 ++;
  delta_value[0] = ((level_0 - start_level)<<15) / (time_0>>1);
  time[0] = time_0;

  time_1 |= OPLOOP-1, time_1 ++;
  delta_value[1] = ((level_1 - level_0)<<15) / (time_1>>1);
  time[1] = time_1;

  time_2 |= OPLOOP-1, time_2 ++;
  delta_value[2] = ((level_2 - level_1)<<15) / (time_2>>1);
  time[2] = time_2;

  time_3 |= OPLOOP-1, time_3 ++;
  delta_value[3] = ((level_3 - level_2)<<15) / (time_3>>1);
  time[3] = time_3;

  time_4 |= OPLOOP-1, time_4 ++;
  delta_value[4] = ((level_4 - level_3)<<15) / (time_4>>1);
  time[4] = time_4;

  env->sustain[op]  = level_3 << 16;

  time_5 |= OPLOOP-1, time_5 ++;
  delta_value[5] = ((level_3 - level_4)<<15) / (time_5>>1);
  time[5] = time_5;


  time_6 += OPLOOP-1, time_6 ++;


  env->release[op] = level_6 ;

  delta_value[6] = ((level_6 - level_5)<<15) / (time_6>>1);
  time[6] = time_6;

  time_7 |= OPLOOP-1, time_7 ++;
  delta_value[7] = ((level_7 - level_6)<<15) / (time_7>>1);
  time[7] = time_7;


  start_level <<= 16;

  switch(op)
    {
    case 0:
      env->state02.d[0] = start_level;
      env->delta02.d[0] = delta_value[0];
      break;
    case 1:
      env->state13.d[0] = start_level;
      env->delta13.d[0] = delta_value[0];
      break;
    case 2:
      env->state02.d[1] = start_level;
      env->delta02.d[1] = delta_value[0];
      break;
    case 3:
      env->state13.d[1] = start_level;
      env->delta13.d[1] = delta_value[0];
      break;
    }

}
/*
inline static
void mix(Mx44state * mx44,Mx44voice *voice,int op,int volume,int balance,int key)
{
  int left,right,b;
  volume = (FIXMUL(volume,volume)+volume)>>1;
  left = right = volume = FIXMUL(volume,(mx44->keyTable[76-(key>>2)]>>6));

  b = FIXMUL(volume,balance)<<1;

  if(b < 0)
    right += b;
  else
    left  -= b;

  voice->mixL.w[op] = left;
  voice->mixR.w[op] = right;
}
*/


inline static
void mix(Mx44state * mx44,Mx44voice *voice,int op,int volume,int balance,int key)
{
  int left,right,b;
  volume = (FIXMUL(volume,volume)+volume)>>1;
  left = right = volume = FIXMUL(volume,(mx44->keyTable[mx44->temperament][76-(key>>2)]>>6));

  b = FIXMUL(volume,balance)<<1;

  if(b < 0)
    right += b;
  else
    left  -= b;

  voice->mixL.w[op] = left;
  voice->mixR.w[op] = right;
}


inline static
void release(Mx44state * mx44,int hold,Mx44voice *voice)
{

  un_link(mx44,voice);

  if(hold)
    append(mx44,hold,voice);
  else
    {
      Mx44env *env = &voice->env;

      //env->release[0]  <<= 16;
      env->delta_value[0][7] = FIXMUL(env->delta_value[0][7],(env->state02.d[0]>>15));
      env->release[0]  *= (env->state02.d[0]>>14);

      env->delta_value[0][6] = ((env->release[0]>>1) - (env->state02.d[0]>>1))
	/ (env->time[0][6]>>1);
      voice->env.time_count[0] = 0x7FffFFff;
      voice->env.step[0] = 6;

      //env->release[1]  <<= 16;
      env->delta_value[1][7] = FIXMUL(env->delta_value[1][7],(env->state13.d[0]>>15));
      env->release[1]  *= (env->state13.d[0]>>14);

      env->delta_value[1][6] = ((env->release[1]>>1) - (env->state13.d[0]>>1))
	/ (env->time[1][6]>>1);
      voice->env.time_count[1] = 0x7FffFFff;
      voice->env.step[1] = 6;

      //env->release[2]  <<= 16;
      env->delta_value[2][7] = FIXMUL(env->delta_value[2][7],(env->state02.d[1]>>15));
      env->release[2]  *= (env->state02.d[1]>>14);

      env->delta_value[2][6] = ((env->release[2]>>1) - (env->state02.d[1]>>1))
	/ (env->time[2][6]>>1);
      voice->env.time_count[2] = 0x7FffFFff;
      voice->env.step[2] = 6;

      //env->release[3]  <<= 16;
      env->delta_value[3][7] = FIXMUL(env->delta_value[3][7],(env->state13.d[1]>>15));
      env->release[3]  *= (env->state13.d[1]>>14);

      env->delta_value[3][6] = ((env->release[3]>>1) - (env->state13.d[1]>>1))
	/ (env->time[3][6]>>1);
      voice->env.time_count[3] = 0x7FffFFff;
      voice->env.step[3] = 6;

      append(mx44,2,voice);

    }
}

inline static
Mx44voice *assign(Mx44state * mx44)
{
  Mx44voice *voice;

  if((voice = pop(mx44,3))) // silent
    {
      append(mx44,0, voice);
      return voice;
    }
  else if((voice = pop(mx44,2))) // released
    {
      append(mx44,0, voice);
      return voice;
    }
  else if((voice = pop(mx44,4))) // excess hold
    {
      append(mx44,0, voice);
      return voice;
    }
  else if((voice = pop(mx44,1))) // on hold
    {
      append(mx44,0, voice);
      if( mx44->keyboard[voice->channel][voice->key][2] == voice)
	mx44->keyboard[voice->channel][voice->key][2] = NULL;
      else
	if( mx44->keyboard[voice->channel][voice->key][1] == voice)
	  mx44->keyboard[voice->channel][voice->key][1] =
	    mx44->keyboard[voice->channel][voice->key][2],
	    mx44->keyboard[voice->channel][voice->key][2] = NULL;

      return voice;
    }
  else if((voice = pop(mx44,0))) // running
    {
      //fprintf(stderr,"\n Mx44: Critical voice stealing\n");
      append(mx44,0, voice);
      mx44->keyboard[voice->channel][voice->key][0] = NULL;
      return voice;
    }

  return NULL;
}

// FIXME: state never used
inline static
int bias(Mx44state * mx44,int op,int key,int velocity,Mx44patch *patch)
{
  int breakpoint,hival,loval,rval = 0;

  breakpoint = patch->breakpoint[op];
  hival = patch->keybias[op][0];
  loval = patch->keybias[op][1];

  if(breakpoint < key)
    rval = (key - breakpoint) * hival;
  else if(breakpoint > key)
    rval = (breakpoint - key) * loval;

  rval =  0x7FFF - rval;

  if(rval < 0)
    rval = 0;
  if(rval > 0x7FFF)
    rval = 0x7FFF;


  velocity = (velocity * patch->velocity[op]) >> 7;

  if(velocity > 0)
    {
      int tmp = rval;
      rval -= FIXMUL(rval,patch->velocity[op]);
      rval += FIXMUL(velocity,tmp);
    }
  else
    {
      rval += FIXMUL(velocity,rval);
    }

  return  rval;
}

void mx44_noteon(Mx44state * mx44,int channel,int key,int velocity)
{
  static /*const*/ mmx_t off_60    = (mmx_t)0x14d414d414d414d4LL;

  //static /*const*/ mmx_t off_60    = (mmx_t)0x14e414e414e414e4LL;

  Mx44voice *voice;
  Mx44voice *next,*previous;
  int op,j;
  Mx44patch *patch = &mx44->tmpPatch[channel &= CHANNELMASK];
  //unsigned long long clck = rdtsc();

  if(!velocity)
    { // this is actually a release
      mx44_noteoff(mx44,channel,key,velocity);
      return;
    }
  if(mx44->keyboard[channel][key][0])
      mx44_noteoff(mx44,channel,key,0);

  voice = assign(mx44);

  next = voice->next;
  previous = voice->previous;
  mx44->keyboard[channel][key][0] = voice;

  memset(voice, 0, sizeof(*voice));

  voice->next = next;
  voice->previous = previous;

  voice->key = key;
  voice->channel = channel;
  voice->magicnumber = off_60;


  //voice->delay_time = ((double)patch->mod_delay/mx44->keyTable[key])*5000;
  voice->delay_time = ((double)patch->mod_delay/mx44->keyTable[mx44->temperament][key])*5000;

  if(patch->button[0]&PHASEFOLLOWKEYBUTTON)
    voice->state02.d[0] = (patch->phase[0][0]<<17) + (patch->phase[0][1]*(key<<12)) - (PHASEALIGN);
  else
    voice->state02.d[0] = (patch->phase[0][0]<<17) + (patch->phase[0][1]*(velocity<<11)) - (PHASEALIGN);

  if(patch->button[1]&PHASEFOLLOWKEYBUTTON)
    voice->state13.d[0] = (patch->phase[1][0]<<17) + (patch->phase[1][1]*(key<<12)) - (PHASEALIGN);
  else
    voice->state13.d[0] = (patch->phase[1][0]<<17) + (patch->phase[1][1]*(velocity<<11)) - (PHASEALIGN);

  if(patch->button[2]&PHASEFOLLOWKEYBUTTON)
    voice->state02.d[1] = (patch->phase[2][0]<<17) + (patch->phase[2][1]*(key<<12)) - (PHASEALIGN);
  else
    voice->state02.d[1] = (patch->phase[2][0]<<17) + (patch->phase[2][1]*(velocity<<11)) - (PHASEALIGN);

  if(patch->button[3]&PHASEFOLLOWKEYBUTTON)
    voice->state13.d[1] = (patch->phase[3][0]<<17) + (patch->phase[3][1]*(key<<12)) - (PHASEALIGN);
  else
    voice->state13.d[1] = (patch->phase[3][0]<<17) + (patch->phase[3][1]*(velocity<<11)) - (PHASEALIGN);

  for(op = 0; op < 4; ++op)
    {
      int k0,k1,k2,k3,k4,b,b1;
      b =  bias(mx44,op,key,velocity, patch)<<1;

      if(patch->button[op]&EXPRESSIONBUTTON)
	voice->exp |= 1<<op;

      if(patch->intonation[op][0]>0)
	voice->intonation[op][0] = (patch->intonation[op][0]*velocity)<<7;
      else
	voice->intonation[op][0] = (patch->intonation[op][0]*(128-(velocity>>1))*(key>>3))<<4;

      voice->intonation[op][1] =  (SHRT_MAX - patch->intonation[op][1])*(key<<1);
      voice->intonation[op][0] = (voice->intonation[op][0]>>7) * mx44->ptmt[channel];
      /*
	voice->intonation[op][1] =
	(key * (SHRT_MAX - FIXMUL( FIXMUL(patch->intonation[op][1],
	patch->intonation[op][1] ),
	FIXMUL(patch->intonation[op][1],
	patch->intonation[op][1])
	)));
      */
      if(patch->button[op]&WAWEBUTTON)
	voice->complexwawe.w[op] = 0xFFFF;

      if(patch->button[op]&WHEELBUTTON)
	voice->wheel.w[op] = 0xFFFF;

      if(patch->button[op]&LOWPASSBUTTON)
	voice->lowpass.w[op] = 0xFFFF;

      if(patch->button[op]&WAWESHAPEBUTTON)
	voice->waweshape.w[op] = 0xFFFF;

      if(patch->button[op]&MAGICBUTTON)
	{
	  int detune = (mx44->clst[voice->channel] * patch->detune[op])>>7;
	  voice->magicmodulation.w[op] = detune & 0x0F;
	  voice->magicmodulation2.w[op] = detune >> 4;

	  switch(op)
	    {
	    case 0:
	      voice->magicnumber.w[0] = voice->state02.d[0]>>12;
	      break;
	    case 1:
	      voice->magicnumber.w[1] = voice->state13.d[0]>>12;
	      break;
	    case 2:
	      voice->magicnumber.w[2] = voice->state02.d[1]>>12;
	      break;
	    case 3:
	      voice->magicnumber.w[3] = voice->state13.d[1]>>12;
	      break;
	    }

	}


      k0 = 32000 - ((patch->velocityfollow[op]*velocity)>>7);
      k1 = 32000 - ((patch->keyfollow[op][0]*key)>>7);
      k2 = 32000 - ((patch->keyfollow[op][1]*key)>>7);

      k0 = FIXMUL(k0,k1);
      k0 = FIXMUL(k0,k0);

      k1 = FIXMUL(k1,k1);
      k2 = FIXMUL(k2,k2);

      k3 = k4 = k1;

      int atck = mx44->atck[channel];
      atck *= atck;

      int dec = mx44->dec[channel];
      dec *= dec;

      int loop = mx44->loop[channel];
      loop *= loop;

      int rel = mx44->rel[channel];
      rel *= rel;

      k0 = (k0 * atck) >> 14;
      k1 = (k1 * dec) >> 14;
      k2 = (k2 * loop) >> 14;
      k3 = (k3 * rel) >> 14;

      b1 = (b * rel) >> 14;
      w(mx44,voice,op,key,patch->harmonic[op],patch->detune[op]);

      voice->od.w[op] = patch->od[op]+1;

      for(j = 0; j < 4;++j)
	{
	  am(voice, op, j,  patch->am[op][j]);
	  pm(voice, op, j,  patch->pm[op][j]);
	}


      env(voice,
	  op, FIXMUL(patch->env_level[op][0],b),
	  FIXMUL(patch->env_time[op][0],k0)<<3, FIXMUL(patch->env_level[op][1],b),
	  FIXMUL(patch->env_time[op][1],k0)<<3, FIXMUL(patch->env_level[op][2],b),
	  FIXMUL(patch->env_time[op][2],k1)<<3, FIXMUL(patch->env_level[op][3],b),
	  FIXMUL(patch->env_time[op][3],k1)<<3, FIXMUL(patch->env_level[op][4],b),
	  FIXMUL(patch->env_time[op][4],k2)<<3, FIXMUL(patch->env_level[op][5],b),
	  FIXMUL(patch->env_time[op][5],k2)<<3, FIXMUL(patch->env_level[op][6],b),
	  FIXMUL(patch->env_time[op][6],k3)<<3, FIXMUL(patch->env_level[op][7],b1),
	  FIXMUL(patch->env_time[op][7],k4)<<3, 0);
      short balance = patch->mix[op][1] + (mx44->pan[channel]<<9);
      mix(mx44,voice, op, patch->mix[op][0],balance,key);
      //printf("mix1: %d mix2: %d balance: %d pan: %d\n",
      //patch->mix[op][0],patch->mix[op][1],balance,mx44->pan[channel]);

    }

  voice->env.running = 0x0F; // go live!

  //printf ("clck: %d\n",(int)(rdtsc()-clck));
}
void mx44_noteoff(Mx44state * mx44,int channel,int key,int velocity)
{
  Mx44voice *voice;
  channel&=CHANNELMASK;
  if(mx44->holdpedal[channel])
    {

      /**/
      voice = mx44->keyboard[channel][key][2];
      if(voice)
	release(mx44,4,voice);

      /**/
      //mx44->keyboard[channel][key][3] = mx44->keyboard[channel][key][2];
      mx44->keyboard[channel][key][2] = mx44->keyboard[channel][key][1];
      mx44->keyboard[channel][key][1] = mx44->keyboard[channel][key][0];

      voice = mx44->keyboard[channel][key][0];

      if(voice)
	release(mx44,1,voice);
      mx44->keyboard[channel][key][0] = NULL;

    }
  else
    {
      mx44->keyboard[channel][key][1] = NULL;

      voice = mx44->keyboard[channel][key][0];

      if(voice)
	release(mx44,0,voice);
      mx44->keyboard[channel][key][0] = NULL;
    }
}

void mx44_pgmchange(Mx44state* mx44,int channel,int patchnumber)
{
  //  short *lfo;
  channel &= CHANNELMASK;

  mx44->patchNo[channel] = patchnumber;
  mx44->tmpPatch[channel] = mx44->patch[patchnumber];

  //  lfo = mx44->tmpPatch[channel].lfo ;

  //mx44->lfo_rate_delta[channel] = 1;//lfo[0];
  //printf("lfo %i\n",mx44->lfo_rate_delta[channel]);
}
void mx44_pitchbend(Mx44state * mx44,int channel, int lsb, int msb)
{
  mx44->pitchbend[channel &= CHANNELMASK] = ((lsb) + ((msb) <<7)) >>4;
}

void mx44_control(Mx44state * mx44,int channel, int control, int data)
{
  channel &= CHANNELMASK;
  //printf("control: %d data: %d\n",control,data);
  switch(control)
    {
      ///////////////////////////////////////////////////////////////////////////////////
      //
      //  CONTROL NUMBER          CONTROL FUNCTION
      //  ------------------------------------------
      // (2nd Byte value)
      //    Decimal      Hex
      //       0         00H    GS Bank Select           MSB
      //       1         01H       Modulation wheel      MSB
    case 1:
      mx44->modulation[channel] = data;
      break;
      //       2         02H       Breath Controller     MSB
      //       3         03H       Undefined
      //       4         04H       Foot controller       MSB
      //       5         05H       Portamento time       MSB
    case 5:
      mx44->ptmt[channel] = data;

      break;
      //       6         06H       Data entry            MSB
      //       7         07H       Channel volume        MSB
    case 7:
      mx44->vol[channel] = data;
      break;
      //       8         08H       Balance               MSB
      //       9         09H       Undefined
      //      10         0AH       Pan                   MSB
    case 10:
      mx44->pan[channel] = (data-64);
      break;
      //      11         0BH       Expression Controller MSB
    case 11:
      mx44->exp[channel] = data;
      break;
      //      12         0CH       GS Effect Control 1      MSB
      //      13         0DH       GS Effect Control 2      MSB
      //   14-15      0E-0FH       Undefined
      //   16-19      10-13H       General Purpose Controllers (1-4)
      //   20-31      14-1FH       Undefined
      //   32-63      20-3FH       LSB for values 0-31
      //      64         40H       Damper pedal on/off (sustain)
    case 64:
      mx44->holdpedal[channel] = data;

      if(!data) // damper off
	{
	  Mx44voice *v,*voice = mx44->que[1]; // held voices
	  int i = 0;
	  do {
	    mx44->keyboard[channel][i][1] = NULL;
	    mx44->keyboard[channel][i][2] = NULL;

	  }while(++i < 128);

	  while(voice)
	    {
	      v = voice->next;
	      if( voice->channel == channel)
		release(mx44,0,voice);
	      voice = v;
	    }
	  voice = mx44->que[4]; // excess held
	  while(voice)
	    {
	      v = voice->next;
	      if( voice->channel == channel)
		release(mx44,0,voice);
	      voice = v;
	    }
	}
      break;

      //      65         41H       Portamento on/off
      //      66         42H       Sostenuto  on/off
      //      67         43H       Soft Pedal on/off
      //      68         44H       Legato     on/off
      //      69         45H       Hold 2     on/off
      //      70                GS Sound Variation
    case 70:
      mx44->vari[channel] = data;
      break;
      //      71                GS Sound Timbre / Harmonic Intensity
    case 71:
      mx44->timb[channel] = data;
      break;

      //      72                GS Sound Release Time
    case 72:
      mx44->rel[channel] = data;
      break;
      //      73                GS Sound Attack Time
    case 73:
      mx44->atck[channel] = data;
      break;
      //      74                GS Sound Cutoff Freq / Brightness
    case 74:
      mx44->ctff[channel] = data - 64;
      break;
      //      75                GS Sound Decay Time
    case 75:
      mx44->dec[channel] = data;
      break;
      //      76                GS Sound Vibrato Rate
      //      77                GS Sound Vibrato Depth
      //      78                GS Sound Vibrato Delay
      //      79                GS Sound Control 10 (default undefined)
    case 79:
      mx44->loop[channel] = data;
      break;
      //   80-83      50-53H       General Purpose Controllers (5-8)
      //   84-90      54-5AH       Undefined
      //      91         5BH       Reverb Send Level
      //      92         5CH       Tremelo Depth
      //      93         5DH       Chorus Send Level
      //      94         5EH       Celeste (Detune) Depth
    case 94:
      mx44->clst[channel] = data;
      break;
      //      95         5FH       Phaser Depth
      //      96         60H       Data increment
      //      97         61H       Data decrement
      //      98         62H       Non-Registered Parameter Number LSB
      //      99         63H       Non-Registered Parameter Number MSB
      //     100         64H       Registered Parameter Number LSB
      //     101         65H       Registered Parameter Number MSB
      // 102-119      66-77H       Undefined
      //     120         78H    GS All Sound Off

    case 120: // All Sound Off
      {
	Mx44voice *voice;



	// FIXME: Not *all* channels ...
	memset(mx44->keyboard,  0, sizeof(mx44->keyboard));
    memset(mx44->holdpedal, 0, sizeof(mx44->holdpedal));

	while((voice = pop(mx44,0)))
	  append(mx44,3,voice);
	while((voice = pop(mx44,1)))
	  append(mx44,3,voice);
	while((voice = pop(mx44,2)))
	  append(mx44,3,voice);

      }
      break;

      // 121-127      79-7FH       Reserved for Channel Mode Messages (See below ...)
      //
      /////////////////////////////////////////////////////////////////////////////////////////////


      /////////////////////////////////////////////////////////////////////////////////////////////
      //
      // CHANNEL MODE MESSAGES
      // ---------------------
      //
      // STATUS                DATABYTES         DESCRIPTION
      // Hex     Binary
      // Bn      1011nnnn      0ccccccc          Mode Messages
      //                       0vvvvvvv
      //                                         ccccccc   =  121: Reset All Controllers
      //                                         vvvvvvv   =  0
      //
      //
      //                                         ccccccc   =  122:Local Control
      //                                         vvvvvvv   =  0, Local Control Off
      //                                         vvvvvvv   =  127, Local Control On
      //
      //                                         ccccccc   =  123: All Notes Off
      //                                         vvvvvvv   =  0
      //
      //
      //                                         ccccccc   =  124: Omni Mode Off (All Notes Off)
      //                                         vvvvvvv   =  0
      //
      //
      //                                         ccccccc   =  125:Omni Mode On (All Notes Off)
      //                                         vvvvvvv   =  0
      //
      //
      //                                         ccccccc   =  126: Mono Mode On (Poly Mode Off)
      //                                                      (All Notes Off)
      //                                         vvvvvvv   =  M, where M is the number of
      //                                                      channels.
      //                                         vvvvvvv   =  0, the number of channels equals the
      //                                                      number of voices in the receiver.
      //
      //                                         ccccccc   =  127: Poly Mode On (Mono Mode Off)
      //                                                      (All Notes Off)
      //                                         vvvvvvv   =  0
      //
      //  Notes
      //  1. nnnn: Basic Channel number (1-16)
      //
      //  2. ccccccc: Controller number (121 - 127)
      //
      //  3. vvvvvvv: Controller value
      //
      /////////////////////////////////////////////////////////////////////////////////////////////

    case 123: // All Notes Off
    case 124: // Omni Mode Off (implicit All Notes Off)
    case 125: // Omni Mode On            ...
    case 126: // Mono Mode On            ...
    case 127: // Poly Mode On            ...
      {
	Mx44voice *v,*voice = mx44->que[0];
	int i = 0;

	do {
	  mx44->keyboard[channel][i][0] = NULL;
	  mx44->keyboard[channel][i][1] = NULL;
	  mx44->keyboard[channel][i][2] = NULL;
	} while(++i < 128);

	while(voice)
	  {
	    v = voice->next;
	    if( voice->channel == channel)
	      release(mx44,0,voice);
	    voice = v;
	  }
	voice = mx44->que[1];
	while(voice)
	  {
	    v = voice->next;
	    if( voice->channel == channel)
	      release(mx44,0,voice);
	    voice = v;
	  }
	voice = mx44->que[4];
	while(voice)
	  {
	    v = voice->next;
	    if( voice->channel == channel)
	      release(mx44,0,voice);
	    voice = v;
	  }
      }
      break;
    default:
      printf("control: %d data %d\n",control,data);
    }
}

void mx44_chanpress(Mx44state* mx44,int channel,int data)
{
  // DANG! My aftertouch is broken now when I need it ...
  // ... and that after *only* 22 years of service. Hmphf!
  //
  // (Oh, wait ...)
}

typedef	union {
  short s[2];
  int i;
}short_2_t;


inline static
void mx44run(Mx44state * mx44,Mx44voice *voice,
	     short * stereo_out,int pitchbend,
	     int vol,int modulation,int mod_13,int mod_24,int *lfo,
	     int exp0,int exp1,int exp2,int exp3,int cutoff)
{  
  Mx44env *env =&(voice->env);
  int *env_step = env->step;
  int i;//, j = ENVLOOP/2;
  int itmp[4];
  movq_m2r(voice->out, mm1);        // Get the previous output             
  
  
  // do the intonation first
  
  itmp[0] = voice->intonation[0][0]>>15;
  itmp[0] *= FIXMUL(itmp[0],itmp[0]);
  
  /**/itmp[2] = voice->intonation[2][0]>>15;
  /**/itmp[2] *= FIXMUL(itmp[2],itmp[2]);
  
  mx44->mmxtmp[0].d[0] 
    = (voice->delta02.d[0]) 
    + ((voice->delta02.d[0] >>9) * (pitchbend + lfo[0]))
    + itmp[0];
  
  if(voice->intonation[0][0])
    {
      if(voice->intonation[0][0] > 0)
	{
	  voice->intonation[0][0]-=(voice->intonation[0][1]);
	  if(voice->intonation[0][0]<0)
	    voice->intonation[0][0] = 0;
	}
      else 
	{
	  voice->intonation[0][0]+=(voice->intonation[0][1]);
	  if(voice->intonation[0][0] > 0)
	    voice->intonation[0][0]= 0;
	}
      voice->intonation[0][1] -= voice->intonation[0][1]>>14;
    }
  
  /**/itmp[1] = voice->intonation[1][0] >> 15;
  /**/itmp[1] *= FIXMUL(itmp[1],itmp[1]);
  
  
  mx44->mmxtmp[0].d[1] 
    = (voice->delta02.d[1]) 
    + ((voice->delta02.d[1] >>9) * (pitchbend + lfo[3]))
    + itmp[2];
  
  if(voice->intonation[2][0])
    {
      if(voice->intonation[2][0] > 0)
	{
	  voice->intonation[2][0]-=(voice->intonation[2][1]);
	  if(voice->intonation[2][0]<0)
	    voice->intonation[2][0]= 0;
	}
      else
	{
	  voice->intonation[2][0]+=(voice->intonation[2][1]);
	  if(voice->intonation[2][0] > 0)
	    voice->intonation[2][0]= 0;
	}
      voice->intonation[2][1] -= voice->intonation[2][1]>>14;
    }
  /**/itmp[3] = voice->intonation[3][0] >> 15;
  /**/itmp[3] *= FIXMUL(itmp[3],itmp[3]);


  mx44->mmxtmp[1].d[0] 
    = (voice->delta13.d[0]) 
    + ((voice->delta13.d[0] >>9) * (pitchbend+lfo[1]))
    + itmp[1];
  
  if(voice->intonation[1][0])
    {
      if(voice->intonation[1][0] > 0)
	{
	  voice->intonation[1][0]-=(voice->intonation[1][1]);
	  if(voice->intonation[1][0]<0)
	    voice->intonation[1][0]= 0;
	}
      else
	{
	  voice->intonation[1][0]+=(voice->intonation[1][1]);
	  if(voice->intonation[1][0] > 0)
	    voice->intonation[1][0]= 0;
	}
      voice->intonation[1][1] -= voice->intonation[1][1]>>14;
    }
  /**/
  /**/
  mx44->mmxtmp[1].d[1] 
    = (voice->delta13.d[1]) 
    + ((voice->delta13.d[1] >>9) * (pitchbend+lfo[2]))
    + itmp[3];
  
  if(voice->intonation[3][0])
    {
      if(voice->intonation[3][0]>0)
	{
	  voice->intonation[3][0]-=(voice->intonation[3][1]);
	  if(voice->intonation[3][0]<0)
	    voice->intonation[3][0]= 0;
	}
      else
	{
	  voice->intonation[3][0]+=(voice->intonation[3][1]);
	  if(voice->intonation[3][0]>0)
	    voice->intonation[3][0]= 0;
	}
      voice->intonation[3][1] -= voice->intonation[3][1]>>14;
    }
  
  //prepare cutoff
  mx44->mmxtmp[2].w[0] = cutoff;
  mx44->mmxtmp[2].w[1] = cutoff;
  mx44->mmxtmp[2].w[2] = cutoff;
  mx44->mmxtmp[2].w[3] = cutoff;

  
  {
    // Set us up the envelopes
    // -- --
    int step = env_step[0]; // op1
    int *running = &voice->env.running;
    *running = 0;
    
    if(step < 9)
      {
	*running = 1; // notify that this voice is still audible 
	
	if(step < 6)          // -- Attack/Sustain:
	  {
	    if(env->time[0][step] <= env->time_count[0])
	      {	
		env->time_count[0] = 0;
		env_step[0] = ++step;
		
		if(step == 6) // -- End of sustain Loop:
		  env_step[0] = step = 4, 
		    env->state02.d[0] = env->sustain[0];
		
		env->delta02.d[0] = env->delta_value[0][step];
	      } 
	  }
	else                 // -- Release:
	  {
	    if(env->time[0][step-1] <= env->time_count[0])
	      {		
		env->time_count[0] = 0;
		    env_step[0] = ++step;
		    env->delta02.d[0] = env->delta_value[0][step-1];
	      } 
	  }
	env->time_count[0] += OPLOOP;
      }
    else
      env->state02.d[0] = 0;
    
    // -- --
    
    step = env_step[1]; // op2
    if(step < 9)
      {
	*running|=2;
	
	if(step < 6) // attack
	      {
		if(env->time[1][step] <= env->time_count[1])
		  {	
		    env->time_count[1] = 0;
		    env_step[1] = ++step;
		    
		    if(step == 6) // sustain loop
		      env_step[1] = step = 4,
			env->state13.d[0] = env->sustain[1];
		    
		    env->delta13.d[0] = env->delta_value[1][step];
		  } 
	      }
	else // release
	  {
	    if(env->time[1][step-1] <= env->time_count[1])
	      {		
		env->time_count[1] = 0;
		    env_step[1] = ++step;
		    env->delta13.d[0] = env->delta_value[1][step-1];
	      } 
	  }
	env->time_count[1] += OPLOOP;
      }
    else
      env->state13.d[0] = 0;
    
    
    step = env_step[2]; // op3
    if(step < 9)
      {
	*running|=4;
	
	if(step < 6) // attack
	  {
	    if(env->time[2][step] <= env->time_count[2])
	      {	
		env->time_count[2] = 0;
		env_step[2] = ++step;
		
		if(step == 6) // sustain loop
		  env_step[2] = step = 4,
		    env->state02.d[1] = env->sustain[2];
		
		env->delta02.d[1] = env->delta_value[2][step];
		  } 
	  }
	else // release
	  {
	    if(env->time[2][step-1] <= env->time_count[2])
	      {		
		env->time_count[2] = 0;
		env_step[2] = ++step;
		env->delta02.d[1] = env->delta_value[2][step-1];
	      } 
	  }
	env->time_count[2] += OPLOOP;
      }
    else
      env->state02.d[1] = 0;
    
    step = env_step[3]; // op4
    if(step < 9)
      {
	*running|=8;
	
	if(step < 6) // attack
	  {
	    if(env->time[3][step] <= env->time_count[3])
	      {	
		env->time_count[3] = 0;
		env_step[3] = ++step;
		
		if(step == 6) // sustain loop
		  env_step[3] = step = 4,
		    env->state13.d[1] = env->sustain[3];
		
		env->delta13.d[1] = env->delta_value[3][step];
	      } 
	  }
	else // release
	  {
	    if(env->time[3][step-1] <= env->time_count[3])
	      {		
		env->time_count[3] = 0;
		env_step[3] = ++step;
		env->delta13.d[1] = env->delta_value[3][step-1];
	      } 
	  }
	env->time_count[3] += OPLOOP;
      }
    else
      env->state13.d[1] = 0;
    
    if(!*running)
      return;//break;
    
  }
  
  
  i = 0;
  
  
  mx44->wheel.w[0] = modulation;
  mx44->wheel.w[1] = modulation;
  mx44->wheel.w[2] = modulation;
  mx44->wheel.w[3] = modulation;
  
  if(!voice->wheel.w[0])
    mx44->wheel.w[0] = mod_13;
  if(!voice->wheel.w[1])
    mx44->wheel.w[1] = mod_24;
  if(!voice->wheel.w[2])
    mx44->wheel.w[2] = mod_13;
  if(!voice->wheel.w[3])
    mx44->wheel.w[3] = mod_24;


  vol *= vol;


  //exp0 = ((exp0*exp0)>>7) + exp0;
  exp0 = (exp0 * vol);
  //  exp1 *= exp1; 
  exp0 >>=6;

  //exp1 = ((exp1*exp1)>>7) + exp1;
  exp1 = (exp1 * vol);
  //  exp1 *= exp1; 
  exp1 >>=6;

  //exp2 = ((exp2*exp2)>>7) + exp2;
  exp2 = (exp2 * vol);
  //  exp2 *= exp2; 
  exp2 >>=6;
  
  //exp3 = ((exp3*exp3)>>7) + exp3;
  exp3 = (exp3 * vol);
  //  exp3 *= exp3; 
  exp3 >>=6;
  
  mx44->volume.w[0] = exp0;
  mx44->volume.w[1] = exp1;
  mx44->volume.w[2] = exp2;
  mx44->volume.w[3] = exp3;

  while(i++ < OPLOOP)  
    { 
      /* FIXME
	 int index = (voice->delay_index + voice->delay_time) & 0x03FF ; // 1024 -1
	 voice->delay_index = (voice->delay_index +1 )  & 0x03FF ; // 1024 -1
      */
      


      movq_r2r(mm1,mm7);
      // Modulation wheel
      movq_m2r(mx44->wheel,mm0);
      //pand_m2r(voice->wheel,mm0);	  
      pmulhw_r2r(mm1,mm0);	  
      psllw_i2r(1,mm0);
      psubw_r2r(mm0,mm1);
      
      // N.state += N.delta
      movq_m2r(voice->state02, mm0);  // Get current state of op 0 and 2      
      movq_m2r(voice->state13, mm3);  // Do the same for op 1 and 3 ..
      
      /**/movq_m2r(voice->pm[0], mm2);    // Prefetch 1st phase modulation (feedback) ...
      /**/movq_m2r(voice->am[0], mm6);    // Prefetch 1st am modulation (feedback)
      
      paddd_m2r(mx44->mmxtmp[0], mm0);  // add the deltas for op 0 and 2 
      movq_r2m(mm0, voice->state02);   // store the result
      
      pand_m2r(high_mask, mm0);        // mask off low word
      psrlq_i2r(16, mm0);        // shift high word from position 1 3 >> 0 2
      
      paddd_m2r(mx44->mmxtmp[1], mm3); // add the deltas for op 1 and 3 
      movq_r2m(mm3, voice->state13);  // store ..
      
      
      pand_m2r(high_mask, mm3);             // mask off low word
      por_r2r(mm3, mm0);              // Combine (as words) op 1 3 and 0 2
      
      //----------------- op 0 (relative to 'self')
      
      
      pmulhw_r2r(mm1, mm2);           // .. adjust pm level
      
      // Rotate previous output (part 1)
      /**/movq_r2r(mm1,mm5);              // copy previous out
      /**/psrlq_i2r(16, mm5);             // shift 3 elements 1 position right .. 
      
      paddw_r2r(mm2, mm0);            // .. and add phase modulation.
      paddw_r2r(mm2, mm0);            // .. and add more phase modulation.
      movq_m2r(voice->pm[1], mm2);    // Get next phase modulation ..
      
      pmulhw_r2r(mm1, mm6);           // .. adjust am level                   
      movq_m2r(voice->am[1], mm4);    // Get next am modulation
      
      
      // Rotate previous output (part 2):
      psllq_i2r(48, mm1);             //  .. rotate 1st element to the end
      por_r2r(mm5, mm1);              //  .. and combine
      
      
      //----------------- op 1
      pmulhw_r2r(mm1, mm2);           // .. adjust pm level                   
      
      // Rotate previous output (part 1)
      /**/movq_r2r(mm1,mm5);              // copy previous out
      /**/psrlq_i2r(16, mm5);             // shift 3 elements 1 position right .. 
      
      paddw_r2r(mm2, mm0);            // .. and add phase modulation.
      paddw_r2r(mm2, mm0);            // .. and add more phase modulation.
      movq_m2r(voice->pm[2], mm2);    // Get next phase modulation ..
      
      pmulhw_r2r(mm1, mm4);           // .. adjust am level                   
      paddsw_r2r(mm4,mm6);            // .. add saturating
      movq_m2r(voice->am[2], mm3);    // Get next am modulation 
      
      
      // Rotate previous output (part 2):
      psllq_i2r(48, mm1);             //  .. rotate 1st element
      por_r2r(mm5, mm1);              //  .. and combine
      
      
      //----------------- op 2
      
      
      pmulhw_r2r(mm1, mm2);           // .. adjust pm level                    
      
      // Rotate previous output (part 1):
      /**/movq_r2r(mm1,mm5);              // copy previous out
      /**/psrlq_i2r(16, mm5);             // .. shift right  
      
      paddw_r2r(mm2, mm0);            // .. add phase modulation.
      paddw_r2r(mm2, mm0);            // .. add phase modulation.
      
      movq_m2r(voice->pm[3], mm2);    // Get next phase modulation ..
      
      pmulhw_r2r(mm1, mm3);           // .. adjust am level                   
      movq_m2r(voice->am[3], mm4);    // Get next am modulation
      
      
      // Rotate previous output (part 2):
      psllq_i2r(48, mm1);             //  .. shift left
      por_r2r(mm5, mm1);              //  .. and combine
      
      
      //----------------- op 3
      
      
      pmulhw_r2r(mm1, mm2);           // .. adjust pm level                   

      //// Rotate previous output (part 1):
      ///**/movq_r2r(mm1,mm5);              // copy previous out
      ///**/psrlq_i2r(16, mm5);             // .. shift right  

      paddw_r2r(mm2, mm0);            // .. and add phase modulation.
      paddw_r2r(mm2, mm0);            // .. and add more phase modulation.
      
      pmulhw_r2r(mm1, mm4);           // .. adjust am level                   
      paddsw_r2r(mm4,mm3);            // .. add am saturating
      

      //// Rotate previous output (part 2):
      //psllq_i2r(48, mm1);             //  .. shift left
      //por_r2r(mm5, mm1);              //  .. and combine
     
      //-----------------
      
      paddsw_r2r(mm6,mm3);            // Sum amplitude modulations
      
      
      // ---------------- Take the 'pseudo sine' / 'waweshape'
      
      //movq_m2r(shrt_min,mm4);  // prepare 180 degree offset
      /**/movq_m2r(voice->magicnumber,mm6); // prepare 60 degree offset
 
      paddw_m2r(voice->magicmodulation2,mm6);
      movq_r2m(mm6,voice->magicnumber);

      /**/paddw_r2r(mm0,mm6);  // n¹ is in mm6, offset 60 degrees

      ////////// experimental ...
      //////////
      
      //pmullw_m2r(voice->od[0],mm0); // n is transposed to some harmonic
      
      //psllw_i2r(2,mm0); // n is transposed some octaves
      //paddw_r2r(mm6,mm0); // .. and then some
      
      //////////
      ////////// ------------

      movq_r2r(mm0,mm5);  // n is in mm0 and mm5
      paddw_m2r(shrt_min,mm0); // x is in mm0, offset 180 degrees
      
      /***/movq_m2r(voice->complexwawe,mm1); // load the condition for selfmodulation
      
      pmulhw_r2r(mm5,mm0); // do halfwawes
      

      
      psraw_i2r(15,mm5); // x is either 0xFFFF or 0
      pxor_r2r(mm5,mm0); // do alternating halfwawes
      
      /***/ pand_r2r(mm0,mm1);   // actually select selfmodulation and ..
      /***/ psubw_r2r(mm1,mm6);  // .. phase modulate n¹ by n if selected
      
      /**/pmullw_m2r(voice->od,mm6); // n¹ is transposed to some harmonic
      
      /**/movq_r2r(mm6,mm5);   // n¹ is in mm5 and mm6
      /**/paddw_m2r(shrt_min,mm6);  // x¹ is in mm6, offset 180 degrees
      /**/pmulhw_r2r(mm5,mm6); // do halfwawes
      
      /**/psraw_i2r(15,mm5);   // x¹ is either 0xFFFF or 0
      /**/pxor_r2r(mm5,mm6);   // do alternating halfwawes
      
      /****/movq_m2r(env->state02, mm5);  /// Get current state of env 0 and 2
      
      paddsw_r2r(mm6,mm0); // sum the two wawe components
      
      /****/movq_m2r(env->state13, mm6);  /// Get current state of env 1 and 3
      
      psllw_i2r(2,mm0);    // normalize the waweform algorithm
      
      //----------------- Envelopes
      
      // N.env.state += N.env.delta
      paddd_m2r(env->delta02, mm5); // add the deltas for op 0 and 2
      movq_r2m(mm5, env->state02);  // store the result
      
      
      pand_m2r(high_mask, mm5);     // mask off low word
      psrlq_i2r(16, mm5);           // shift high word from position 3,1 >> 0,2
      
      paddd_m2r(env->delta13, mm6); // add the deltas for op 1 and 3
      movq_r2m(mm6, env->state13);  // store the result
      
      pand_m2r(high_mask, mm6);       // mask off low word
      por_r2r(mm6, mm5);              // Combine env 0,2 and 1,3 
      
      paddsw_r2r(mm5,mm5);	      // ...
      pmulhw_r2r(mm5, mm5);           // square the envelope values
      paddsw_r2r(mm5,mm5);            // normalize after multiplication
      
      pmulhw_r2r(mm5, mm0);           // Apply envelope
      
      paddsw_r2r(mm0, mm0);           // normalize after multiplication
      
      //----------------- Amplitude Modulation
      
      paddsw_r2r(mm3,mm3);            // .. add saturating to self  	  
      pmulhw_r2r(mm0, mm3);           // Do the amplitude  modulation 
      
      /**/movq_m2r(voice->mixL, mm5);   /// get the left mixer level
      
      paddsw_r2r(mm3,mm3);            // .. add saturating to self  	  
      paddsw_r2r(mm3,mm0);            // .. add saturating to output
      
      
      /**/movq_m2r(voice->mixR, mm3);   /// get the right mixer level
      
      
      // mm1, mm2, mm4 mm6 ...
      //----------------- Resonant Waweshaping

      /**/
      movq_m2r(mx44->mmxtmp[2],mm2);  // Get GS parameter
      pmulhw_r2r(mm0,mm2);
 
      movq_r2r(mm0,mm6);//////////
      psraw_i2r(1,mm6);
      paddsw_r2r(mm2,mm6); 

      psllw_i2r(3,mm6);               // Transpose by 3.5 octaves
      paddw_r2r(mm0,mm6);             //   

      /**/movq_r2r(mm6,mm1);          // Transpose by 2.5 octaves
      /**/psubw_r2r(mm0,mm1);         //

      movq_r2r(mm6,mm4);              // n is in mm6 and mm4
      paddw_m2r(shrt_min,mm6);        // x is in mm6, offset 180 degrees
      /**/movq_r2r(mm1,mm2);              // n¹ is in mm1 and mm4
      pmulhw_r2r(mm4,mm6);            // do halfwawes
      /**/paddw_m2r(shrt_min,mm1);        // x¹ is in mm1, offset 180 degrees
      psraw_i2r(15,mm4);              // x is either 0xFFFF or 0
      /**/pmulhw_r2r(mm2,mm1);            // do halfwawes
      pxor_r2r(mm4,mm6);              // do alternating halfwawes
      /**/psraw_i2r(15,mm2);              // x¹ is either 0xFFFF or 0
      /**/pxor_r2r(mm2,mm1);              // do alternating halfwawes

            
      psubw_r2r(mm2,mm6);             // difference between nx, nx¹

      movq_r2r(mm0,mm2);
      psraw_i2r(1,mm2);               // dampen raw signal

      pand_m2r(voice->waweshape,mm6); // select the effect
      pand_m2r(voice->waweshape,mm2);

      psubsw_r2r(mm2,mm0);            // apply effect
      psubsw_r2r(mm6,mm0);            // 

      //---------------- Lowpass
      psubw_r2r(mm0,mm7);	  	  	  
      
      pand_m2r(voice->lowpass,mm7);
      
      psraw_i2r(1,mm7);
      paddsw_r2r(mm7,mm0);  
      psraw_i2r(1,mm7);
      paddsw_r2r(mm7,mm0);  
      

      //--------------------
      movq_r2r(mm0,mm1);              // Keep value for next loop

      //----------------- Mixer
      // GS channel volume & expression
      
      movq_m2r(mx44->volume,mm6);
      pmulhw_r2r(mm6,mm5);
      paddsw_r2r(mm5,mm5); 
      pmulhw_r2r(mm6,mm3);
      paddsw_r2r(mm3,mm3);
      //paddsw_r2r(mm6,mm0);      
      
      // Left
      pmulhw_r2r(mm0,mm5);            // apply left volume
      movq_r2r(mm5,mm2);
      psrlq_i2r(32, mm2);             // .. shift 32 bits  
      paddsw_r2r(mm5,mm2);            // and sum op1+op3, op2+op4
      movq_r2r(mm2,mm5);
      psrlq_i2r(16, mm5);             // .. shift 16 bits
      paddsw_r2r(mm2,mm5);            // and do the final sum
      
      
      // //**/movd_m2r(*((int*)stereo_out),mm2); // get the sum of n-1 samples
      
      // Right
      pmulhw_r2r(mm0,mm3);            // apply right volume
      movq_r2r(mm3,mm4);
      psrlq_i2r(32, mm4);             // .. shift 32 bits
      paddsw_r2r(mm3,mm4);            // and sum op1+op3, op2+op4
      movq_r2r(mm4,mm3);
      psrlq_i2r(16, mm3);             // .. shift 16 bits
      paddsw_r2r(mm4,mm3);            // and do the final sum
      
      
      
      // Stereo
      punpcklwd_r2r(mm3, mm5);        // combine left and right
      
      // Output
      //      paddsw_r2r(mm2,mm5);            // add the n'th sample
      
   
      movd_m2r(*((int*)stereo_out),mm2); // get the sum of n-1 samples
      paddsw_r2r(mm2,mm5);
      
      movd_r2m(mm5,*((int*)stereo_out)); // .. and store

      //vol += vol_diff;
      stereo_out += 2;                // Next stereo sample
      
      
    }
  
  
  
  // Remember the final state of this voice 
  movq_r2m(mm0,voice->out);
  
  
  //         --: Thats all Folks! :--         //
}

int mx44_play(Mx44state * mx44,short *stereo_out)
{
  int voicecount = 0;
  int q,i;
  int lfo[MIDICHANNELS][4];

  // LFO -----------------------------------------------------------
  //
  for(i = 0; i < MIDICHANNELS; ++i)
    {
      int j;
      short offset = mx44->tmpPatch[i].lfo_button[0]
	? 0 : 0x014d4;//SHRT_MAX >> 1;

      short lfo_env_state  = mx44->tmpPatch[i].lfo_button[1] 
	? ( mx44->modulation[i] - 64 ) << 8 : mx44->lfo_env_state[i] >> 16;

      short wawe0 = (mx44_psin(lfo_env_state)>>1)+(SHRT_MIN>>1);
      short wawe1 = -wawe0 + SHRT_MAX;

      int lfo_rate = 
	((mx44->tmpPatch[i].lfo[0] * wawe0) >> 1)
	+ ((mx44->tmpPatch[i].lfo[1] * wawe1) >> 1);
      int lfo_level = 
	(((mx44->tmpPatch[i].lfo[2] * wawe0) >> 1)
	 + ((mx44->tmpPatch[i].lfo[3] * wawe1) >> 1)) >> 21;
      int lfo_envrate = 
	(((mx44->tmpPatch[i].lfo[4] - 32000) * wawe0) >> 1)
	+ (((mx44->tmpPatch[i].lfo[5] - 32000) * wawe1) >> 1);
      

      for(j = 0 ; j < 4; ++j)
	{
	  lfo[i][j] = -FIXMUL( mx44_psin(( mx44->lfo_rate_state[i] >> 16) + (offset*j)), 
			       lfo_level);
	  
	}

      mx44->lfo_env_state[i] +=  ((lfo_envrate -1300000) >> 4);            
      mx44->lfo_rate_state[i] += ((lfo_rate +1300000)>>2);
      //if(!i) printf("%d\n",lforate);
    } 
  //
  // ---------------------------------------------------------------

  // Filter out noise from fast controller motion
  //int vol_diff;
  for(i = 0;i < MIDICHANNELS;++i)
    {
      if(mx44->modulation[i] != mx44->mod_now[i])
	{
	  if(mx44->modulation[i] > mx44->mod_now[i])
	    ++mx44->mod_now[i];
	  else 
	    --mx44->mod_now[i];
	}
      /*
      if(mx44->vari[i] != mx44->vari_now[i])
	{
	  if(mx44->vari[i] > mx44->vari_now[i])
	    ++mx44->vari_now[i];
	  else 
	    --mx44->vari_now[i];
	}
      */
      if(mx44->timb[i] != mx44->timb_now[i])
	{
	  if(mx44->timb[i] > mx44->timb_now[i])
	    ++mx44->timb_now[i];
	  else 
	    --mx44->timb_now[i];
	}
      if(mx44->vol[i] != mx44->vol_now[i])
	{
	  if(mx44->vol[i] > mx44->vol_now[i])
	    {
	      ++mx44->vol_now[i];
	      //vol_diff = 1;
	    }
	  else
	    {
	      --mx44->vol_now[i];
	      //vol_diff = -1;
	    }
	}
      if(mx44->exp[i] != mx44->exp_now[i])
	{
	  if(mx44->exp[i] > mx44->exp_now[i])
	    {
	      ++mx44->exp_now[i];
	      //exp_diff = 1;
	    }
	  else
	    {
	      --mx44->exp_now[i];
	      //exp_diff = -1;
	    }
	}

    }
  
  for(q = 0; q < 5; ++q)
    {
      Mx44voice *voice;
      
      if(q == 3)
	continue;
      else
	voice = mx44->que[q];
      
      while(voice)
	{
	  if(voice->env.running)
	    {
	      int channel = voice->channel;
	      unsigned int mod = mx44->mod_now[channel];
	      int m1 = mx44->timb_now[channel];
	      int m2 = m1;
	      
	      int ctff = mx44->ctff[channel] << 9;
	      
	      int exp0 = mx44->exp_now[channel];
	      int exp1 = mx44->exp_now[channel];
	      int exp2 = mx44->exp_now[channel];
	      int exp3 = mx44->exp_now[channel];

	      if(voice->exp & 1)
		exp0 = 127-exp0;
	      
	      if(voice->exp & 2)
		exp1 = 127-exp1;

	      if(voice->exp & 4)
		exp2 = 127-exp2;

	      if(voice->exp & 8)
		exp3 = 127-exp3;
	      /*
	      if(mx44->tmpPatch[channel].button[1] &EXPRESSIONBUTTON)
		exp1 = 127-exp1;
	      if(mx44->tmpPatch[channel].button[2] &EXPRESSIONBUTTON)
		exp2 = 127-exp2;
	      if(mx44->tmpPatch[channel].button[3] &EXPRESSIONBUTTON)
		exp3 = 127-exp3;
	      */
	      mod += mod << 7;
	      mod = SHRT_MAX - (mod << 1);
	      m1 += m1 << 7;
	      m2 += m2 << 7;
	      /*
	      m1 = m1 * (127 - mx44->vari_now[voice->channel]); 
	      m2 = m2 * mx44->vari_now[voice->channel]; 
	      */
	      m1 = m1 * (127 - mx44->vari[voice->channel]); 
	      m2 = m2 * mx44->vari[voice->channel]; 
	      m1 >>= 5;
	      m2 >>= 5;


	      if(m1 > 0x07FFF) m1 = 0x07FFF;
	      if(m2 > 0x07FFF) m2 = 0x07FFF;
	      m1 = SHRT_MAX - m1;
	      m2 = SHRT_MAX - m2;


	      voicecount ++;
	      
	      for(i=0;i<4;++i) 
		voice->magicnumber.w[i] += 
		  ((voice->magicmodulation.w[i]));// * mx44->clst[voice->channel])>>7);
	      
	      mx44run( mx44,voice, stereo_out, 
		       mx44->pitchbend[voice->channel],
		       mx44->vol_now[voice->channel],
		       mod,m1,m2,
		       lfo[voice->channel],
		       exp0,exp1,exp2,exp3,ctff 
		       );
	      
	      for(i=0;i<4;++i) 
		voice->magicnumber.w[i] += 
		  ((voice->magicmodulation.w[i]));// * mx44->clst[voice->channel])>>7);
	      
	      mx44run( mx44,voice, stereo_out + SAMPLES, 
		       mx44->pitchbend[voice->channel],
		       mx44->vol_now[voice->channel],
		       mod,m1,m2,
		       lfo[voice->channel],
		       exp0,exp1,exp2,exp3,ctff
		      );
	      voice = voice->next;
	    }
	  else
	    {
	      Mx44voice *next = voice->next;
	      un_link(mx44,voice);
	      append(mx44,3,voice);
	      voice = next;
	    }
	}
    }

  emms();
  return voicecount;
}
