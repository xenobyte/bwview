//
//	This is the analysis unit
//
//        Copyright (c) 2002 Jim Peters <http://uazu.net/>.  Released
//        under the GNU GPL version 2 as published by the Free
//        Software Foundation.  See the file COPYING for details, or
//        visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	This is designed to do its work in small pieces so that it can
//	be called regularly from the main loop without interrupting
//	other things too much.  It is also designed so that the
//	parameters can be changed before calculations are complete,
//	and the calculations restarted.
//
//	This code takes care of getting the blocks loaded as required,
//	and processing them into large arrays of magnitudes and
//	frequency estimates.  At that point they can be displayed onto
//	the screen.
//

//
//	TYPICAL USAGE:
//
//	// Create new analysis object
//	BWAnal *aa;
//	aa= bwanal_new(format, filename);	// Arguments as for bwfile_open()
//
//	while (...) {
//	   // Specify parameters and start/restart calculations
//	   aa->req.(whatever)= (whatever);	
//	   ...
//	   bwanal_start(aa);
//	   // read now aa->sig[], aa->sig0[], aa->sig1[], aa->freq[]
//
//	   // Do calculations, bit by bit.  Possible to abort early if necessary.
//	   while (bwanal_calc(aa)) {
//	      // aa->yy shows how far we've got; read data out of: 
//	      // aa->mag[x+y*sx], aa->est[x+y*sx]
//	   }
//
//	   // Optionally, at any point, calculate an example window for display purposes
//	   bwanal_window(aa, freq, off);  // read now aa->sig[], aa->sig0[], aa->sig1[]
//	   // And restore original signal information to these arrays, if required
//	   bwanal_signal(aa);
//	}
//
//	// Optionally recheck the file before a new bwanal_start() to
//	// see if any further data has been written to it
//	bwanal_recheck_file(aa);
//
//	// Optionally find the total length of the file in samples (this implies 
//	// scanning to the end of the file if this has not been done already)
//	int len= bwanal_length(aa);
//
//	// Delete the analysis object when done (also shuts file)
//	bwanal_del(aa);
//
//
//	HANDLING FFT OPTIMISATION (known as 'wisdom' in FFTW):
//
//	// Load up saved 'wisdom' file if it exists
//	bwanal_load_wisdom(filename);
//
//	// Optimise all the FFTs we're currently using (this may take some time)
//	bwanal_optimise(aa);
//
//	// Save current 'wisdom' to a file
//	bwanal_save_wisdom(filename);
//

#ifdef HEADER

typedef struct BWAnal BWAnal;
typedef struct BWSetup BWSetup;

//
//	This describes the setup of the analysis engine.  It is used
//	in two ways -- one to show what the current setup is, and the
//	other to request a new setup.
//
//	Analysis types are as follows:
//	  0  Default Blackman window
//	  1  IIR biquad filter, Q=0.5 (i.e. no 0-overshoot in impulse response)
//	  2  IIR biquad filter, Q=0.72 (squarest freq response, but 0-overshoot in impulse reponse)
//
//	The other analysis types are only there to test the IIR
//	filterbanks, which are more likely to be used in real-time
//	situations.
//

struct BWSetup {
   int typ;		// Analysis type (see above)
   int off;		// Offset within input file (in samples counting from 0)
   int chan;		// Channel to display (counting from 0)
   int tbase;		// Time-base (i.e. samples per data point horizontally)
   int sx;		// Number of columns to calculate (size-X)
   int sy;		// Number of lines to calculate (size-Y)
   double freq0, freq1;	// Top+bottom frequencies (see note)
   double wwrat;	// Ratio of window width to the centre-frequency wavelength
};

// Note on top+bottom frequencies.  The number of lines 'sy' will be
// arranged as a number of equally-spaced (in log-freq-space) bands
// between freq0 and freq1.  This means that the top band will have a
// centre frequency just below freq0 and the bottom one will have a
// frequency just above freq1.  So, to get 6 bands in one octave from
// 128Hz to 256Hz, for example, set sy=6, freq0=128, freq1=256.

struct BWAnal {
   BWFile *file;
   BWBlock **blk;	// List of blocks loaded
   int n_blk;		// Number of blocks in list
   int bsiz;		// Block size
   int bnum;		// Number of block at front of list
   int half;		// Does this filter only require the left half of the data ? 0 no, 1 yes

   fftw_plan *plan;	// Big list of FFTW plans (see note below for ordering)
   int m_plan;		// Maximum plans (i.e. allocated size of plan[])

   int inp_siz;		// Size of data in inp[], or 0 if not valid
   fftw_real *inp;	// FFT'd input data (half-complex)
   fftw_real *wav;	// FFT'd wavelet (real)
   fftw_real *tmp;	// General workspace (complex), also used by IIR
   fftw_real *out;	// Output (complex)

   // Publically readable unchanging information
   int n_chan;		// Number of channels in input file
   double rate;		// Sample rate in input file

   // Publically readable changing information
   BWSetup c;		// Current setup
   float *sig;		// Signal mid-point values: sig[x], or NAN for sync errors
   float *sig0;		// Signal minimum values: sig0[x], or NAN for sync errors
   float *sig1;		// Signal maximum values: sig1[x], or NAN for sync errors
   float *mag;		// Magnitude information: mag[x+y*sx]
   float *est;		// Estimated nearby peak frequencies: est[x+y*sx], or NAN if can't calc
   float *freq;		// Centre-frequency of each line (Hz): freq[y]
   float *wwid;		// Logical width of window in samples: wwid[y]
   int *awwid;		// Actual width of window, taking account of IIR tail: awwid[y]
   int *fftp;		// FFT plan to use (index into ->plan[], fftp[y]%3==0)
   double *iir;		// IIR filter coefficients: iir[y*3], iir[y*3+1], iir[y*3+2]
   int yy;		// Number of lines currently correctly calculated in arrays
   int sig_wind;	// Are the ->sig arrays windowed ? 0 no, 1 yes
   
   // Publically writable information
   BWSetup req;		// Requested setup
};

// Storage of plans in aa->plan[]: For index 'a', a%3 gives the type
// of the plan: 0: real->complex, 1: complex->real, 2:
// complex->complex.  (a/3%2 ? 3 : 2) << (a/6) gives the size of the
// plan.  This means they go in the order (2,3,4,6,8,12,16,24,etc).

#define PLAN_SIZE(n) ((n)/3%2 ? 3 : 2) << ((n)/6)

#else

#include "all.h"

//
//	Make sure that we have all the data we need in the blk[] array
//

static void 
load_data(BWAnal *aa, int siz) {
   int len, off, off0, off1, blk0, blk1, n_blk, a;
   BWBlock **blk;

   len= aa->c.sx * aa->c.tbase;
   off= aa->c.off + len/2;
   if (aa->half) {
      off0= off - siz/2 - 1;
      off1= off + len/2 + 1;		// Don't need beyond right end of screen
   } else {
      off0= off - siz/2 - 1;
      off1= off + siz/2 + 1;
   }

   blk0= (off0 < 0) ? 0 : off0/aa->bsiz;
   blk1= (off1 + aa->bsiz - 1) / aa->bsiz;
   n_blk= blk1-blk0;
   blk= ALLOC_ARR(n_blk, BWBlock*);
   
   DEBUG("Loading offsets %d -> %d", off0, off1);

   // Delete any blocks in aa->blk[] we know we're not going to need
   // so that memory is freed before new blocks are allocated
   for (a= 0; a<aa->n_blk; a++) {
      int num= aa->bnum + a;
      if (num < blk0 || num >= blk1) {
	 if (aa->blk[a])
	    bwfile_free(aa->file, aa->blk[a]);
	 aa->blk[a]= 0;
      }
   }
   
   // Load up the blocks.  By loading up the new ones before freeing
   // the previous set, we use the caching and ref-counting of BWFile
   // to handle getting all this right without the hassle of doing it
   // ourselves.

   for (a= 0; a<n_blk; a++)
      blk[a]= bwfile_get(aa->file, blk0+a);
   
   // Release previous set
   for (a= 0; a<aa->n_blk; a++) 
      if (aa->blk[a])
	 bwfile_free(aa->file, aa->blk[a]);
   free(aa->blk);
   
   // Install new set
   aa->blk= blk;
   aa->n_blk= n_blk;
   aa->bnum= blk0;
}

//
//	Copy data from the BWFile input blocks into a straight-line
//	array.  Zeros are inserted for data before the beginning of
//	the file or after the end.  If the 'errors' flag is set, then
//	any sync errors result in NAN values in the output array.
//

static void 
copy_samples(BWAnal *aa, fftw_real *arr, int off, int chan, int len, int errors) {
   int bsiz= aa->bsiz;

   DEBUG("Copy samples: off %d, len %d, end %d", off, len, off+len);

   // Handle zeros before start of file
   while (off < 0 && len > 0) {
      *arr++= 0;
      len--; off++;
   }

   // Handle main part of file
   while (len > 0) {
      int num= off / bsiz;
      int boff= off - bsiz * num;
      BWBlock *bb;

      num -= aa->bnum;
      if (num < 0 || num >= aa->n_blk)
	 error("Internal error -- block not loaded: %d", num);

      // Copy as much as possible from the block
      if (bb= aa->blk[num]) {
	 int siz= bb->len;
	 while (len > 0 && boff < siz) {
	    *arr++= bb->chan[chan][boff];
	    if (errors && bb->err[boff]) arr[-1]= NAN;
	    len--; boff++; off++;
	 }
      }

      // Fill in the remainder of the block with zeros
      while (len > 0 && boff < bsiz) {
	 *arr++= 0.0;
	 len--; boff++; off++;
      }
   }
}

//
//	Recreate all the arrays within BWAnal
//
   
static void 
recreate_arrays(BWAnal *aa) {
   if (aa->sig) free(aa->sig);
   if (aa->sig0) free(aa->sig0);
   if (aa->sig1) free(aa->sig1);
   if (aa->mag) free(aa->mag);
   if (aa->est) free(aa->est);
   if (aa->freq) free(aa->freq);
   if (aa->wwid) free(aa->wwid);
   if (aa->awwid) free(aa->awwid);
   if (aa->fftp) free(aa->fftp);
   if (aa->iir) free(aa->iir);
   
   aa->sig= ALLOC_ARR(aa->c.sx, float);
   aa->sig0= ALLOC_ARR(aa->c.sx, float);
   aa->sig1= ALLOC_ARR(aa->c.sx, float);
   aa->mag= ALLOC_ARR(aa->c.sx * aa->c.sy, float);
   aa->est= ALLOC_ARR(aa->c.sx * aa->c.sy, float);
   aa->freq= ALLOC_ARR(aa->c.sy, float);
   aa->wwid= ALLOC_ARR(aa->c.sy, float);
   aa->awwid= ALLOC_ARR(aa->c.sy, int);
   aa->fftp= ALLOC_ARR(aa->c.sy, int);
   aa->iir= ALLOC_ARR(aa->c.sy*3, double);
}


//
//	Process one sample through an IIR filter
//

static inline double 
iir_step(double *buf, double *iir, double in) {
   double v0, v1, v2;
   
   v0= in * iir[0];
   v1= buf[0];
   v2= buf[1];
   v0 += v1 * iir[1];
   v0 += v2 * iir[2];
   buf[0]= v0;
   buf[1]= v1;

   return v0 + v1 + v1 + v2;
}


//
//	Setup a sincos generator (i.e. e^ibt for suitable b)
//

inline void 
sincos_init(double *buf, double freq) {
   buf[0]= 1;
   buf[1]= 0;
   buf[2]= cos(freq * 2 * M_PI);
   buf[3]= sin(freq * 2 * M_PI);
}


//
//	Generate the next values from a sincos generator
//

inline void 
sincos_step(double *buf) {
   double v0, v1;
   v0= buf[0] * buf[2] - buf[1] * buf[3];
   v1= buf[0] * buf[3] + buf[1] * buf[2];
   buf[0]= v0;
   buf[1]= v1;
}


//
//	Create a new analysis object for the given file 'fnam'.  The
//	file is loaded with format 'fmt' (see BWFile).
//

BWAnal *
bwanal_new(char *fmt, char *fnam) {
   BWAnal *aa= ALLOC(BWAnal);

   aa->bsiz= 1024;
   aa->file= bwfile_open(fmt, fnam, aa->bsiz, 0);
   aa->n_chan= aa->file->chan;
   aa->rate= aa->file->rate;
   
   // Put a few safe values in place just in case
   aa->req.tbase= 1;
   aa->req.sx= 1;
   aa->req.sy= 1;
   aa->req.freq0= aa->rate / 2;
   aa->req.freq1= aa->rate / 4;
   aa->req.wwrat= 1;
   
   return aa;
}

//
//	Start or restart calculations.  Picks up required setup from
//	aa->req.
//

void 
bwanal_start(BWAnal *aa) {
   BWSetup x, y;
   int maxsiz= 0;
   int analtyp;

   memcpy(&x, &aa->c, sizeof(BWSetup));
   memcpy(&y, &aa->req, sizeof(BWSetup));
   memcpy(&aa->c, &aa->req, sizeof(BWSetup));

   // Release FFT calculation arrays
   if (aa->inp) free(aa->inp), aa->inp= 0;
   if (aa->wav) free(aa->wav), aa->wav= 0;
   if (aa->tmp) free(aa->tmp), aa->tmp= 0;
   if (aa->out) free(aa->out), aa->out= 0;

   // Recreate result arrays if size has changed
   if (x.sx != y.sx || 
       x.sy != y.sy)
      recreate_arrays(aa);

   // Check analysis type
   analtyp= aa->c.typ;
   if (analtyp < 0 || analtyp > 2) 
      error("Bad analysis type value %d in bwanal_start", aa->c.typ);
   aa->half= analtyp != 0;
   
   // Work everything out from scratch (no clever partial calculations
   // for half-page scrolls or anything like that for now).

   // Fill in ->freq, ->wwid, ->awwid, ->fftp and ->iir arrays
   {
      int a;
      int sy= aa->c.sy;
      double log0= log(aa->c.freq0);
      double log1= log(aa->c.freq1);

      for (a= 0; a<sy; a++) {
	 int siz, c, b;

	 aa->freq[a]= exp(log0 + (a + 0.5)/sy * (log1-log0));
	 aa->wwid[a]= (aa->rate / aa->freq[a]) * aa->c.wwrat;
	 
	 if (analtyp == 0) {
	    siz= aa->c.sx * aa->c.tbase + 
	       (int)aa->wwid[a] + 2 + 10; 		// +2 for rounding, +10 for luck
	    if (siz < 0) error("Internal error in plan-size calculations");
	    
	    for (c= siz, b= -1; c; c>>=1) b++;	// 2<<b > siz
	    b *= 6;
	    if (PLAN_SIZE(b) < siz) 
	       error("Internal error -- plan size calculation failed: %d %d", siz, b);
	    while (PLAN_SIZE(b-1) > siz) b--;
	    aa->fftp[a]= b;
	    aa->awwid[a]= PLAN_SIZE(b);
	    
	    maxsiz= PLAN_SIZE(b);		// Rely on fact that biggest will be last
	 } else {
	    // Equate wwid[a] with the 95%-complete point of the impulse response
	    // (for 95%, use 0.7550 : 0.6522)
	    // (for 90%, use 0.6191 : 0.5046)
	    double freq= ((analtyp == 1) ? 0.7550 : 0.6522) / aa->wwid[a];
	    double omega= freq * 2 * M_PI;
	    double Q= (analtyp == 1) ? 0.50 : 0.72;
	    double alpha= sin(omega) / (2 * Q);
	    double aa0= 1 + alpha;
	    double a1= -2 * cos(omega) / -aa0;
	    double a2= (1 - alpha) / -aa0;
	    
	    aa->iir[a*3]= (1 - a1 - a2) / 4 * 2;	// Gain adjust, *2 to match Blackman
	    aa->iir[a*3+1]= a1;
	    aa->iir[a*3+2]= a2;

	    DEBUG("IIR %d: %g %g %g", a, aa->iir[a*3], aa->iir[a*3+1], aa->iir[a*3+2]);
	    
	    // Set awwid according to the 99.9%-complete point of the
	    // impulse response, doubled because we have ->half set,
	    // +1 for safety.
	    siz= (int)(1 + 2 * ((analtyp == 1) ? 1.4695 : 1.6647) / freq);
	    aa->awwid[a]= siz;

	    // Rely on fact that biggest will be last
	    maxsiz= siz + aa->c.sx * aa->c.tbase;  
	 }
      }
   }

   // Setup all the plans we're going to need
   if (analtyp == 0) {
      int a= aa->fftp[aa->c.sy-1] + 3;
      if (a > aa->m_plan) {
	 fftw_plan *tmp= ALLOC_ARR(a, fftw_plan);
	 if (aa->plan) {
	    memcpy(tmp, aa->plan, aa->m_plan*sizeof(fftw_plan));
	    free(aa->plan);
	 }
	 aa->plan= tmp;
	 aa->m_plan= a;
      }
      
      for (a= 0; a<aa->c.sy; a++) {
	 int b, c, ii= aa->fftp[a];
	 int siz= PLAN_SIZE(ii);
	 for (b= 0; b < 3; b++) if (!aa->plan[c= ii+b]) {
	    if (b == 2)
	       aa->plan[c]= fftw_create_plan(siz, FFTW_BACKWARD, 
					     FFTW_ESTIMATE | FFTW_USE_WISDOM);
	    else
	       aa->plan[c]= rfftw_create_plan(siz, b ? FFTW_COMPLEX_TO_REAL : 
					      FFTW_REAL_TO_COMPLEX, 
					      FFTW_ESTIMATE | FFTW_USE_WISDOM);
	    if (!aa->plan[c]) error("FFTW create_plan call failed unexpectedly");
	 }
      }
   }

   // Load up the data
   load_data(aa, maxsiz);

   // Fill in the ->sig arrays.  NAN is inserted for sync errors
   bwanal_signal(aa);

   // Allocate FFT arrays big enough for any line that we need to
   // calculate.  (They were released above)
   if (analtyp == 0) {
      aa->inp= ALLOC_ARR(maxsiz, fftw_real);
      aa->wav= ALLOC_ARR(maxsiz, fftw_real);
      aa->tmp= ALLOC_ARR(maxsiz*2, fftw_real);
      aa->out= ALLOC_ARR(maxsiz*2, fftw_real);
      aa->inp_siz= 0;
   } else {
      aa->tmp= ALLOC_ARR(maxsiz, fftw_real);
   }

   // Ready to start filling in lines
   aa->yy= 0;
}

//
//	Fill the aa->sig signal arrays with data, either from the
//	original untouched signal, or from one modified by the window
//	corresponding to the given point in the current analysis
//	setup.
//

void 
bwanal_signal(BWAnal *aa) {
   bwanal_window(aa, -1, -1);
}

void 
bwanal_window(BWAnal *aa, int xx, int yy) {
   int a, b, c;
   int sx= aa->c.sx;
   int tbase= aa->c.tbase;
   int len= sx * tbase;
   fftw_real *tmp= ALLOC_ARR(len, fftw_real);
   int wind= yy >= 0 && xx >= 0;	// Are we applying a window ?

   aa->sig_wind= wind;

   copy_samples(aa, tmp, aa->c.off, aa->c.chan, len, 1);

   // Apply window to tmp[] if required, and store window in ->sig[]
   if (wind) {
      if (aa->c.typ == 0) {
	 int off= xx * tbase + tbase/2;
	 double wwid= aa->wwid[yy] * 0.5;
	 int wid= floor(wwid);
	 for (a= 0; a<len; a++) {
	    int dist= abs(a-off);
	    if (dist <= wid) {
	       double ang= dist/wwid * (M_PI * 1.0);
	       double mag= 0.42 + 0.5 * cos(ang) + 0.08 * cos(2*ang);    // Blackman window
	       tmp[a] *= mag;
	    } else 
	       tmp[a]= 0;
	 }
	 for (a= 0; a<sx; a++) {
	    int dist= abs(a*tbase + tbase/2 - off);
	    if (dist <= wid) {
	       double ang= dist/wwid * (M_PI * 1.0);
	       double mag= 0.42 + 0.5 * cos(ang) + 0.08 * cos(2*ang);    // Blackman window
	       aa->sig[a]= mag;
	    } else 
	       aa->sig[a]= 0;
	 }
      } else {
	 double buf[2];		// IIR workspace
	 double max= 0;
	 int off= xx * tbase + tbase/2;
	 int awid= aa->awwid[yy] / 2;

	 for (a= 0; a<sx; a++) 
	    aa->sig[a]= 0;

	 buf[0]= buf[1]= 0;
	 for (a= len-1; a>=0; a--) {
	    if (a > off || a <= off-awid)
	       tmp[a]= 0;
	    else {
	       double amp= iir_step(buf, &aa->iir[yy*3], (a==off) ? 1.0 : 0);
	       tmp[a] *= amp;
	       amp= fabs(amp);
	       if (amp > max) max= amp;
	       if (amp > aa->sig[a/tbase])
		  aa->sig[a/tbase]= amp;
	    }
	 }
	 for (a= 0; a<len; a++) tmp[a] /= max;
	 for (a= 0; a<sx; a++) aa->sig[a] /= max;
      }
   }
   
   // Map samples to ->sig arrays
   for (a= b= 0; a<sx; a++) {
      double min= HUGE_VAL;
      double max= -HUGE_VAL;
      int nan= 0;
      if (!wind) aa->sig[a]= tmp[b + tbase/2];
      for (c= tbase; c>0; c--) {
	 float val= tmp[b++];
	 if (isnan(val)) nan= 1;
	 if (val < min) min= val;
	 if (val > max) max= val;
      }
      if (nan) {
	 aa->sig[a]= NAN;
	 aa->sig0[a]= NAN;
	 aa->sig1[a]= NAN;
      } else {
	 aa->sig0[a]= min;
	 aa->sig1[a]= max;
      }
   }
   
   free(tmp);
}


//
//	Do a small part of the calculations.  aa->yy always indicates
//	the number of lines processed so far.  Returns: 1 more to do,
//	0 all done.
//

int 
bwanal_calc(BWAnal *aa) {
   int yy, bas, pl, siz, siz2, a, b, c;
   double wwid, freq, dmy, adj, wadj;
   double freq_tb_pha;		// Phase difference (0..1) due to 'tbase' samples at 'freq'
   int wid, pwid;
   int start;
   int sx, tbase;
   fftw_real *p, *p0, *p1, *q, *r;
   float *fp;
   double sincos[4];

   yy= aa->yy;
   if (yy >= aa->c.sy) return 0;
   bas= yy * aa->c.sx;

   wwid= aa->wwid[yy] * 0.5;
   wid= floor(wwid);
   freq= aa->freq[yy] / aa->rate;
   sx= aa->c.sx;
   tbase= aa->c.tbase;

   // Handle IIR stuff separately as it doesn't need any FFTs
   if (aa->c.typ != 0) {
      double buf[4];		// IIR buffer
      double val, cc, ss;
      siz= aa->awwid[yy];
      siz2= siz/2;
      start= siz2;
  
      copy_samples(aa, aa->tmp, aa->c.off - start, 
		   aa->c.chan, start + sx*tbase, 0);
      
      memset(buf, 0, sizeof(buf));
      sincos_init(sincos, freq);
      for (a= 0, b= 0, c=start; b<sx; ) {
	 val= aa->tmp[a++];
	 cc= iir_step(&buf[0], &aa->iir[yy*3], val * sincos[0]);	//cos(ang));
	 ss= iir_step(&buf[2], &aa->iir[yy*3], val * sincos[1]);	//sin(ang));
	 //ang += freq * 2 * M_PI;
	 sincos_step(sincos);
	 
	 if (--c <= 0) {
	    aa->mag[bas+b]= hypot(cc, ss);
	    aa->est[bas+b]= 0;
	    b++;
	    c= tbase;
	 }
      }
      return ++aa->yy < aa->c.sy; 
   }

   // Remainder is FFT-based convolution stuff (typ == 0)
   pl= aa->fftp[yy];
   siz= PLAN_SIZE(pl);
   siz2= siz/2;

   // Setup input data if not done already
   if (siz != aa->inp_siz) {
      copy_samples(aa, aa->tmp, aa->c.off + (sx*tbase)/2 - siz2, 
		   aa->c.chan, siz, 0);
      rfftw_one(aa->plan[pl], aa->tmp, aa->inp);
      aa->inp_siz= siz;
   }

   // Calculate combined window and AM-carrier
   p= aa->tmp;
   for (a= 0; a<siz; a++) p[a]= 0.0;
   p[0]= 1.0;
   wadj= 1.0;
   for (a= 1; a<=wid; a++) {
      double ang= a/wwid * (M_PI * 1.0);
      double mag= 0.42 + 0.5 * cos(ang) + 0.08 * cos(2*ang);	// Blackman window
      double ang2= a * freq * (2 * M_PI);
      double re= mag * cos(ang2);
      double im= mag * sin(ang2);
      p[a]= re; p[siz-a-1]= im;
      wadj += 2 * mag;
   }

   // Transform it
   rfftw_one(aa->plan[pl+1], aa->tmp, aa->wav);

   // Do convolution by multiplying ->inp and ->wav
   q= aa->wav;
   r= aa->tmp;

   // Handle 0th element
   *r++= *q++ * aa->inp[0]; *r++= 0.0;

   // Handle 1..(siz/2-1)
   p0= aa->inp+1; p1= aa->inp + siz;
   for (a= 1; a<siz2; a++) {
      fftw_real mult= *q++;
      *r++= *p0++ * mult;
      *r++= *--p1 * mult;
   }

   // Handle (siz/2)th element
   *r++= *q++ * *p0; *r++= 0.0;

   // Handle (siz/2+1)..(siz-1)
   for (a= siz2+1; a<siz; a++) {
      fftw_real mult= *q++;
      *r++= *--p0 * mult;
      *r++= *p1++ * -mult;	// Complex conjugate
   }

   // Reverse FFT to get the output data
   fftw_one(aa->plan[pl+2], (fftw_complex*)aa->tmp, (fftw_complex*)aa->out);

   // Run through to pick up the output magnitudes and calculate phases
   start= siz2 - ((sx-1) * tbase)/2;
   p= aa->out + start*2;
   q= aa->tmp;
   adj= 2.0 / siz / wadj;		// Adjust for magnitudes of various things
   freq_tb_pha= modf(freq * tbase, &dmy);
   for (a= 0; a<sx; a++) {
      double mag= hypot(p[0], p[1]) * adj;
      double pha= atan2(p[0], p[1]) / (2 * M_PI) - a * freq_tb_pha;
      aa->mag[bas+a]= mag;
      pha= 1.0 + modf(pha-2.0, &dmy);
      *q++= pha;
      p += tbase*2;
   }

   // Work out the 'closest peak frequency' estimates
   pwid= 1;		// Preferred width @@@ use 1 for now, see how it comes out
   fp= aa->est + bas;
   for (a= 0; a<sx; a++) {
      double diff;
      if (a-pwid < 0 || a+pwid >= sx) {
	 *fp++= NAN;
	 continue;
      }
      diff= -aa->tmp[a-pwid] + aa->tmp[a+pwid];
      diff -= 2.5;		// Make sure it's -ve and offset by 0.5
      diff= 0.5 + modf(diff, &dmy);	// Now in range -0.5 to 0.5
      diff *= aa->rate / (pwid * 2 * tbase);
      *fp++= aa->freq[yy] + diff;
   }

   return ++aa->yy < aa->c.sy; 
}

//
//	Delete an analysis object
//

void 
bwanal_del(BWAnal *aa) {
   int a;

   bwfile_close(aa->file);
   if (aa->blk) free(aa->blk);

   for (a= 0; a<aa->m_plan; a++) {
      if (aa->plan[a]) {
	 if (a%3 == 2) fftw_destroy_plan(aa->plan[a]);
	 else rfftw_destroy_plan(aa->plan[a]);
      }
   }
   if (aa->plan) free(aa->plan);
   if (aa->inp) free(aa->inp);
   if (aa->wav) free(aa->wav);
   if (aa->tmp) free(aa->tmp);
   if (aa->out) free(aa->out);

   if (aa->sig) free(aa->sig);
   if (aa->sig0) free(aa->sig0);
   if (aa->sig1) free(aa->sig1);
   if (aa->mag) free(aa->mag);
   if (aa->est) free(aa->est);
   if (aa->freq) free(aa->freq);
   if (aa->wwid) free(aa->wwid);
   if (aa->awwid) free(aa->awwid);
   if (aa->fftp) free(aa->fftp);
   if (aa->iir) free(aa->iir);

   free(aa);
}

//
//	Recheck file to see if it has grown
//

void 
bwanal_recheck_file(BWAnal *aa) {
   int a;

   bwfile_check_eof(aa->file);
 
   // Make sure we're not caching the final block which the above call
   // has renumbered to -999
   for (a= 0; a<aa->n_blk; a++) {
      if (aa->blk[a] && aa->blk[a]->num < 0) {
	 bwfile_free(aa->file, aa->blk[a]);
	 aa->blk[a]= 0;
      }
   }
}

//
//	Find the size of the file in samples.  This means scanning all
//	the way to the end of the file if we have not already gone
//	that far.
//

int 
bwanal_length(BWAnal *aa) {
   BWFile *ff= aa->file;
   BWBlock *bb;

   bwanal_recheck_file(aa);
   
   while (!ff->eof) {
      bb= bwfile_get(ff, ff->n_blk);
      if (!bb) break;
      bwfile_free(ff, bb);
   }

   if (!ff->eof || ff->len < 0) 
      error("Internal error in bwanal_length()");

   return ff->len;
}


// 
//	Load up saved 'wisdom' file if it exists
//

void 
bwanal_load_wisdom(char *fnam) {
   FILE *in= fopen(fnam, "r");
   if (!in) return;

   if (FFTW_SUCCESS != fftw_import_wisdom_from_file(in))
      error("Bad wisdom file \"%s\".  Delete it and try again.");

   fclose(in);
}

//
//	Optimise all the FFTs we're currently using (may take some time)
//

void 
bwanal_optimise(BWAnal *aa) {
   int a;
   for (a= aa->m_plan-1; a>=0; a--) {
      int siz= PLAN_SIZE(a);
      int typ= a%3;
      
      if (!aa->plan[a]) continue;
      
      if (typ == 2) {
	 fftw_destroy_plan(aa->plan[a]);
	 aa->plan[a]= fftw_create_plan(siz, FFTW_BACKWARD, FFTW_MEASURE | FFTW_USE_WISDOM);
      } else {
	 rfftw_destroy_plan(aa->plan[a]);
	 aa->plan[a]= rfftw_create_plan(siz, typ ? FFTW_COMPLEX_TO_REAL : FFTW_REAL_TO_COMPLEX, 
					FFTW_MEASURE | FFTW_USE_WISDOM);
      }
      if (!aa->plan[a]) error("FFTW create_plan call failed unexpectedly");
   }
}
   
//
// 	Save current 'wisdom' to a file
//

void 
bwanal_save_wisdom(char *fnam) {
   FILE *out= fopen(fnam, "w");
   
   if (!out) error("Can't create wisdom file: %s", fnam);
   fftw_export_wisdom_to_file(out);
   if (fclose(out)) error("Error writing wisdom file: %s", fnam);
}

#endif

// END //

