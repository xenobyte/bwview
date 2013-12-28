//
//	Settings handling code.  Oh dear, this is all a bit of a hack.
//
//        Copyright (c) 2002 Jim Peters <http://uazu.net/>.  Released
//        under the GNU GPL version 2 as published by the Free
//        Software Foundation.  See the file COPYING for details, or
//        visit <http://www.gnu.org/copyleft/gpl.html>.
//

#include "all.h"

//
//	List of settings codes + names
//

#define N_CODES 11
char *set_codes= "oncsvtbwmfx";

//
//	Names of settings (for status line)
//

char *set_names[N_CODES]= {
   "Top octave to display",
   "Number of octaves to display vertically",
   "Channel number",
   "Gain for signal display",
   "Vertical 'pixel' size.  Increase this for faster updates",
   "Time-base (number of samples per pixel, horizontally)",
   "Brightness of main display",
   "Width of window function (determines relative focus between time and frequency)",
   "Display mode",
   "Font size",
   "Algorithm: Blackman, IIR Q=0.5, or IIR Q=0.72",
};

//
//	Settings increments.  "i<num>" is an integer increment.
//	"e<num>" is an exponential increase, such that <num> steps are
//	required to cover an octave. "ie<num>" is like "e<num>",
//	except that it is an integer increment, minimum +1.
//

char *set_inc[N_CODES]= {
   "i1",
   "i1",
   "i1",
   "e8",
   "i1",
   "ie8",
   "e8",
   "e8",
   "i1",
   "i1",
   "i1",
};

//
//	Last setting made from a preset, or '?' if incremented/
//	decremented.
//

char s_preset[N_CODES];

//
//	Presets
//

double set_preset_values[N_CODES][10];


//
//	Initialise settings globals (using loaded config)
//

void 
set_init() {
   int a, b;
   char buf[3];

   buf[2]= 0;
   for (a= 0; a<N_CODES; a++) {
      buf[0]= set_codes[a];
      for (b= 0; b<10; b++) {
	 buf[1]= '0' + b;
	 set_preset_values[a][b]= 
	    config_get_fp(buf);		// Will be NAN if not there
      }
      s_preset[a]= '?';
   }

   // Wipe out 'x' option if -x not given
   if (!opt_x) {
      char *p;
      set_codes= StrDup(set_codes);
      p= strchr(set_codes, 'x'); *p= 0;
   }
}


//
//	Convert a setting character into a 'set' index for all the
//	other calls.  Returns -1 if not a valid character.
//
   
int 
set_index(int ch) {
   char *p= strchr(set_codes, ch);
   if (!p || !ch) return -1;
   return p-set_codes;
}

//
//	Correct the s_preset[] value for this set
//

static void 
fix_preset(BWAnal *aa, int set) {
   double fp= set_get(aa, set);
   int a;

   s_preset[set]= '?';
   for (a= 0; a<10; a++) {
      double pre= set_preset_values[set][a];
      if (!isnan(pre) && (pre == fp || fabs(pre-fp) < 0.0005 * (fabs(pre) + fabs(fp))))
	 s_preset[set]= a + '0';
   }
}

//
//	Code to set a new value for a setting.  Returns: 1 accepted, 0 rejected
//

int 
set_put(BWAnal *aa, int set, double fp) {
   int ii= (int)fp;

   switch (set) {
    case 0:
       if (ii < 1) ii= 1;
       if (ii == s_oct0) return 0;
       s_oct0= ii; restart= 1; break;
    case 1:
       if (ii < 1) ii= 1;
       if (ii == s_noct) return 0;
       s_noct= ii; restart= 1; break;
    case 2: 
       if (ii < 0 || ii >= aa->n_chan) {
	  status("\x8A There are only %d channels in this file ", aa->n_chan);
	  return 0;
       }
       if (ii == s_chan) return 0;
       s_chan= ii; restart= 1; break;
    case 3:
       if (fp == s_gain) return 0;
       s_gain= fp; redraw= 1; break;
    case 4:
       if (ii < 1) ii= 1;
       if (ii == s_vert) return 0;
       s_vert= ii; restart= 1; break;
    case 5:
       if (ii < 1) ii= 1;
       if (ii == s_tbase) return 0;
       s_tbase= ii; restart= 1; break;
    case 6:
       if (fp == s_bri) return 0;
       s_bri= fp; redraw= 1; break;
    case 7:
       if (fp == s_focus) return 0;
       s_focus= fp; restart= 1; break;
    case 8:
       ii &= 7;
       if (ii == s_mode) return 0;
       s_mode= ii; redraw= 1; break;
    case 9:
       s_font= ii &= 1;
       disp_font= ii ? 16 : 8; 
       rearrange= 1;
       break;
    case 10:
       if (ii < 0 || ii > 2) return 0;
       s_iir= ii; restart= 1;
       break;
   }

   // Make sure s_preset[] is up-to-date
   fix_preset(aa, set);
   
   return 1;
}

//
//	Code to get the value of a setting
//

double 
set_get(BWAnal *aa, int set) {
   switch (set) {
    case 0: return s_oct0;
    case 1: return s_noct;
    case 2: return s_chan;
    case 3: return s_gain;
    case 4: return s_vert;
    case 5: return s_tbase;
    case 6: return s_bri;
    case 7: return s_focus;
    case 8: return s_mode;
    case 9: return s_font;
    case 10: return s_iir;
   }
   return 0;
}

//
//	Format a setting's value into a 6-character string in the
//	destination buffer provided.
//

void 
set_format(BWAnal *aa, int set, char *dst) {
   char buf[32], *p;
   int ii= -1;
   double fp= -1;

   switch (set) {
    case 0: ii= s_oct0; break;
    case 1: ii= s_noct; break;
    case 2: ii= s_chan + 1; break;
    case 3: fp= s_gain; break;
    case 4: ii= s_vert; break;
    case 5: ii= s_tbase; break;
    case 6: fp= s_bri; break;
    case 7: fp= s_focus; break;
    case 8: ii= s_mode + 1; break;
    case 9: ii= s_font; break;
    case 10: ii= s_iir; break;
   }
   
   strcpy(buf, "*ERR*");
   if (ii >= 0) sprintf(buf, "%d", ii);
   if (fp >= 0) sprintf(buf, "%.6f", fp);
   buf[6]= 0;
   p= strchr(buf, 0);
   while (p < buf+6) *p++= ' ';
   strcpy(dst, buf);
}

//
//	Set a setting to a preset value from the config file
//

void 
set_preset(BWAnal *aa, int set, int pre) {
   double fp;

   fp= set_preset_values[set][pre];
   if (isnan(fp)) {
      status("\x8A No preset entry in config file for \"%c%c\" ", 
	     set_codes[set], '0'+pre);
      return;
   }

   set_put(aa, set, fp);
}

//
//	Increment or decrement a setting
//

void 
set_incdec(BWAnal *aa, int set, int dir) {
   double fp= set_get(aa, set);
   char *inc; 
   int ii;
   double n_steps;

   inc= set_inc[set];
   if (1 == sscanf(inc, "i%d", &ii))
      fp += ii * dir;
   else if (1 == sscanf(inc, "e%lf", &n_steps))
      fp *= pow(2, dir/n_steps);
   else if (1 == sscanf(inc, "ie%lf", &n_steps)) {
      int delta= floor(0.5 + pow(2, dir/n_steps));
      if (delta == 0) delta= dir;
      fp += delta;
   } else 
      error("Bad increment code in set_int[%d]", set);

   set_put(aa, set, fp);
}

//
//	Fix all the s_preset[] values (used only in startup)
//

void 
set_fix_s_preset(BWAnal *aa) {
   int set;
   for (set= 0; set<N_CODES; set++) fix_preset(aa, set);
}


// END //
