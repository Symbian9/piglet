#include "db.h"
#include "xwin.h"
#include "token.h"
#include "lex.h"
#include "rubber.h"
#include "math.h"

#define MAXDBUF 20

/*
 *
 * DISTANCE xypnt1 xypnt2
 *        Computes and display both the orthogonal and diagonal distances
 *	  no refresh is done.
 *
 */

static double x1, yy1; 	/* cant call it "y1" because of math lib */
void draw_dist(); 

int com_distance(lp, layer)
LEXER *lp;
int *layer;
{
    enum {START,NUM1,COM1,NUM2,NUM3,COM2,NUM4,END} state = START;

    int npairs=0;
    double x2,y2;

    int done=0;
    int error=0;
    TOKEN token;
    char word[BUFSIZE];
    int debug=0;

    if (debug) printf("layer %d\n",*layer);
    rl_setprompt("DISTANCE> ");

    while (!done) {
	token = token_look(lp,word);
	if (debug) printf("got %s: %s\n", tok2str(token), word); 
	if (token==CMD) {
	    state=END;
	} 
	switch(state) {	
	case START:		/* get option or first xy pair */
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
	    } else {
		token_err("DISTANCE", lp, "expected NUMBER", token);
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
		token_err("DISTANCE", lp, "expected NUMBER", token);
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
		token_err("DISTANCE", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM2:
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &yy1);	/* scan it in */
		rubber_set_callback(draw_dist);
		state = NUM3;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("DISTANCE: cancelling DISTANCE\n");
	        state = END;
	    } else {
		token_err("DISTANCE", lp, "expected NUMBER", token);
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
		token_err("DISTANCE", lp, "expected NUMBER", token);
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
		token_err("DISTANCE", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM4:
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y2);	/* scan it in */
	        printf("(%g,%g) (%g,%g) dx=%g, dy=%g, dxy=%g\n",
	        x1, yy1, x2, y2, fabs(x1-x2), fabs(yy1-y2),
	        sqrt(pow((x1-x2),2.0)+pow((yy1-y2),2.0)));
		rubber_clear_callback();
		state = NUM1;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("DISTANCE: cancelling DISTANCE\n");
	        state = END;
	    } else {
		token_err("DISTANCE", lp, "expected NUMBER", token);
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
	    rubber_clear_callback();
	    done++;
	    break;
	}
    }
    return(1);
}


void draw_dist(x2, y2, count) 
double x2, y2;
int count; /* number of times called */
{
	static double x1old, x2old, y1old, y2old;
	int i;
	BOUNDS bb;
	bb.init=0;
	static char dxbuf[MAXDBUF];
	static char dybuf[MAXDBUF];
	static char dxybuf[MAXDBUF];
	static char dxbufold[MAXDBUF];
	static char dybufold[MAXDBUF];
	static char dxybufold[MAXDBUF];

	int debug=0;

	snprintf(dxbuf,  MAXDBUF, "%g", fabs(x1-x2));
	snprintf(dybuf,  MAXDBUF, "%g", fabs(yy1-y2)),
	snprintf(dxybuf, MAXDBUF, "%g", sqrt(pow((x1-x2),2.0)+pow((yy1-y2),2.0)));

	if (count == 0) {		/* first call */
	    jump(); /* draw new shape */
	    draw(x1,yy1, &bb, D_RUBBER);
	    draw(x2,y2, &bb, D_RUBBER);
	    draw(x2,yy1, &bb, D_RUBBER);
	    draw(x1,yy1, &bb, D_RUBBER);
	    xwin_draw_point(x1, yy1);
	    xwin_draw_point(x2, y2);
	    xwin_draw_text(x2, (yy1+y2)/2.0, dybuf);
	    xwin_draw_text((x1+x2)/2.0, yy1, dxbuf);
	    xwin_draw_text((x1+x2)/2.0, (yy1+y2)/2.0, dxybuf);
	} else if (count > 0) {		/* intermediate calls */
	    jump(); /* erase old shape */
	    draw(x1old,y1old, &bb, D_RUBBER);
	    draw(x2old,y2old, &bb, D_RUBBER);
	    draw(x2old,y1old, &bb, D_RUBBER);
	    draw(x1old,y1old, &bb, D_RUBBER);
	    xwin_draw_point(x1old, y1old);
	    xwin_draw_point(x2old, y2old);
	    xwin_draw_text(x2old, (y1old+y2old)/2.0, dybufold);
	    xwin_draw_text((x1old+x2old)/2.0, y1old, dxbufold);
	    xwin_draw_text((x1old+x2old)/2.0, (y1old+y2old)/2.0, dxybufold);
	    jump(); /* draw new shape */
	    draw(x1,yy1, &bb, D_RUBBER);
	    draw(x2,y2, &bb, D_RUBBER);
	    draw(x2,yy1, &bb, D_RUBBER);
	    draw(x1,yy1, &bb, D_RUBBER);
	    xwin_draw_text(x2, (yy1+y2)/2.0, dybuf);
	    xwin_draw_text((x1+x2)/2.0, yy1, dxbuf);
	    xwin_draw_text((x1+x2)/2.0, (yy1+y2)/2.0, dxybuf);
	    xwin_draw_point(x1, yy1);
	    xwin_draw_point(x2, y2);
	} else {			/* last call, cleanup */
	    jump(); /* erase old shape */
	    draw(x1old,y1old, &bb, D_RUBBER);
	    draw(x2old,y2old, &bb, D_RUBBER);
	    draw(x2old,y1old, &bb, D_RUBBER);
	    draw(x1old,y1old, &bb, D_RUBBER);
	    xwin_draw_point(x1old, y1old);
	    xwin_draw_point(x2old, y2old);
	    xwin_draw_text(x2old, (y1old+y2old)/2.0, dybufold);
	    xwin_draw_text((x1old+x2old)/2.0, y1old, dxbufold);
	    xwin_draw_text((x1old+x2old)/2.0, (y1old+y2old)/2.0, dxybufold);
	}

	/* save old values */
	x1old=x1;
	y1old=yy1;
	x2old=x2;
	y2old=y2;
	strncpy(dxbufold, dxbuf, MAXDBUF);
	strncpy(dybufold, dybuf, MAXDBUF);
	strncpy(dxybufold, dxybuf, MAXDBUF);
	jump();
}

