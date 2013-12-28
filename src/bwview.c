//
//	BWView -- recorded brain-wave data viewing application
//
//        Copyright (c) 2002 Jim Peters <http://uazu.net/>.  Released
//        under the GNU GPL version 2 as published by the Free
//        Software Foundation.  See the file COPYING for details, or
//        visit <http://www.gnu.org/copyleft/gpl.html>.
//

#include "all.h"

//
//	Globals
//

// Display-related
SDL_Surface *disp;	// Display
Uint32 *disp_pix32;	// Pixel data for 32-bit display, else 0
Uint16 *disp_pix16;	// Pixel data for 16-bit display, else 0
int disp_my;            // Display pitch, in pixels
int disp_sx, disp_sy;   // Display size

int disp_rl, disp_rs;	// Red component transformation
int disp_gl, disp_gs;	// Green component transformation
int disp_bl, disp_bs;	// Blue component transformation
int disp_am;		// Alpha mask

int disp_font;		// Current font size: 8 or 16 (pixels-high)
int *colour;		// Array of colours mapped to current display: see colour_data

// Display areas
int d_sig_xx, d_sig_yy;	// Signal display area
int d_sig_sx, d_sig_sy;
int d_tim_xx, d_tim_yy;	// Time-line display area
int d_tim_sx, d_tim_sy;
int d_mag_xx, d_mag_yy;	// Magnitude display area
int d_mag_sx, d_mag_sy;
int d_key_xx, d_key_yy;	// Key display area
int d_key_sx, d_key_sy;
int d_set_xx, d_set_yy;	// Settings display area
int d_set_sx, d_set_sy;

// Settings
int s_chan;		// Channel number to display
int s_tbase;		// Time base (samples per pixel)
int s_noct;		// Number of octaves to display
int s_oct0;		// Top octave (always >= 1)
double s_gain;		// Gain for signal display
double s_bri;		// Main display brightness
double s_focus;		// Focus: window-width in centre-frequency wavelengths
int s_vert;		// Vertical size of 'pixels' on main display
int s_mode;		// Display mode: 0 gray-scale, 1 with colours, 2 with peak lines too
int s_off;		// Current offset into file (in samples)
int s_font;		// Current font: 0 small, 1 big
int s_iir;		// IIR mode: 0, 1, 2
int c_set;		// Current setting (index in set_codes[]), or -1
int s_follow;		// Follow mode on? (1/0)


// Main loop
int rearrange;		// Set to request a complete rearrange/recalc/redraw of the screen
int restart;		// Set to request a restart of the calculations
int redraw;		// Set to request a redraw of the screen
int part_cmd= 0;	// Partial command status, or 0
int opt_x= 0;		// Option -x set to enable IIR modes
int follow_tmo;		// Time at which next 'follow-mode' update is required


// Hacks
double nan_global;	// Used on MSVC because 0.0/0.0 as a constant is not understood


//
//      Utility functions
//

void 
error(char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   fprintf(stderr, PROGNAME ": ");
   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n");
   exit(1);
}

void
errorSDL(char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   fprintf(stderr, PROGNAME ": ");
   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n  %s\n", SDL_GetError());
   exit(1);
}

#define NL "\n"

void
usage() {
   error("Recorded Brain-Wave Data Viewer, version " VERSION
	 NL "Copyright (c) 2002 Jim Peters, http://uazu.net, all rights reserved,"
	 NL "  released under the GNU GPL v2; see file COPYING"
	 NL "FFTW: Copyright (c) 1997-1999 Massachusetts Institute of Technology,"
	 NL "  released under the GNU GPL; see http://www.fftw.org/"
	 NL
	 NL "Usage: bwview [options] <file-format> <filename>"
	 NL "See output of option -f for a list of supported formats"
	 NL
	 NL "Options:"
	 NL "  -f            Display list of all supported file formats"
	 NL "  -c <cmds>     Execute the given key-commands on startup"
	 NL "  -F <mode>     Run full-screen with the given mode, <wid>x<hgt>x<bpp>"
	 NL "                <bpp> may be 16 or 32.  For example: 800x600x16"
	 NL "  -W <size>     Run as a window with the given size: <wid>x<hgt>"
	 NL "  -x            Enable 'x' key to select IIR testing modes"
	 );
}

void warn(char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n");
}

void *Alloc(int size) {
   void *vp= calloc(1, size);
   if (!vp) error("Out of memory");
   return vp;
}

void *StrDup(char *str) {
   char *cp= strdup(str);
   if (!cp) error("Out of memory");
   return cp;
}

//	void 
//	temp_hack() {
//	   int a, b;
//	   for (a= 0; a<d_mag_sx; a++) 
//	      for (b= 0; b<d_mag_sy; b++)
//		 plot_hue(d_mag_xx + a, d_mag_yy + b, 1, 
//			  b*1.0/d_mag_sy, a*0.4/d_mag_sx - 0.2);
//	}


//
//	Main routine
//

int 
main(int ac, char **av) {
   char *cmd= 0;
   char *p;
   char *fmt, *fnam;
   int sx= 640, sy= 480, bpp= 0;	// Default is 640x480 resizable window
   BWAnal *aa;
   SDL_Event ev;

   // Generate a NAN for MSVC
   nan_global= 0.0;
   nan_global /= 0.0;	
   
   // Process arguments
   ac--; av++;
   while (ac > 0 && av[0][0] == '-' && av[0][1]) {
      char ch, dmy, *p= *av++ + 1; 
      ac--;
      while (ch= *p++) switch (ch) {
       case 'f':
	  printf("Supported formats:\n\n");
	  bwfile_list_formats(stdout);
	  exit(0); break;
       case 'c':
	  if (ac < 1) usage();
	  cmd= *av++; ac--;
	  break;
       case 'F':
	  if (ac-- < 1) usage();
	  if (3 != sscanf(*av++, "%dx%dx%d %c", &sx, &sy, &bpp, &dmy) ||
	      (bpp != 16 && bpp != 32))
	     error("Bad mode-spec: %s", av[-1]);
	  break;
       case 'W':
	  if (ac-- < 1) usage();
	  if (2 != sscanf(*av++, "%dx%d %c", &sx, &sy, &dmy))
	     error("Bad window size: %s", av[-1]);
	  break;
       case 'x':
	  opt_x= 1; break;
       default:	
	  error("Unknown option '%c'", ch);
      }
   }

   // Read in config file and initialise settings globals
   config_load("bwview.cfg");
   set_init();
   
   // Load up any FFTW wisdom
   bwanal_load_wisdom("bwview.wis");

   // Open file  (the bwanal_new call will drop dead if there are any problems)
   if (ac != 2) usage();
   fmt= *av++;
   fnam= *av++;
   aa= bwanal_new(fmt, fnam);

   // Initialize SDL
   if (0 > SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE))            // 
      errorSDL("Couldn't initialize SDL");
   atexit(SDL_Quit);
   SDL_EnableUNICODE(1);		// So we get translated keypresses

   // Set key-repeat (alternatively, use SDL_DEFAULT_REPEAT_INTERVAL (==30))
   if (SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 40))
      errorSDL("Couldn't enable key repeat");

   // Initialise default settings
   s_chan= 0;
   s_tbase= 1;
   s_noct= 10;
   s_oct0= 1;
   s_gain= 1;
   s_bri= 1;
   s_focus= 4;
   s_vert= 1;
   s_mode= 3;
   s_off= 0;
   s_font= (sx > 700);
   s_iir= 0;
   c_set= 0;
   set_fix_s_preset(aa);

   // Initialise graphics
   graphics_init(sx, sy, bpp);
   disp_font= s_font ? 16 : 8;
   arrange_display();

   rearrange= 0;
   restart= 1;

   // Run through config-file startup commands
   if (p= config_get_str("init"))
      while (*p++) exec_key(aa, p[-1]);

   // Run through command-line startup commands
   if (cmd) while (*cmd++) exec_key(aa, cmd[-1]);

   // Main loop
   status("+\x8A Copyright (c) 2002 Jim Peters. All rights reserved. "
	  "Released under the GNU GPL v2; see file \"COPYING\". ");
   c_set= -1;

   while (1) {
      if (rearrange) {
	 disp_font= s_font ? 16 : 8;
	 arrange_display();
	 clear_rect(0, 0, sx, sy, colour[0]);
	 update(0, 0, sx, sy);
	 draw_status();
	 rearrange= 0;
	 restart= 1;
      }
      if (restart) {
	 clear_rect(d_mag_xx, d_mag_yy, d_mag_sx, d_mag_sy, colour[0]);
	 update(d_mag_xx, d_mag_yy, d_mag_sx, d_mag_sy);

	 aa->req.off= s_off;
	 aa->req.chan= s_chan;
	 aa->req.tbase= s_tbase;
	 aa->req.sx= d_mag_sx;
	 aa->req.sy= d_mag_sy / s_vert;
	 aa->req.freq0= aa->rate * pow(0.5, s_oct0);
	 aa->req.freq1= aa->rate * pow(0.5, s_oct0 + s_noct);
	 aa->req.wwrat= s_focus;
	 aa->req.typ= s_iir;
	 bwanal_start(aa);

	 draw_settings(aa);
	 draw_key(aa);
	 draw_signal(aa);
	 draw_timeline(aa);
	 
	 restart= redraw= 0;
      }
      
      if (redraw) {
	 int a;
	 draw_signal(aa);
	 for (a= 0; a < aa->yy; a += 16) 
	    draw_mag_lines(aa, a, (aa->yy-a) > 16 ? 16 : aa->yy-a);
	 redraw= 0;
      }

      // Follow-mode handling
      if (s_follow) {
	 int now= SDL_GetTicks();
	 if (now - follow_tmo >= 0) {
	    s_off= bwanal_length(aa) - d_mag_sx * s_tbase * 7 / 8; 
	    if (s_off < 0) s_off= 0;
	    restart= 1;
	    status("Following ... (Press shift-F to turn off)");
	    follow_tmo= now + 1000;
	    continue;
	 }
      }

      // Do a bit more analysis processing if required, or else wait
      // for an event
      if (aa->yy < aa->c.sy) {
	 int yy= aa->yy;
	 bwanal_calc(aa);
	 draw_mag_lines(aa, yy, aa->yy-yy);
      } else {
	 if (s_follow)
	    SDL_Delay(10);	// Wait 10ms if following
	 else if (!SDL_WaitEvent(0)) 
	    errorSDL("Unexpected error waiting for events");
      }
      
      // Process all outstanding events
      while (SDL_PollEvent(&ev)) {
	 int ch;
	 switch (ev.type) {
	  case SDL_KEYDOWN:
	     status("");	// Use keypress to clear temporary messages from status line
	     ch= ev.key.keysym.unicode;
	     if (ch && (isalnum(ch) || strchr("-+_=.", ch)))
		exec_key(aa, ch);
	     else switch (ev.key.keysym.sym) {
              case SDLK_ESCAPE:
		 exit(0);
	      case SDLK_BACKSPACE:
	      case SDLK_PAGEUP:
		 s_off -= d_mag_sx * s_tbase;
		 if (s_off < 0) s_off= 0;
		 restart= 1;
		 break;
	      case SDLK_SPACE:
	      case SDLK_PAGEDOWN:
		 bwanal_recheck_file(aa);
		 s_off += d_mag_sx * s_tbase;
		 restart= 1;
		 break;
	      case SDLK_LEFT:
		 s_off -= d_mag_sx * s_tbase / 2;
		 if (s_off < 0) s_off= 0;
		 restart= 1;
		 break;
	      case SDLK_RIGHT:
		 bwanal_recheck_file(aa);
		 s_off += d_mag_sx * s_tbase / 2;
		 restart= 1;
		 break;
	      case SDLK_HOME:
                 s_off= 0;
                 restart= 1;
                 break;
	      case SDLK_END:
                 s_off= bwanal_length(aa) - d_mag_sx * s_tbase * 7 / 8; 
		 if (s_off < 0) s_off= 0;
                 restart= 1;
                 break;
	      case SDLK_UP:
		 set_incdec(aa, 0, -1);
		 draw_settings(aa);
		 restart= 1;
		 break;
	      case SDLK_DOWN:
		 set_incdec(aa, 0, 1);
		 draw_settings(aa);
		 restart= 1;
		 break;
	      default:
		 continue;
	     }
	     break;
	  case SDL_MOUSEMOTION:
	     if (ev.motion.x >= d_mag_xx &&
		 ev.motion.x - d_mag_xx < d_mag_sx &&
		 ev.motion.y >= d_mag_yy &&
		 ev.motion.y - d_mag_yy < aa->yy*s_vert)
		show_mag_status(aa, ev.motion.x - d_mag_xx, ev.motion.y - d_mag_yy);
	     break;
	  case SDL_MOUSEBUTTONDOWN:
	     if (ev.motion.x >= d_mag_xx &&
		 ev.motion.x - d_mag_xx < d_mag_sx &&
		 ev.motion.y >= d_mag_yy &&
		 ev.motion.y - d_mag_yy < d_mag_sy) {
		bwanal_window(aa, ev.motion.x - d_mag_xx, (ev.motion.y - d_mag_yy) / s_vert);
		draw_signal(aa);
	     }
	     break;
	  case SDL_MOUSEBUTTONUP:
	     if (aa->sig_wind) {
		bwanal_signal(aa);
		draw_signal(aa);
	     }
	     break;
	  case SDL_VIDEORESIZE:
	     sx= ev.resize.w;
	     sy= ev.resize.h;
	     s_font= (sx > 700);
	     graphics_init(sx, sy, 0);
	     rearrange= 1;
	     break;
	  case SDL_QUIT:
	     exit(0);
	 }
      }

      // Loop ...
   }

   return 0;
}


//
//	Run key-commands made up of [a-zA-Z0-9] characters
//

void 
exec_key(BWAnal *aa, int key) {
   int ii= set_index(key);

   if (isalpha(key) && ii == -1) {
      switch (key) {
       case 'q':
       case 'Q':
	  exit(0);
       case 'F':
	  follow_tmo= SDL_GetTicks() + 1000;	// 1 sec from now
	  s_follow= !s_follow;
	  status("Follow mode %s", s_follow ? "ON" : "OFF");
	  if (s_follow) {
	     s_off= bwanal_length(aa) - d_mag_sx * s_tbase * 7 / 8; 
	     if (s_off < 0) s_off= 0;
	     restart= 1;
	  }
	  return;
       case 'O':
	  status("Optimising FFTs -- this may take a while ...");
	  bwanal_optimise(aa);
	  bwanal_save_wisdom("bwview.wis");
	  status("FFT optimisation complete");
	  return;
       default:
	  status("\x8A KEY NOT KNOWN \x80 -- check you have CAPS LOCK turned off");
	  c_set= -1; draw_settings(aa);		// Chance to show the logo again!
      }
      return;
   }

   if (ii >= 0) {
      c_set= ii;
      draw_settings(aa);
      status("+%s", set_names[ii]);
      return;
   }

   if (isdigit(key)) {
      if (c_set < 0) return;
      set_preset(aa, c_set, key - '0');
      draw_settings(aa);
      return;
   }

   switch (key) {
    case '+':
    case '=':
       set_incdec(aa, c_set, 1);
       draw_settings(aa);
       return;
    case '-':
    case '_':
       set_incdec(aa, c_set, -1);
       draw_settings(aa);
       return;
    case '.':
       bwanal_recheck_file(aa);
       restart= 1;
       return;
   }
}

//
//	Show details corresponding to the current mouse position, for example:
//
//	CURSOR: Time 432.22, Freq 54.432, Mag 0.0442, EstPkF 53.234, AccPkF 53.432, PkMag 0.2302
//

void 
show_mag_status(BWAnal *aa, int xx, int yy) {
   int oy= yy / s_vert;
   double tim= (aa->c.off + xx * aa->c.tbase) / aa->rate;
   double freq0= log(aa->c.freq0);
   double freq1= log(aa->c.freq1);
   double freq= exp(freq0 + (yy + 0.5) * (freq1-freq0) / (aa->c.sy * s_vert));
   double mag= aa->mag[xx + oy * aa->c.sx];
   double pkf, pkm;
   char buf[1024], *p= buf;
   int oy1, oy2, oyp;

   // Scan for nearest peak frequency
   for (oy1= oy; oy1 > 0; oy1--)
      if (aa->mag[xx + (oy1-1) * aa->c.sx] <=
	  aa->mag[xx + oy1 * aa->c.sx])
	 break;
   for (oy2= oy; oy2 <= aa->c.sy-2; oy2++)
      if (aa->mag[xx + (oy2+1) * aa->c.sx] <=
	  aa->mag[xx + oy2 * aa->c.sx])
	 break;
   oyp= ((oy==oy1) ? oy2 : 
	 (oy==oy2) ? oy1 : 
	 (oy-oy1 < oy2-oy) ? oy1 : oy2);
   pkf= exp(freq0 + (oyp + 0.5) * (freq1-freq0) / aa->c.sy);
   pkm= aa->mag[xx + oyp * aa->c.sx];

   p += sprintf(p, "\x8C" "CURSOR:\x80 Time \x8C");
   sprintf(p, "%.6f", tim); p += 6;
   p += sprintf(p, "\x80, Freq \x8C");
   sprintf(p, "%.6f", freq); p += 6;
   p += sprintf(p, "\x80, Mag \x8C");
   sprintf(p, "%.6f", mag); p += 6;
   p += sprintf(p, "\x80, PkF \x8C");
   sprintf(p, "%.6f", pkf); p += 6;
   p += sprintf(p, "\x80, PkM \x8C");
   sprintf(p, "%.6f", pkm); p += 6;
   *p= 0;

   // Old code to handle peak estimates from phase values
   //double est= aa->est[xx + oy * aa->c.sx];
   //double acc= 0;
   //double pmag= 0;
   //   p += sprintf(p, "\x80, EstPkF \x8C");
   //   sprintf(p, "%.6f", est); p += 6;
   //   p += sprintf(p, "\x80, AccPkF \x8C");
   //   sprintf(p, "%.6f", acc); p += 6;
   //   p += sprintf(p, "\x80, PkMag \x8C");
   //   sprintf(p, "%.6f", pmag); p += 6;

   status(buf);
}

// END //
