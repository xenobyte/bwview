//
//      Config file handling.  Very simplistic.
//
//        Copyright (c) 2002 Jim Peters <http://uazu.net/>.  Released
//        under the GNU GPL version 2 as published by the Free
//        Software Foundation.  See the file COPYING for details, or
//        visit <http://www.gnu.org/copyleft/gpl.html>.
//

//
//	Usage:
//
//	config_load(filename);
//
//	char *key;
//	double val;
//	val= config_get_fp(key);	// Value, or NAN
//
//	char *str;
//	str= config_get_str(key);	// Pointer, or 0
//

#ifdef HEADER

//
//      Config structures + prototypes
//
//        Copyright (c) 2002 Jim Peters.  Released under the GNU
//        GPL version 2.  See the file COPYING for details.
//

typedef struct ConfigKey ConfigKey;
struct ConfigKey {
   ConfigKey *nxt;	// Next in chain, or 0
   int key;
   double fp;		// Floating point value, or NAN if not available
   char *str;		// StrDup'd pointer, or 0 if floating point
};

#else

#include "all.h"

static ConfigKey *config;

//
//	Load up config file
//

void
config_load(char *fnam) {
   char buf[4096];
   FILE *in= fopen(fnam, "r");
   if (!in) error("Can't open config file: %s", fnam);

   while (fgets(buf, sizeof(buf), in)) {
      char *p= buf;
      int key= 0;
      char dmy;
      ConfigKey *cc;

      while (isspace(*p)) p++;
      if (*p == '#' || !*p) continue;
      
      if (!isalnum(*p) && *p != '_')
	 error("Bad line in config file:\n  %s", buf);
      
      while ((key & 0xFF000000) == 0 &&
	     (isalnum(*p) || *p == '_'))
	 key= (key<<8) | *p++;

      cc= ALLOC(ConfigKey);
      cc->key= key;
      cc->nxt= config;
      config= cc;
      
      if (!isspace(*p))
	 error("Bad line in config file:\n  %s", buf);
      
      while (isspace(*p)) p++;

      if (*p == '"') {
	 char *p0= ++p;
	 char *p1;
	 while (*p && *p != '"') p++;
	 if (!*p) error("Missing closing quote on string in config file:\n  %s", buf);
	 p1= p++;
	 while (isspace(*p)) p++;
	 if (*p) error("Rubbish after string in config file:\n  %s", buf);
	 *p1= 0;
	 
	 cc->fp= NAN;
	 cc->str= StrDup(p0);
	 continue;
      }
      
      if (1 != sscanf(p, "%lf %c", &cc->fp, &dmy))
	 error("Bad floating-point number in config file:\n  %s", buf);
      cc->str= 0;
   }
}

//
//	Get a floating-point value.  Returns NAN if the key is missing.
//
      
double 	
config_get_fp(char *key_str) {
   int key= 0;
   ConfigKey *cc;

   while (*key_str) key= (key<<8) | *key_str++;
   for (cc= config; cc; cc= cc->nxt)
      if (key == cc->key) 
	 return cc->fp;

   return NAN;
}

//
//	Get a string value.  Returns 0 if not found.
//
   
char *
config_get_str(char *key_str) {
   int key= 0;
   ConfigKey *cc;

   while (*key_str) key= (key<<8) | *key_str++;
   for (cc= config; cc; cc= cc->nxt)
      if (key == cc->key) 
	 return cc->str;

   return 0;
}

#endif
   
// END //
