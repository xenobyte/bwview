extern inline void sincos_init(double *buf, double freq) ;
extern inline void sincos_step(double *buf) ;
extern BWAnal * bwanal_new(char *fmt, char *fnam) ;
extern void bwanal_start(BWAnal *aa) ;
extern void bwanal_signal(BWAnal *aa) ;
extern void bwanal_window(BWAnal *aa, int xx, int yy) ;
extern int bwanal_calc(BWAnal *aa) ;
extern void bwanal_del(BWAnal *aa) ;
extern void bwanal_recheck_file(BWAnal *aa) ;
extern void bwanal_load_wisdom(char *fnam) ;
extern void bwanal_optimise(BWAnal *aa) ;
extern void bwanal_save_wisdom(char *fnam) ;
extern SDL_Surface *disp;
extern Uint32 *disp_pix32;
extern Uint16 *disp_pix16;
extern int disp_my;
extern int disp_sx, disp_sy;
extern int disp_rl, disp_rs;
extern int disp_gl, disp_gs;
extern int disp_bl, disp_bs;
extern int disp_am;
extern int disp_font;
extern int *colour;
extern int d_sig_xx, d_sig_yy;
extern int d_sig_sx, d_sig_sy;
extern int d_tim_xx, d_tim_yy;
extern int d_tim_sx, d_tim_sy;
extern int d_mag_xx, d_mag_yy;
extern int d_mag_sx, d_mag_sy;
extern int d_key_xx, d_key_yy;
extern int d_key_sx, d_key_sy;
extern int d_set_xx, d_set_yy;
extern int d_set_sx, d_set_sy;
extern int s_chan;
extern int s_tbase;
extern int s_noct;
extern int s_oct0;
extern double s_gain;
extern double s_bri;
extern double s_focus;
extern int s_vert;
extern int s_mode;
extern int s_off;
extern int s_font;
extern int s_iir;
extern int c_set;
extern int rearrange;
extern int restart;
extern int redraw;
extern int part_cmd;
extern int opt_x;
extern double nan_global;
extern void error(char *fmt, ...) ;
extern void errorSDL(char *fmt, ...) ;
extern void usage() ;
extern void warn(char *fmt, ...) ;
extern void *Alloc(int size) ;
extern void *StrDup(char *str) ;
extern int main(int ac, char **av) ;
extern void exec_key(BWAnal *aa, int key) ;
extern void show_mag_status(BWAnal *aa, int xx, int yy) ;
extern void config_load(char *fnam) ;
extern double config_get_fp(char *key_str) ;
extern char * config_get_str(char *key_str) ;
extern void arrange_display() ;
extern void draw_status() ;
extern void status(char *fmt, ...) ;
extern void draw_key(BWAnal *aa) ;
extern void draw_signal(BWAnal *aa) ;
extern void draw_timeline(BWAnal *aa) ;
extern void draw_mag_lines(BWAnal *aa, int lin, int cnt) ;
extern void draw_settings(BWAnal *aa) ;
extern BWFile * bwfile_open(char *fmt, char *fnam, int bsiz, int max_unref) ;
extern BWBlock * bwfile_get(BWFile *ff, int num) ;
extern void bwfile_free(BWFile *ff, BWBlock *bb) ;
extern void bwfile_close(BWFile *ff) ;
extern void bwfile_check_eof(BWFile *ff) ;
extern void bwfile_list_formats(FILE *out) ;
extern int colour_data[];
extern int suspend_update;
extern void graphics_init(int sx, int sy, int bpp) ;
extern int map_rgb(int col) ;
extern void update(int xx, int yy, int sx, int sy) ;
extern void mouse_pointer(int on) ;
extern void clear_rect(int xx, int yy, int sx, int sy, int val) ;
extern void vline(int xx, int yy, int sy, int val) ;
extern void drawtext(int siz, int xx, int yy, char *str) ;
extern int pure_hue_src[] [4];
extern Uint8 pure_hue_mem[] ;
extern Uint8 *pure_hue_data[] [3];
extern void init_pure_hues() ;
extern void plot_hue(int xx, int yy, int sy, double ii, double hh) ;
extern int cint_table[] ;
extern void init_cint_table() ;
extern void plot_cint(int xx, int yy, int sy, double ii) ;
extern void plot_cint_bar(int xx, int yy, int sx, int sy, int unit, double ii) ;
extern void plot_gray(int xx, int yy, int sy, double ii) ;
extern char *font6x8[];
extern char *font8x16[];
extern int main(int ac, char **av) ;
extern char *set_codes;
extern char *set_names[] ;
extern char *set_inc[] ;
extern char s_preset[] ;
extern double set_preset_values[] [10];
extern void set_init() ;
extern int set_index(int ch) ;
extern int set_put(BWAnal *aa, int set, double fp) ;
extern double set_get(BWAnal *aa, int set) ;
extern void set_format(BWAnal *aa, int set, char *dst) ;
extern void set_preset(BWAnal *aa, int set, int pre) ;
extern void set_incdec(BWAnal *aa, int set, int dir) ;
extern void set_fix_s_preset(BWAnal *aa) ;
