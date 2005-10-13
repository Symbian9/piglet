#include "db.h"
#include "xwin.h"
#include "token.h"
#include "rubber.h"
#include "opt_parse.h"
#include "rlgetc.h"
#include "lex.h"
#include <string.h>

/* :Mmirror :Rrot :Xscale :Yyxratio :Zslant */

static double bb_xmin, bb_ymin, bb_xmax, bb_ymax;
void draw_inst_bb();

int add_inst(LEXER *lp, char *inst_name)
{
    enum {START,NUM1,COM1,NUM2,END} state = START;

    double x1, y1;
    int done=0;
    TOKEN token;
    OPTS opts;
    char word[BUFSIZE];
    char buf[BUFSIZE];
    int debug=0;
    FILE *fp;
    LEXER *my_lp;

    DB_TAB *ed_rep;
    char *save_rep;

    BOUNDS bb;
    XFORM *xp;

    double xx, yy;

    opt_set_defaults(&opts);

    rl_saveprompt();
    rl_setprompt("ADD_INST> ");

    if (debug) printf("currep = %s\n", currep->name);
    if (debug) printf("adding inst %s\n", inst_name);

    /* don't destroy it if it's already in memory */
    if (debug) printf("calling db_lookup with %s\n", inst_name);

    if ((ed_rep = db_lookup(inst_name)) == NULL) {	/* not in memory */

	snprintf(buf, MAXFILENAME, "./cells/%s.d", inst_name);
	if((fp = fopen(buf, "r")) == 0) { 		/* cannot find copy on disk */	
	    printf("can't add a null instance: %s\n", inst_name);
	    token_flush_EOL(lp);
	    done++;
	} else { 					/* found it on disk, read it in */	
	    ed_rep = db_install(inst_name);  /* create blank stub */
	    printf("reading %s from disk\n", buf);
	    my_lp = token_stream_open(fp, buf);

	    if (currep != NULL) {
		save_rep=strsave(currep->name);
	    } else {
		save_rep=NULL;
	    }

	    currep = ed_rep;
	    xwin_display_set_state(D_OFF);
	    show_set_modify(currep, ALL, 0,1);		/* make rep modifiable */
	    parse(my_lp);
	    show_set_modify(currep, ALL, 0,0);		/* set rep not modifiable */
	    show_set_visible(currep, ALL, 0,1);		/* and make it visible */
	    currep->modified = 0;
    	    bb.init=0;
	    db_render(currep, 0, &bb, D_READIN); 	/* set boundbox, etc */
	    xwin_display_set_state(D_ON);
	    
	    token_stream_close(my_lp); 

	    if (save_rep != NULL) {
		currep=db_lookup(save_rep);
		free(save_rep);
	    } else {
		currep=NULL;
	    }
	}
    }

    if ((ed_rep = db_lookup(inst_name)) == 0) {
    	printf("ADD INST: instance not found: %s\n", inst_name );
	return(-1);
    }

    bb_xmin=ed_rep->minx;
    bb_xmax=ed_rep->maxx;
    bb_ymin=ed_rep->miny;
    bb_ymax=ed_rep->maxy;

    if (debug) printf("currep = %s\n", currep->name);

    if (strcmp(currep->name, inst_name) == 0 || db_contains(inst_name, currep->name)) {
    	printf("ADD INST: Sorry, to do that would cause a recursive definition\n" );
	return(-1);
    }

    while (!done) {
	token = token_look(lp, word);
	/* printf("got %s: %s\n", tok2str(token), word); */
	if (token==CMD) {
	    state=END;
	} 
	switch(state) {	
	    case START:		/* get option or first xy pair */
		rubber_set_callback(draw_inst_bb);
		if (token == OPT ) {
		    token_get(lp, word); 
		    if (opt_parse(word, INST_OPTS, &opts) == -1) {
			state = END;
		    } else {
			/* an option may have scaled the bounding box */
			/* clear callback, recompute and then restart */

			rubber_clear_callback(draw_inst_bb);

			xp = matrix_from_opts(&opts);
			bb_xmin = bb_xmax = bb_ymin = bb_ymax = 0.0;

			xx = ed_rep->minx;
			yy = ed_rep->miny;
			xform_point(xp, &xx, &yy); 

			if (xx < bb_xmin) bb_xmin = xx;
			if (yy < bb_ymin) bb_ymin = yy;
			if (xx > bb_xmax) bb_xmax = xx;
			if (yy > bb_ymax) bb_ymax = yy;

			xx = ed_rep->maxx;
			yy = ed_rep->maxy;
			xform_point(xp, &xx, &yy); 

			if (xx < bb_xmin) bb_xmin = xx;
			if (yy < bb_ymin) bb_ymin = yy;
			if (xx > bb_xmax) bb_xmax = xx;
			if (yy > bb_ymax) bb_ymax = yy;

			xx = ed_rep->maxx;
			yy = ed_rep->miny;
			xform_point(xp, &xx, &yy); 

			if (xx < bb_xmin) bb_xmin = xx;
			if (yy < bb_ymin) bb_ymin = yy;
			if (xx > bb_xmax) bb_xmax = xx;
			if (yy > bb_ymax) bb_ymax = yy;

			xx = ed_rep->minx;
			yy = ed_rep->maxy;
			xform_point(xp, &xx, &yy); 

			if (xx < bb_xmin) bb_xmin = xx;
			if (yy < bb_ymin) bb_ymin = yy;
			if (xx > bb_xmax) bb_xmax = xx;
			if (yy > bb_ymax) bb_ymax = yy;

			free(xp);

			state = START;
			rubber_set_callback(draw_inst_bb);
		    }
		} else if (token == NUMBER) {
		    state = NUM1;
		} else if (token == EOL) {
		    token_get(lp, word); 	/* just eat it up */
		    state = START;
		} else if (token == EOC  || token == CMD) {
		    state = END; 
		} else {
		    token_err("INST", lp, "expected OPT or NUMBER", token);
		    state = END; 
		}
		break;
	    case NUM1:		/* get pair of xy coordinates */
		if (token == NUMBER) {
		    token_get(lp, word); 
		    sscanf(word, "%lf", &x1);	/* scan it in */
		    state = COM1;
		} else if (token == EOL) {
		    token_get(lp, word); /* just ignore it */
		} else if (token == EOC  || token == CMD) {
		    state = END; 
		} else {
		    token_err("INST", lp, "expected NUMBER", token);
		    state = END; 
		}
		break;
	    case COM1:		
		if (token == EOL) {
		    token_get(lp, word); /* just ignore it */
		} else if (token == COMMA) {
		    token_get(lp, word);
		    state = NUM2;
		} else {
		    token_err("INST", lp, "expected COMMA", token);
		    state = END;	
		}
		break;
	    case NUM2:
		if (token == NUMBER) {
		    token_get(lp, word);
		    sscanf(word, "%lf", &y1);	/* scan it in */
		    
		    db_add_inst(currep, ed_rep, opt_copy(&opts), x1, y1);
		    rubber_clear_callback();
		    need_redraw++;
		    rubber_set_callback(draw_inst_bb);
		    state = START;
		} else if (token == EOL) {
		    token_get(lp, word);
		} else if (token == EOC  || token == CMD) {
		    state = END; 
		} else {
		    token_err("INST", lp, "expected NUMBER", token);
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
    rubber_clear_callback();
    rl_restoreprompt();
    return(1);
}

void draw_inst_bb(x, y, count) 
double x, y;
int count; /* number of times called */
{
	static double xold, yold;
	BOUNDS bb;
	bb.init=0;

	if (count == 0) {		/* first call */
	    db_drawbounds(bb_xmin+x, bb_ymin+y, bb_xmax+x, bb_ymax+y, D_RUBBER);
	} else if (count > 0) {		/* intermediate calls */
	    db_drawbounds(bb_xmin+xold, bb_ymin+yold, bb_xmax+xold, bb_ymax+yold, D_RUBBER);
	    db_drawbounds(bb_xmin+x, bb_ymin+y, bb_xmax+x, bb_ymax+y, D_RUBBER);
	} else {			/* last call, cleanup */
	    db_drawbounds(bb_xmin+xold, bb_ymin+yold, bb_xmax+xold, bb_ymax+yold, D_RUBBER);
	}

	/* save old values */
	xold=x;
	yold=y;
	jump(&bb, D_RUBBER);
}

