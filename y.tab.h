#define FILES 257
#define FILE_NAME 258
#define EDIT 259
#define SHOW 260
#define LOCK 261
#define GRID 262
#define LEVEL 263
#define WINDOW 264
#define ADD 265
#define SAVE 266
#define QUOTED 267
#define ALL 268
#define NUMBER 269
#define EXIT 270
#define OPTION 271
#define UNKNOWN 272
#define ARC 273
#define CIRC 274
#define INST 275
#define LINE 276
#define NOTE 277
#define OVAL 278
#define POLY 279
#define RECT 280
#define TEXT 281
typedef union {
	double num;
	PAIR pair;
	COORDS *pairs;
	OPTS *opts;
	char *name;
	} YYSTYPE;
extern YYSTYPE yylval;
