#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>		/* for strchr() */
#include <ctype.h>		/* for toupper */
#include <math.h>

#include <readline/readline.h> 	/* for command line editing */
#include <readline/history.h>  

#include "rlgetc.h"
#include "db.h"
#include "token.h"
#include "xwin.h" 	
#include "lex.h"
#include "eprintf.h"
#include "readfont.h"
#include "equate.h"
#include "path.h"

int readin();

/* The names of functions that actually do the manipulation. */

int com_add(), com_archive(), com_area(), com_background();
int com_bye(), com_change(), com_copy(), com_define();
int com_delete(), com_display(), com_distance(), com_dump(), com_edit();
int com_equate(), com_exit(), com_files(), com_grid();
int com_group(), com_help(),  com_identify();
int com_input(), com_interrupt(), com_layer(), com_level();
int com_list(), com_lock(), com_macro(), com_menu();
int com_move(), com_plot(), com_point(), com_process();
int com_purge(), com_retrieve(), com_save(), com_search();
int com_set(), com_shell(), com_show(), com_smash();
int com_split(), com_step(), com_stretch(), com_trace();
int com_undo(), com_units(), com_version(), com_window();
int com_wrap();

typedef struct {
    char *name;			/* User printable name of the function. */
    Function *func;		/* Function to call to do the job. */
    char *doc;			/* Documentation for this function.  */
} COMMAND;

COMMAND commands[] =
{
    {"ADD", com_add, "add a component to the current device"},
    {"ARCHIVE", com_archive, "create an archive file of the specified device"},
    {"AREA", com_area, "calculate and display the area of selected component"},
    {"BACKGROUND", com_background, "specified device for background overlay"},
    {"BYE", com_bye, "terminate edit session"},
    {"CHANGE", com_change, "change characteristics of selected components"},
    {"COPY", com_copy, "copy a component from one location to another"},
    {"DEFINE", com_define, "define a macro"},
    {"DELETE", com_delete, "delete a component from the current device"},
    {"DISTANCE", com_distance, "measure the distance between two points"},
    {"DISPLAY", com_display, "turn the display on or off"},
    {"DUMP", com_dump, "dump graphics window to file or printer"},
    {"EDIT", com_edit, "begin edit of an old or new device"},
    {"EQUATE", com_equate, "define characteristics of a mask layer"},
    {"EXIT", com_exit, "leave an EDIT, PROCESS, or SEARCH subsystem"},
    {"$FILES", com_files, "purge named files"},
    {"GRID", com_grid, "set grid spacing or turn grid on/off"},
    {"GROUP", com_group, "create a device from existing components"},
    {"HELP", com_help, "print syntax diagram for the specified command"},
    {"?", com_help, "Synonym for HELP"},
    {"IDENTIFY", com_identify, "identify named instances or components"},
    {"INPUT", com_input, "take command input from a file"},
    {"INTERRUPT", com_interrupt, "interrupt an ADD to issue another command"},
    {"LAYER", com_layer, "set a default layer number"},
    {"LEVEL", com_level, "set the logical level of the current device"},
    {"LIST", com_list, "list information about the current environment"},
    {"LOCK", com_lock, "set the default lock angle"},
    {"MACRO", com_macro, "enter the MACRO subsystem"},
    {"MENU", com_menu, "change or save the current menu"},
    {"MOVE", com_move, "move a component from one location to another"},
    {"PLOT", com_plot, "make a plot of the current device"},
    {"POINT", com_point, "display the specified point on the screen"},
    {"PROCESS", com_process, "enter the PROCESS subsystem"},
    {"PURGE", com_purge, "remove device from memory and disk"},
    {"QUIT", com_bye, "terminate edit session"},
    {"RETRIEVE", com_retrieve, "read commands from an ARCHIVE file"},
    {"SAVE", com_save, "save the current file or device to disk"},
    {"SEARCH", com_search, "modify the search path"},
    {"SET", com_set, "set environment variables"},
    {"SHELL", com_shell, "run a program from within the editor"},
    {"!", com_shell, "Synonym for `shell'"},
    {"SHOW", com_show, "define which kinds of things to display"},
    {"SMASH", com_smash, "replace an instance with its components"},
    {"SPLIT", com_split, "cut a component into two halves"},
    {"STEP", com_step, "copy a component in an array fashion"},
    {"STRETCH", com_stretch, "make a component larger or smaller"},
    {"TRACE", com_trace, "highlight named signals"},
    {"UNDO", com_undo, "undo the last command"},
    {"UNITS", com_units, "set editor resolution and user unit type"},
    {"VERSION", com_version, "identify the version number of program"},
    {"WINDOW", com_window, "change the current window parameters"},
    {"WRAP", com_wrap, "create a new device using existing components"},
    {(char *) NULL, (Function *) NULL, (char *) NULL}
};

static int def_layer=0;

#include <signal.h>
#define MAXSIGNAL 31	/* biggest signal under linux */
void sighandler(); 	/* catch signal */

int main(argc,argv)
int argc;
char **argv;
{
    LEXER *lp;		/* lexer struct for main cmd loop */
    int err=0;
    char buf[128];

    /* set program name for eprintf() error report package */
    setprogname(argv[0]);

    /* set up to catch all signal */
    /* for (i=1; i<=MAXSIGNAL; i++) { */

    err+=(signal(2, &sighandler) == SIG_ERR);		/* SIGINT */
    err+=(signal(3, &sighandler) == SIG_ERR);		/* SIGQUIT */
    err+=(signal(15, &sighandler) == SIG_ERR);		/* SIGTERM */
    err+=(signal(20, &sighandler) == SIG_ERR);		/* SIGSTP */
    if (err) {
    	printf("main() had difficulty setting sighandler\n");
	return(err);
    }

    initX();			/* create window, load MENUDATA */

    findfile(PATH, "NOTEDATA.F", buf);
    if (buf[0] == '\0') {
	printf("Could not file NOTEDATA.F\n");
	printf("PATH=\"%s\"\n", PATH);
	exit(5);
    } else {
	loadfont(buf,0);	/* load NOTE, TEXT definitions */
    }

    findfile(PATH, "TEXTDATA.F", buf);
    if (buf[0] == '\0') {
	printf("Could not file TEXTDATA.F\n");
	printf("PATH=\"%s\"\n", PATH);
	exit(5);
    } else {
	loadfont(buf,1);	/* load NOTE, TEXT definitions */
    }

    initialize_readline();

    initialize_equates();

    findfile(PATH, "PROCDATA.P", buf);
    if (buf[0] == '\0') {
	printf("Could not PROCDATA.P in $PATH\n");
	printf("PATH=\"%s\"\n", PATH);
	exit(5);
    } else {
	readin(buf,0);	/* load PROCESS FILE definitions */
    }

    findfile(PATH, "piglogo.d", buf);
    if (buf[0] == '\0') {
	printf("Could not piglogo in $PATH\n");
	printf("PATH=\"%s\"\n", PATH);
	exit(5);
    } else {
        currep = db_install("piglogo");           /* create blank stub */
	readin(buf,1);	
	currep->modified = 0;
	show_init(currep);
	currep = NULL;
    }

    rl_pending_input='\n';
    rl_setprompt("");

    lp = token_stream_open(stdin,"STDIN");
    parse(lp);
    return(1);
}

void parse(lp)
LEXER *lp;
{
    int debug=0;
    TOKEN token;
    char word[128];
    char buf[128];
    int retcode;
    COMMAND *command;
    COMMAND * find_command();

    while((token=token_get(lp, word)) != EOF) {
        if (debug) printf("%s, line %d: IN MAIN: got %s: %s\n", 
	    lp->name,  lp->line, tok2str(token), word);
	switch (lp->mode) {
	    case MAIN:
		rl_setprompt("MAIN> ");
		break;
	    case PRO:
		rl_setprompt("PROCESS> ");
		break;
	    case SEA:
		rl_setprompt("SEARCH> ");
		break;
	    case MAC:
		rl_setprompt("MACRO> ");
		break;
	    case EDI:
		if (currep != NULL) {
		    sprintf(buf, "EDIT %s> ", currep->name);
		    rl_setprompt(buf);
		} else {
		    rl_setprompt("EDIT> ");
		}
		break;
	    default:
		rl_setprompt("> ");
		break;
	}                           

	switch(token) {
	    case CMD:	/* find and call the command */
		command = find_command(word);
		if (command == NULL) {
		    printf(" bad command\n");
		    token_flush_EOL(lp);
		} else {
		    if (debug) printf("MAIN: found command\n");
		    retcode = ((*(command->func)) (lp, ""));
		}
		break;
	    case EOL:
	    case END:
	    case EOC:
		break;
	    default:
		printf("MAIN: expected COMMAND, got %s: %s\n",
			tok2str(token), word);
		token_flush_EOL(lp);
		break;
	}
    }
}

void sighandler(x)
int x;
{
    static int last=-1;

    printf("caught %d: %s. Use QUIT command to end program",x, strsignal(x));
    if (x == 3) {
       if (last==x) {
            exit(0);
       } else {
            printf(": do it again and I'll die!");
       }
    }
    last = x;
    printf("\n");
}

	
/* Look up NAME as the name of a command, and return a pointer to that
   command.  Return a NULL pointer if NAME isn't a command name. */

COMMAND * find_command(name)
char *name;
{
    register int i, size;
    int debug=0;

    size = strlen(name);
    size = size > 2 ? size : 3;
    
    for (i = 0; commands[i].name; i++)
	if (strncasecmp(name, commands[i].name, size) == 0) {
	    if (debug) {
	    	;
	    }
	    return (&commands[i]);
	}

    return ((COMMAND *) NULL);
}

/* returns 1 if name found, 0 if not */
int lookup_command(name)
char *name;
{
    register int i, size;
 
    size = strlen(name);
    size = size > 2 ? size : 3;
 
    for (i = 0; commands[i].name; i++)
        if (strncasecmp(name, commands[i].name, size) == 0)
            return (1);
 
    return ((TOKEN) NULL);
}                            


/* **************************************************************** */
/*                                                                  */
/*                    Built-in   Commands                           */
/*                                                                  */
/* **************************************************************** */

int is_comp(char c)
{

    switch(toupper(c)) {
    	case 'A':
	    return(ARC);
	    break;
    	case 'C':
	    return(CIRC);
	    break;
    	case 'L':
	    return(LINE);
	    break;
    	case 'N':
	    return(NOTE);
	    break;
    	case 'O':
	    return(OVAL);
	    break;
    	case 'P':
	    return(POLY);
	    break;
    	case 'R':
	    return(RECT);
	    break;
    	case 'T':
	    return(TEXT);
	    break;
	default:
	    return(0);
	    break;
    }
}

/* now in com_add.c ...
com_add(LEXER *lp, char *arg)		
*/


/* now in geom_arc.c...
int add_arc(LEXER *lp, int *layer)
*/

int add_oval(LEXER *lp, int *layer)
{
    rl_setprompt("ADD_OVAL> ");
    printf("in add_oval (unimplemented)\n");
    token_flush_EOL(lp);
    return(1);
}

int com_archive(LEXER *lp, char *arg)   /* create archive file of currep */
{
    need_redraw++; 

    /* check for :P option to write PROCDATA info */
    /* make sure and purge PROCDATA on read in */

    if (currep != NULL) {
	if (db_def_archive(currep)) {
	    printf("unable to archive %s\n", currep->name);
	    exit(-1);
	};
	printf("    archived %s\n", currep->name);
	currep->modified = 0;
    } else {
	printf("error: not currently editing a cell\n");
    }    	

    /*
	if(<name> != curr_name() && name != NULL) {
	    if (exists_on_disk(name)) {
		if_ask("do you want to overwrite?",name)
		    writedb(currdev, name);
		    mark_unmodified(name);
	    } else {
	    	writedb(currdev,name)
		mark_unmodified(name);
	    }
	} else {
	    if (currrname == NULL) {
		prompt for new name or break
	    }
	    writedb(currdev, currname);
	    mark_unmodified(name);
	}
    */

    return (0);
}


/* now in com_area.c */
/* com_area(LEXER *lp, char *arg) */	  /* display area of selected component */

int com_background(LEXER *lp, char *arg)	/* use device for background overlay */
{
    printf("    com_background %s\n", arg);
    return (0);
}


int com_bye(lp, arg)		/* terminate edit session */
LEXER *lp;
char *arg;
{
    /* The user wishes to quit using this program */
    /* Just set quit_now non-zero. */

    static int linenumber;	/* remember last request */

    if ( (lp->line != linenumber+1) && db_list_unsaved() ) {
	printf("    you have one or more unsaved instances!\n");
	printf("    typing either \"QUIT\" or \"BYE\" twice will force exit\n");
    } else {
	quit_now++;
	exit(0); 	/* for now just bail */
    }

    linenumber = lp->line;
    return (0);
}

/* now in com_change.c */
/* com_change(LEXER *lp, char *arg) */ /* change properties of selected components */

/* now in com_copy.c */
/* com_copy(LEXER *lp, char *arg) */	/* copy component  */

int com_define(lp, arg)		/* define a macro */
LEXER *lp;
char *arg;
{
    printf("    com_define\n");
    return (0);
}

/* now in com_delete.c */
/*com_delete(lp, arg) */ 	/* delete component from currep */

int com_display(lp, arg)	/* turn the display on or off */
LEXER *lp;
char *arg;
{
    TOKEN token;
    int done=0;
    char buf[128];
    char word[128];
    DISPLAYSTATE display_state = D_TOGGLE;	

    buf[0]='\0';
    while(!done && (token=token_get(lp, word)) != EOF) {
	switch(token) {
	    case IDENT: 	/* identifier */
		if (strncasecmp(word, "ON", 2) == 0) {
		    display_state=D_ON;
		} else if (strncasecmp(word, "OFF", 2) == 0) {
		    display_state=D_OFF;
		} else {
	    	     printf("bad argument to DISP: %s\n", word);
		}
		break;
	    case CMD:		/* command */
		token_unget(lp, token, word);
		break;
	    case EOC:		/* end of command */
		done++;
		break;
	    case EOL:		/* newline or carriage return */
	    	break;
	    case COMMA:		/* comma */
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    case NUMBER: 	/* number */
	    default:
		printf("DISP: expected ON/OFF, got %s\n", tok2str(token));
		return(-1);
		break;
	}
    }

    xwin_display_set_state(display_state);

    if (strcmp(lp->name, "STDIN") == 0) {
	switch (xwin_display_state()) {
	    case D_ON:
		printf("display now ON\n");
		break;
	    case D_OFF:
		printf("display now OFF\n");
		break;
	    default:
		printf("display state UNKNOWN\n");
		break;
	}
    }

    return (0);
}

/* now in com_distance.c */
/* com_distance(lp, arg) */ /* measure the distance between two points */


int com_dump(lp, arg)	/* dump graphics window to file or printer */
LEXER *lp;
char *arg;
{
    TOKEN token;
    int done=0;
    char word[128];
    int debug=0;
    char *s = NULL;
    int i;

    while(!done && (token=token_get(lp, word)) != EOF) {
	if (debug) printf("COM_DUMP: got %s: %s\n", tok2str(token), word);
	switch(token) {
	    case CMD:		/* command */
		token_unget(lp, token, word);
		done++;
		break;
	    case EOC:		/* end of command */
		done++;
		xwin_raise_window();
		need_redraw++;

		/* FIXME: horrible kludge, we have to wait until the display is properly */
		/* updated.  How do you know when everything has been properly sloshed */
		/* through the server?  This is simply an empirical hack that works */
		/* on my system */

		for (i=0; i<=20; i++) {
		    xwin_doXevent(s);
		}
		xwin_dump_graphics();
		xwin_doXevent(s);
		break;
	    case EOL:		/* newline or carriage return */
	    	break;	/* ignore */
	    case NUMBER: 	/* number */
	    case IDENT: 	/* identifier */
	    case COMMA:		/* comma */
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    default:
	        printf("DUMP: expected EOC, got: %s\n", tok2str(token));
		return(-1);
	    	break;
	}
    } 
    return (0);
}

/* now in com_edit.c */
/* com_edit(lp, arg) */		/* begin edit of an old or new device */


/* now in com_equate.c */
/* int com_equate(lp, arg) */   /* define characteristics of a mask layer */


int com_exit(lp, arg)		/* leave an EDIT, PROCESS, SEARCH subsystem */
LEXER *lp;
char *arg;
{
    int debug=0;
    int editlevel;
    static int linenumber;	/* remember last request so that */
    				/* two EXIts in a row will force exit */

    if (debug) printf("line#: %d\n", lp->line);

    if (lp->mode == EDI) {
	if (lp->line != linenumber+1 && currep != NULL && currep->modified) {
	    printf("    current device is unsaved!");
	    printf("    typing \"EXIT\" a second time will force exit\n");
	} else {
	    if (currep != NULL) {
		editlevel=currep->being_edited;
		currep->being_edited = 0;

		if ((currep = db_edit_pop(editlevel)) != NULL) {
		    ;
		} else {
		    lp->mode = MAIN;
		}

	    } else {
		/* can get here if someone purges the very cell */
		/* that they are editing and then does an EXIT */
	    }
	    need_redraw++;
	}
	linenumber=lp->line;
    } else if (lp->mode == PRO) {
        /* FIXME: need to check if PROCFILE is modified */
	lp->mode = MAIN;
    }
    return (0);
}

int com_files(lp, arg)		/* purge named files */
LEXER *lp;
char *arg;
{
    TOKEN token;
    int done=0;
    char buf[128];
    char word[128];
    int debug=0;

    if (debug) printf("in com_files\n");

    buf[0]='\0';
    while(!done && (token=token_get(lp, word)) != EOF) {
	switch(token) {
	    case IDENT: 	/* identifier */
	        db_purge(lp, word);
	    	break;      
	    case EOC:		/* end of command */
		done++;
		break;
	    case EOL:		/* newline or carriage return */
	    case COMMA:		/* comma */
	    	break;	/* ignore */
	    case NUMBER: 	/* number */
	    case CMD:		/* command */
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    default:
	        printf("FILES: expected IDENT, got: %s\n", tok2str(token));
		return(-1);
	    	break;
	}
    }
    return (0);
}


/* "GRID [ON | OFF] [:Ccolor] [spacing mult [xypnt]]" */
/* com_grid(STATE, color, spacing, mult, x, y) */

int com_grid(lp, arg)		/* change or redraw grid */
LEXER *lp;
char *arg;
{
    TOKEN token;
    int done=0;
    char buf[128];
    char word[128];
    int gridcolor=0;
    GRIDSTATE grid_state = G_TOGGLE;	
    double pts[6] = {0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    int npts=0;
    int nopts=0;
    int debug=0;
    int i;
    double UNITS=100.0;
    double tmp;

    buf[0]='\0';
    while(!done && (token=token_get(lp, word)) != EOF) {
	switch(token) {
	    case IDENT: 	/* identifier */
		if (strncasecmp(word, "ON", 2) == 0) {
		    grid_state=G_ON;
		    xwin_grid_state(grid_state);
		} else if (strncasecmp(word, "OFF", 2) == 0) {
		    grid_state=G_OFF;
		    xwin_grid_state(grid_state);
		} else {
	    	     printf("bad argument to GRID: %s\n", word);
		}
		break;
	    case QUOTE: 	/* quoted string */
		break;
	    case OPT:		/* option */
		if (strncasecmp(word, ":C", 2) == 0) { /* set color */
		    if(sscanf(word+2, "%d", &gridcolor) != 1) {
		         weprintf("invalid GRID argument: %s\n", word+2);
			return(-1);
		    }
		    nopts++;
		    if (debug) printf("setting color: %d\n", (gridcolor%8));
		    grid_state=G_ON;
		    xwin_grid_state(grid_state);
		    xwin_grid_color((gridcolor%8));
		} else {
	    	    weprintf("bad option to GRID: %s\n", word);
		    return(-1);
		}
		break;
	    case END:		/* end of file */
		break;
	    case CMD:		/* command */
		/* token_unget(lp, token, word); */
		break;
	    case NUMBER: 	/* number */
		if(sscanf(word, "%lf", &tmp) != 1) {
		    weprintf("invalid GRID argument: %s\n", word);
		    return(-1);
		}
		if (debug) printf("com_grid: point #%d: %g\n", npts, tmp);

		switch (npts) {
		    case 0:
		    case 1:
		    	if (tmp < 0) {
			    weprintf("GRID SPACING/MULT must be positive numbers\n");
			    return(-1);
			}
		    case 2:
		    case 3:
		    case 4:
		    case 5:
		    	break;
		    default:
			weprintf("too many numbers to GRID\n");
			return(-1);
		    	break;
		}

		pts[npts++] = tmp;
		break;
	    case EOL:		/* newline or carriage return */
	    case EOC:		/* end of command */
		done++;
		break;
	    case COMMA:		/* comma */
		/* ignore */
		break;
	    default:
		eprintf("bad case in com_grid");
		break;
	}
    }

    for (i=0; i<npts; i++) {
    	tmp = floor((pts[i]*UNITS)+0.5)/UNITS;
	if (pts[i] != tmp) {
	    printf("GRID: can't fit grid within UNITS, changing %d: %g to %g\n",i, pts[i], tmp);
	    pts[i] = tmp;
	}
    }

    if (npts == 2) {
	if (debug) printf("setting grid ON\n");
	xwin_grid_state(G_ON);
	xwin_grid_pts(pts[0], pts[0], pts[1], pts[1], pts[4], pts[5]);
    } else if (npts == 4) {
	if (debug) printf("setting grid ON\n");
	xwin_grid_state(G_ON);
	xwin_grid_pts(pts[0], pts[0], pts[1], pts[1], pts[2], pts[3]);
    } else if (npts == 6) {
	if (debug) printf("setting grid ON\n");
	xwin_grid_state(G_ON);
	xwin_grid_pts(pts[0], pts[1], pts[2], pts[3], pts[4], pts[5]);
    } else if (npts == 0) { 	/* no points */
    	if( nopts == 0) { 	/* and no options */
	    if (debug) printf("no args, toggling grid %d\n", grid_state);
	    xwin_grid_state(grid_state);
    	}
    } else {
	printf("bad number of arguments\n");
	return(-1);
    }

    if (currep != NULL && (strcmp(lp->name, "STDIN") == 0)) {
	printf("   grid ");
    	if (currep->grid_xd == currep->grid_yd) {
	    printf("delta=%.5g ", currep->grid_xd);
	} else {
	    printf("dx=%.5g dy=%.5g ", currep->grid_xd, currep->grid_yd);
	}
    	if (currep->grid_xs == currep->grid_ys) {
	    printf("skip=%.5g ", currep->grid_xs);
	} else {
	    printf("xskip=%.5g yskip=%.5g ", currep->grid_xs, currep->grid_ys);
	}

    	if (currep->grid_xo != 0.0 || currep->grid_yo != 0.0) {
	    printf("origin=%.5g,%.5g ", currep->grid_xo, currep->grid_yo);
	}
	printf("\n");
    }

    return (0);
}

int com_group(lp, arg)	/* create a device from existing components */
LEXER *lp;
char *arg;
{
    printf("    com_group %s\n", arg);
    return (0);
}

/* Print out help for ARG, or for all commands if ARG is not present. */

int com_help(lp, arg)
LEXER *lp;
char *arg;
{
    TOKEN token;
    int done=0;
    char word[128];
    int debug=0;
    register int i;
    int printed = 0;
    int size;

    while(!done && (token=token_get(lp, word)) != EOF) {
	switch(token) {
	    case IDENT: 	/* identifier */
	    case CMD:		/* command */
		if (debug) printf("HELP: got %s\n", word);
		size = strlen(word);
		size = size > 2 ? size : 3;
    
		for (i = 0; commands[i].name; i++) {
		    if (strncasecmp(word, commands[i].name, size) == 0) {
			printf("    %-12s: %s.\n", 
				commands[i].name, commands[i].doc);
			printed++;

			/* doesn't connect to stdin... */
			/*system("nroff -man web/man1p/grid.1p | less");*/
		    }
		} 
	    	break;
	    case EOC:		/* end of command */
		done++;
		break;
	    case NUMBER: 	/* number */
	    case EOL:		/* newline or carriage return */
	    case COMMA:		/* comma */
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    default:
		; /* eat em up! */
	    	break;
	}
    }
    if (!printed) {
	for (i = 0; commands[i].name; i++) {
	    /* Print in six columns. */
	    if (printed == 6) {
		printed = 0;
		printf("\n");
	    }
	    printf("%-12s", commands[i].name);
	    printed++;
	}
	if (printed)
	    printf("\n");
    }
    return (0);

}


/* now in com_ident.c */
/* com_identify(lp, arg) */  /*	identify named instances or components */

int readin(filename, editmode)		/* work routine for com_input */
char *filename;
int editmode;
{
    LEXER *my_lp;
    FILE *fp;

    char *save_rep;

    if((fp = fopen(filename, "r")) == 0) {
	return(0);
    } else {
	my_lp = token_stream_open(fp, filename);
	my_lp->mode = EDI;

	if (currep != NULL) {
	    save_rep=strsave(currep->name);
	} else {
	    save_rep=NULL;
	}

	if (editmode) {
	    show_set_modify(currep, ALL,0,1); 
	}

	printf ("reading %s from disk\n", filename);
	xwin_display_set_state(D_OFF);
	parse(my_lp);
	xwin_display_set_state(D_ON);
	token_stream_close(my_lp); 

	if (editmode) {
	    show_set_modify(currep, ALL,0,0);
	    show_set_visible(currep, ALL,0,1);
	}

	if (save_rep != NULL) {
	    currep=db_lookup(save_rep);
	    free(save_rep);
	} else {
	    currep=NULL;
	}

    }
    return(1);
}

int com_input(lp, arg)		/* take command input from a file */
LEXER *lp;
char *arg;
{
    TOKEN token;
    char buf[MAXFILENAME];
    char word[MAXFILENAME];
    int debug=0;
    int done=0;
    int nfiles=0;

    if (debug) printf("in com_input\n");

    rl_saveprompt();
    rl_setprompt("INP> ");

    buf[0]='\0';
    while(!done && (token=token_get(lp, word)) != EOF) {
	if (debug) printf("COM_INPUT: got %s: %s\n",
			tok2str(token), word);
	switch(token) {
	    case IDENT: 	/* identifier */
		if (nfiles == 0) {
		    snprintf(buf, MAXFILENAME, "%s", word);
		    nfiles++;
		    if (nfiles == 1) {
			if(readin(buf,0) == 0) {
			    printf("COM_INPUT: could not open %s\n", buf);
			    return(1);
			} 
		    } else {
			printf("INPUT: wrong number of args\n");
			return(-1);
		    }
		} 
		break;
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    case NUMBER: 	/* number */
	    case EOL:		/* newline or carriage return */
	    case COMMA:		/* comma */
	    	break;
	    case EOC:		/* end of command */
		done++;
		break;
	    case CMD:		/* command */
	    	token_unget(lp, token,word);
		done++;
		break;
	    default:
		eprintf("bad case in com_input");
		break;
	}
    }
    rl_restoreprompt();
    return(0);
}

int com_interrupt(lp, arg)	/* interrupt an ADD to issue another command */
LEXER *lp;
char *arg;
{
    printf("    com_interrupt\n");
    return (0);
}

int com_layer(lp, arg)		/* set a default layer number */
LEXER *lp;
char *arg;
{
    extern int def_layer;

    TOKEN token;
    int done=0;
    int layer;
    char buf[128];
    char word[128];
    int nnums=0;

    buf[0]='\0';
    while(!done && (token=token_get(lp, word)) != EOF) {
	switch(token) {
	    case NUMBER: 	/* number */
		if(sscanf(word, "%d", &layer) != 1) {
		    weprintf("LEVEL invalid layer number: %s\n", word);
		    return(-1);
		} 
		if (layer < 0 || layer > MAX_LAYER) {
		    printf("LEVEL: layer must be positive integer less than %d\n", MAX_LAYER);
		    return(-1);
		}
		nnums++;
		break;
	    case CMD:		/* command */
		token_unget(lp, token, word);
		done++;
		break;
	    case EOC:		/* end of command */
		done++;
		break;
	    case EOL:		/* newline or carriage return */
	    	break;	/* ignore */
	    case IDENT: 	/* identifier */
	    case COMMA:		/* comma */
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    default:
	        printf("LEVEL: expected NUMBER, got: %s\n", tok2str(token));
		return(-1);
	    	break;
	}
    }

    if (nnums==1) {
	 def_layer=layer;
    } else if (nnums==0) {
	 printf("LAYER: current default is %d\n", def_layer);
    } else {
	printf("LAYER: wrong number of arguments\n");
	return(-1);
    }

    return (0);
}

int com_level(lp, arg)	/* set the logical level of the current device */
LEXER *lp;
char *arg;
{
    TOKEN token;
    int done=0;
    int level;
    char buf[128];
    char word[128];
    int nnums=0;

    buf[0]='\0';
    while(!done && (token=token_get(lp, word)) != EOF) {
	switch(token) {
	    case NUMBER: 	/* number */
		if(sscanf(word, "%d", &level) != 1) {
		    weprintf("LEVEL invalid level number: %s\n", word);
		    return(-1);
		}
		nnums++;
		break;
	    case CMD:		/* command */
		token_unget(lp, token, word);
		done++;
		break;
	    case EOC:		/* end of command */
		done++;
		break;
	    case EOL:		/* newline or carriage return */
	    	break;	/* ignore */
	    case IDENT: 	/* identifier */
	    case COMMA:		/* comma */
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    default:
	        printf("LEVEL: expected NUMBER, got: %s\n", tok2str(token));
		return(-1);
	    	break;
	}
    }

    if (currep != NULL) {
	if (nnums==1) {
	     currep->logical_level = level;
	} else if (nnums==0) {
	     printf("LEVEL: %s is at level %d\n", 
	     	currep->name, currep->logical_level);
	} else {
	    printf("LEVEL: wrong number of arguments\n");
	    return(-1);
	}
    } else {
	printf("LEVEL: can't set level, not editing any cell\n");
	return(-1);
    }

    return (0);
}

int com_list(lp, arg)	/* list information about the current environment */
LEXER *lp;
char *arg;
{

    if (lp->mode == EDI || lp->mode == MAIN) { 
	if (currep != NULL) {
	    db_list(currep);
	} else { 
	    printf("no current rep to list!\n");
	}
    } else if (lp->mode == MAC) {
        /* mac_print(); */
    } else if (lp->mode == PRO) {
        equate_print();
    } else if (lp->mode == SEA) {
       /* search_path_print(); */
    }
    return (0);
}

int com_lock(lp, arg)		/* set the default lock angle */
LEXER *lp;
char *arg;
{
    TOKEN token;
    int done=0;
    double angle;
    char word[128];
    int nnums=0;

    while(!done && (token=token_get(lp, word)) != EOF) {
	switch(token) {
	    case NUMBER: 	/* number */
		if(sscanf(word, "%lf", &angle) != 1) {
		    weprintf("LOCK: invalid lock angle: %s\n", word);
		    return(-1);
		} else {
		   if (angle > 90.0 || angle < 0.0) {
		    weprintf("LOCK: invalid lock angle: %s\n", word);
		    return(-1);
		   }
		}
		angle = floor(angle*100.0 + 0.5)/100.0;
		nnums++;
		break;
	    case CMD:		/* command */
		token_unget(lp, token, word);
		done++;
		break;
	    case EOC:		/* end of command */
		done++;
		break;
	    case EOL:		/* newline or carriage return */
	    	break;	/* ignore */
	    case IDENT: 	/* identifier */
	    case COMMA:		/* comma */
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    default:
	        printf("LOCK: expected NUMBER, got: %s\n", tok2str(token));
		return(-1);
	    	break;
	}
    }
    if (nnums==1) {		/* set the lock angle of currep */
	if (currep != NULL) {
	    currep->lock_angle=angle;
	    /* currep->modified++; */
        } 
    } else if (nnums==0) {	/* print the current lock angle */
	if (currep != NULL) {
	    printf("LOCK ANGLE = %g\n", currep->lock_angle);
        } else {
	    printf("not editing anything...\n");
	}
    } else {
       ; /* FIXME: a syntax error */
    }
    return (0);
}

int com_macro(lp, arg)		/* enter the MACRO subsystem */
LEXER *lp;
char *arg;
{
    printf("    com_macro\n");
    if (lp->mode == MAIN) {
	lp->mode = MAC;
    } else {
	printf("    must EXIT before entering MACRO subsystem\n");
    }
    return (0);
}

int com_menu(lp, arg)		/* change or save the current menu */
LEXER *lp;
char *arg;
{
    printf("    com_menu\n");
    return (0);
}

/* now in com_move.c */
/* com_move(lp, arg) */	/* move a component from one location to another */

int com_plot(lp, arg)		/* make a plot of the current device */
LEXER *lp;
char *arg;
{
    printf("    com_plot\n");
    db_plot();
    return (0);
}

/* Now in com_point.c... */
/* com_point(lp, arg) */ /*	display the specified point on the screen */

int com_process(lp, arg)	/* enter the PROCESS subsystem */
LEXER *lp;
char *arg;
{
    printf("    com_process\n");
    if (lp->mode == MAIN) {
	lp->mode = PRO;
    } else {
	printf("    must EXIT before entering PROCESS subsystem\n");
    }
    return (0);
}

int com_purge(lp, arg)		/* remove device from memory and disk */
LEXER *lp;
char *arg;
{
    TOKEN token;
    int done=0;
    char word[128];

    rl_saveprompt();
    rl_setprompt("PURGE> ");

    while(!done && (token=token_get(lp, word)) != EOF) {
	switch(token) {
	    case IDENT: 	/* identifier */
	        db_purge(lp, word);
		done++;
	    	break;
	    case CMD:		/* command */
		token_unget(lp, token, word);
		done++;
		break;
	    case EOC:		/* end of command */
		done++;
		break;
	    case NUMBER: 	/* number */
	    case EOL:		/* newline or carriage return */
	    case COMMA:		/* comma */
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    default:
		; /* eat em up! */
	    	break;
	}
    }
    rl_restoreprompt();
    return (0);
}

int com_retrieve(lp, arg)	/* read commands from an ARCHIVE file */
LEXER *lp;
char *arg;
{
    TOKEN token;
    char buf[MAXFILENAME];
    char word[MAXFILENAME];
    int debug=0;
    int done=0;
    int nfiles=0;

    if (debug) printf("in com_retrieve\n");
    rl_saveprompt();
    rl_setprompt("RET> ");

    buf[0]='\0';
    while(!done && (token=token_get(lp, word)) != EOF) {
	if (debug) printf("COM_RETRIEVE: got %s: %s\n",
			tok2str(token), word);
	switch(token) {
	    case IDENT: 	/* identifier */
		if (nfiles == 0) {
		    snprintf(buf, MAXFILENAME, "./cells/%s_I", word);
		    nfiles++;
		    if (nfiles == 1) {
			if(readin(buf,0) == 0) {
			    printf("COM_RET: could not open %s\n", buf);
			    return(1);
			} 
			
		    } else {
			printf("RET: wrong number of args\n");
			return(-1);
		    }
		} 
		break;
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    case NUMBER: 	/* number */
	    case EOL:		/* newline or carriage return */
	    case COMMA:		/* comma */
	    	break;
	    case EOC:		/* end of command */
		done++;
		break;
	    case CMD:		/* command */
	    	token_unget(lp, token,word);
		done++;
		break;
	    default:
		eprintf("bad case in com_retrieve");
		break;
	}
    }
    rl_restoreprompt();
    return(0);
}

int com_save(lp, arg)	/* save the current file or device to disk */
LEXER *lp;
char *arg;
{
    need_redraw++; 

    TOKEN token;
    int done=0;
    char word[128];
    char name[128];
    int debug=0;
    int nfiles=0;
   
    if (debug) printf("    com_save <%s>\n", arg); 

    name[0]=0;
    while(!done && (token=token_get(lp, word)) != EOF) {
	switch(token) {
	    case IDENT: 	/* identifier */
		if (nfiles == 0) {
		    strncpy(name, word, 128);
		    nfiles++;
		} else {
		    printf("SAVE: wrong number of args\n");
		    return(-1);
		}
	    	break;
	    case QUOTE: 	/* quoted string */
	    case OPT:		/* option */
	    case END:		/* end of file */
	    case NUMBER: 	/* number */
	    case COMMA:		/* comma */
		printf("SAVE: expected IDENT: got %s\n", tok2str(token));
	    	break;
	    case EOL:		/* newline or carriage return */
	    	break;	/* ignore */
	    case EOC:		/* end of command */
		done++;
		break;
	    case CMD:		/* command */
		if (strncasecmp(word, "SAV", 3) == 0) {
		     /* a classic HP Piglet cleverness: */
		     /* in the event that a user accidently */
		     /* mouses "SAV SAV cell EXI; */ 
		     /* we inhibit parsing of second SAV */
		     /* to avoid losing changes to cell */
		     ;
		} else {
		    token_unget(lp, token, word);
		    done++;
		}
		break;
	    default:
		eprintf("bad case in com_save");
		break;
	}
    }

    if (currep != NULL) {
	if (nfiles == 0) {	/* save current cell */
	    if (currep->modified) {
		if (db_save(lp, currep, currep->name)) {
		    printf("unable to save %s\n", currep->name);
		} else {
		    printf("saved %s\n", currep->name);
		    currep->modified = 0;
		}
	    } else {
		/* printf("SAVE: cell not modified - no save done\n"); */
		/* I decided to let the save proceed, to store window, lock and grid */
		if (db_save(lp, currep, currep->name)) {
		    printf("unable to save %s\n", currep->name);
		} else {
		    printf("saved %s\n", currep->name);
		    currep->modified = 0;
		}
	    }
	} else {		/* save copy to a different name */
	    if (db_save(lp, currep, name)) {
		printf("unable to save %s\n", name);
	    } else {
		printf("saved %s as %s\n", currep->name, name);
		/* currep->modified = 0; */
	    }
	}

    } else {
	printf("error: not currently editing a cell\n");
    }    	
    return (0);
}

int com_search(lp, arg)		/* modify the search path */
LEXER *lp;
char *arg;
{
    printf("    com_search\n");

    if (lp->mode == MAIN) {
	lp->mode = SEA;
    } else {
	printf("    must EXIT before entering SEARCH subsystem\n");
    }
    return (0);
}

int com_set(lp, arg)		/* set environment variables */
LEXER *lp;
char *arg;
{
    printf("    com_set\n");
    return (0);
}

int com_shell(lp, arg)		/* run a program from within the editor */
LEXER *lp;
char *arg;
{
    printf("    com_shell\n");
    return (0);
}

/* com_show(lp, arg)		define which kinds of things to display */

int com_smash(lp, arg)	/* replace an instance with its components */
LEXER *lp;
char *arg;
{
    printf("    com_smash\n");
    return (0);
}

int com_split(lp, arg)		/* cut a component into two halves */
LEXER *lp;
char *arg;
{
    printf("    com_split\n");
    return (0);
}

int com_step(lp, arg)	/* copy a component in an array fashion */
LEXER *lp;
char *arg;
{
    printf("    com_step\n");
    return (0);
}

/* moved to com_stretch.c */
/* com_stretch(lp, arg) */	/* make a component larger or smaller */

int com_trace(lp, arg)		/* highlight named signals */
LEXER *lp;
char *arg;
{
    printf("    com_trace\n");
    return (0);
}

/* now in com_delete.c */
/* com_undo(lp, arg) */ 	/* undo the last command */

int com_units(lp, arg)	/* set editor resolution and user unit type */
LEXER *lp;
char *arg;
{
    printf("    com_units\n");
    return (0);
}

int com_version(lp, arg)	/* identify the version number of program */
LEXER *lp;
char *arg;
{
    printf("    com_version: %s\n", version);
    return (0);
}

/* com_window(lp, arg)	  (defined in com_window.c) */

int com_wrap(lp, arg)	/* create a new device using existing components*/
LEXER *lp;
char *arg;
{
    printf("    com_wrap\n");
    return (0);
}

int default_layer()
{
   return(def_layer);
}
