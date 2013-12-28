//
//	File interface.
//
//        Copyright (c) 2002 Jim Peters <http://uazu.net/>.  Released
//        under the GNU GPL version 2 as published by the Free
//        Software Foundation.  See the file COPYING for details, or
//        visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	This allows accessing immensely long files without ever having
//	to bring them fully into memory.  It allows winding
//	forwards/backwards through the file, and it also allows error
//	information (e.g. loss of sync in the file-format) to be
//	indicated by the underlying format code so that this
//	information can be shown to the user.
//
//	(TODO: It also allows a new file to be created for writing.
//	Samples are always added to the end of the file, but the read
//	functions can scan backwards and forwards independent of the
//	writing functions.)
//
//	For reading, all data is converted to 32-bit floating point
//	numbers.  Integer input data is scaled so that the integer
//	range fits between -1 and +1.  Floating-point input data is
//	left untouched.
//
//	The file is read in blocks of 1024 samples (i.e. 1024 * n_chan
//	data points).  The last block may contain less than this
//	number of samples.
//

//
//	Typical usage:
//	-------------
//
//	// Open a file with block-size == 1024, max-unref == 20
//	BWFile *ff;
//	ff= bwfile_open(format, filename, 1024, 20);
//
//	ff->rate;		// Sample rate of file
//	ff->chan;		// Number of channels in the file
//	ff->len;		// Length of file in samples, or -1 if not reached yet
//
//	// Get a random block from a file
//	BWBlock *bb;
//	bb= bwfile_get(ff, block_number);	// Block numbers count from 0
//
//	bb->len;		// Number of samples in this block (normally 1024, but 
//				// less if this is the last block.
//	bb->chan[n][];		// Array of float data for channel 'n' (0..(ff->chan-1))
//	bb->err[];		// Array of error flags for the data: 0 no error, 1 sync error
//				// It is intended that these errors should be indicated on 
//				// the user display.
//
//	// Release a block no longer needed
//	bwfile_free(ff, bb);	// Don't access bb->??? after this point
//
//	// Check to see if more has been written to the file
//	bwfile_check_eof(ff);
//
//	// Close the file and release resources, including any blocks 
//	// not explicitly bwfile_free()'d
//	bwfile_close(ff)
//
//
//	// Print a list of supported formats to 'stderr'
//	bwfile_list_formats(stderr);

//
//	The format argument to bwfile_open selects the format-handling
//	code, and can also provide extra information that might not be
//	present in the format itself.  Formats defined so far are:
//
//	  jm2/<sample-rate>
//
//	    Two-channel Jim-Meissner files, 0x03 sync byte, plus 2
//	    unsigned bytes.
//
//	  jm4/<sample-rate>
//
//	    Four-channel Jim-Meissner files, 0x03 sync byte, plus 4
//	    unsigned bytes.
//

//
//	Implementation:
//
//	Blocks are cached to some extent.  A reference count is kept
//	of the number of times a block has been returned by the
//	bwfile_get() call.  When this goes to 0, the block is not
//	deleted right away, but only when the number of unused cached
//	blocks reaches the limit setup when the file was opened (20,
//	maybe).  This saves fetching the data from disk again.
//
//	Random access is implemented by storing the file-offsets of
//	the start of each block.  This means that to reach a
//	high-numbered block that has not already been read, it is
//	necessary to scan the entire file up to that point.  This is
//	the only solution I can think of that allows a huge variety of
//	file formats to be supported reliably and easily.
//

#ifdef HEADER

typedef struct BWFile BWFile;
typedef struct BWBlock BWBlock;
typedef struct FormatInfo FormatInfo;

struct BWFile {
   FILE *fp;		// File pointer for reading
   fpos_t *blk;		// Block offsets in file
   int m_blk;		// Max blocks in blk[] (i.e. size of array)
   int n_blk;		// Number of block-offsets stored in blk[].
   fpos_t pos;		// Position to find next block after n_blk
   int eof;		// Hit EOF yet ?

   int bsiz;		// Block size in samples
   int max_unref;	// Maximum number of unreferenced blocks in cache

   int (*read)(BWFile*,BWBlock*,FILE*,float**,char*,int);  // Format-specific read routine
   void *read_data;	// Special format-specific data, or 0.  Released with free()
   double rate;		// Sample rate of file
   int chan;		// Number of channels in the file
   int len;		// Length of file in samples, or -1 if end not reached yet
   
   BWBlock *cache;	// Cached block list, or 0
   int use;		// Use counter, used to find old blocks to delete
};

struct BWBlock {
   BWBlock *nxt;	// Next in list, or 0
   int num;		// Block number in file, counting from 0
   int ref;		// Reference count
   int last_used;	// Set from BWFile.use when ref goes to 0
   int len;		// Number of samples in this block
   float **chan;	// Array of float data for channels
   char *err;		// Array of error flags for the data: 0 no error, 1 sync error
};

struct FormatInfo {
   int (*setup)(BWFile *ff, FILE *in, char *fmt, char *arg);
   char *desc;
};

#else 

#include "all.h"

//
//	File format specific code is separate
//

#include "file_formats.inc"


//
//	Open a file
//
//	fmt	File format spec (see docs above for details)
//	fnam	File name
//	bsiz	Size to use for blocks, in samples, e.g. 1000 or 1024
//	max_unref  Maximum number of unreferenced blocks to keep in cache
//		    (e.g. 0 or 20)
//
//	Memory consumption for each block when it is brought into
//	memory is roughly 4 * channels * bsiz.  
//
//	Also note that a file-position has to be stored for each block
//	in the file (to enable random access), which is roughly 8
//	bytes per block.  For this reason, you don't want to make the
//	block size too small.
//

BWFile *
bwfile_open(char *fmt, char *fnam, int bsiz, int max_unref) {
   BWFile *ff= ALLOC(BWFile);
   int a;
   char *tmp, *arg;

   ff->bsiz= bsiz;
   ff->max_unref= max_unref;
   
   if (!(ff->fp= fopen(fnam, "rb")))
      error("File not found: %s", fnam);

   ff->m_blk= 256;
   ff->blk= ALLOC_ARR(ff->m_blk, fpos_t);
   ff->len= -1;

   tmp= StrDup(fmt);
   arg= strchr(tmp, '/');
   if (arg) *arg++= 0; else arg= "";
   
   for (a= 0; format_list[a].setup; a++) 
      if (format_list[a].setup(ff, ff->fp, tmp, arg))
	 break;
   
   free(tmp);

   if (!ff->read) 
      error("Format-specification not recognised: %s", fmt);
   if (ff->rate <= 0) 
      error("Bad sample rate from format or file: %g", ff->rate);
   if (ff->chan < 1 || ff->chan > 256)
      error("Bad number of channels from format or file: %d", ff->chan);

   if (0 != fgetpos(ff->fp, &ff->pos)) 
      error("Unexpected error getting file position: %s", strerror(errno));

   return ff;
}

//
//	Read a block of data from the file (ignores cache).
//
//	Returns 0 if the block does not exist (e.g. beyond end of
//	file).  Also handles scanning forwards through file if
//	necessary.  All of the data associated with a block is
//	allocated with it so that it can all be freed at once.
//

static BWBlock *
get_block(BWFile *ff, int num) {
   int a;
   FILE *fp= ff->fp;

   // Allocate BWBlock data all together in one chunk
   int len1= sizeof(BWBlock);
   int len2= len1 + ff->chan * sizeof(float*);
   int len3= len2 + ff->chan * ff->bsiz * sizeof(float);
   int len4= len3 + ff->bsiz * sizeof(char);
   char *cp= (char *)Alloc(len4);
   BWBlock *bb= (BWBlock *)cp;
   bb->chan= (float**)(cp + len1);
   cp += len2;
   for (a= 0; a<ff->chan; a++) {
      bb->chan[a]= (float *)cp;
      cp += ff->bsiz * sizeof(float);
   }
   bb->err= cp;
   bb->num= num;

   // Sanity check
   if (num < 0) { free(bb); return 0; }

   // Do a simple re-read if this has already been read once
   if (num < ff->n_blk) {
      if (0 != fsetpos(fp, &ff->blk[num]))
	 error("Unexpected error setting file position: %s", strerror(errno));
      bb->len= ff->read(ff, bb, fp, bb->chan, bb->err, ff->bsiz);

      // No need to save file-position, because it has already been done
      return bb;
   }

   // Reallocate the ff->blk array if it isn't big enough
   if (num + 2 > ff->m_blk) {
      fpos_t *blk;
      int siz= ff->m_blk * 2;
      while (siz < num+2) siz *= 2;
      
      blk= ALLOC_ARR(siz, fpos_t);
      memcpy(blk, ff->blk, ff->n_blk * sizeof(fpos_t));
      free(ff->blk);
      ff->blk= blk;
   }

   // Skip over as many blocks as necessary to find the file-position
   // for this block
   if (0 != fsetpos(fp, &ff->pos))
      error("Unexpected error setting file position: %s", strerror(errno));

   if (num > ff->n_blk && !ff->eof) {
      while (num > ff->n_blk) {
	 int len;
	 memcpy(&ff->blk[ff->n_blk++], &ff->pos, sizeof(fpos_t));
	 len= ff->read(ff, bb, fp, bb->chan, bb->err, ff->bsiz);
	 if (feof(fp)) { 
	    ff->eof= 1; 
	    ff->len= (ff->n_blk-1)*ff->bsiz + len; 
	    break;
	 }
	 if (0 != fgetpos(ff->fp, &ff->pos))
	    error("Unexpected error getting file position: %s", strerror(errno));
      }
   }

   // Off end of file
   if (ff->eof) { free(bb); return 0; }

   // Clear bb->err[], which we might have messed up in our scan above
   memset(bb->err, 0, ff->bsiz * sizeof(char));

   // Read the block in
   memcpy(&ff->blk[ff->n_blk++], &ff->pos, sizeof(fpos_t));
   bb->len= ff->read(ff, bb, fp, bb->chan, bb->err, ff->bsiz);
 
   if (feof(fp)) {
      ff->eof= 1;
      ff->len= (ff->n_blk-1)*ff->bsiz + bb->len;
   } else if (0 != fgetpos(ff->fp, &ff->pos))
      error("Unexpected error getting file position: %s", strerror(errno));

   return bb;
}

//
//	Get a block from the file (using cache if possible)
//

BWBlock *
bwfile_get(BWFile *ff, int num) {
   BWBlock *bb;

   // Scan to see if we have it in cache
   for (bb= ff->cache; bb; bb= bb->nxt) {
      if (bb->num == num) {
	 bb->ref++;
	 return bb;
      }
   }
   
   // Fetch it from disk, then
   bb= get_block(ff, num);
   if (!bb) return 0;

   // Save it in the cache
   bb->nxt= ff->cache;
   ff->cache= bb;

   bb->ref++;
   return bb;
}

//
//	Release a block no longer needed
//

void 
bwfile_free(BWFile *ff, BWBlock *bb) {
   bb->ref--;
   
   if (bb->ref == 0) {
      BWBlock **old_prvp= 0;
      BWBlock **prvp;
      int old_age= 0;
      int cnt= 0;

      bb->last_used= ff->use++;

      // Scan through all cached blocks to see how many unreferenced
      // ones we have, and to delete the oldest if there are too many.
      // It would be more efficient to keep a count of the number of
      // unreferenced blocks, but that would be more fragile to any
      // later changes to the code.  Scanning a short chain of
      // structures is not too big an overhead.

      for (prvp= &ff->cache; bb= *prvp; prvp= &bb->nxt) {
	 bb= *prvp;
	 if (bb->ref == 0) {
	    cnt++;
	    if (!old_prvp ||
		ff->use - bb->last_used > old_age) {
	       old_prvp= prvp;
	       old_age= ff->use - bb->last_used;
	    }
	 }
      }

      // Zap the oldest if we have too many
      if (cnt > ff->max_unref) {
	 bb= *old_prvp;
	 *old_prvp= bb->nxt;
	 free(bb);
      }
   }
}

//
//	Close a BWFile
//   

void 
bwfile_close(BWFile *ff) {
   // Delete all the cached blocks
   while (ff->cache) {
      void *vp= ff->cache;
      ff->cache= ff->cache->nxt;
      free(vp);
   }

   // Close the file
   fclose(ff->fp);
   
   // Release any other memory
   free(ff->blk);
   if (ff->read_data) free(ff->read_data);
   free(ff);
}

//
//	Check to see if more data has been written to the file since
//	we last looked.
//

void 
bwfile_check_eof(BWFile *ff) {
   BWBlock *bb;
   
   if (!ff->eof) return;

   ff->eof= 0;
   ff->len= -1;
   if (ff->n_blk == 0)
      memset(&ff->pos, 0, sizeof(fpos_t));
   else {
      ff->n_blk--;
      memcpy(&ff->pos, &ff->blk[ff->n_blk], sizeof(fpos_t));
   }

   // This means that the previous last block will now be re-read if
   // fetched.  There could still be an old version knocking about in
   // our cache, though, so renumber it to -999 so that it will not be
   // used.

   for (bb= ff->cache; bb; bb= bb->nxt) {
      if (bb->num >= ff->n_blk)
	 bb->num= -999;
   }
}

//
//	List the supported formats
//

void 
bwfile_list_formats(FILE *out) {
   int a;
   for (a= 0; format_list[a].desc; a++) {
      fprintf(out, "%s\n", format_list[a].desc);
   }
}

#endif

// END //

