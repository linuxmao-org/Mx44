/*
 * This software is released under GPL. If you don't know what that is
 * immediately point your webbrowser to  http://www.gnu.org and read
 * all about it.
 *
 * The short version: You can modify the sources for your own purposes
 * but you must include the new sources if you distribute a derived work.
 *
 * (C) 2002, 2003, 2004, 2005, 2006, 2009 Jens M Andreasen <ja@linux.nu>
 *     2009 James Morris <james@jwm-art.net>
 *
 *
 *
 *    (
 *     )
 *   c[]
 *
 *
 */ 

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "mx44.h"
#include <math.h>
// hint_t groups
#define MISC 0
#define ENV1 1 /* time  */
#define ENV2 2 /* level */
#define ENV3 3 /* other */
#define MOD1 4 /* phase */
#define MOD2 5 /* amplitude */
#define FREQ 6
#define BIAS 7

#define DELAY 8
#define DIST 9
#define LFO 10

#define CSZ 15 // cell size
//static char * widget_theme = "/usr/share/themes/Ia Ora One/gtk-2.0/gtkrc";


extern Mx44state *      mx44;

static Mx44patch *      mx44patch;      // Stored patches
static char*            mx44patchNo;    // index of patch in use
static Mx44patch*       mx44tmpPatch;   // copy of patch in use.

static int midichannel = 0;
static int group,bank,patch;
static int patch_change =1;

static int saving;
static int copied = 0;

static const char *op_label[]= {
        "Op 1",
        "Op 2",
        "Op 3",
        "Op 4",
        NULL
};

static const char *od_tips []= {
        "1st",
        "2nd",
        "3rd",
        "4th",
        "5th",
        "6th",
        "7th",
        "8th",
        NULL
};

static const char *temp_tips [] = {
        " Even Tempered (Hammond Tonewheel) ",
        " Well Tempered (Werckmeister IV) ",
        " Mean Tone (Italian Renaisance Cembalo) ",
        " Natural Open D (Modern Indian Shruti) "
};

static const char *shruti_tips [] = {
     /* "'D' tonica (Sa)",  */
        " pythagorean limma | minor diatonic semitone (ri)",
        " minor- | major whole tone (RI)",
        " pythagorean- | just minor third (ga)",
        " just- | pythagorean major third (GA)",
        " perfect- | acute fourth (MA)",
        " just- | pythagorean tritonus (ma)",
     /* "'A' perfect fifth (Pa)",   */
        " pythagorean- | just minor sixth (da)",
        " just- | pythagorean major sixth (DA)",
        " pythagorean- | just minor seventh (ni)",
        " just- | pythagorean major seventh (NI)"
};


static GtkWidget        *window1 = 0, *scale_table;
static GtkWidget        *patchname = 0;
static GtkWidget        *patch_group_1 = 0;
static GtkWidget        *patch_group_2 = 0;
static GtkComboBox      *bank_entry = 0;
static GtkComboBox      *patch_entry = 0;
static GtkObject        *midichannel_spinner_adj = 0;
static GtkAdjustment    *spin_adj = 0;
static GtkToggleButton  *savebutton = 0;

volatile struct
{
  Mx44patch *tmp;
  int channel ;
  int number;
} newpatch = { 0, 0, 0 };


typedef struct _hint
{
  int op;
  int group;
  int index;
  float min;
  float max;
  char * spinlabel;
  
}hint_t;

static struct mx44patch_edit
{
  GtkWidget *pm[4][4]; // [destination] [source]
  GtkWidget *am[4][4];
  GtkWidget *mix[4][2]; // [op] [level/balance]

  GtkWidget *env_level[4][8]; // [op] [stage]
  GtkWidget *env_time[4][8];

  GtkWidget *wheelbutton[4][2];

  GtkWidget *copybutton[4];
  GtkWidget *pastebutton[4];

  GtkWidget *od[4][8];
  GtkWidget *od_group[4];
  GtkWidget *wawebutton[4];
  GtkWidget *lowpassbutton[4];
  GtkWidget *waweshapebutton[4];
  GtkWidget *phasefollowkeybutton[4];
  GtkWidget *magicbutton[4];
  GtkWidget *expressionbutton[4];

  GtkWidget *harmonic[4];
  GtkWidget *detune[4];
  GtkWidget *intonation[4][2];


  GtkWidget *velocityfollow[4];
  // key bias
  GtkWidget *breakpoint[4];    // [op]
  GtkWidget *keybias[4][2];    // [op][hi/lo value]
  // env keyfollow
  GtkWidget *keyfollow[4][2]; // [op][attack/sustain-loop]
  // key velocity
  GtkWidget *velocity[4]; // [op]

  GtkWidget *phase[4][2]; // [op][offset/velocityfollow]

  GtkWidget *temperament[7];
  GtkWidget *shruti[10];

  //GtkWidget  *mod_delay; // delay for am/pm
  //GtkWidget  *mod_keyfollow; // keyfollow
  //GtkWidget  *delay_table;
  //GtkWidget  *
  //GtkWidget  *
  //GtkWidget  *
  //  GtkWidget *spinbutton[4];
  // GtkWidget *spinlabel[4];


  GtkWidget *common_spinbutton;
  GtkWidget *common_spinlabel;
  GtkWidget *common_oplabel;

  GtkWidget *monobutton;
  GtkWidget *lfo[6];
  GtkWidget *lfo_button[2];


  GtkWidget *window;
  GtkWidget *table[4];
  GtkWidget *canvas;
  int op;

}ed;


typedef struct
{
  short  pm[4];
  short  am[4];
  short mix[2];
  short  env_level[8];
  short  env_time[8];
  short harmonic;
  short intonation[2];
  short detune;
  short velocityfollow;
  unsigned char od;
  unsigned char button;
  short breakpoint;
  short keybias[2];
  short keyfollow[2];
  short velocity;
  short phase[2];
}mx44op_copypaste_buf;

static mx44op_copypaste_buf mx44op_buf;

static
void set_value(GtkRange* range,double value)
{
  if(range)
    {
      GtkAdjustment *adj = gtk_range_get_adjustment(range);
      gtk_adjustment_set_value(adj,value);
    }
  
}


static
void set_widgets(Mx44patch *tmp_patch,int channel ,int patchNumber)
{
  int i, op;

  patch_change = 1;
  
  if(midichannel == -1)
    {
      midichannel = channel;
      gtk_adjustment_set_value((GtkAdjustment*)midichannel_spinner_adj,midichannel+1);
    }
  //if(midichannel != channel)    return;
  
  tmp_patch = tmp_patch + channel;
  
  
   if( patchNumber != -1)
     {
      group = patchNumber >= 64;
      bank  = (patchNumber & 0x03F)>>3;
      patch = patchNumber & 0x07;
      
      gtk_combo_box_set_active((GtkComboBox*)bank_entry,bank);
      
      gtk_combo_box_set_active((GtkComboBox*)patch_entry,patch);
      
      if(group)
	g_signal_emit_by_name(patch_group_2,"clicked");
      //gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (patch_group_2), TRUE);
      else
	g_signal_emit_by_name(patch_group_1,"clicked");
      //gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (patch_group_1), TRUE);
      
      
      
    }

   //set_value((GtkRange*)ed.mod_delay , tmp_patch->mod_delay/320.0); 
   gtk_entry_set_text (GTK_ENTRY (patchname), tmp_patch->name);
   //printf("name: %s\n", tmp_patch->name);
   for(op = 0; op < 4; ++op)
    {
      
      for(i=0; i < 4; ++i) 
	{
	  set_value((GtkRange*)ed.pm[op][i] , tmp_patch->pm[op][i]/320.0);
	  set_value((GtkRange*)ed.am[op][i] , tmp_patch->am[op][i]/320.0);
	}
      set_value((GtkRange*)ed.mix[op][0] , tmp_patch->mix[op][0]/320.0); 
      set_value((GtkRange*)ed.mix[op][1] , tmp_patch->mix[op][1]/177.0); 
      
      for(i=0; i < 8; ++i) 
	{
	  set_value((GtkRange*)ed.env_level[op][i] , tmp_patch->env_level[op][i]/80.0);
	  set_value((GtkRange*)ed.env_time[op][i] , tmp_patch->env_time[op][i]/320.0);
	}

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.od[op][tmp_patch->od[op]&0x07]), TRUE);
      set_value((GtkRange*)ed.harmonic[op] , tmp_patch->harmonic[op]/128.0);

      //if(tmp_patch->detune[op] <0)
      set_value((GtkRange*)ed.detune[op] , tmp_patch->detune[op]/100.0);
      //else
      //set_value((GtkRange*)ed.detune[op] , pow(tmp_patch->detune[op],1.0/3.0)/3.2);
      set_value((GtkRange*)ed.intonation[op][0] , tmp_patch->intonation[op][0]/327.6);
      set_value((GtkRange*)ed.intonation[op][1] , tmp_patch->intonation[op][1]/327.6);
      
      set_value((GtkRange*)ed.velocityfollow[op] , tmp_patch->velocityfollow[op]/320.0);
      set_value((GtkRange*)ed.breakpoint[op] , tmp_patch->breakpoint[op]);
      set_value((GtkRange*)ed.keybias[op][1] , -sqrt(tmp_patch->keybias[op][0]));
      set_value((GtkRange*)ed.keybias[op][0] , -sqrt(tmp_patch->keybias[op][1]));
      set_value((GtkRange*)ed.keyfollow[op][0] , tmp_patch->keyfollow[op][0]/320.0);
      set_value((GtkRange*)ed.keyfollow[op][1] , tmp_patch->keyfollow[op][1]/320.0);
      set_value((GtkRange*)ed.velocity[op] , tmp_patch->velocity[op]/320.0);
      set_value((GtkRange*)ed.phase[op][0] , tmp_patch->phase[op][0]/320.0);
      set_value((GtkRange*)ed.phase[op][1] , tmp_patch->phase[op][1]/320.0);
      
      if(tmp_patch->button[op]&WAWEBUTTON)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.wawebutton[op]), TRUE);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.wawebutton[op]), FALSE);
      
      if(tmp_patch->button[op]&WHEELBUTTON)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.wheelbutton[op][0]), TRUE);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.wheelbutton[op][0]), FALSE);

      if(tmp_patch->button[op]&LOWPASSBUTTON)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.lowpassbutton[op]), TRUE);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.lowpassbutton[op]), FALSE);
      
      if(tmp_patch->button[op]&WAWESHAPEBUTTON)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.waweshapebutton[op]), TRUE);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.waweshapebutton[op]), FALSE);

      if(tmp_patch->button[op]&PHASEFOLLOWKEYBUTTON)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.phasefollowkeybutton[op]), TRUE);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.phasefollowkeybutton[op]), FALSE);
      
      if(tmp_patch->button[op]&MAGICBUTTON)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.magicbutton[op]), TRUE);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.magicbutton[op]), FALSE);

      if(tmp_patch->button[op]&EXPRESSIONBUTTON)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.expressionbutton[op]), TRUE);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.expressionbutton[op]), FALSE);

    }
   for(i = 0;i<6;++i)
     set_value((GtkRange*)ed.lfo[i] , tmp_patch->lfo[i]/320.0);

   for(i = 0;i<2;++i)
    if(tmp_patch->lfo_button[i])
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.lfo_button[i]), TRUE);
    else
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.lfo_button[i]), FALSE);
   
   if(mx44->monomode[channel])
     gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.monobutton), TRUE);
   else
     gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.monobutton), FALSE);
   
   patch_change = 0;
}


void setwidgets(Mx44patch *tmp_patch,int channel ,int patchNumber)
{
  //  printf("channel: %d midichannel: %d\n",channel,midichannel);
  if(channel == midichannel)
    {
      newpatch.tmp = tmp_patch;
      newpatch.channel = channel;
    }
}


static
int check_patch(void* data)
{
  if(mx44->patchNo[midichannel] != newpatch.number)
    {
      newpatch.number = mx44->patchNo[midichannel];
      set_widgets(newpatch.tmp, newpatch.channel, newpatch.number);
    }
  return 1;
}


// unique widget names
static char * name_n()
{
  static int n =0;
  char tmp[64]; 
  sprintf(tmp,"%s%d","widget_",n++);
  return strdup(tmp);
}


static
void line(GtkWidget *table,int x,int y,int width,int height)
{
  char*name;
  GtkWidget *line = gtk_hseparator_new ();
  name = name_n();
  gtk_widget_set_name (line, name);
  g_object_ref (line);
  g_object_set_data_full (G_OBJECT (ed.window), name, line,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (line);
  gtk_table_attach (GTK_TABLE (table), line, x, x+width, y, y+height,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

}

static char * label_font = "Sans 9";
static
GtkWidget *label(GtkWidget *table,int x,int y,int width,char*text)
{
  //text= "";
  GtkWidget *label = gtk_label_new (text);
  gtk_widget_modify_font (label,pango_font_description_from_string (label_font));
  name_n();
  gtk_widget_set_name (label, text);
  g_object_ref (label);
  g_object_set_data_full (G_OBJECT (table), text, label,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label);
  
  gtk_table_attach (GTK_TABLE (table), label, x,x+width, y, y+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  /*
  {
    //gtk_widget_show (label);
    GtkRequisition rq;
    gtk_widget_size_request ((GtkWidget *)label,
			     &rq);

    printf("h: %d w: %d \n",rq.height,rq.width);
  }
  */
  gtk_widget_set_sensitive (label, FALSE);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  return label;
}


static
int  on_focus( GtkRange *range, GdkEvent *event, hint_t* hint)
{
  GtkAdjustment *adj  = gtk_range_get_adjustment(range);
  if(patch_change)
    return 0;
  if(spin_adj != adj)
    {
      spin_adj = adj;
      /*
	gtk_label_set_text((GtkLabel*)ed.spinlabel[hint->op],hint->spinlabel);
	gtk_spin_button_set_adjustment( (GtkSpinButton*)ed.spinbutton[hint->op], adj);
	gtk_spin_button_set_value ((GtkSpinButton*)ed.spinbutton[hint->op],adj->value);
	
      */
      gtk_label_set_text((GtkLabel*)ed.common_spinlabel,hint->spinlabel);
      if(hint->op <0)
	gtk_label_set_text((GtkLabel*)ed.common_oplabel,"Mx44");
      else
	gtk_label_set_text((GtkLabel*)ed.common_oplabel,op_label[hint->op]);
      gtk_spin_button_set_adjustment( (GtkSpinButton*)ed.common_spinbutton, adj);
      gtk_spin_button_set_value ((GtkSpinButton*)ed.common_spinbutton,adj->value);
    }
  return 0;
}


static
int  on_range( GtkRange *range, GdkEvent *event, hint_t* hint)
{ 
  if(patch_change)
    return 1;
  on_focus(range,event,hint);
  gtk_widget_grab_focus((GtkWidget*)ed.common_spinbutton); 
  
  return 0;
}


static
void on_value_changed( GtkAdjustment *adj, hint_t* hint)
{
  double value = adj->value;
  if(patch_change)
    return;
  if(spin_adj != adj)
    {
      spin_adj = adj;
      /*
	gtk_label_set_text((GtkLabel*)ed.spinlabel[hint->op],hint->spinlabel);
	gtk_spin_button_set_adjustment( (GtkSpinButton*)ed.spinbutton[hint->op],
	adj);
      */
      //gtk_label_set_text((GtkLabel*)ed.common_spinlabel,hint->spinlabel);
      //gtk_spin_button_set_adjustment( (GtkSpinButton*)ed.common_spinbutton,adj);
    }
  
  switch(hint->group)
    {
    case MISC:
      if(hint->index == 0) // phase init
	{
	  mx44tmpPatch[ midichannel ].phase[hint->op][0]
	    = value * 320 ;
	}
      else if(hint->index == 1) // phase velocity
	{
	  mx44tmpPatch[ midichannel ].phase[hint->op][1]
	    = value * 320 ;
	}
      else if(hint->index == 2) // velocity sensitivity
	{
	  mx44tmpPatch[ midichannel ].velocity[hint->op]
	    = value * 320 ;
	}
      else if(hint->index == 3) // output volume
	{
	  mx44tmpPatch[ midichannel ].mix[hint->op][0]
	    = value * 320 ;
	}
      else if(hint->index == 4)  // output balance
	{
	  mx44tmpPatch[ midichannel ].mix[hint->op][1]
	    = value * 177 ;
	}
      break;
    case ENV1: // envelope time
      {
	mx44tmpPatch[ midichannel ].env_time[hint->op][hint->index]
	  = value * 320 ;
	break;
      }
    case ENV2: // envelope level
      {
	mx44tmpPatch[ midichannel ].env_level[hint->op][hint->index]
	  = value * 80;
	break;
      }
    case ENV3:
      if(hint->index == 0) // attacktime velocityfollow
	{
	  mx44tmpPatch[ midichannel ].velocityfollow[hint->op]
	    = value * 320 ;
	}
      else if(hint->index == 1) // attack/release keyfollow
	{
	  mx44tmpPatch[ midichannel ].keyfollow[hint->op][0]
	    = value * 320 ;
	}
      else if(hint->index == 2) // sustainloop keyfollow
	{
	  mx44tmpPatch[ midichannel ].keyfollow[hint->op][1]
	    = value * 320 ;
	}

      break;
    case FREQ:
      if(hint->index == 0) // frequency course
	{
	  mx44tmpPatch[ midichannel ].harmonic[hint->op]
	    = value * 128 ; 
	}
      else if(hint->index == 2) // frequency offset
	{
	  mx44tmpPatch[ midichannel ].detune[hint->op]
	    = value * 100;
	}
      else if(hint->index == 3) // intonation amount
	{
	  mx44tmpPatch[ midichannel ].intonation[hint->op][0]
	    = value * 327.6 ; 
	}
      else if(hint->index == 4) // intonation decay
	{
	  mx44tmpPatch[ midichannel ].intonation[hint->op][1]
	    = value * 327.6 ; 
	}
      break;
    case BIAS:
      if(hint->index == 0) // breakpoint
	{
	  mx44tmpPatch[ midichannel ].breakpoint[hint->op]
	    = value;
	}
      else if(hint->index == 1) // bias low
	{
	  mx44tmpPatch[ midichannel ].keybias[hint->op][1]
	    = value * value;
	}
      else if(hint->index == 2) // bias low
	{
	  mx44tmpPatch[ midichannel ].keybias[hint->op][0]
	    = value * value;
	}
      break;
    case MOD1:
      mx44tmpPatch[ midichannel ].pm[hint->op][hint->index] 
	= value * 320;
      break;
    case MOD2:
      mx44tmpPatch[ midichannel ].am[hint->op][hint->index] 
	= value * 320;
      break;
    case DELAY:
      puts("no delay yet");
      break;
    case DIST:
      puts("no overdrive yet");

      break;
    case LFO:

      mx44tmpPatch[ midichannel ].lfo[hint->index] 
	= value * 320;
      //puts("no LFO yet");
      //printf("index: %f\n",value * 320);//hint->index);

      break;
    }
    if (copied && hint->group != DELAY && hint->group != DIST && hint->group != LFO)
    {
      gtk_widget_set_sensitive((GtkWidget*)ed.copybutton[hint->op], TRUE);
      gtk_widget_set_sensitive((GtkWidget*)ed.pastebutton[hint->op], TRUE);
    }
}


/*
static
void on_delay( GtkAdjustment *adj, hint_t* hint)
{
  double value = adj->value;
  printf("delay: %f\n",value);
  switch(((int)hint))
    {
    case 0:
      mx44tmpPatch[ midichannel ].mod_delay = value * 320;
      break;
      
    case 1:
      mx44tmpPatch[ midichannel ].mod_keyfollow = value * 320;
      break;
    }
}
*/


static
GtkWidget *scale(GtkWidget *table,int x,int y,int width,int height,
		  float min, float max,int op,int group,int groupindex,char*help)
{
  GtkAdjustment *adj;
  GtkWidget *widget;
  hint_t *hint = malloc(sizeof(hint_t));


  char*name = name_n();
  
  hint->spinlabel=help;
  hint->op = op;
  hint->group = group;
  hint->index = groupindex;
  hint->min = min;
  hint->max = max;
  
  adj = (GtkAdjustment*)gtk_adjustment_new (0, min, max, 0.1, 
						   1.0, 0.0);

  g_signal_connect (adj, "value_changed",
                    G_CALLBACK(on_value_changed), hint);
  

  if(width > height)
    {
      //widget = gtk_hscrollbar_new (GTK_ADJUSTMENT (adj));
      widget = gtk_hscale_new (GTK_ADJUSTMENT (adj));
    }
  else
    {
      //widget = gtk_vscrollbar_new (GTK_ADJUSTMENT (adj));
      widget = gtk_vscale_new (GTK_ADJUSTMENT (adj));
      gtk_range_set_inverted ((GtkRange*)widget, 1);
    }

  g_signal_connect (widget, "enter_notify_event",
		    G_CALLBACK(on_range), hint);
  g_signal_connect (widget, "focus_in_event",
  	        G_CALLBACK(on_focus), hint);
  g_signal_connect (widget, "button_release_event",
		    G_CALLBACK(on_range), hint);

  gtk_widget_set_name (widget,name);
  g_object_ref (widget);
  g_object_set_data_full (G_OBJECT (ed.window),name, widget,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (widget);
  gtk_table_attach (GTK_TABLE (table), widget, x, x+width, y, y+height,
                    (GtkAttachOptions) (GTK_EXPAND|GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND|GTK_FILL), 0, 0);
  gtk_scale_set_draw_value (GTK_SCALE (widget), FALSE);
  return widget;
}


static
void mk_envelope(GtkWidget *table,int x,int y,int op,char*name)
{
  int h = 8;
  int w = 0;
  int i;
  int index = 0;
  int size = 6;
  char*time_help[]={"  attack time 1","  attack time 2","  attack time 3","  attack time 4",
		    "  sustainloop time 1","  sustainloop time 2",
		    "  release time 1","  release time 2"};
  char*level_help[]={"  startlevel","  attack level 1","  attack level 2","  attack level 3",
		     "  sustainloop level 1","  sustainloop level 2",
		     "  release level 1","  release level 2"};

  for(i = 0;i<4;++i,--h,++w)
    {
      //printf("index: %d\n",index);
      ed.env_time[op][index] = scale(table,x+i+1,y+h,size-1,1,
				     0,100,op,ENV1,index,time_help[index]);
      if(i)
	ed.env_level[op][index] = scale(table,x+i+size-1,y+h+1,1,size -2,
					0,100,op,ENV2,index,level_help[index]);
      else
	ed.env_level[op][index] = scale(table,x,y+h,1,size -2,
					0,100,op,ENV2,index,level_help[index]);
    
      index++;
    }

  label(table,h-2,i,size+6,name);

  while(h-->0)
    {
      ++i;
      //printf("index: %d\n",index);
      ed.env_time[op][index] = scale(table,x+i+1,y+h,size-1,1,
				       0,100,op,ENV1,index,time_help[index ]);

      if(h>1)
	ed.env_level[op][index] = scale(table,x+i+size-1,y+h+1,1,size -2,
					  0,100,op,ENV2,index,level_help[index ]);
      else if(h == 1)
	ed.env_level[op][index+1] = scale(table,x+i+size,y+h,1,size -2,
					  0,100,op,ENV2,index+1,level_help[index ]);
      index++;
    }

  ed.velocityfollow[op] = scale(table,
				  6+x,12+y,3,1,
				  0,100,op,ENV3,0,"  attacktime velocityfollow");
  label(ed.table[op],9+x,10+y,5,"Keyfollow");
  ed.keyfollow[op][0] = scale(table,
				9+x,9+y,3,1,
				0,100,op,ENV3,1,"  attack/release keyfollow");
  ed.keyfollow[op][1] = scale(table,
				10+x,8+y,3,1,
				0,100,op,ENV3,2,"  sustainloop keyfollow");
}



static
void mk_patchbay(GtkWidget *table,int x,int y,int op,char*name)
{

  int i;
  char*am_help[]={"  op 1 amplitude modulation","  op 2 amplitude modulation",
		  "  op 3 amplitude modulation","  op 4 amplitude modulation"};
  char*pm_help[]={"  op 1 phase modulation","  op 2 phase modulation",
		  "  op 3 phase modulation","  op 4 phase modulation"};
  for(i = 0;i<8;++i)
    {
      int x2=0;
      char*name;
      GtkWidget *line = gtk_vseparator_new ();

      if(i&2)
	{
	  x2 = 2;
	}

      if(i&1)
	{
	  ed.am[op][i>>1] = scale(table,x+x2,y+i,4,1,
				  -100,100,op,MOD2,i>>1,am_help[i>>1]);
	}
      else
	{
	  name = name_n();
	  gtk_widget_set_name (line, name);
	  g_object_ref (line);
	  g_object_set_data_full (G_OBJECT (ed.window), name, line,
				    (GDestroyNotify) g_object_unref);

	  gtk_table_attach (GTK_TABLE (table), line, x+x2+1, x+x2+3, y+i-(i==0), y+2+i,
			    (GtkAttachOptions) (GTK_FILL),
			    (GtkAttachOptions) (GTK_FILL), 0, 0);
	  ed.pm[op][i>>1] = scale(table,x+x2,y+i,4,1,
				  -100,100,op,MOD1,i>>1,pm_help[i>>1]);
	  gtk_widget_show (line);

	}
    }
  label(table,x,y+2,3," fm");
  label(table,x,y+3,3," am");
}


static
GtkWidget* tab_label(GtkWidget *window,char *name)
{
  GtkWidget *label;
  label = gtk_label_new (name);
  g_object_ref (label);
  g_object_set_data_full (G_OBJECT (window), name_n(), label,
			    (GDestroyNotify) g_object_unref);
  gtk_widget_show (label);
  return label;
}


static
void on_od_clicked (GtkButton *button,
		    void*  user_data)
{
  int value = ((long)user_data)&0x0F;
  int op = ((long)user_data)>>4;
  mx44tmpPatch[ midichannel ].od[op] = value;
  gtk_widget_set_sensitive((GtkWidget*)ed.copybutton[op], TRUE);
  if (copied)
      gtk_widget_set_sensitive((GtkWidget*)ed.pastebutton[op], TRUE);
}


static
void on_temperament_clicked (GtkButton *button,
		    void*  user_data)
{
  int value = user_data - NULL;
  mx44->temperament = value;
//  printf("temperament: %d\n",value);
}


static 
void on_shruti_button_clicked (GtkToggleButton *button,
		    void*  user_data)
{
  int data = user_data - NULL;
  int tmp = 0;
  //  printf("%d\n",data);
  if(gtk_toggle_button_get_active((GtkToggleButton*)ed.shruti[data]))
    tmp = 1;

  data +=1;
  data += (data > 6);
//  printf("shruti[%d] = %d\n",data,tmp);
  mx44->shruti[data] = tmp;

}


static
void on_lfo_button_clicked (GtkToggleButton *button,
		    void*  user_data)
{

  if(button == (GtkToggleButton*)ed.lfo_button[0])
    mx44tmpPatch[ midichannel ].lfo_button[0] = gtk_toggle_button_get_active(button); 
  else if(button == (GtkToggleButton*)ed.lfo_button[1])
    mx44tmpPatch[ midichannel ].lfo_button[1] = gtk_toggle_button_get_active(button); 
}


static
void on_button_clicked (GtkToggleButton *button,
		    void*  user_data)
{
  int op = ((long)user_data)&0x0F;
  int number = ((long)user_data)>>4;
  char b = mx44tmpPatch[ midichannel ].button[op];

  if(gtk_toggle_button_get_active(button)) 
    {
      mx44tmpPatch[ midichannel ].button[op] = b |number;
    } 
  else 
    {
      mx44tmpPatch[ midichannel ].button[op] = b &(~number);
    }
  gtk_widget_set_sensitive((GtkWidget*)ed.copybutton[op], TRUE);
  if (copied)
    gtk_widget_set_sensitive((GtkWidget*)ed.pastebutton[op], TRUE);
}


static
void  patch_changed(void)
{
  int midipatch = group*64+bank*8+patch;
  if(midipatch < 0 || midipatch > 127)
    return;
  mx44patchNo [midichannel] = midipatch;
  newpatch.number = midipatch;
  if(patch_change)
    return;

  if(!saving)
    if(midichannel != -1)
      {

	//puts("patch_changed");
	
	mx44tmpPatch[midichannel] = mx44patch[midipatch];
	
	set_widgets(mx44tmpPatch,midichannel,-1);
      }

}


static
void on_patch_group_1_clicked (GtkButton *button,
			       void*  user_data)
{
  group = 0;
  patch_changed();
  int i;
  for (i = 0; i < 4; ++i)
  {
    gtk_widget_set_sensitive((GtkWidget*)ed.copybutton[i],  TRUE);
    if (copied)
        gtk_widget_set_sensitive((GtkWidget*)ed.pastebutton[i], TRUE);
  }
}


static
void on_patch_group_2_clicked (GtkButton *button,
			       void* user_data)
{
  group = 1;
  patch_changed();
  int i;
  for (i = 0; i < 4; ++i)
  {
    gtk_widget_set_sensitive((GtkWidget*)ed.copybutton[i],  TRUE);
    if (copied)
        gtk_widget_set_sensitive((GtkWidget*)ed.pastebutton[i], TRUE);
  }
}


static
int on_bank_entry_changed (GtkComboBox *combo,
			   void* user_data)
{
  /*
  char* txt = gtk_editable_get_chars(editable,0,50);
  if(*txt)
    {
      bank = txt[1] - 'A';
      patch_changed();
    }
  */
  bank = gtk_combo_box_get_active(combo);
  patch_changed();
  return 0;
}


static
int on_patch_entry_changed (GtkComboBox *combo,
			    void* user_data)
{
  /*
  char* txt = gtk_editable_get_chars(editable,0,50);
  if(*txt)
    {
      patch = txt[1] - '1';
      patch_changed();
    }
  */
  patch = gtk_combo_box_get_active(combo);
  patch_changed();
  return 0;
}


static
void on_save_button_toggled (GtkToggleButton *togglebutton,
			     void* user_data)
{

  if(gtk_toggle_button_get_active(togglebutton))
    saving = TRUE;
  else
    {
      if(savebutton)
	{
	  printf("patch: %i channel %i\n",mx44patchNo [midichannel] ,midichannel);
	  strcpy( mx44tmpPatch[midichannel].name,gtk_entry_get_text((GtkEntry*) patchname ));
	  mx44patch[(unsigned)mx44patchNo[midichannel]]
            = mx44tmpPatch[midichannel];
	}
      
      saving = FALSE;
    }

  savebutton = togglebutton;

}

static
void on_copy_button_pressed (GtkButton * button,
                              void * user_data)
{
    if (midichannel == -1)
        return;
    int op = ((long)user_data)>>4;

    int i;

    for (i = 0; i < 8; ++i)
    {
        mx44op_buf.env_level[i] = mx44tmpPatch[midichannel].env_level[op][i];
        mx44op_buf.env_time[i] =  mx44tmpPatch[midichannel].env_time[op][i];
    }
    for (i = 0; i < 4; ++i)
    {
        mx44op_buf.pm[i] = mx44tmpPatch[midichannel].pm[op][i];
        mx44op_buf.am[i] = mx44tmpPatch[midichannel].am[op][i];
    }
    for (i = 0; i < 2; ++i)
    {
        mx44op_buf.mix[i] =        mx44tmpPatch[midichannel].mix[op][i];
        mx44op_buf.intonation[i] = mx44tmpPatch[midichannel].intonation[op][i];
        mx44op_buf.keybias[i] =    mx44tmpPatch[midichannel].keybias[op][i];
        mx44op_buf.keyfollow[i] =  mx44tmpPatch[midichannel].keyfollow[op][i];
        mx44op_buf.phase[i] =      mx44tmpPatch[midichannel].phase[op][i];
    }
    mx44op_buf.harmonic =       mx44tmpPatch[midichannel].harmonic[op];
    mx44op_buf.detune =         mx44tmpPatch[midichannel].detune[op];
    mx44op_buf.velocityfollow = mx44tmpPatch[midichannel].velocityfollow[op];
    mx44op_buf.od =             mx44tmpPatch[midichannel].od[op];
    mx44op_buf.button =         mx44tmpPatch[midichannel].button[op];
    mx44op_buf.breakpoint =     mx44tmpPatch[midichannel].breakpoint[op];
    mx44op_buf.velocity =       mx44tmpPatch[midichannel].velocity[op];

    gtk_widget_set_sensitive((GtkWidget*)button, FALSE);

    for (i = 0; i < 4; ++i)
    {
        if (i == op)
            gtk_widget_set_sensitive((GtkWidget*)ed.pastebutton[op],FALSE);
        else
        {
            gtk_widget_set_sensitive((GtkWidget*)ed.pastebutton[i], TRUE);
            gtk_widget_set_sensitive((GtkWidget*)ed.copybutton[i],  TRUE);
        }
    }
    copied = 1;
}


static
void on_paste_button_pressed (GtkButton * button,
                               void * user_data)
{
    if (midichannel == -1)
        return;
    int op = ((long)user_data)>>4;
    int i;
    for (i = 0; i < 8; ++i)
    {
        mx44tmpPatch[midichannel].env_level[op][i] = mx44op_buf.env_level[i];
        mx44tmpPatch[midichannel].env_time[op][i]  = mx44op_buf.env_time[i];
    }
    for (i = 0; i < 4; ++i)
    {
        mx44tmpPatch[midichannel].pm[op][i] = mx44op_buf.pm[i];
        mx44tmpPatch[midichannel].am[op][i] = mx44op_buf.am[i];
    }
    for (i = 0; i < 2; ++i)
    {
        mx44tmpPatch[midichannel].mix[op][i] =        mx44op_buf.mix[i];
        mx44tmpPatch[midichannel].intonation[op][i] = mx44op_buf.intonation[i];
        mx44tmpPatch[midichannel].keybias[op][i] =    mx44op_buf.keybias[i];
        mx44tmpPatch[midichannel].keyfollow[op][i] =  mx44op_buf.keyfollow[i];
        mx44tmpPatch[midichannel].phase[op][i] =      mx44op_buf.phase[i];
    }
    mx44tmpPatch[midichannel].harmonic[op] =       mx44op_buf.harmonic;
    mx44tmpPatch[midichannel].detune[op] =         mx44op_buf.detune;
    mx44tmpPatch[midichannel].velocityfollow[op] = mx44op_buf.velocityfollow;
    mx44tmpPatch[midichannel].od[op] =             mx44op_buf.od;
    mx44tmpPatch[midichannel].button[op] =         mx44op_buf.button;
    mx44tmpPatch[midichannel].breakpoint[op] =     mx44op_buf.breakpoint;
    mx44tmpPatch[midichannel].velocity[op] =       mx44op_buf.velocity;

    set_widgets(mx44tmpPatch,midichannel,mx44patchNo[midichannel]);

    gtk_widget_set_sensitive((GtkWidget*)ed.copybutton[op],  FALSE);
    gtk_widget_set_sensitive((GtkWidget*)ed.pastebutton[op], FALSE);

}


static
void on_esc_save_button_pressed (GtkButton *button,
				 void* user_data)
{
  GtkToggleButton *sb = savebutton;
  savebutton = 0;
  if(sb)
    gtk_toggle_button_set_active(sb,FALSE);
}


static
void on_monobutton_toggled (GtkToggleButton *togglebutton,
			     void* user_data)
{
  mx44->monomode[midichannel] = gtk_toggle_button_get_active(togglebutton);
}


static
int on_ch_combo( GtkComboBox *combo, void* user_data)
{
  midichannel = gtk_combo_box_get_active(combo);
  set_widgets(mx44tmpPatch,midichannel,mx44patchNo[midichannel]);
  newpatch.number = mx44->patchNo[midichannel];
  return 0;
}


static
GtkWidget* create_window (int has_rc)
{

  GtkWidget  *frame,*basetable = 0;

  ed.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  if(has_rc)
    {
      GtkWidget *label = gtk_label_new 
	("ABCD EFG HIJKLMN opqest uvw xyz 123..890!");
      
      g_object_ref (label);
      gtk_container_add (GTK_CONTAINER (ed.window), label);
      
      
      GtkRequisition rq;
      int i = 7;
      char buf[32];
      
      do {
	sprintf(buf,"%s %2d","Sans",i);
	
	gtk_widget_modify_font (label,pango_font_description_from_string (buf));
	gtk_widget_size_request ((GtkWidget *)label,&rq);
	
	printf("'%7s'  h:%3d  w:%4d \n",buf,rq.height,rq.width);
	
      } while(rq.height < 13 && ++i < 42);
      
      label_font = strdup(buf);
      gtk_container_remove (GTK_CONTAINER (ed.window), label);
    }



  char *name = name_n();

  gtk_container_set_border_width (GTK_CONTAINER (ed.window), 1);
  gtk_window_set_title (GTK_WINDOW (ed.window), "Mx44.2");
  
  g_signal_connect (ed.window, "destroy",
		    G_CALLBACK(gtk_main_quit),NULL);

  window1=ed.window;
  gtk_widget_set_name (ed.window, name);
  g_object_set_data (G_OBJECT (ed.window), name, ed.window);
  
  {
    GtkWidget *patch_table,*fx_table;
    
    void *bank_combo;
    void *patch_combo;

    GtkWidget *save_button;

    GtkWidget *esc_save_button;

    GtkWidget *ch_combo;
    GSList *patchgroup_group = NULL;

    basetable = gtk_table_new (3, 4, FALSE);


    g_object_ref (basetable);
    gtk_widget_show(basetable);

    patch_table = gtk_table_new (1, 63, TRUE);
    g_object_ref (patch_table);
    g_object_set_data_full (G_OBJECT (window1), "patch_table", patch_table,
			      (GDestroyNotify) g_object_unref);
    gtk_widget_show (patch_table);
    
    gtk_table_attach (GTK_TABLE (basetable),
		      patch_table, 0,3, 1, 2,
		      (GtkAttachOptions) (GTK_FILL|GTK_SHRINK),
		      (GtkAttachOptions) (GTK_FILL), 0, 0);
   
    
    patch_group_1 = gtk_radio_button_new (patchgroup_group);
    patchgroup_group = gtk_radio_button_get_group (
                            GTK_RADIO_BUTTON (patch_group_1));
    g_object_ref (patch_group_1);
    g_object_set_data_full (G_OBJECT (window1), "patch_group_1", patch_group_1,
			      (GDestroyNotify) g_object_unref);
    gtk_widget_show (patch_group_1);

    gtk_table_attach (GTK_TABLE (patch_table), patch_group_1, 1, 3, 0, 1,
		      (GtkAttachOptions) (GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);

    gtk_widget_set_tooltip_text (patch_group_1, "patch group 1");
    
    patch_group_2 = gtk_radio_button_new (patchgroup_group);
    patchgroup_group = gtk_radio_button_get_group (
                        GTK_RADIO_BUTTON (patch_group_2));
    g_object_ref (patch_group_2);
    g_object_set_data_full (G_OBJECT (window1), "patch_group_2", patch_group_2,
			      (GDestroyNotify) g_object_unref);
    gtk_widget_show (patch_group_2);
    gtk_table_attach (GTK_TABLE (patch_table), patch_group_2, 3, 5, 0, 1,
		      (GtkAttachOptions) (GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);

    gtk_widget_set_tooltip_text (patch_group_2, "patch group 2");

    bank_combo = gtk_combo_box_new_text ();
    g_object_ref (bank_combo);
    g_object_set_data_full (G_OBJECT (window1), "bank_combo", bank_combo,
			      (GDestroyNotify) g_object_unref);
    gtk_widget_set_tooltip_text (bank_combo, "bank selection");
    gtk_widget_show (bank_combo);
    gtk_table_attach (GTK_TABLE (patch_table), bank_combo, 5, 9, 0, 1,
		      (GtkAttachOptions) (GTK_EXPAND),
		      (GtkAttachOptions) (0), 0, 0);
    //gtk_widget_set_usize (bank_combo, 45, 25);
    //gtk_container_set_border_width (GTK_CONTAINER (bank_combo), 5);
    GTK_WIDGET_SET_FLAGS (bank_combo, GTK_CAN_FOCUS);
    gtk_combo_box_append_text (GTK_COMBO_BOX (bank_combo), "A");
    gtk_combo_box_append_text (GTK_COMBO_BOX (bank_combo), "B");
    gtk_combo_box_append_text (GTK_COMBO_BOX (bank_combo), "C");
    gtk_combo_box_append_text (GTK_COMBO_BOX (bank_combo), "D");
    gtk_combo_box_append_text (GTK_COMBO_BOX (bank_combo), "E");
    gtk_combo_box_append_text (GTK_COMBO_BOX (bank_combo), "F");
    gtk_combo_box_append_text (GTK_COMBO_BOX (bank_combo), "G");
    gtk_combo_box_append_text (GTK_COMBO_BOX (bank_combo), "H");
    gtk_combo_box_set_active  (GTK_COMBO_BOX (bank_combo), 0);

    bank_entry = bank_combo;

    patch_combo = gtk_combo_box_new_text ();
    g_object_ref (patch_combo);
    g_object_set_data_full (G_OBJECT (window1), "patch_combo", patch_combo,
			      (GDestroyNotify) g_object_unref);
    gtk_widget_show (patch_combo);
    gtk_table_attach (GTK_TABLE (patch_table), patch_combo, 9, 13, 0, 1,
		      (GtkAttachOptions) (GTK_EXPAND),
		      (GtkAttachOptions) (0), 0, 0);

    gtk_combo_box_append_text (GTK_COMBO_BOX (patch_combo), "1");
    gtk_combo_box_append_text (GTK_COMBO_BOX (patch_combo), "2");
    gtk_combo_box_append_text (GTK_COMBO_BOX (patch_combo), "3");
    gtk_combo_box_append_text (GTK_COMBO_BOX (patch_combo), "4");
    gtk_combo_box_append_text (GTK_COMBO_BOX (patch_combo), "5");
    gtk_combo_box_append_text (GTK_COMBO_BOX (patch_combo), "6");
    gtk_combo_box_append_text (GTK_COMBO_BOX (patch_combo), "7");
    gtk_combo_box_append_text (GTK_COMBO_BOX (patch_combo), "8");
    gtk_combo_box_set_active  (GTK_COMBO_BOX (patch_combo), 0);
    
    patch_entry = patch_combo;

    gtk_widget_set_tooltip_text (patch_combo, "patch selection");

    patchname = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(patchname),31);
    g_object_ref (patchname);
    g_object_set_data_full (G_OBJECT (window1), "patchname", patchname,
			      (GDestroyNotify) g_object_unref);
    gtk_widget_show (patchname);
    gtk_table_attach (GTK_TABLE (patch_table), patchname, 13, 27, 0, 1,
		      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		      (GtkAttachOptions) (0), 2, 7);
    
    save_button = gtk_toggle_button_new_with_label ("SAVE");
    g_object_ref (save_button);
    g_object_set_data_full (G_OBJECT (window1), "save_button", save_button,
			      (GDestroyNotify) g_object_unref);
    gtk_widget_show (save_button);
    gtk_table_attach (GTK_TABLE (patch_table), save_button, 27, 31, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);
    gtk_widget_set_tooltip_text (save_button,
                    "select patch to save in, then release button");


    esc_save_button = gtk_button_new_with_label ("Esc");
    g_object_ref (esc_save_button);
    g_object_set_data_full (G_OBJECT (window1), "esc_save_button", esc_save_button,
			      (GDestroyNotify) g_object_unref);
    gtk_widget_show (esc_save_button);
    gtk_table_attach (GTK_TABLE (patch_table), esc_save_button, 31, 34, 0, 1,
		      (GtkAttachOptions) (GTK_FILL|GTK_SHRINK),
		      (GtkAttachOptions) (0), 0, 0);
    gtk_widget_set_tooltip_text (esc_save_button, "cancel save procedure");

    //--

    ed.common_oplabel = tab_label(ed.window, "Mx44");
    gtk_label_set_justify (GTK_LABEL (ed.common_oplabel), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (ed.common_oplabel), 0, 0.5);
    gtk_widget_set_size_request (ed.common_oplabel ,20, -1);
    gtk_table_attach (GTK_TABLE (patch_table),
    	    ed.common_oplabel, 36,39, 0, 1,
    	    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
    	    (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_widget_show(ed.common_oplabel);

 
    ed.common_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (NULL), 1, 2);
    g_object_ref(ed.common_spinbutton);
    gtk_spin_button_set_numeric ((GtkSpinButton*)ed.common_spinbutton,1 );
    gtk_entry_set_alignment ((GtkEntry*)ed.common_spinbutton,1 );
    gtk_widget_set_size_request (ed.common_spinbutton ,50, -1);
    gtk_widget_show (ed.common_spinbutton);
    gtk_table_attach (GTK_TABLE (patch_table), ed.common_spinbutton, 39, 44, 0, 1,
		      (GtkAttachOptions) (GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);
    
    
    ed.common_spinlabel = tab_label(ed.window, "  (C) Jens M Andreasen, 2009");
    gtk_label_set_justify (GTK_LABEL (ed.common_spinlabel), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (ed.common_spinlabel), 0, 0.5);
    gtk_widget_set_size_request (ed.common_spinlabel ,60, -1);
    gtk_table_attach (GTK_TABLE (patch_table),
    	    ed.common_spinlabel, 44,59, 0, 1,
    	    (GtkAttachOptions) (GTK_FILL|GTK_SHRINK),
    	    (GtkAttachOptions) (GTK_FILL|GTK_SHRINK), 0, 0);
    gtk_widget_show(ed.common_spinlabel);
    
    // ----

    ed.monobutton = gtk_toggle_button_new_with_label ("M");

    g_object_ref (ed.monobutton);
    g_object_set_data_full (G_OBJECT (window1), "monobutton", ed.monobutton,
			      (GDestroyNotify) g_object_unref);


    // ----

    ch_combo = gtk_combo_box_new_text ();
    g_object_ref (ch_combo);
    g_object_set_data_full (G_OBJECT (window1), "ch_combo", ch_combo,
			      (GDestroyNotify) g_object_unref);
    //gtk_tooltips_set_tip (tooltips, ch_combo, "bank selection", NULL);
    gtk_widget_show (ch_combo);
   
    gtk_table_attach (GTK_TABLE (patch_table), ch_combo, 59, 65, 0, 1,
		      (GtkAttachOptions) (GTK_SHRINK),
		      (GtkAttachOptions) (0), 0, 0);
     
    //gtk_widget_set_usize (bank_combo, 45, 25);
    //gtk_container_set_border_width (GTK_CONTAINER (ch_combo), 5);
    GTK_WIDGET_SET_FLAGS (ch_combo, GTK_CAN_FOCUS);
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch 1");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch 2");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch 3");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch 4");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch 5");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch 6");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch 7");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch 8");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch 9");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch10");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch11");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch12");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch13");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch14");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch15");
    gtk_combo_box_append_text (GTK_COMBO_BOX (ch_combo), "Ch16");
    gtk_combo_box_set_active  (GTK_COMBO_BOX (ch_combo), 0);


    g_signal_connect (ch_combo, "changed",
		      G_CALLBACK (on_ch_combo),NULL);

    
    g_signal_connect (bank_combo, "changed",
		      G_CALLBACK (on_bank_entry_changed),
		      NULL);
    g_signal_connect (patch_entry, "changed",
		      G_CALLBACK (on_patch_entry_changed),
		      NULL);
    g_signal_connect (save_button, "toggled",
		      G_CALLBACK (on_save_button_toggled),
		      NULL);
    g_signal_connect (esc_save_button, "clicked",
		      G_CALLBACK (on_esc_save_button_pressed),
		      NULL);

    g_signal_connect (ed.monobutton, "toggled",
		      G_CALLBACK (on_monobutton_toggled),
		      NULL);

    g_signal_connect (patch_group_1, "pressed",
		      G_CALLBACK (on_patch_group_1_clicked),
		      NULL);
    g_signal_connect (patch_group_2, "pressed",
		      G_CALLBACK (on_patch_group_2_clicked),
		      NULL);
    

    gtk_widget_grab_focus((GtkWidget*)ed.common_spinbutton);     
    
    {
      GtkWidget *widget;
      widget = gtk_frame_new(NULL);
      gtk_frame_set_shadow_type( GTK_FRAME(widget), GTK_SHADOW_NONE);
      gtk_container_set_border_width (GTK_CONTAINER (widget), 1);      
      gtk_widget_show(widget);
  
      //gtk_container_set_border_width (GTK_CONTAINER (ed.table[ed.op]), 6);
      


      scale_table =  gtk_table_new (1,32, FALSE);
      gtk_widget_ref (scale_table);
      gtk_object_set_data_full (GTK_OBJECT (window1), "scale_table", scale_table,
				(GtkDestroyNotify) gtk_widget_unref);
      gtk_widget_show (scale_table);
      gtk_container_set_border_width (GTK_CONTAINER (scale_table), 0);
      gtk_container_add (GTK_CONTAINER (widget), scale_table);
      
      
      gtk_table_attach (GTK_TABLE (basetable),
			widget, 0, 2, 3, 4,
			(GtkAttachOptions) (GTK_FILL|GTK_SHRINK),
			(GtkAttachOptions) (GTK_FILL|GTK_SHRINK), 0, 0);
      
      widget = gtk_frame_new(NULL);
      gtk_frame_set_shadow_type( GTK_FRAME(widget), GTK_SHADOW_NONE);
      gtk_container_set_border_width (GTK_CONTAINER (widget), 2);      
      gtk_widget_show(widget);
  
      //label(scale_table,0,0,9,"Temperament");



      fx_table = gtk_table_new (35,4, TRUE);
      g_object_ref (fx_table);
      g_object_set_data_full (G_OBJECT (window1), "fx_table", fx_table,
				(GDestroyNotify) g_object_unref);
      gtk_widget_show (fx_table);
      gtk_container_set_border_width (GTK_CONTAINER (fx_table), 1);
      gtk_container_add (GTK_CONTAINER (widget), fx_table);
      
      gtk_table_attach (GTK_TABLE (basetable),
			widget, 2,3, 0, 3,
			(GtkAttachOptions) (GTK_FILL|GTK_SHRINK),
			(GtkAttachOptions) (GTK_FILL|GTK_SHRINK), 0, 0);
      
      {
	int i;
	for (i=0;i < 7;++i)
	  line(fx_table,1,1+i,3,1);
      }

      //label(fx_table,0,10,5,"LFO");

      ed.lfo[0] = scale(fx_table,
			1,12,1,4,
			0,100,-1,LFO,0,"  LFO rate 1");
      ed.lfo[1] = scale(fx_table,
			2,12,1,4,
			0,100,-1,LFO,1,"  LFO rate 2");
      ed.lfo[2] = scale(fx_table,
			4,10,1,4,
			0,100,-1,LFO,2,"  LFO amount 1");
      ed.lfo[3] = scale(fx_table,
			5,9,1,4,
			0,100,-1,LFO,3,"  LFO amount 2");
      ed.lfo[4] = scale(fx_table,
			0,9,5,1,
			0,100,-1,LFO,4,"  LFO time 1");
      ed.lfo[5] = scale(fx_table,
			1,8,5,1,
			0,100,-1,LFO,5,"  LFO time 2");

      label(fx_table,1,10,4," Sync");
      label(fx_table,1,11,4," Wheel");
      
      
      //-------------

      widget = gtk_check_button_new();
      g_object_ref(widget);
      g_object_set_data_full (G_OBJECT (window1), "loopbutton", widget,
				(GDestroyNotify) g_object_unref);
      
      g_signal_connect (widget, "clicked",
			G_CALLBACK (on_lfo_button_clicked),
			NULL);
      
      gtk_widget_show(widget);
      gtk_table_attach (GTK_TABLE (fx_table), widget, 0, 1, 10, 11,
			(GtkAttachOptions) (GTK_SHRINK),
			(GtkAttachOptions) (GTK_SHRINK), 0, 0);
      gtk_widget_set_size_request (widget, CSZ, CSZ);

      ed.lfo_button[0] = widget;
      //-------------

      widget = gtk_check_button_new();
      g_object_ref(widget);
      g_object_set_data_full (G_OBJECT (window1), "touchbutton", widget,
				(GDestroyNotify) g_object_unref);
      /*
      g_signal_connect (ed.wheelbutton[ed.op][0], "clicked",
			G_CALLBACK (on_button_clicked),
			(NULL+(WHEELBUTTON<<4))+ed.op);
      */
      g_signal_connect (widget, "clicked",
			G_CALLBACK (on_lfo_button_clicked),
			NULL+1);
      gtk_widget_show(widget);
      gtk_table_attach (GTK_TABLE (fx_table), widget, 0, 1, 11, 12,
			(GtkAttachOptions) (GTK_SHRINK),
			(GtkAttachOptions) (GTK_SHRINK), 0, 0);
      gtk_widget_set_size_request (widget, CSZ, CSZ);

      ed.lfo_button[1] = widget;
      //---------------
      {
	int i;
	for (i=0;i < 10;++i)
	  line(fx_table,1,22+i,3,1);
      }
    }
  }


  gtk_container_add (GTK_CONTAINER (ed.window), basetable);

  for(ed.op = 0;op_label[ed.op];++ed.op)
    {

      name = name_n();
      ed.table[ed.op] = gtk_table_new (15, 22, TRUE);
      
      
      gtk_widget_set_name (ed.table[ed.op], name);
      g_object_ref (ed.table[ed.op]);
      
      g_object_set_data_full (G_OBJECT (ed.window), name, ed.table[ed.op],
				(GDestroyNotify) g_object_unref);
      
      
      mk_envelope(ed.table[ed.op],0,0,ed.op," Envelope  time / level");

      label(ed.table[ed.op],13,7,4," Wheel");

      ed.wheelbutton[ed.op][0] = gtk_check_button_new();
      g_object_ref(ed.wheelbutton[ed.op][0]);
      g_object_set_data_full (G_OBJECT (window1), "wheelbutton", ed.wheelbutton[ed.op][0] ,
				(GDestroyNotify) g_object_unref);
      g_signal_connect (ed.wheelbutton[ed.op][0], "clicked",
			G_CALLBACK (on_button_clicked),
			(NULL+(WHEELBUTTON<<4))+ed.op);
      gtk_widget_set_tooltip_text (ed.wheelbutton[ed.op][0], "modulation source");
      gtk_widget_show(ed.wheelbutton[ed.op][0]);
      gtk_table_attach (GTK_TABLE (ed.table[ed.op]), ed.wheelbutton[ed.op][0], 12, 13, 7, 8,
			(GtkAttachOptions) (GTK_SHRINK),
			(GtkAttachOptions) (GTK_SHRINK), 0, 0);
      gtk_widget_set_size_request (ed.wheelbutton[ed.op][0], CSZ, CSZ);
      

      label(ed.table[ed.op],0,1,5,"Keybias");
      ed.breakpoint[ed.op] = scale(ed.table[ed.op],
				0,0,5,1,
				0,128,ed.op,BIAS,0, "  bias breakpoint");
      ed.keybias[ed.op][0] = scale(ed.table[ed.op],
				0,2,1,4,
				-100,0,ed.op,BIAS,1,"  bias low");
      ed.keybias[ed.op][1] = scale(ed.table[ed.op],
				1,2,1,4,
				-100,0,ed.op,BIAS,2,"  bias high");


      
      label(ed.table[ed.op],0,14,3,"Phase");
      ed.phase[ed.op][0] = scale(ed.table[ed.op],
			    2,11,1,4,
			      0,100,ed.op,MISC,0,"  phase init");
      ed.phase[ed.op][1] = scale(ed.table[ed.op],
			      3,11,1,4,
			      0,100,ed.op,MISC,1,"  phase velocity / keyfollow");
      
      label(ed.table[ed.op],6,14,5,"Velocity");
      line(ed.table[ed.op],4,12,2,1);
      ed.velocity[ed.op] = scale(ed.table[ed.op],
			      4,10,2,5,
			      -100,100,ed.op,MISC,2,"  velocity sensitivity");

      label(ed.table[ed.op],15,13,3,"Out");
      ed.mix[ed.op][0] = scale(ed.table[ed.op],
			       14,9,1,5,
			       0,100,ed.op,MISC,3,"  output volume");
      ed.mix[ed.op][1] = scale(ed.table[ed.op],
			       12,14,5,1,
			       -180,180,ed.op,MISC,4,"  output balance");

	ed.expressionbutton[ed.op] = gtk_check_button_new();
	gtk_widget_ref(ed.expressionbutton[ed.op]);
	gtk_widget_set_tooltip_text (ed.expressionbutton[ed.op], "expression-pedal inversion");
	gtk_widget_show(ed.expressionbutton[ed.op]);
	gtk_object_set_data_full (GTK_OBJECT (window1), "expression", ed.expressionbutton[ed.op] ,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_table_attach (GTK_TABLE (ed.table[ed.op]),	ed.expressionbutton[ed.op] ,13, 14, 13, 14,
			  (GtkAttachOptions) (GTK_SHRINK),
			  (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	gtk_widget_set_size_request (ed.expressionbutton[ed.op], CSZ, CSZ);
	
	g_signal_connect (ed.expressionbutton[ed.op], "clicked",
			  GTK_SIGNAL_FUNC (on_button_clicked),
			  (NULL+(EXPRESSIONBUTTON<<4))+ed.op);
	


/*----------------------------------------------------*/
/*----------------------------------------------------*/
/*----------------------------------------------------*/
/*----------------------------------------------------*/
/*----------------------------------------------------*/


      ed.copybutton[ed.op] = gtk_button_new_with_label ("C");
      g_object_ref(ed.copybutton[ed.op]);

      g_object_set_data_full (G_OBJECT (window1), "copy_button", ed.copybutton[ed.op],
                              (GDestroyNotify) g_object_unref);

      g_signal_connect (ed.copybutton[ed.op], "clicked",
                        G_CALLBACK(on_copy_button_pressed), (void*)((long)ed.op<<4));
      gtk_widget_show (ed.copybutton[ed.op]);
      

      gtk_table_attach (GTK_TABLE (ed.table[ed.op]), ed.copybutton[ed.op], 10, 11, 12, 14,
		      (GtkAttachOptions) (GTK_FILL|GTK_SHRINK),
		      (GtkAttachOptions) (0), 0, 0);
      

      gtk_widget_set_tooltip_text (ed.copybutton[ed.op], "copy this op");

      ed.pastebutton[ed.op] = gtk_button_new_with_label ("P");
      g_object_ref(ed.pastebutton[ed.op]);

      /*
      g_object_set_data_full (G_OBJECT (window1), "paste_button", ed.pastebutton[ed.op],
                              (GDestroyNotify) g_object_unref);
      */
      g_signal_connect (ed.pastebutton[ed.op], "clicked",
                        G_CALLBACK(on_paste_button_pressed), (void*)((long)ed.op<<4));
      gtk_widget_show (ed.pastebutton[ed.op]);
      
      gtk_table_attach (GTK_TABLE (ed.table[ed.op]), ed.pastebutton[ed.op], 11, 12, 12, 14,
		      (GtkAttachOptions) (GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);
      
      gtk_widget_set_tooltip_text (ed.pastebutton[ed.op], "paste to this op");

      gtk_widget_set_sensitive((GtkWidget*)ed.pastebutton[ed.op], FALSE);

/*----------------------------------------------------*/
/*----------------------------------------------------*/
/*----------------------------------------------------*/
/*----------------------------------------------------*/
/*----------------------------------------------------*/



                  
      {
	int x;

	for(x=0;x<8;++x)
	  {
	    ed.od[ed.op][x] = gtk_radio_button_new ((void*)ed.od_group[ed.op]);
	    ed.od_group[ed.op] = (void*)gtk_radio_button_get_group (
                                        GTK_RADIO_BUTTON (ed.od[ed.op][x]));
	    g_object_ref (ed.od[ed.op][x]);
	    
	    g_object_set_data_full (G_OBJECT (window1), "wawe 0", ed.od[ed.op][x],
				      (GDestroyNotify) g_object_unref);
	    gtk_widget_set_tooltip_text (ed.od[ed.op][x], od_tips[x]);
	    gtk_widget_set_size_request (ed.od[ed.op][x], CSZ, CSZ);
	    gtk_widget_show (ed.od[ed.op][x]);

	    if(x <4)
	      gtk_table_attach (GTK_TABLE (ed.table[ed.op]), ed.od[ed.op][x],x+ 15, x+16, 0, 1,
				(GtkAttachOptions) (GTK_SHRINK),
				(GtkAttachOptions) (GTK_SHRINK), 0, 0);
	    else
	      gtk_table_attach (GTK_TABLE (ed.table[ed.op]), ed.od[ed.op][x],x+ 14, x+15, 1, 2,
				(GtkAttachOptions) (GTK_SHRINK),
				(GtkAttachOptions) (GTK_SHRINK), 0, 0);


	    g_signal_connect (ed.od[ed.op][x], "clicked",
			      G_CALLBACK (on_od_clicked),
			      (NULL+x)+(ed.op<<4));

	  }

	ed.wawebutton[ed.op] = gtk_check_button_new();
	g_object_ref(ed.wawebutton[ed.op]);
	gtk_widget_set_tooltip_text (ed.wawebutton[ed.op], "cross mod");
	gtk_widget_show(ed.wawebutton[ed.op]);
	g_object_set_data_full (G_OBJECT (window1), "complex", ed.wawebutton[ed.op] ,
				  (GDestroyNotify) g_object_unref);
	gtk_table_attach (GTK_TABLE (ed.table[ed.op]),	ed.wawebutton[ed.op] ,20, 21, 0, 1,
			  (GtkAttachOptions) (GTK_SHRINK),
			  (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	gtk_widget_set_size_request (ed.wawebutton[ed.op], CSZ, CSZ);

	g_signal_connect (ed.wawebutton[ed.op], "clicked",
			  G_CALLBACK (on_button_clicked),
			  (NULL+(WAWEBUTTON<<4))+ed.op);
	

	ed.lowpassbutton[ed.op] = gtk_check_button_new();
	g_object_ref(ed.lowpassbutton[ed.op]);
	gtk_widget_set_tooltip_text (ed.lowpassbutton[ed.op], "lowpass");
	gtk_widget_show(ed.lowpassbutton[ed.op]);
	g_object_set_data_full (G_OBJECT (window1), "lowpass", ed.lowpassbutton[ed.op] ,
				  (GDestroyNotify) g_object_unref);
	gtk_table_attach (GTK_TABLE (ed.table[ed.op]),	ed.lowpassbutton[ed.op] ,21, 22, 0, 1,
			  (GtkAttachOptions) (GTK_SHRINK),
			  (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	gtk_widget_set_size_request (ed.lowpassbutton[ed.op], CSZ, CSZ);
	
	g_signal_connect (ed.lowpassbutton[ed.op], "clicked",
			  G_CALLBACK (on_button_clicked),
			  (NULL+(LOWPASSBUTTON<<4))+ed.op);
	

	ed.waweshapebutton[ed.op] = gtk_check_button_new();
	g_object_ref(ed.waweshapebutton[ed.op]);
	gtk_widget_set_tooltip_text (ed.waweshapebutton[ed.op], "env wave shaping");
	gtk_widget_show(ed.waweshapebutton[ed.op]);

	g_object_set_data_full (G_OBJECT (window1), "waweshape", ed.waweshapebutton[ed.op] ,
				  (GDestroyNotify) g_object_unref);
	gtk_table_attach (GTK_TABLE (ed.table[ed.op]),	ed.waweshapebutton[ed.op] ,14, 15, 0, 2,
			  (GtkAttachOptions) (GTK_SHRINK),
			  (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	gtk_widget_set_size_request (ed.waweshapebutton[ed.op], CSZ, CSZ);
	
	g_signal_connect (ed.waweshapebutton[ed.op], "clicked",
			  G_CALLBACK (on_button_clicked),
			  (NULL+(WAWESHAPEBUTTON<<4))+ed.op);
	

	ed.phasefollowkeybutton[ed.op] = gtk_check_button_new();
	g_object_ref(ed.phasefollowkeybutton[ed.op]);
	gtk_widget_set_tooltip_text (ed.phasefollowkeybutton[ed.op], "phase follow key");
	gtk_widget_show(ed.phasefollowkeybutton[ed.op]);

	g_object_set_data_full (G_OBJECT (window1), "complex", ed.phasefollowkeybutton[ed.op] ,
				  (GDestroyNotify) g_object_unref);
	gtk_table_attach (GTK_TABLE (ed.table[ed.op]),	ed.phasefollowkeybutton[ed.op] ,3, 4, 10, 11,
			  (GtkAttachOptions) (GTK_SHRINK),
			  (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	gtk_widget_set_size_request (ed.phasefollowkeybutton[ed.op], CSZ, CSZ);

	g_signal_connect (ed.phasefollowkeybutton[ed.op], "clicked",
			  G_CALLBACK (on_button_clicked),
			  (NULL+(PHASEFOLLOWKEYBUTTON<<4))+ed.op);

	
	ed.magicbutton[ed.op] = gtk_check_button_new();
	g_object_ref(ed.magicbutton[ed.op]);
	gtk_widget_set_tooltip_text (ed.magicbutton[ed.op], "internal detune");
	gtk_widget_show(ed.magicbutton[ed.op]);
	
	g_object_set_data_full (G_OBJECT (window1), "magic", ed.magicbutton[ed.op] ,
				  (GDestroyNotify) g_object_unref);
	gtk_table_attach (GTK_TABLE (ed.table[ed.op]),	ed.magicbutton[ed.op] ,21, 22, 2, 3,
			  (GtkAttachOptions) (GTK_SHRINK),
			  (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	gtk_widget_set_size_request (ed.magicbutton[ed.op], CSZ, CSZ);
	
	g_signal_connect (ed.magicbutton[ed.op], "clicked",
			  G_CALLBACK (on_button_clicked),
			  (NULL+(MAGICBUTTON<<4))+ed.op);
	


	//label(ed.table[ed.op],15,1,3," Wawe &");

	label(ed.table[ed.op],16,2,4," Oscillator");
	ed.harmonic[ed.op] = scale(ed.table[ed.op],
				   16,4,5,2,
				   0,100,ed.op,FREQ,0,"  frequency multiplyer");
	
	line(ed.table[ed.op],21,4,1,2);
	ed.detune[ed.op] = scale(ed.table[ed.op],
				 21,3,1,4,
				 -100,100,ed.op,FREQ,2,"  frequency offset");
    
	line(ed.table[ed.op],15,3,1,2);
	ed.intonation[ed.op][0] = scale(ed.table[ed.op],
					15,2,1,4,
					-100,100,ed.op,FREQ,3,"  intonation amount");
	ed.intonation[ed.op][1] = scale(ed.table[ed.op],
					16,3,4,2,
					0,100,ed.op,FREQ,4,"  intonation decay");
  
      }

      mk_patchbay(ed.table[ed.op],16,7,ed.op,"Modulation");
      
      /* Create a Frame */
      frame = gtk_frame_new(NULL);
      //      gtk_container_add(GTK_CONTAINER(notebook), frame);
      
      switch(ed.op)
	{
	case 0:
	  gtk_table_attach (GTK_TABLE (basetable), frame, 
			    0,1, 
			    0,1,
			    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
			    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND), 0, 0);
	  break;
	  
	case 1:
	  gtk_table_attach (GTK_TABLE (basetable), frame, 
			    1,2, 
			    0,1,
			    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
			    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND), 0, 0);
	  break;
	case 2:
	  gtk_table_attach (GTK_TABLE (basetable), frame, 
			    0,1 , 
			    2,3,
			    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
			    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND), 0, 0);
	  break;
	case 3:
	  gtk_table_attach (GTK_TABLE (basetable), frame, 
			    1,2, 
			    2,3,
			    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
			    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND), 0, 0);
	  
	}
      
      /* Set the style of the frame */
      gtk_frame_set_shadow_type( GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
      gtk_container_set_border_width (GTK_CONTAINER (frame), 1);      
      gtk_widget_show(frame);
  
      gtk_container_set_border_width (GTK_CONTAINER (ed.table[ed.op]), 1);
      gtk_container_add (GTK_CONTAINER (frame), ed.table[ed.op]);
      gtk_widget_show (ed.table[ed.op]);

    }

  ///////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////
  {
    int x = 1;
    int t;
    GtkWidget *t_group=0;
    label(scale_table,0,0,1," Temperament ");

    for(t=0;t < 4; ++t)
      {
	ed.temperament[t] = gtk_radio_button_new ((void*)t_group);
	//RG
	t_group = (void*)
	  gtk_radio_button_group (GTK_RADIO_BUTTON (ed.temperament[0]));
	gtk_widget_ref (ed.temperament[t]);
	gtk_object_set_data_full (GTK_OBJECT (window1), "temperament",
				  ed.temperament[t],
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_set_tooltip_text (ed.temperament[t], temp_tips[t]);
	gtk_widget_show (ed.temperament[t]);

	if(t < 3)
	  gtk_table_attach (GTK_TABLE (scale_table), ed.temperament[t],
			    x, x+1, 0, 1,
			    (GtkAttachOptions) (GTK_SHRINK),
			    (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	else
	  gtk_table_attach (GTK_TABLE (scale_table), ed.temperament[t],
			    x, x+2, 0, 1,
			    (GtkAttachOptions) (GTK_SHRINK),
			    (GtkAttachOptions) (GTK_SHRINK), 0, 0);

	gtk_widget_set_size_request (ed.temperament[t], CSZ, CSZ);
	g_signal_connect (ed.temperament[t], "clicked",
			  GTK_SIGNAL_FUNC (on_temperament_clicked),
			  (NULL+t));

	if (t == mx44->temperament)
            gtk_toggle_button_set_active((GtkToggleButton*)ed.temperament[t], TRUE);
	++x;
      }
    x++;

    for ( t = 0; t < 10; ++t)
      {
	GtkWidget *gw;

        if (t == 0)
	  {
	    gw = label(scale_table, x, 0, 1, ":  D ");
	    gtk_widget_set_tooltip_text (gw," tonica (SA)");
	    gtk_widget_set_sensitive (gw, TRUE);
	    ++x;
	  }
        else if (t == 6)
	  {
            gw = label(scale_table, x, 0, 1, " A ");
	    gtk_widget_set_tooltip_text (gw," perfect fifth (PA)");
	    gtk_widget_set_sensitive (gw, TRUE);
	    ++x;
	  }

	ed.shruti[t] = gtk_check_button_new();
	gtk_widget_ref(ed.shruti[t]);
	gtk_object_set_data_full (GTK_OBJECT (window1), "", ed.shruti[t],
				  (GtkDestroyNotify) gtk_widget_unref);
	g_signal_connect (ed.shruti[t], "clicked",
			  GTK_SIGNAL_FUNC (on_shruti_button_clicked),
			  NULL+t);
	gtk_widget_show(ed.shruti[t]);

	if(t < 6)
	  gtk_table_attach (GTK_TABLE (scale_table), ed.shruti[t],
			    x, x + 1, 0, 1,
			    (GtkAttachOptions) (GTK_SHRINK),
			    (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	else
	{
	  gtk_table_attach (GTK_TABLE (scale_table), ed.shruti[t],
			    x, x + 2, 0, 1,
			    (GtkAttachOptions) (GTK_SHRINK),
			    (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	  ++x;
	}
	gtk_widget_set_size_request (ed.shruti[t], CSZ, CSZ);
	gtk_widget_set_tooltip_text (ed.shruti[t], shruti_tips[t]);
	++x;
      }
    
  }
  
  //set_widgets(mx44tmpPatch,midichannel,mx44patchNo[midichannel]);
  gtk_widget_show(window1);
  
  return NULL;
}


int
main_interface (int argc, char *argv[])
{
  /* FIXME: Globals for the userinterface
   */
  mx44patch =   mx44->patch;
  mx44patchNo = mx44->patchNo;
  mx44tmpPatch = newpatch.tmp = mx44->tmpPatch;

  memset(&mx44op_buf, 0, sizeof(mx44op_copypaste_buf));


  char* rc_path = "../data/gtk-2.0/gtkrc";
  int has_rc = FALSE;
  if(open(rc_path, O_RDONLY, 0444)>0)
    gtk_rc_add_default_file (rc_path),has_rc = TRUE;
  else
    {
      rc_path = malloc(strlen(DATADIR) +strlen("gtkrc") + 2);
      sprintf(rc_path,"%s%s",DATADIR,"gtkrc");
      if(open(rc_path, O_RDONLY, 0444)>0)
	gtk_rc_add_default_file (rc_path),has_rc = TRUE;
      else
	gtk_rc_add_default_file ("/usr/share/themes/Mist/gtk-2.0/gtkrc" );
      free(rc_path);
    }
  gtk_set_locale ();


  gtk_init (&argc, &argv);
  create_window (has_rc);

  set_widgets(mx44tmpPatch,0,mx44patchNo[0]);

  int i,x;
  for(i = 0, x=0; i < 12; ++i)
    {
      if(i == 0|| i == 7)
	continue;
      if(mx44->shruti[i])
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ed.shruti[x]), TRUE);
      ++x;
    }
  g_timeout_add (50,
		 check_patch,
		 NULL);
  gtk_main ();
  return 0;
}
