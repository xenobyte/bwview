//	(Tell emacs it's -*- C -*- mode)
//
//	File format specific code
//
//        Copyright (c) 2002 Jim Peters.  Released under the GNU
//        GPL version 2.  See the file COPYING for details.
//
//	To add a new format, you will need to write a setup routine
//	for your format which recognises whatever format-tag you
//	choose, and one or more read routines.  Add your setup routine
//	to the format_list[] array at the end of the file, with a
//	short description of the format for user help.  That's it.
//

//
//	Format-specific read routines.
//
//	  len= read_*(BWFile *ff, BWBlock *bb, FILE *in, float **chan,
//	              char *err, int max);
//
//	These routines should read a maximum of 'max' samples (usually
//	1024, i.e. 1024 on each channel) from stream 'in', which will
//	already have been correctly positioned in the file, and should
//	write the results into chan[] and err[].  Note that err[] will
//	have been zeroed before this call is made, so it only needs to
//	be modified if errors are detected.  Integer input data should
//	be scaled so that the maximum scale range fits in the range -1
//	to +1, centred on 0.
//
//	The number of samples read should be returned, and this will
//	be automatically written into bb->len.  The file position
//	after this call should be the position ready to read the next
//	sample (i.e. if you do read-ahead, you'll need to undo it)
//	because this position will be stored and used to read the next
//	block.
//

static int 
read_jm2(BWFile *ff, BWBlock *bb, FILE *in, float **chan, char *err, int max) {
   int len= 0;
   
   while (len < max) {
      int ch;
      while (1) {
	 ch= fgetc(in);
	 if (ch == 3) break;
	 if (ch == EOF) return len; 
	 err[len]= 1;		// Mark sync error
      }
      chan[0][len]= (fgetc(in) - 128) * (1.0 / 128.0);
      chan[1][len]= (fgetc(in) - 128) * (1.0 / 128.0);
      len++;
   }
   return len;
}

static int 
read_jm4(BWFile *ff, BWBlock *bb, FILE *in, float **chan, char *err, int max) {
   int len= 0;
   
   while (len < max) {
      int ch;
      while (1) {
	 ch= fgetc(in);
	 if (ch == 3) break;
	 if (ch == EOF) return len; 
	 err[len]= 1;		// Mark sync error
      }
      chan[0][len]= (fgetc(in) - 128) * (1.0 / 128.0);
      chan[1][len]= (fgetc(in) - 128) * (1.0 / 128.0);
      chan[2][len]= (fgetc(in) - 128) * (1.0 / 128.0);
      chan[3][len]= (fgetc(in) - 128) * (1.0 / 128.0);
      len++;
   }
   return len;
}

static int 
read_bm2e_1(BWFile *ff, BWBlock *bb, FILE *in, float **chan, char *err, int max) {
   int len= 0;
   
   while (len < max) {
      int ch= fgetc(in);
      if (ch == EOF) return len; 
      chan[0][len]= (ch - 128) * (1.0 / 128.0);
      len++;
   }
   return len;
}

static int 
read_bm2e_2(BWFile *ff, BWBlock *bb, FILE *in, float **chan, char *err, int max) {
   int len= 0;
   int expect= -1;

   // According to the docs, the top three bits of the sync byte go
   // 001->111 and repeat.  The low 5 bits are supposed to be 0.
   // However, in the example files that are all 1.  I can only assume
   // that they will be constant, and that is the basis of this code.

   while (len < max) {
      int ch= fgetc(in);
      if (ch == EOF) return len; 
      if (expect >= 0 && ch != expect) {
	 err[len]= 1;		// Sync error, drop a byte
	 ch= fgetc(in);		// Try the next byte as a valid sync byte
	 if (ch == EOF) return len; 
      }
      expect= ch + 32;
      if (expect >= 256) expect -= 224;
	    
      chan[0][len]= (fgetc(in) - 128) * (1.0 / 128.0);
      chan[1][len]= (fgetc(in) - 128) * (1.0 / 128.0);
      len++;
   }
   return len;
}

static int 
read_mod0(BWFile *ff, BWBlock *bb, FILE *in, float **chan, char *err, int max) {
   int len= 0;
   int count= -1;
   unsigned char buf[15];
   
   while (len < max) {
      int ch1, ch2;
      int cnt, a;

      // First scan forwards to the start-mark
      ch1= fgetc(in); cnt= 0;
      if (ch1 == EOF) return len;
      while (1) {
	 ch2= fgetc(in);
	 if (ch2 == EOF) return len;
	 if (ch1 == 0xa5 && ch2 == 0x5a) break;
	 err[len]= 1;		// Mark sync error
	 ch1= ch2;
	 if (++cnt > 100) error("No start mark found in 100 bytes of data stream");
      }

      // Read the rest of the packet
      if (1 != fread(buf, 15, 1, in)) return len;

      // Check the version number
      if (2 != buf[0]) printf("WARNING: version %d not supported", buf[0]);

      // Check the counter.  Note this only checks within the current
      // run of data -- it doesn't check continuity from the last
      // block read.  Also, we could perhaps insert big runs of zeros
      // for gaps in the count, but this does not seem worth it.
      if (count >= 0 && count != buf[1]) err[len]= 1;
      count= 255 & (buf[1] + 1);

      // Convert the data.  Bad data gives error marks
      for (a= 0; a<6; a++) {
	 int val= (buf[2*a+2]<<8) + buf[2*a+3];
	 if (val < 0 || val >= 1024) { err[len]= 1; val= 512; }
	 chan[a][len]= (val-512) * (1.0/512.0);
      }

      // Switch settings on last four channels
      for (a= 0; a<4; a++) 
	 chan[6+a][len]= (buf[14] & (8>>a)) ? 1.0 : -1.0;

      len++;
   }
   return len;
}

static int 
read_mod(BWFile *ff, BWBlock *bb, FILE *in, float **chan, char *err, int max) {
   int len= 0;
   int count= -1;
   unsigned char buf[64];
   int plen;
   int p_cnt;
   int p_aux;
   
   while (len < max) {
      int ch;
      int cnt, a;

      // Read a whole packet in
      plen= 0;
      while (1) {
	 ch= fgetc(in);
	 if (ch == EOF) return len;
	 buf[plen++]= ch;
	 if (ch & 128) break;
	 if (plen == 64) {		// Extra-long packet (bad data)
	    memmove(buf, buf+32, 32);
	    plen= 32;
	 }
      }

      // Bad packet
      if (plen != 5 && plen != 8 && plen != 11) {
	 err[len]= 1;
	 for (a= 0; a<10; a++) chan[a][len]= 0;
	 len++;
	 continue;
      }

      // Decode packet
      p_cnt= buf[0] >> 1;
      p_aux= buf[1] + ((buf[0] & 1) ? 128 : 0);

      // Check the counter.  Note this only checks within the current
      // run of data -- it doesn't check continuity from the last
      // block read.  
      if (count >= 0 && count != p_cnt) err[len]= 1;
      count= 63 & (p_cnt + 1);
      
      // Decode channel data
      for (a= 0; a<6; a++) {
	 int off= 2 + (a>>1) * 3;
	 int val= ((off >= plen) ? 512 : 
		   !(a&1) ?
		   buf[off] + ((buf[off+2] & 0x70) << 3) :
                   buf[off+1] + ((buf[off+2] & 0x7) << 7));
	 chan[a][len]= (val-512) * (1.0/512.0);
      }

      // Decode switch settings data (these just flash up once every 8 samples)
      for (a= 0; a<4; a++) {
	 int flag= ((p_cnt & 7) == 4 ? p_aux : 0) & (8>>a);
	 chan[6+a][len]= flag ? 1.0 : 0.0;
      }

      len++;
   }
   return len;
}

static int 
read_raw(BWFile *ff, BWBlock *bb, FILE *in, float **chan, char *err, int max) {
   int len= 0;
   char *p, *q;
   
   while (len < max) {
      int ch, a, v0;
      p= (char*)ff->read_data;
      for (a= 0; (ch= *p++); a++) {
	 float val;
	 switch (ch) {
	  case '_':	// Dummy byte
	     fgetc(in); a--; continue;
	  case 'b':	// Unsigned byte
	     val= (fgetc(in)-128) * (1.0 / 128); break;
	  case 'w':	// Unsigned 16-bit word, little-endian
	     v0= fgetc(in); v0 += fgetc(in) << 8;
	     val= (v0 - 32768) * (1.0 / 32768); break;
	  case 'W':	// Unsigned 16-bit word, big-endian
	     v0= fgetc(in) << 8; v0 += fgetc(in);
	     val= (v0 - 32768) * (1.0 / 32768); break;
	  case 'c':	// Signed char (8-bit)
	     val= ((fgetc(in)^128)-128) * (1.0 / 128); break;
	  case 's':	// Signed 16-bit word, little-endian
	     v0= fgetc(in); v0 += fgetc(in) << 8;
	     val= ((v0^32768) - 32768) * (1.0 / 32768); break;
	  case 'S':	// Signed 16-bit word, big-endian
	     v0= fgetc(in) << 8; v0 += fgetc(in);
	     val= ((v0^32768) - 32768) * (1.0 / 32768); break;
	  case 'f':	// Machine-format 32-bit float
	     q= (char*)&val; 
	     *q++= fgetc(in); *q++= fgetc(in);
	     *q++= fgetc(in); *q++= fgetc(in);
	     if (isnan(val)) val= 0;
	     break;
	  default:
	     error("Internal error -- unknown raw type '%c'", ch);
	 }
	 chan[a][len]= val;
      }

      len++;
   }
   return len;
}


//
//	Format-specific setup routines
//
//	  okay= setup_*(BWFile *ff, FILE *in, char *fmt, char *arg);
//
//	The format-string is split into two parts 'fmt' and 'arg'.
//	'arg' is the part after the '/', if there is one, or else "".
//	The setup routines are called in order until one of them
//	recognises the 'fmt' part.  If the routine does not recognise
//	'fmt', then it should return 0 immediately.  However, if it
//	knows the format, it should interpret the 'arg' argument if
//	required, and read enough of the file to get past any headers
//	that might be there.  The file-position after this call is
//	saved as the start-position of block 0.  The call should also
//	fill in the following fields, and then finally return 1.
//
//	  ff->read		Read callback routine
//	  ff->read_data		Extra saved info, if required (else leave as 0)
//	  ff->rate		Sample rate in Hz (may be fractional)
//	  ff->chan		Number of channels
//
//	If there are any errors in the format string or arguments
//	'arg', then the routine may call error() to abort the program
//	with an error message.
//

static int 
setup_jm(BWFile *ff, FILE *in, char *fmt, char *arg) {
   char dmy;

   if (0 == strcmp(fmt, "jm2")) {
      ff->read= read_jm2;
      ff->chan= 2;
   } else if (0 == strcmp(fmt, "jm4")) {
      ff->read= read_jm4;
      ff->chan= 4;
   } else return 0;

   if (1 != sscanf(arg, "%lf %c", &ff->rate, &dmy))
      error("Expecting sample rate in format-spec: %s/%s", fmt, arg);

   return 1;
}

static int 
setup_bm2e(BWFile *ff, FILE *in, char *fmt, char *arg) {
   char dmy;

   if (0 == strcmp(fmt, "bm1")) {
      ff->read= read_bm2e_1;
      ff->chan= 1;
   } else if (0 == strcmp(fmt, "bm2")) {
      ff->read= read_bm2e_2;
      ff->chan= 2;
   } else return 0;

   if (arg[0] == 0)		// 120Hz is the default rate for the brainmaster
      ff->rate= 120;
   else if (1 != sscanf(arg, "%lf %c", &ff->rate, &dmy))
      error("Bad sample rate in format-spec: %s/%s", fmt, arg);

   return 1;
}

static int 
setup_mod0(BWFile *ff, FILE *in, char *fmt, char *arg) {
   char dmy;

   if (0 == strcmp(fmt, "mod0")) {
      ff->read= read_mod0;
      ff->chan= 10;		// 6 real channels, and 4 switch settings
   } else return 0;

   if (1 != sscanf(arg, "%lf %c", &ff->rate, &dmy))
      error("Expecting sample rate in format-spec: %s/%s", fmt, arg);

   return 1;
}

static int 
setup_mod(BWFile *ff, FILE *in, char *fmt, char *arg) {
   char dmy;

   if (0 == strcmp(fmt, "mod")) {
      ff->read= read_mod;
      ff->chan= 10;		// 6 real channels, and 4 switch settings
   } else return 0;

   if (1 != sscanf(arg, "%lf %c", &ff->rate, &dmy))
      error("Expecting sample rate in format-spec: %s/%s", fmt, arg);

   return 1;
}

static int 
setup_raw(BWFile *ff, FILE *in, char *fmt, char *arg) {
   char *p, dmy, ch, *data;

   if (0 != strcmp(fmt, "raw"))
      return 0;
   
   if (!(p= strchr(arg, ':')))
      error("Expecting <rate>:<fmt> in format spec for 'raw': %s/%s", fmt, arg);
   *p++= 0;

   if (1 != sscanf(arg, "%lf %c", &ff->rate, &dmy))
      error("Expecting sample rate in format-spec: %s/%s:%s", fmt, arg, p);

   data= StrDup(p);
   ff->read_data= data;
   ff->read= read_raw;
   ff->chan= 0;

   while ((ch= *p++)) {
      if (!strchr("_bwWcsSf", ch))
	 error("Invalid character '%c' in raw format-spec: %s/%s:%s", 
	       ch, fmt, arg, data);
      if (ch != '_') ff->chan++;
   }

   return 1;
}
   

//
//	List of setup routines
//

#define NL "\n"
static FormatInfo format_list[]= {
   { setup_jm, 
     "jm2/<rate>         Jim-Meissner file, 2 channels" NL
     "jm4/<rate>         Jim-Meissner file, 4 channels" },
   { setup_bm2e,
     "bm1                BrainMaster 2E file, 120Hz, single channel" NL
     "bm2                BrainMaster 2E file, 120Hz, two channels" },
   { setup_mod0,
     "mod0/<rate>        Old-style modularEEG file, 6 EEG channels + 4 switch channels" },
   { setup_mod,
     "mod/<rate>         ModularEEG file, 6 EEG channels + 4 switch channels" },
   { setup_raw,
     "raw/<rate>:<fmt>   Raw file, with format given by <fmt> (see docs)" },
   { 0, 0 } 	// Marks end of list
};


// END //

