//
//	Code to setup and draw display
//
//        Copyright (c) 2002 Jim Peters <http://uazu.net/>.  Released
//        under the GNU GPL version 2 as published by the Free
//        Software Foundation.  See the file COPYING for details, or
//        visit <http://www.gnu.org/copyleft/gpl.html>.
//

#include "all.h"

//
//	Arrange the display, based on current display size and font
//	size
//

void 
arrange_display() {
   int big= disp_font == 16;
   int sig_sy= -4 & ((disp_sy - 2 * disp_font) / 6);
   int set_req= ((strlen(set_codes) + 1) / 2 + 3) * disp_font;
   if (sig_sy < set_req) sig_sy= set_req;

   d_key_xx= 0;
   d_key_yy= sig_sy + disp_font;
   d_key_sx= 6 * (big ? 8 : 6);
   d_key_sy= disp_sy - disp_font - d_key_yy;
   
   d_sig_xx= d_key_sx;
   d_sig_yy= 0;
   d_sig_sx= disp_sx - d_sig_xx;
   d_sig_sy= sig_sy;

   d_set_xx= 0;
   d_set_yy= 0;
   d_set_sx= d_key_sx;
   d_set_sy= d_sig_sy + disp_font;

   d_tim_xx= d_sig_xx;
   d_tim_yy= d_sig_yy + d_sig_sy;
   d_tim_sx= d_sig_sx;
   d_tim_sy= disp_font;

   d_mag_xx= d_sig_xx;
   d_mag_yy= d_tim_yy + d_tim_sy;
   d_mag_sx= d_sig_sx;
   d_mag_sy= disp_sy - disp_font - d_mag_yy;

   if (d_mag_sy != d_key_sy) 
      error("Internal error -- mag region isn't same size as key region");
}

//
//	Draw status line (redraw it, really)
//

static char *status_str= 0;

void 
draw_status() {
   drawtext(disp_font, d_key_sx, disp_sy-disp_font, status_str);
   update(0, disp_sy-disp_font, disp_sx, disp_font);
}

//
//      Display a status line.  Colours may be selected using
//      characters from 128 onwards (see drawtext()).  There are two
//      types of status lines -- temporary ones and permanent ones.
//      Permanent ones have a '+' at the front of the formatted text
//      (although this is not displayed).  If the status is cleared
//      using status(""), then a permanent message will not go away.
//      However, it will go away if any other message is displayed,
//      including status("+").
//

void
status(char *fmt, ...) {
   va_list ap;
   char buf[4096], *p;
   static int c_perm= 0;
   int perm= 0;

   if (fmt[0] == '+') { fmt++; perm= 1; }
   if (!fmt[0] && c_perm && !perm) return;
   c_perm= perm;

   va_start(ap, fmt);
   buf[0]= 128;         // Select white on black
   vsprintf(buf+1, fmt, ap);
   p= strchr(buf, 0);
   *p++= 0x80;          // Restore white on black
   *p++= '\n';          // Blank to end of line
   *p= 0;

   if (status_str) free(status_str);
   status_str= StrDup(buf);
   draw_status();
}


//
//	Draw key area based on given analysis object
//

void 
draw_key(BWAnal *aa) {
   int xx= d_key_xx;
   int yy= d_key_yy;
   int sx= d_key_sx;
   int sy= d_key_sy;
   double freq0= log(aa->c.freq0);
   double freq1= log(aa->c.freq1);
   int o0, o1;
   int bsy, a;
   char buf[32];

   // Clear background in upper region
   clear_rect(xx, yy, sx, d_mag_yy - yy, colour[0]);
   
   // Draw band above beta
   o1= (int)((log(30) - freq0) / (freq1 - freq0) * sy);
   if (o1 > 0) clear_rect(xx, d_mag_yy, sx, o1, colour[0]);

   // Draw beta band
   o0= o1 > 0 ? o1 : 0;
   o1= (int)((log(13) - freq0) / (freq1 - freq0) * sy);
   if (o1 > sy) o1= sy;
   if (o1 > 0 && o0 < sy)
      clear_rect(xx, d_mag_yy + o0, sx, o1-o0, colour[5]);
   
   // Draw alpha band
   o0= o1 > 0 ? o1 : 0;
   o1= (int)((log(8) - freq0) / (freq1 - freq0) * sy);
   if (o1 > sy) o1= sy;
   if (o1 > 0 && o0 < sy)
      clear_rect(xx, d_mag_yy + o0, sx, o1-o0, colour[4]);
   
   // Draw theta band
   o0= o1 > 0 ? o1 : 0;
   o1= (int)((log(4) - freq0) / (freq1 - freq0) * sy);
   if (o1 > sy) o1= sy;
   if (o1 > 0 && o0 < sy)
      clear_rect(xx, d_mag_yy + o0, sx, o1-o0, colour[3]);
   
   // Draw delta band
   o0= o1 > 0 ? o1 : 0;
   o1= (int)((log(0.5) - freq0) / (freq1 - freq0) * sy);
   if (o1 > sy) o1= sy;
   if (o1 > 0 && o0 < sy)
      clear_rect(xx, d_mag_yy + o0, sx, o1-o0, colour[2]);
   
   // Draw band below delta
   o0= o1 > 0 ? o1 : 0;
   if (o0 < sy) clear_rect(xx, d_mag_yy + o0, sx, sy-o0, colour[0]);

   // Fill in frequency numbers
   bsy= sy / aa->c.sy;
   if (bsy >= disp_font) {
      // Bands are large enough to fit font-height in, so label each
      // band separately
      for (a= 0; a<aa->c.sy; a++) {
	 double freq= exp(freq0 + ((a + 0.5) / aa->c.sy) * (freq1-freq0));
         sprintf(buf, "%.8f", freq); buf[6]= 0;
	 drawtext(disp_font, xx, d_mag_yy + a * bsy + (bsy - disp_font)/2, buf);
      }
   } else {
      // Just run down the entire length, filling in as many frequency
      // values as we can
      for (a= 0; a+disp_font<=sy; a += disp_font) {
	 double freq= exp(freq0 + ((a + disp_font/2) / (double)sy) * (freq1-freq0));
         sprintf(buf, "%.8f", freq); buf[6]= 0;
	 drawtext(disp_font, xx, d_mag_yy + a, buf);
      }
   }

   // Update
   update(d_key_xx, d_key_yy, d_key_sx, d_key_sy);
}   


//
//	Draw signal area based on given analysis object
//

void 
draw_signal(BWAnal *aa) {
   int a;
   int xx= d_sig_xx;
   int yy= d_sig_yy;
   int sx= d_sig_sx;
   int sy= d_sig_sy;
   Uint32 *dp32= 0;
   Uint16 *dp16= 0;
   int bg= colour[0];
   int fg= colour[6];
   int hi= colour[7];
   int fill= colour[16];
   int err= colour[8];
   int p0, p1, p2, pz;

   if (disp_pix32) dp32= disp_pix32 + xx + yy * disp_my;
   if (disp_pix16) dp16= disp_pix16 + xx + yy * disp_my;

   for (a= 0; a<sx; a++, xx++) {
      vline(xx, yy, sy, bg);
      if (a >= aa->c.sx) continue;

      if (isnan(aa->sig[a])) {
	 vline(xx, yy, sy, err);
	 continue;
      }

      p0= (int)floor(sy/2 * (1.0 - aa->sig1[a] * s_gain));
      p1= (int)floor(sy/2 * (1.0 - aa->sig[a] * s_gain));
      p2= (int)floor(sy/2 * (1.0 - aa->sig0[a] * s_gain));
      pz= (int)floor(sy/2);

      if (aa->sig_wind) {
	 p1= (int)floor(sy/2 * (1.0 - aa->sig[a]));
	 vline(xx, yy + p1, (pz-p1) * 2, fill);
      } else {
	 if (p0 < pz && p2 < pz)
	    vline(xx, yy + p2, pz-p2, fill);
	 else if (p0 >= pz && p2 >= pz)
	    vline(xx, yy + pz, p0-pz, fill);
      }
      
      if (p0 < 0) p0= 0;
      if (p2 >= sy) p2= sy-1;
      vline(xx, yy + p0, p2-p0+1, fg);

      if (!aa->sig_wind && p1 >= 0 && p1 < sy)
	 vline(xx, yy + p1, 1, hi);
   }

   // Update
   update(d_sig_xx, d_sig_yy, d_sig_sx, d_sig_sy);
}
      
//
//	Draw time-line based on given analysis object
//

void 
draw_timeline(BWAnal *aa) {
   double off0= (aa->c.off) / aa->rate;
   double off1= (aa->c.off + aa->c.sx * aa->c.tbase) / aa->rate;
   double step= (off1 - off0)/5;
   int log10= (int)floor(log(step) / M_LN10);
   double step10= exp(M_LN10 * log10);
   double off;
   
   if (step >= step10 * 5) step= step10*5;
   else if (step >= step10 * 2) step= step10*2;
   else step= step10;

   modf((off0 + step * 0.999) / step, &off);
   off *= step;

   clear_rect(d_tim_xx, d_tim_yy, d_tim_sx, d_tim_sy, colour[0]);

   while (off < off1) {
      int xx= d_tim_xx + (int)((off-off0) / (off1-off0) * d_tim_sx);
      char buf[32];
      
      vline(xx, d_tim_yy + d_tim_sy/2, d_tim_sy/2, colour[9]);

      if (log10 < 0)
	 sprintf(buf, "\x8E%.*f", -log10, off);
      else 
	 sprintf(buf, "\x8E%g", off);

      drawtext(disp_font, xx + 3, d_tim_yy, buf);
      off += step;
   }

   // Update
   update(d_tim_xx, d_tim_yy, d_tim_sx, d_tim_sy);
}

//
//	Draw a number of lines within the 'mag' region based on the
//	given analysis object
//

void 
draw_mag_lines(BWAnal *aa, int lin, int cnt) {
   int a, b;
   int end= lin + cnt;
   int x0= d_mag_xx;
   int xx, yy;
   int sx= aa->c.sx;
   int mx, my;		// Max x,y

   yy= d_mag_yy + lin * s_vert;
   mx= d_mag_xx + d_mag_sx;
   my= d_mag_yy + d_mag_sy;

   switch (s_mode) {
    case 0:	// Gray-scale magnitudes
       {
	  for (a= lin; a<end; a++, yy+=s_vert) {
	     xx= x0;
	     for (b= 0; b<sx; b++, xx++) {
		double val= s_bri * aa->mag[b + a * sx];
		plot_gray(xx, yy, s_vert, val);
	     }
	  }
       } 
       break;
    case 1:	// Sideways waterfall display based on magnitude, stepping in 8s, sloping left
       {
	  int cnt, unit= d_mag_sx / 10;
	  double mval, val;
	  for (a= lin; a<end; a++, yy+=s_vert) {
	     xx= x0;
	     mval= 0; cnt= 0;
	     for (b= 0; b<sx; b++, xx++) {
		val= s_bri * aa->mag[b + a * sx];
		if (val > mval) mval= val;	// Look for maximum value in each 8
		cnt++;
		if ((b & 7) != 7) continue;
		clear_rect(xx, yy, cnt, s_vert, colour[0]);
		plot_cint_bar(xx-4, yy, d_mag_xx-xx+3, s_vert, unit, mval);
		mval= 0; cnt= 0;
	     }
	  }
       } 
       break;
    case 2:	// Sideways waterfall display based on magnitude, sloping left
       {
	  int unit= d_mag_sx / 10;
	  for (a= lin; a<end; a++, yy+=s_vert) {
	     xx= x0;
	     for (b= 0; b<sx; b++, xx++) {
		double val= s_bri * aa->mag[b + a * sx];
		plot_cint_bar(xx, yy, d_mag_xx-xx-1, s_vert, unit, val);
	     }
	  }
       } 
       break;
    case 3:	// Colour-intensity scale to show magnitude
       {
	  for (a= lin; a<end; a++, yy+=s_vert) {
	     xx= x0;
	     for (b= 0; b<sx; b++, xx++) {
		double val= s_bri * aa->mag[b + a * sx];
		plot_cint(xx, yy, s_vert, val);
	     }
	  }
       } 
       break;
    case 4:	// Sideways waterfall display based on magnitude, sloping right
       {
	  int unit= d_mag_sx / 10;
	  for (a= lin; a<end; a++, yy+=s_vert) {
	     xx= x0 + sx - 1;
	     for (b= sx-1; b>=0; b--, xx--) {
		double val= s_bri * aa->mag[b + a * sx];
		plot_cint_bar(xx, yy, mx-xx, s_vert, unit, val);
	     }
	  }
       } 
       break;
    case 5:	// Sideways waterfall display based on magnitude, stepping in 8s, sloping right
       {
	  int cnt, unit= d_mag_sx / 10;
	  double mval, val;
	  for (a= lin; a<end; a++, yy+=s_vert) {
	     xx= x0 + sx - 1;
	     mval= 0; cnt= 0;
	     for (b= sx-1; b>=0; b--, xx--) {
		val= s_bri * aa->mag[b + a * sx];
		if (val > mval) mval= val;	// Look for maximum value in each 8
		cnt++;
		if ((b & 7) != 0) continue;
		clear_rect(xx, yy, cnt, s_vert, colour[0]);
		plot_cint_bar(xx+4, yy, mx-xx-4, s_vert, unit, mval);
		mval= 0; cnt= 0;
	     }
	  }
       } 
       break;
    case 6:	// Magnitude and hue to show out of phase-ness
       {
	  for (a= lin; a<end; a++, yy+=s_vert) {
	     xx= x0;
	     for (b= 0; b<sx; b++, xx++) {
		double val= s_bri * aa->mag[b + a * sx];
		double est= aa->est[b + a * sx];
		
		if (isnan(est))
		   plot_gray(xx, yy, s_vert, val);
		else {
		   double freq= aa->freq[a];
		   double diff= (est-freq)/freq * 2.0;
		   if (diff < 0) diff= -diff;
		   if (diff > 1.0)
		      plot_gray(xx, yy, s_vert, val);
		   else 
		      plot_hue(xx, yy, s_vert, val, 0.5 - diff);	// 0.5 is Cyan
		}
	     }
	  }
       } 
       break;
    case 7:	// Grey-scale showing magnitude, adjusted to hide out-of-phase areas
       {
	  for (a= lin; a<end; a++, yy+=s_vert) {
	     xx= x0;
	     for (b= 0; b<sx; b++, xx++) {
		double val= s_bri * aa->mag[b + a * sx];
		double est= aa->est[b + a * sx];
		
		if (isnan(est))
		   val= 0;
		else {
		   double freq= aa->freq[a];
		   double diff= (est-freq)/freq;
		   double adj= 1.5 - 3 * (diff < 0 ? -diff : diff);
		   val *= (adj <= 0) ? 0 : adj;
		}
		plot_gray(xx, yy, s_vert, val);
	     }
	  }
       }      
       break;
   }

   // Update
   update(d_mag_xx, d_mag_yy + lin * s_vert, d_mag_sx, cnt * s_vert);
}

//
//	Draw settings area
//

void 
draw_settings(BWAnal *aa) {
   int xx= d_set_xx;
   int yy= d_set_yy;
   int sx= d_set_sx;
   int sy= d_set_sy;
   int fsx= s_font ? 8 : 6;
   int cyy;
   int set;
   char ch;
   char buf[10];

   clear_rect(xx, yy, sx, sy, colour[0]);
   
   // Display logo or current setting value
   if (c_set < 0) {
      int sy2= 2 * disp_font;
      int x, y;
      for (y= 0; y<sy2; y++)
	 for (x= 0; x<sx; x++) {
	    double fx= (0.5 + x - sx/2) / sy2;
	    double fy= (0.5 + y - sy2/2) / sy2;
	    double mag= hypot(fx, fy);
	    double pha= atan2(fx, fy) / 2 / M_PI;
	    plot_hue(xx+x, yy+y, 1, 0.8 - 0.5 / (1 + mag*1), pha - mag * 0.8 + 0.088);
	 }
      drawtext(disp_font, xx, yy + disp_font/2, "\x94" "BWView");
   } else {
      int pre;
      int mox= sx - (s_font ? 8 : 6) * 2;	// Maximum offset-X

      buf[0]= '\x98';
      buf[1]= set_codes[c_set];
      buf[2]= pre= s_preset[c_set];
      buf[3]= 0;

      if (!isdigit(pre)) {
	 double fp= set_get(aa, c_set);
	 for (pre= 1; pre < 10; pre++) 
	    if (fp >= set_preset_values[c_set][pre] &&
		fp <= set_preset_values[c_set][(pre+1)%10])
	       break;
      } else if (pre == '0') 
	 pre= 10;
      else
	 pre= (pre-'0');

      drawtext(disp_font, xx + mox * (pre-1) / 9, yy, buf);
      
      buf[0]= '\x8C';
      set_format(aa, c_set, buf+1);
      drawtext(disp_font, xx, yy + disp_font, buf);
   }
      
   // Draw main display of settings
   for (set= 0; ch= set_codes[set]; set++) {
      cyy= yy + (set/2 + 3) * disp_font;
      if (set == c_set)
	 drawtext(disp_font, xx + (set&1)*fsx*3, cyy, "\x91   ");
      buf[0]= ch;
      buf[1]= s_preset[set];
      buf[2]= 0;
      drawtext(disp_font, xx + (set&1)*fsx*3 + fsx/2, cyy, buf);
   }

   update(xx, yy, sx, sy);
}

// END //

