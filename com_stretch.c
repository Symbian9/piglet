#include <ctype.h>
#include <math.h>
#include <string.h>

#include "db.h"
#include "xwin.h"
#include "token.h"
#include "rubber.h"
#include "lex.h"
#include "rlgetc.h"
#include "lock.h"

#define POINT  0
#define REGION 1

/*
 *
 * STR [[<component>[<layer>]] [:P] { <xysel> <xyref> <xyloc> }...
 * STR [[<component>[<layer>]] :R { <xyll> <xyur> <xyref> <xyloc> }...
 *
 *  classic piglet:
 *
 *    STRETCH [descriptor] xysel xyselref xyloc
 *    STRETCH [descriptor] :G xyfrom xyto xyselref [xyselref ...]
 *    STRETCH [descriptor] :W xyfrom xyto xyll xyur [xyll xyur ...]
 *    STRETCH [descriptor] :V xysel xyedge xyloc
 *
 *      :p stretch by point (default mode: requires single coordinate) 
 *      :r stretch by region (requires 2 points to define a region)
 *
 * NOTE: default is stretch by point.  However, if you change to stretch
 *       by region with :r, you can later go back to stretch by point with
 *       the :p option.
 *
 *
 */


static double x1, yy1;
static double x2, y2;
static double x3, y3;
static double x4, y4;
static double x5, y5;
int mode=POINT;
void stretch_draw_box();
void stretch_draw_point();
STACK *stack;
STACK *tmp;

DB_DEFLIST *p_best;

typedef struct selpnt {
  double *xsel;		/* pointers to coordinates in component */
  double *ysel;
  double xselorig;	/* original values prior to any applied offset */
  double yselorig;
  struct selpnt *next;
} SELPNT;

#define MAXSELPNT 1024
SELPNT seltab[MAXSELPNT];
int nselpnts;

void clear_selpoint() {
   nselpnts=0;
}

void save_selpoint(double *x, double *y, double xorig, double yorig) {
    seltab[nselpnts].xsel = x;
    seltab[nselpnts].ysel = y;
    seltab[nselpnts].xselorig = xorig;
    seltab[nselpnts].yselorig = yorig;
    nselpnts++;
}

int bounded(double *x, double *y, double xmin, double ymin, double xmax, double ymax) {

     double tmp;

     /* canonicalize bounding box */

     if (xmax < xmin) { tmp = xmax; xmax = xmin; xmin = tmp; }
     if (ymax < ymin) { tmp = ymax; ymax = ymin; ymin = tmp; }

     if ((*x >= xmin && *x <= xmax) && (*y >= ymin && *y <= ymax)) {
        return 1;
     } else {
        return 0;
     }
}

int com_stretch(LEXER *lp, char *arg)
{
    enum {START,
          NUM1,COM1,NUM2,	/* xysel, xyll: x1, yy1 */
	  NUM3,COM2,NUM4,	/* xyur:        x2, y2  */
	  NUM5,COM3,NUM6,       /* ----:        x3, y3  */
	  NUM7,COM4,NUM8,	/* xyref:       x4, y4  */	
	  NUM9,COM5,NUM10,	/* xyloc:       x5, y5  */
	  END} state = START;

    int done=0;
    TOKEN token;
    char word[BUFSIZE];
    int debug=0;
    DB_DEFLIST *p_prev = NULL;
    double *xmin, *xmax, *ymin, *ymax;
    double d, distance, dbest;
    COORDS *coords;
    double xx, yy;
    double *xsel,*ysel;
    double *xselold,*yselold;
    double *xselfirst,*yselfirst;
    int sel;

    int my_layer=0; 	/* internal working layer */
    int valid_comp=0;
    int i;

    int comp=ALL;
    
    rl_saveprompt();
    rl_setprompt("STR> ");

    while (!done) {
	token = token_look(lp,word);
	if (debug) printf("state: %d got %s: %s\n", state, tok2str(token), word); 
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
			    printf("STR: bad option: %s\n", word);
			    state=END;
			    break;
		    } 
		} else {
		    printf("STR: bad option: %s\n", word);
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
	    } else if (token == IDENT) {
		token_get(lp,word);
	    	state = NUM1;
		/* check to see if is a valid comp descriptor */
		valid_comp=0;
		if ((comp = is_comp(toupper(word[0])))) {
		    if (strlen(word) == 1) {
			valid_comp++;	/* no layer given */
		    } else {
			valid_comp++;
			/* check for any non-digit characters */
			/* to allow instance names like "L1234b" */
			for (i=0; i<strlen(&word[1]); i++) {
			    if (!isdigit(word[1+i])) {
				valid_comp=0;
			    }
			}
			if (valid_comp) {
			    if(sscanf(&word[1], "%d", &my_layer) == 1) {
				if (debug) printf("given layer=%d\n",my_layer);
			    } else {
				valid_comp=0;
			    }
			} 
			if (valid_comp) {
			    if (my_layer > MAX_LAYER) {
				printf("layer must be less than %d\n",
				    MAX_LAYER);
				valid_comp=0;
				done++;
			    }
			    if (!show_check_modifiable(currep, comp, my_layer)) {
				printf("layer %d is not modifiable!\n",
				    my_layer);
				token_flush_EOL(lp);
				valid_comp=0;
				done++;
			    }
			}
		    }
		} else { 
		    /* here need to handle a valid cell name */
		    printf("looks like a descriptor to me: %s\n", word);
		}
	    } else {
		token_err("STR", lp, "expected NUMBER", token);
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
		token_err("STR", lp, "expected NUMBER", token);
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
		token_err("STR", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM2:
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &yy1);	/* scan it in */

		if (p_prev != NULL) {
		    /* db_highlight(p_prev); */ /* unhighlight it */
		    p_prev = NULL;
		}
		if (mode == POINT) {
		    if ((p_best=db_ident(currep, x1,yy1,1, my_layer, comp, 0)) != NULL) {
			db_highlight(p_best); 
			state = NUM7;
		    } else {
			printf("    nothing here to STRETCH, try SHO #E?\n");
			state = START;
		    }
		} else {			/* mode == REGION */
		    rubber_set_callback(stretch_draw_box);
		    state = NUM3;
		}
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("STR: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("STR", lp, "expected NUMBER", token);
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
		token_err("STR", lp, "expected NUMBER", token);
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
		token_err("STR", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM4:
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y2);	/* scan it in */
		rubber_clear_callback();
		state = NUM7;
		stack=db_ident_region(currep, x1,yy1, x2, y2, 2, my_layer, comp, 0);

		if (stack == NULL) {
		    printf("Nothing here to wrap.  Try \"SHO #E\"?\n");
		    state = END;
		} else { 	/* for each comp, add bounded nodes to selpoint array */

		     clear_selpoint();

		     tmp=stack;
		     while (tmp!=NULL) {
		        p_best = (DB_DEFLIST *) stack_walk(&tmp);

			db_highlight(p_best);

			switch (p_best->type) {

			case CIRC:

			    xsel = &(p_best->u.c->x2);
			    ysel = &(p_best->u.c->y2);
			    if (bounded (xsel, ysel, x1, yy1, x2, y2)) {
				save_selpoint(xsel, ysel, *xsel, *ysel);
			    }

			    xsel = &(p_best->u.c->x1);
			    ysel = &(p_best->u.c->y1);
			    if (bounded (xsel, ysel, x1, yy1, x2, y2)) {
				save_selpoint(xsel, ysel, *xsel, *ysel);
			    }
			    break;

			case INST:

			    xsel = &(p_best->u.i->x);
			    ysel = &(p_best->u.i->y);
			    if (bounded (xsel, ysel, x1, yy1, x2, y2)) {
				save_selpoint(xsel, ysel, *xsel, *ysel);
			    }
			    break;

			case LINE:

			    for (coords=p_best->u.l->coords;coords!=NULL;coords=coords->next) {
				xsel = &(coords->coord.x);
				ysel = &(coords->coord.y);
				if (bounded (xsel, ysel, x1, yy1, x2, y2)) {
				    save_selpoint(xsel, ysel, *xsel, *ysel);
				}
			    }
			    break;

			case NOTE:

			    xsel = &(p_best->u.n->x);
			    ysel = &(p_best->u.n->y);
			    if (bounded (xsel, ysel, x1, yy1, x2, y2)) {
				save_selpoint(xsel, ysel, *xsel, *ysel);
			    }
			    break;

			case POLY:

			    for (coords=p_best->u.p->coords;coords!=NULL;coords=coords->next) {
				xsel = &(coords->coord.x);
				ysel = &(coords->coord.y);
				if (bounded (xsel, ysel, x1, yy1, x2, y2)) {
				    save_selpoint(xsel, ysel, *xsel, *ysel);
				}
			    }
			    break;

			case RECT:
			    xmin = &(p_best->u.r->x1);
			    xmax = &(p_best->u.r->x2);
			    ymin = &(p_best->u.r->y1);
			    ymax = &(p_best->u.r->y2);

			    sel=0;

			    if (bounded (xmin, ymin, x1, yy1, x2, y2)) sel += 1;
			    if (bounded (xmin, ymax, x1, yy1, x2, y2)) sel += 2;
			    if (bounded (xmax, ymax, x1, yy1, x2, y2)) sel += 4;
			    if (bounded (xmax, ymin, x1, yy1, x2, y2)) sel += 8;

			    switch (sel) {
			    case 1:
			    	save_selpoint(xmin,ymin, *xmin, *ymin);
				break;
			    case 2:
			    	save_selpoint(xmin,ymax, *xmin, *ymax);
				break;
			    case 3:	/* left side */
			    	save_selpoint(xmin, NULL, *xmin, 0.0);
				break;
			    case 4:
			    	save_selpoint(xmax,ymax, *xmax, *ymax);
				break;
			    case 6:	/* top side */
			    	save_selpoint(NULL ,ymax, 0.0, *ymax);
				break;
			    case 8:
			    	save_selpoint(xmax,ymin, *xmax, *ymin);
				break;
			    case 9:	/* bottom side */
			    	save_selpoint(NULL ,ymin, 0.0, *ymin);
				break;
			    case 12:	/* right side */
			    	save_selpoint(xmax ,NULL, *xmax, 0.0);
				break;
			    case 15:	/* entire rectangle */
			    	save_selpoint(xmin,ymin, *xmin, *ymin);
			    	save_selpoint(xmax,ymax, *xmax, *ymax);
				break;
			    default:
			    	printf("   COM_IDENT: case %d should never occur!\n",sel);
				break;
			    }

			    break;

			case TEXT:

			    xsel = &(p_best->u.t->x);
			    ysel = &(p_best->u.t->y);

			    if (bounded (xsel, ysel, x1, yy1, x2, y2)) {
				save_selpoint(xsel, ysel, *xsel, *ysel);
			    }
			    break;

			default:
			    printf("    not a stretchable object\n");
			    db_notate(p_best);	/* print information */
			    state = START;
			    break;
			}
		     }
		     if (!nselpnts) {
			printf("   nothing selected!\n");
			state = START;
		     }
		}

	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("STR: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("STR", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case NUM5:		/* get pair of xy coordinates */
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &x3);	/* scan it in */
		state = COM3;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = END;	
	    } else {
		token_err("STR", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case COM3:		
	    if (token == EOL) {
		token_get(lp,word); /* just ignore it */
	    } else if (token == COMMA) {
		token_get(lp,word);
		state = NUM6;
	    } else {
		token_err("STR", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM6:
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y3);	/* scan it in */
		state = NUM7;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("STR: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("STR", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case NUM7:		/* get pair of xy coordinates */
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &x4);	/* scan it in */
		state = COM4;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = END;	
	    } else {
		token_err("STR", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case COM4:		
	    if (token == EOL) {
		token_get(lp,word); /* just ignore it */
	    } else if (token == COMMA) {
		token_get(lp,word);
		state = NUM8;
	    } else {
		token_err("STR", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;

	case NUM8:
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y4);	/* scan it in */
		state = NUM9;

		if (mode == POINT) {
		    db_highlight(p_best);		/* unhighlight it */

		    switch (p_best->type) {

		    case CIRC:

			xsel = &(p_best->u.c->x2);
			ysel = &(p_best->u.c->y2);
			clear_selpoint();
			save_selpoint(xsel, ysel, *xsel, *ysel);
			rubber_set_callback(stretch_draw_point);

			state = NUM9;
			break;

		    case INST:

			xsel = &(p_best->u.i->x);
			ysel = &(p_best->u.i->y);
			clear_selpoint();
			save_selpoint(xsel, ysel, *xsel, *ysel);
			rubber_set_callback(stretch_draw_point);

			state = NUM9;
			break;

		    case LINE:
			coords = p_best->u.l->coords;

			xsel = &(coords->coord.x);
			ysel = &(coords->coord.y);
			distance = dist((*xsel)-x4, (*ysel)-y4);
			dbest = distance;
			coords = coords->next;
			clear_selpoint();
			save_selpoint(xsel, ysel, *xsel, *ysel);

			while(coords != NULL) {
			    xselold = xsel;
			    yselold = ysel;
			    xsel = &(coords->coord.x);
			    ysel = &(coords->coord.y);

			    /* test midpoint */
			    distance = dist( ((*xsel+*xselold)/2.0)-x4,
					     ((*ysel+*yselold)/2.0)-y4 );
			    if (distance < dbest) {
				dbest = distance;
				clear_selpoint();
				save_selpoint(xsel, ysel, *xsel, *ysel);
				save_selpoint(xselold, yselold, *xselold, *yselold);
			    }

			    distance = dist(*xsel-x4, *ysel-y4);
			    if (distance < dbest) {
				dbest = distance;
				clear_selpoint();
				save_selpoint(xsel, ysel, *xsel, *ysel);
			    }
			    coords = coords->next;
			}
			rubber_set_callback(stretch_draw_point);

			state = NUM9;
			break;

		    case NOTE:

			xsel = &(p_best->u.n->x);
			ysel = &(p_best->u.n->y);
			clear_selpoint();
			save_selpoint(xsel, ysel, *xsel, *ysel);
			rubber_set_callback(stretch_draw_point);

			state = NUM9;
			break;

		    case POLY:
			coords = p_best->u.p->coords;

			xselfirst = xsel = &(coords->coord.x);		/* first point */
			yselfirst = ysel = &(coords->coord.y);

			distance = dist((*xsel)-x4, (*ysel)-y4);
			dbest = distance;
			coords = coords->next;
			clear_selpoint();
			save_selpoint(xsel, ysel, *xsel, *ysel);

			while(coords != NULL) {
			    xselold = xsel;
			    yselold = ysel;
			    xsel = &(coords->coord.x);
			    ysel = &(coords->coord.y);

			    /* test midpoint */
			    distance = dist( ((*xsel+*xselold)/2.0)-x4,	/* mid points */
					     ((*ysel+*yselold)/2.0)-y4 );
			    if (distance < dbest) {
				dbest = distance;
				clear_selpoint();
				save_selpoint(xsel, ysel, *xsel, *ysel);
				save_selpoint(xselold, yselold, *xselold, *yselold);
			    }

			    distance = dist(*xsel-x4, *ysel-y4);		/* next points */
			    if (distance < dbest) {
				dbest = distance;
				clear_selpoint();
				save_selpoint(xsel, ysel, *xsel, *ysel);
			    }
			    coords = coords->next;
			}

			distance = dist( ((*xsel+*xselfirst)/2.0)-x4,	/* between first/last */
					 ((*ysel+*yselfirst)/2.0)-y4 );
			if (distance < dbest) {
			    dbest = distance;
			    clear_selpoint();
			    save_selpoint(xsel, ysel, *xsel, *ysel);
			    save_selpoint(xselfirst, yselfirst, *xselfirst, *yselfirst);
			}


			rubber_set_callback(stretch_draw_point);

			state = NUM9;
			break;

			
		    case (POLY+99):
			coords = p_best->u.p->coords;

			xx = coords->coord.x;
			yy = coords->coord.y;
			distance = dist(xx-x4, yy-y4);
			dbest = distance;
			xsel = &(coords->coord.x);
			ysel = &(coords->coord.y);
			coords = coords->next;

			while(coords != NULL) {
			    xx = coords->coord.x;
			    yy = coords->coord.y;
			    distance = dist(xx-x4, yy-y4);
			    if (distance < dbest) {
				xsel = &(coords->coord.x);
				ysel = &(coords->coord.y);
				dbest = distance;
			    }
			    coords = coords->next;
			}
			clear_selpoint();
			save_selpoint(xsel, ysel, *xsel, *ysel);
			rubber_set_callback(stretch_draw_point);

			state = NUM9;
			break;


		    case RECT:
			xmin = &(p_best->u.r->x1);
			xmax = &(p_best->u.r->x2);
			ymin = &(p_best->u.r->y1);
			ymax = &(p_best->u.r->y2);

			/* check the eight sides + vertices */

			xsel = xmin, ysel = ymin;
			dbest = d = dist(*xmin-x4, *ymin-y4);

			d = dist(*xmin-x4, *ymax-y4);
			if (d < dbest) { xsel = xmin, ysel = ymax; dbest = d; }

			d = dist(*xmax-x4, *ymax-y4);
			if (d < dbest) { xsel = xmax, ysel = ymax; dbest = d; }

			d = dist(*xmax-x4, *ymin-y4);
			if (d < dbest) { xsel = xmax, ysel = ymin; dbest = d; }

			d = dist(*xmin-x4, ((*ymin+*ymax)/2.0)-y4);
			if (d < dbest) { xsel = xmin, ysel = NULL; dbest = d; }

			d = dist(*xmax-x4, ((*ymin+*ymax)/2.0)-y4);
			if (d < dbest) { xsel = xmax, ysel = NULL; dbest = d; }

			d = dist(((*xmin+*xmax)/2.0)-x4, *ymin-y4);
			if (d < dbest) { xsel = NULL, ysel = ymin; dbest = d; }

			d = dist(((*xmin+*xmax)/2.0)-x4, *ymax-y4);
			if (d < dbest) { xsel = NULL, ysel = ymax; dbest = d; }
			
			clear_selpoint();
			/* save_selpoint(xsel, ysel, *xsel, *ysel); */

			save_selpoint(xsel, ysel, 
			    (xsel==NULL)?0.0:(*xsel) , 
			    (ysel==NULL)?0.0:(*ysel) );

			rubber_set_callback(stretch_draw_point);

			/* xwin_draw_circle(*xsel, *ysel); */

			state = NUM9;
			break;

		    case TEXT:

			xsel = &(p_best->u.t->x);
			ysel = &(p_best->u.t->y);
			clear_selpoint();
			save_selpoint(xsel, ysel, *xsel, *ysel);
			rubber_set_callback(stretch_draw_point);

			state = NUM9;
			break;

		    default:
			printf("    not a stretchable object\n");
			db_notate(p_best);	/* print information */
			state = START;
			break;
		    }
		    p_prev=p_best;
		} else { 			/* mode == REGION */
		     if (nselpnts) {
			if (debug) {
			   printf("setting rubber callback\n");
			}
		        rubber_set_callback(stretch_draw_point);
		     } else {
			if (debug) {
			   printf("NOT setting rubber callback\n");
			}
		     }
		}
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("STR: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("STR", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case NUM9:		/* get pair of xy coordinates */
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &x5);	/* scan it in */
		state = COM5;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = END;	
	    } else {
		token_err("STR", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case COM5:		
	    if (token == EOL) {
		token_get(lp,word); /* just ignore it */
	    } else if (token == COMMA) {
		token_get(lp,word);
		state = NUM10;
	    } else {
		token_err("STR", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM10:
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y5);	/* scan it in */
		state = START;
		rubber_clear_callback();
		need_redraw++;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("STR: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("STR", lp, "expected NUMBER", token);
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

void stretch_draw_point(double xx, double yy, int count)
{
        static double xxold, yyold;
        BOUNDS bb;
        bb.init=0;
	int debug = 1;
	int i;

	if (debug) {
	   printf("in stretch_draw_point: %g %g\n", xx, yy);
	}
        
	setlockpoint(x4, y4);
	lockpoint(&xx, &yy, currep->lock_angle);

        if (count == 0) {               /* first call */
	    for (i=0; i<nselpnts; i++) {
	        if (seltab[i].xsel != NULL) {
		   *(seltab[i].xsel) = seltab[i].xselorig + xx - x4;
		}
	        if (seltab[i].ysel != NULL) {
		   *(seltab[i].ysel) = seltab[i].yselorig + yy - y4;
		}
		/* xwin_draw_circle(*seltab[i].xsel, *seltab[i].ysel); */
	    }
	    if (mode == POINT) {
		db_highlight(p_best);
	    } else {
		tmp=stack;
		while (tmp!=NULL) {
		   p_best = (DB_DEFLIST *) stack_walk(&tmp);
		   db_highlight(p_best);
		}
	    }
        } else if (count > 0) {         /* intermediate calls */
	    for (i=0; i<nselpnts; i++) {
	        if (seltab[i].xsel != NULL) {
		   *(seltab[i].xsel) = seltab[i].xselorig + xxold - x4;
		}
	        if (seltab[i].ysel != NULL) {
		   *(seltab[i].ysel) = seltab[i].yselorig + yyold - y4;
		}
		/* xwin_draw_circle(*seltab[i].xsel, *seltab[i].ysel); */
	    }
	    if (mode == POINT) {
		db_highlight(p_best);
	    } else {
		tmp=stack;
		while (tmp!=NULL) {
		   p_best = (DB_DEFLIST *) stack_walk(&tmp);
		   db_highlight(p_best);
		}
	    }
	    for (i=0; i<nselpnts; i++) {
	        if (seltab[i].xsel != NULL) {
		   *(seltab[i].xsel) = seltab[i].xselorig + xx - x4;
		}
	        if (seltab[i].ysel != NULL) {
		   *(seltab[i].ysel) = seltab[i].yselorig + yy - y4;
		}
		/* xwin_draw_circle(*seltab[i].xsel, *seltab[i].ysel); */
	    }
	    if (mode == POINT) {
		db_highlight(p_best);
	    } else {
		tmp=stack;
		while (tmp!=NULL) {
		   p_best = (DB_DEFLIST *) stack_walk(&tmp);
		   db_highlight(p_best);
		}
	    }
        } else {                        /* last call, cleanup */
	    /*
	    for (i=0; i<nselpnts; i++) {
	        if (seltab[i].xsel != NULL) {
		   *(seltab[i].xsel) = seltab[i].xselorig + xxold - x4;
		}
	        if (seltab[i].ysel != NULL) {
		   *(seltab[i].ysel) = seltab[i].yselorig + yyold - y4;
		}
	    }
	    */
	    if (mode == POINT) { /* erase old shape */	
	        db_highlight(p_best);
	    } else {
		tmp=stack;
		while (tmp!=NULL) {
		   p_best = (DB_DEFLIST *) stack_walk(&tmp);
		   db_highlight(p_best);
		}
	    }
	    
        }

        /* save old values */
        xxold=xx;
        yyold=yy;
        jump(&bb, D_RUBBER);
}

void stretch_draw_box(x3, y3, count)
double x3, y3;
int count; /* number of times called */
{
        static double x1old, x3old, y1old, y3old;
        BOUNDS bb;
        bb.init=0;

        if (count == 0) {               /* first call */
            db_drawbounds(x1,yy1,x3,y3,D_RUBBER);                /* draw new shape */
        } else if (count > 0) {         /* intermediate calls */
            db_drawbounds(x1old,y1old,x3old,y3old,D_RUBBER);    /* erase old shape */
            db_drawbounds(x1,yy1,x3,y3,D_RUBBER);                /* draw new shape */
        } else {                        /* last call, cleanup */
            db_drawbounds(x1old,y1old,x3old,y3old,D_RUBBER);    /* erase old shape */
        }

        /* save old values */
        x1old=x1;
        y1old=yy1;
        x3old=x3;
        y3old=y3;
        jump(&bb, D_RUBBER);
}

