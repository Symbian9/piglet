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

com_edit(lp, arg)		/* begin edit of an old or new device */
LEXER *lp;
char *arg;
{
    TOKEN token;
    int done=0;
    char word[128];
    char name[128];
    char buf[128];
    int error=0;
    int debug=0;
    int nfiles=0;
    FILE *fp;
    LEXER *my_lp;
    DB_TAB *save_rep;  
    DB_TAB *new_rep;  
    DB_TAB *old_rep = NULL;  
    double x1, y1;
    DB_DEFLIST *p_best;
    double xmin, ymin, xmax, ymax;

    enum {START,NUM1,COM1,NUM2,NUM3,COM2,NUM4,END} state = START;

/* an edi parse loop that will allow picking of instances for EIP */

/* current thinking is to add flag bits in the db_deflist so that the picking */
/* is managed in the rendering routine in-situ.  Setting and clearing the flags */
/* may also be able to manage EIP'ing in and out in a stack-like fashion */
/* need to make EDI <coord> do a pick on instances only to select EIP rep */

    if (debug) printf("    com_edit <%s>\n", arg); 

    name[0]=0;

    rl_saveprompt();
    rl_setprompt("EDIT> ");

    while(!done) {
	token = token_look(lp,word);
	if (debug) printf("got %s: %s\n", tok2str(token), word);
	if (token==CMD) {
	    state=END;
	} 
	switch(state) {	
	case START:		/* get cellname or xy pick point */
	    if (debug) printf("in START\n");
	    if (token == OPT ) {
		token_get(lp,word); /* ignore for now */
		state = START;
	    } else if (token == NUMBER) {
		state = NUM1;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just eat it up */
		state = START;
	    } else if (token == EOC || token == CMD) {
		state = END;
	    } else if (token == IDENT) {
		if (nfiles == 0) {
		    strncpy(name, word, 128);
		    nfiles++;
		} else {
		    printf("EDIT: wrong number of args\n");
		    return(-1);
		}
		state = END;
	    } else {
		token_err("EDIT", lp, "expected DESC or NUMBER", token);
		state = END;	/* error */
	    }
	    break;
	case NUM1:		/* get pair of xy coordinates */
	    if (debug) printf("in NUM1\n");
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &x1);	/* scan it in */
		state = COM1;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = END;	
	    } else {
		token_err("EDIT", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case COM1:		
	    if (debug) printf("in COM1\n");
	    if (token == EOL) {
		token_get(lp,word); /* just ignore it */
	    } else if (token == COMMA) {
		token_get(lp,word);
		state = NUM2;
	    } else {
		token_err("EDIT", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM2:
	    if (debug) printf("in NUM2\n");
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y1);	/* scan it in */

		if ((p_best=db_ident(currep, x1,y1,1, 0, INST, 0)) != NULL) {
		    db_notate(p_best);	    /* print out id information */
		    db_highlight(p_best);
		    xmin=p_best->xmin;
		    xmax=p_best->xmax;
		    ymin=p_best->ymin;
		    ymax=p_best->ymax;
		    printf("sorry, edit in place is not yet implemented...\n");
		    printf("best I can do is highlight the component that I would\n");
		    printf("have edited\n");
		    state = END;
		} else {
		    printf("nothing here to EDIT... try SHO command?\n");
		    state = START;
		}
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("EDIT: cancelling EDIT\n");
	        state = END;
	    } else {
		token_err("EDIT", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case END:
	default:
	    if (token == EOC || token == CMD) {
		;
	    } else {
		token_flush_EOL(lp);
	    }
	    done++;
	    break;
	}
    }

/****************************************************************************/

    if (lp->mode == MAIN || currep == NULL  || lp->mode == EDI) {

	if (lp->mode == EDI) {	
	    if (currep != NULL) {
		/* currep->being_edited = 0;  */

		old_rep = currep;

		/* the old policy was to just dump it if it was not modified */
		/* Now we leave being_edited */
		/* alone and use it to manage an edit stack.  When we EXIT a cell */
		/* with being_edited = N, we then re-edit the next cell in the db */
		/* with the highest being_edited < N.  We have to do a search for */
		/* the biggest one because of the possibility that a cell could be */
		/* purged along the way... */
	    }
	}

	/* check for a name provided */
	if (strlen(name) == 0) {
	    printf("    must provide a name: EDIT <name>\n");
	    return (0);
	} 

	if (debug) printf("got %s\n", name); 

	lp->mode = EDI;
    
	if ((new_rep=db_lookup(name)) == NULL) { /* Not already in memory */

	    currep = db_install(name);  	/* create blank stub */
	    show_init();
	    need_redraw++;

	    /* now check if on disk */
	    snprintf(buf, MAXFILENAME, "./cells/%s.d", name);
	    if((fp = fopen(buf, "r")) == 0) {
		/* cannot find copy on disk */
		/* so leave the user with an empty start */
		if (debug) printf("calling dowin 1\n");
		do_win(lp, 4, -100.0, -100.0, 100.0, 100.0, 1.0);
		if (old_rep == NULL ) {
		    currep->being_edited = 1;
		} else {
		    /* push the stack */
		    currep->being_edited = old_rep->being_edited+1;
		}
	    } else {
		/* read it in */
		xwin_display_set_state(D_OFF);
		my_lp = token_stream_open(fp, buf);
		my_lp->mode = EDI;
		save_rep=currep;
		printf ("reading %s from disk\n", name);
		/* FIXME should save and restore show sets or store them in currep */
		show_set_modify(ALL,0,1);
		parse(my_lp);
		token_stream_close(my_lp); 
		if (debug) printf ("done reading %s from disk\n", name);
		show_set_modify(ALL,0,0);
		show_set_visible(ALL,0,1);
		xwin_display_set_state(D_ON);
		
		currep=save_rep;
		if (debug) printf("calling dowin 2\n");
		do_win(lp, 4, currep->vp_xmin, currep->vp_ymin, currep->vp_xmax, currep->vp_ymax, 1.0); 
		currep->modified = 0;

		if (old_rep == NULL ) {
		    currep->being_edited = 1;
		} else {
		    /* push the stack */
		    currep->being_edited = old_rep->being_edited+1;
		}

		show_init();
		need_redraw++;
	    }
	} else {			/* was already in memory */
	    if (new_rep->being_edited) {
	       printf("can't edit the same cell twice!\n");
	       return(0);
	    } else {
		currep = new_rep;
		xwin_window_set(
		    currep->minx, 
		    currep->miny,
		    currep->maxx,
		    currep->maxy
		);
		if (old_rep != NULL) {
		    currep->being_edited = old_rep->being_edited+1;
		} else {
		    currep->being_edited = 1;
		}
	    } 
	}	
    } else if (lp->mode == EDI) {
	printf("    must SAVE current device before new EDIT\n");
    } else {
	printf("    must EXIT before entering EDIT subsystem\n");
    }
    if (debug) printf("leaving  com_edit <%s>\n", arg); 

    rl_restoreprompt();
    return(1);
}

