#include <ctype.h>

#include "db.h"
#include "xwin.h"
#include "token.h"
#include "rubber.h"
#include "lex.h"
#include "rlgetc.h"

#define POINT  0
#define REGION 1

/*
 *
 * IDE [:r xy1 xy2] | [[:p] xypnt] ... EOC
 *	highlight and print out details of rep under xypnt.
 *	  no refresh is done.
 * OPTIONS:
 *      :r identify by region (requires 2 points to define a region)
 *      :p identify by point (default mode: requires single coordinate) 
 *
 * NOTE: default is pick by point.  However, if you change to pick
 *       by region with :r, you can later go back to picking by point with
 *       the :p option.
 *
 */

static double x1, y1;
static double x2, y2;
void ident_draw_box();
STACK *stack;

int com_identify(lp, layer)
LEXER *lp;
int *layer;
{
    enum {START,NUM1,COM1,NUM2,NUM3,COM2,NUM4,END} state = START;

    int done=0;
    TOKEN token;
    char word[BUFSIZE];
    int debug=0;
    DB_DEFLIST *p_best;
    DB_DEFLIST *p_prev = NULL;
    double area;
    int mode=POINT;
    
    rl_saveprompt();
    rl_setprompt("IDENT> ");

    while (!done) {
	token = token_look(lp,word);
	if (debug) printf("got %s: %s\n", tok2str(token), word); 
	if (token==CMD) {
	    state=END;
	} 
	switch(state) {	
	case START:		/* get option or first xy pair */
	    if (token == OPT ) {
		token_get(lp,word); 
		if (word[0]==':') {
		    switch (toupper(word[1])) {
		        case 'R':
			    mode = REGION;
			    break;
			case 'P':
			    mode = POINT;
			    break;
			default:
			    printf("IDENT: bad option: %s\n", word);
			    state=END;
			    break;
		    } 
		} else {
		    printf("IDENT: bad option: %s\n", word);
		    state = END;
		}
		state = START;
	    } else if (token == NUMBER) {
		state = NUM1;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just eat it up */
		state = START;
	    } else if (token == EOC || token == CMD) {
		 state = END;
	    } else {
		token_err("IDENT", lp, "expected NUMBER", token);
		state = END;	/* error */
	    }
	    break;
	case NUM1:		/* get pair of xy coordinates */
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &x1);	/* scan it in */
		state = COM1;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = END;	
	    } else {
		token_err("IDENT", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case COM1:		
	    if (token == EOL) {
		token_get(lp,word); /* just ignore it */
	    } else if (token == COMMA) {
		token_get(lp,word);
		state = NUM2;
	    } else {
		token_err("IDENT", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM2:
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y1);	/* scan it in */

		if (mode==POINT) {
		    if (p_prev != NULL) {
			db_highlight(p_prev);	/* unhighlight it */
		    }
		    if ((p_best=db_ident(currep, x1,y1,0, 0, 0, 0)) != NULL) {
			db_highlight(p_best);	
			db_notate(p_best);	/* print information */
			area = db_area(p_best);
			if (area >= 0) {
			    printf("   area = %g\n", db_area(p_best));
			}
			p_prev=p_best;
		    }
		    state = START;
		} else {			/* mode == REGION */
		    rubber_set_callback(ident_draw_box);
		    state = NUM3;
		}
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("IDENT: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("IDENT", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case NUM3:		/* get pair of xy coordinates */
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &x2);	/* scan it in */
		state = COM2;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = END;	
	    } else {
		token_err("IDENT", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case COM2:		
	    if (token == EOL) {
		token_get(lp,word); /* just ignore it */
	    } else if (token == COMMA) {
		token_get(lp,word);
		state = NUM4;
	    } else {
		token_err("IDENT", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM4:
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y2);	/* scan it in */
		state = START;
		rubber_clear_callback();
		need_redraw++;

		printf("IDENT: got %g,%g %g,%g\n", x1, y1, x2, y2);
		stack=db_ident_region(currep, x1,y1, x2, y2, 0, 0, 0, 0);
		while ((p_best = (DB_DEFLIST *) stack_pop(&stack))!=NULL) {
		    db_highlight(p_best);	
		    db_notate(p_best);		/* print information */
		    area = db_area(p_best);
		    if (area >= 0) {
			printf("   area = %g\n", db_area(p_best));
		    }
		}
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("IDENT: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("IDENT", lp, "expected NUMBER", token);
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
	    rubber_clear_callback();
	    break;
	}
    }
    if (p_prev != NULL) {
	db_highlight(p_prev);	/* unhighlight any remaining component */
    }
    rl_restoreprompt();
    return(1);
}

void ident_draw_box(x2, y2, count)
double x2, y2;
int count; /* number of times called */
{
        static double x1old, x2old, y1old, y2old;
        BOUNDS bb;
        bb.init=0;

        if (count == 0) {               /* first call */
            db_drawbounds(x1,y1,x2,y2,D_RUBBER);                /* draw new shape */
        } else if (count > 0) {         /* intermediate calls */
            db_drawbounds(x1old,y1old,x2old,y2old,D_RUBBER);    /* erase old shape */
            db_drawbounds(x1,y1,x2,y2,D_RUBBER);                /* draw new shape */
        } else {                        /* last call, cleanup */
            db_drawbounds(x1old,y1old,x2old,y2old,D_RUBBER);    /* erase old shape */
        }

        /* save old values */
        x1old=x1;
        y1old=y1;
        x2old=x2;
        y2old=y2;
        jump(&bb, D_RUBBER);
}

