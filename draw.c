#include "db.h"
#include "rubber.h"
#include "xwin.h"

/* global variables that need to eventually be on a stack for nested edits */
int layer;
int drawon=1;		  /* 0 = dont draw, 1 = draw (used in nesting)*/
int showon=1;		  /* 0 = layer currently turned off */
int nestlevel=9;
int boundslevel=3;
int X=1;		  /* 1 = draw to X, 0 = emit autoplot commands */
FILE *PLOT_FD;		  /* file descriptor for plotting */

/********************************************************/
/* rendering routines
/* db_render() sets globals xmax, ymax, xmin, ymin;
/********************************************************/

void db_set_nest(nest) 
int nest;
{
    extern int nestlevel;
    nestlevel = nest;
}

/* a debugging stub to turn pick-bounds display on/off */
void db_set_bounds(bounds) 
int bounds;
{
    extern int boundslevel;
    boundslevel = bounds;
}

void db_bounds_update(mybb, childbb) 
BOUNDS *mybb;
BOUNDS *childbb;
{

    if (mybb->init==0) {
    	mybb->xmin = childbb->xmin;
    	mybb->ymin = childbb->ymin;
    	mybb->xmax = childbb->xmax;
    	mybb->ymax = childbb->ymax;
	mybb->init++;
    } else if (childbb->init != 0 && mybb->init != 0) {
        /* pass bounding box back */
	mybb->xmin=min(mybb->xmin, childbb->xmin);
	mybb->ymin=min(mybb->ymin, childbb->ymin);
	mybb->xmax=max(mybb->xmax, childbb->xmax);
	mybb->ymax=max(mybb->ymax, childbb->ymax);
    }
    return;
}

/* a fast approximation to euclidian distance */
/* with no sqrt() and about 5% accuracy */

double dist(dx,dy) 
double dx,dy;
{
    dx = dx<0?-dx:dx; /* absolute value */
    dy = dy<0?-dy:dy;

    if (dx > dy) {
	return(dx + 0.3333*dy);
    } else {
	return(dy + 0.3333*dx);
    }
}


/* called with pick point x,y and bounding box in p return zero if xy */
/* inside bb, otherwise return a number  between zero and 1 */
/* monotonically related to inverse distance of xy from bounding box */

double bb_distance(x, y, p, fuzz)
double x,y;
DB_DEFLIST *p;
double fuzz;
{
    int outcode;
    double retval;
    outcode = 0;

    /*  9 8 10
	1 0 2
	5 4 6 */

    /* FIXME - this check can eventually go away for speed */
    if (p->xmin >= p->xmax) { printf("bad!\n"); }
    if (p->ymin >= p->ymax) { printf("bad!\n"); }

    if (x <= p->xmin-fuzz)  outcode += 1; 
    if (x >  p->xmax+fuzz)  outcode += 2;
    if (y <= p->ymin-fuzz)  outcode += 4;
    if (y >  p->ymax+fuzz)  outcode += 8;

    switch (outcode) {
	case 0:		/* p is inside BB */
	    retval = -1.0; 	
	    break;
	case 1:		/* p is West */
	    retval = 1.0/(1.0 + (p->xmin-x));
	    break;
	case 2:		/* p is East */
	    retval = 1.0/(1.0 + (x-p->xmax));
	    break;
	case 4:		/* p is South */
	    retval = 1.0/(1.0 + (p->ymin-y));	
	    break;
	case 5:		/* p is SouthWest */
	    retval = 1.0/(1.0 + dist(p->ymin-y,p->xmin-x));
	    break;
	case 6:		/* p is SouthEast */
	    retval = 1.0/(1.0 + dist(p->ymin-y,x-p->xmax));
	    break;
	case 8:		/* p is North */
	    retval = 1.0/(1.0 + (y-p->ymax));	
	    break;
	case 9:		/* p is NorthWest */
	    retval = 1.0/(1.0 + dist(y-p->ymax, p->xmin-x));
	    break;
	case 10:	/* p is NorthEast */
	    retval = 1.0/(1.0 + dist(y-p->ymax, x-p->xmax));
	    break;
	default:
	    printf("bad case in bb_distance %d\n", outcode);
	    exit(1);
	    break;
    }
    /* printf("%g %g %g %g %g %d\n", p->xmin, p->ymin, p->xmax, p->ymax, retval, outcode); */
    return (retval);
}


/* 
 * db_ident() takes a pick point (x,y) and renders through 
 * the database to find the best candidate near the point.
 *
 * Run through all components in the device and
 * give each a score.  The component with the best score is 
 * returned as the final pick.
 * 
 * three different scores:
 *    CASE 1: ; pick point is outside of BB, higher score the 
 *      closer to BB
 *	(0<  score < 1)	
 *	score is 1/(1+distance to cell BB)
 *    CASE 2: ; pick inside of BB, doesn't touch any rendered lines
 *	(1<= score < 2)
 *	score is 1+1/(1+min_dimension_of cell BB)
 *	score is weighted so that smaller BBs get better score
 *	This allows picking the inner of nested rectangles
 *    CASE 3: ; pick inside of BB, touches a rendered line
 *	(2<= score < 3)
 *	score is 2+1/(1+min_dimension_of line BB)
 *
 *    CASE 1 serves as a crude selector in the case that the
 *    pick point is not inside any bounding box.  It also serves
 *    as a crude screening to avoid having to test all the components
 *    in detail. 
 * 
 *    CASE 2 is for picks that fall inside a bounding box. It has a
 *    higher score than anything in CASE 1 so it will take priority
 *    Smaller bounding boxes get higher ranking.
 *
 *    CASE 3 overrides CASE 1 and CASE 2.  It comes into play when
 *    the pick point is near an actual drawn segment of a component.
 *
 */

int db_ident(cell, x, y)
DB_TAB *cell;			/* device being edited */
double x, y;	 		/* pick point */
{
    DB_DEFLIST *p;
    DB_DEFLIST *p_best;
    int debug=0;
    BOUNDS childbb;
    double pick_score;
    double pick_best=0.0;
    int outcode;
    double fuzz;
    double xmin, ymin, xmax, ymax;
    int layer;

    if (cell == NULL) {
    	printf("no cell currently being edited!\n");
	return(0);
    }

    /* pick fuzz should be a fraction of total window size */
    xwin_window_get(&xmin, &ymin, &xmax, &ymax);
    fuzz = max((xmax-xmin),(ymax-ymin))/100.0;

    for (p=cell->dbhead; p!=(DB_DEFLIST *)0; p=p->next) {

	childbb.init=0;

	/* screen components by crude bounding boxes then */
	/* render each one in D_PICK mode for detailed test */

	switch (p->type) {
	case ARC:   /* arc definition */
	    layer = p->u.a->layer;
	    break;
	case CIRC:  /* arc definition */
	    layer = p->u.c->layer;
	    break;
	case LINE:  /* arc definition */
	    layer = p->u.l->layer;
	    break;
	case OVAL:  /* oval definition */
	    layer = p->u.o->layer;
	    break;
	case POLY:  /* polygon definition */
	    layer = p->u.p->layer;
	    break;
	case RECT:  /* rectangle definition */
	    layer = p->u.r->layer;
	    break;
	case TEXT:  /* text and note definition */
	    layer = p->u.t->layer;
	    break;
	case INST:  /* text and note definition */
	    layer = 0;
	    break;
	default:
	    printf("error in db_ident switch\n");
	    break;
	}

	if (show_check_visible(p->type, layer)) {

	    if ((pick_score=bb_distance(x,y,p,fuzz)) < 0.0) { 	/* inside BB */

		/* pick point simply being inside BB gives a score ranging from */
		/* 1.0 to 2.0, with smaller BB's getting higher score */
		pick_score= 1.0 + 1.0/(1.0+min(fabs(p->xmax-p->xmin),
		    fabs(p->ymax-p->ymin)));

		/* a bit of a hack, but we overload the childbb structure to */
		/* pass the pick point into the draw routine.  If there is a hit, */
		/* then childbb.xmax is set to 1.0 */
		childbb.xmin = x;
		childbb.ymin = y;
		childbb.xmax = 0.0;

		switch (p->type) {
		case ARC:  /* arc definition */
		    do_arc(p, &childbb, D_PICK); 
		    if (debug) printf("in arc %g\n", childbb.xmax);
		    break;
		case CIRC:  /* circle definition */
		    do_circ(p, &childbb, D_PICK); 
		    if (debug) printf("in circ %g\n", childbb.xmax);
		    break;
		case LINE:  /* line definition */
		    do_line(p, &childbb, D_PICK); 
		    if (debug) printf("in line %g\n", childbb.xmax);
		    break;
		case OVAL:  /* oval definition */
		    do_oval(p, &childbb, D_PICK);
		    if (debug) printf("in oval %g\n", childbb.xmax);
		    break;
		case POLY:  /* polygon definition */
		    do_poly(p, &childbb, D_PICK);
		    if (debug) printf("in poly %g\n", childbb.xmax);
		    break;
		case RECT:  /* rectangle definition */
		    do_rect(p, &childbb, D_PICK);
		    if (debug) printf("in rect %g\n", childbb.xmax);
		    break;
		case TEXT:  /* text and note definition */
		    do_text(p, &childbb, D_PICK); 
		    if (debug) printf("in text %g\n", childbb.xmax);
		    break;
		case INST:  /* recursive instance call */
		    if (debug) printf("in inst %g\n", childbb.xmax);
		    break;
		default:
		    eprintf("unknown record type: %d in db_ident\n", p->type);
		    return(1);
		    break;
		}

		pick_score += childbb.xmax*3.0;
	    }
	}

	/* keep track of the best of the lot */
	if (pick_score > pick_best) {
	    pick_best=pick_score;
	    p_best = p;
	}
    }

    db_notate(p_best);		/* print out identifying information */
    db_highlight(p_best);	
}

db_drawbounds(p)
DB_DEFLIST *p;
{
    BOUNDS childbb;
    childbb.init=0;
    set_layer(0,0);

    jump();
	/* a square bounding box outline in white */
	draw(p->xmax, p->ymax, &childbb, D_RUBBER); 
	draw(p->xmax, p->ymin, &childbb, D_RUBBER);
	draw(p->xmin, p->ymin, &childbb, D_RUBBER);
	draw(p->xmin, p->ymax, &childbb, D_RUBBER);
	draw(p->xmax, p->ymax, &childbb, D_RUBBER);
    jump();
	/* and diagonal lines from corner to corner */
	draw(p->xmax, p->ymax, &childbb, D_RUBBER);
	draw(p->xmin, p->ymin, &childbb, D_RUBBER);
    jump() ;
	draw(p->xmin, p->ymax, &childbb, D_RUBBER);
	draw(p->xmax, p->ymin, &childbb, D_RUBBER);
}

int db_notate(p)
DB_DEFLIST *p;			/* print out identifying information */
{

    if (p == NULL) {
    	printf("no component!\n");
	return(0);
    }

    switch (p->type) {
    case ARC:  /* arc definition */
	break;
    case CIRC:  /* circle definition */
	printf("   CIRC %d LL=%.2g,%.2g UR=%.2g,%.2g ", p->u.c->layer, p->xmin, p->ymin, p->xmax, p->ymax);
	db_print_opts(stdout, p->u.c->opts, CIRC_OPTS);
	printf("\n");
	break;
    case LINE:  /* line definition */
	printf("   LINE %d LL=%g,%g UR=%g,%g ", p->u.l->layer, p->xmin, p->ymin, p->xmax, p->ymax);
	db_print_opts(stdout, p->u.l->opts, LINE_OPTS);
	printf("\n");
	break;
    case OVAL:  /* oval definition */
	break;
    case POLY:  /* polygon definition */
	printf("   POLY %d LL=%g,%g UR=%g,%g ", p->u.p->layer, p->xmin, p->ymin, p->xmax, p->ymax);
	db_print_opts(stdout, p->u.p->opts, POLY_OPTS);
	printf("\n");
	break;
    case RECT:  /* rectangle definition */
	printf("   RECT %d LL=%g,%g UR=%g,%g ", p->u.r->layer, p->xmin, p->ymin, p->xmax, p->ymax);
	db_print_opts(stdout, p->u.r->opts, RECT_OPTS);
	printf("\n");
	break;
    case TEXT:  /* text and note definition */
	printf("   TEXT %d LL=%g,%g UR=%g,%g ", p->u.t->layer, p->xmin, p->ymin, p->xmax, p->ymax);
	db_print_opts(stdout, p->u.t->opts, TEXT_OPTS);
	printf(" \"%s\"\n", p->u.t->text);
	break;
    case INST:  /* instance call */
	printf("   INST %s LL=%g,%g UR=%g,%g ", p->u.i->name, p->xmin, p->ymin, p->xmax, p->ymax);
	db_print_opts(stdout, p->u.i->opts, INST_OPTS);
	printf("\n");
	break;
    default:
	eprintf("unknown record type: %d in db_notate\n", p->type);
	return(1);
	break;
    }
}

int db_highlight(p)
DB_DEFLIST *p;			/* component to display */
{
    BOUNDS childbb;
    childbb.init=0;

    if (p == NULL) {
    	printf("no component!\n");
	return(0);
    }

    childbb.init=0;

    switch (p->type) {
    case ARC:  /* arc definition */
	db_drawbounds(p);
	do_arc(p, &childbb, D_RUBBER);
	break;
    case CIRC:  /* circle definition */
	db_drawbounds(p);
	do_circ(p, &childbb, D_RUBBER);
	break;
    case LINE:  /* line definition */
	db_drawbounds(p);
	do_line(p, &childbb, D_RUBBER);
	break;
    case OVAL:  /* oval definition */
	db_drawbounds(p);
	do_oval(p, &childbb, D_RUBBER);
	break;
    case POLY:  /* polygon definition */
	db_drawbounds(p);
	do_poly(p, &childbb, D_RUBBER);
	break;
    case RECT:  /* rectangle definition */
	db_drawbounds(p);
	do_rect(p, &childbb, D_RUBBER);
	break;
    case TEXT:  /* text and note definition */
	db_drawbounds(p);
	do_text(p, &childbb, D_RUBBER);
	break;
    case INST:  /* instance call */
	db_drawbounds(p);
	break;
    default:
	eprintf("unknown record type: %d in db_highlight\n", p->type);
	return(1);
	break;
    }
}

int db_list(cell)
DB_TAB *cell;
{
    DB_DEFLIST *p;

    for (p=cell->dbhead; p!=(DB_DEFLIST *)0; p=p->next) {
	db_notate(p);
    }
}

int db_plot() {
    BOUNDS bb;
    bb.init=0;
    char buf[MAXFILENAME];

    if (currep == NULL) {
    	printf("not currently editing any rep!\n");
	return(0);
    }

    snprintf(buf, MAXFILENAME, "%s.plot", currep->name);
    printf("plotting to %s\n", buf);
    fflush(stdout);

    if((PLOT_FD=fopen(buf, "w+")) == 0) {
    	printf("db_plot: can't open plotfile: \"%s\"\n", buf);
    } else {
	X=0;				    /* turn X display off */
	db_render(currep, 0, &bb, D_NORM);  /* dump plot */
	X=1;				    /* turn X back on*/
    }
}


int db_render(cell, nest, bb, mode)
DB_TAB *cell;
int nest;	/* nesting level */
BOUNDS *bb;
int mode; 	/* drawing mode: one of D_NORM, D_RUBBER, D_BB, D_PICK */
{

    extern XFORM *global_transform;
    extern XFORM unity_transform;

    DB_DEFLIST *p;
    OPTS *op;
    XFORM *xp;
    XFORM *save_transform;
    extern int nestlevel;
    double optval;
    int debug=0;
    double xminsave;
    double xmaxsave;
    double yminsave;
    double ymaxsave;

    BOUNDS childbb;
    BOUNDS mybb;
    mybb.init=0; 

    if (nest == 0) {
	unity_transform.r11 = 1.0;
	unity_transform.r12 = 0.0;
	unity_transform.r21 = 0.0;
	unity_transform.r22 = 1.0;
	unity_transform.dx  = 0.0;
	unity_transform.dy  = 0.0;
        global_transform = &unity_transform;
    }

    if (!X && (nest == 0)) {	/* autoplot output */
	fprintf(PLOT_FD, "nogrid\n");
	fprintf(PLOT_FD, "isotropic\n");
	fprintf(PLOT_FD, "back\n");
    }

    if (nest == 0) {
	mybb.xmax=0.0;		/* initialize globals to 0,0 location*/
	mybb.xmin=0.0;		/* draw() routine will modify these */
	mybb.ymax=0.0;
	mybb.ymin=0.0;
	mybb.init++;
    }

    for (p=cell->dbhead; p!=(DB_DEFLIST *)0; p=p->next) {

	childbb.init=0;

	switch (p->type) {
        case ARC:  /* arc definition */
	    do_arc(p, &childbb, mode);
	    break;
        case CIRC:  /* circle definition */
	    do_circ(p, &childbb, mode);
	    break;
        case LINE:  /* line definition */
	    do_line(p, &childbb, mode);
	    break;
        case OVAL:  /* oval definition */
	    do_oval(p, &childbb, mode);
	    break;
        case POLY:  /* polygon definition */
	    do_poly(p, &childbb, mode);
	    if (debug) printf("poly bounds = %g,%g %g,%g\n",
		childbb.xmin, childbb.ymin, childbb.xmax, childbb.ymax);
	    break;
	case RECT:  /* rectangle definition */
	    do_rect(p, &childbb, mode);
	    break;
        case TEXT:  /* text and note definition */
	    do_text(p, &childbb, mode);
	    break;
        case INST:  /* recursive instance call */

	    if( !show_check_visible(INST,0)) {
	    	;
	    }

	    /* create a unit xform matrix */

	    xp = (XFORM *) emalloc(sizeof(XFORM));  
	    xp->r11 = 1.0;
	    xp->r12 = 0.0;
	    xp->r21 = 0.0;
	    xp->r22 = 1.0;
	    xp->dx  = 0.0;
	    xp->dy  = 0.0;

	    /* ADD [I] devicename [.cname] [:Mmirror] [:Rrot] [:Xscl]
	     *	   [:Yyxratio] [:Zslant] [:Snx ny xstep ystep]
	     *	   [{:F0 | :F1}romfile] coord   
	     */

	    /* create transformation matrix from options */

	    /* NOTE: To work properly, these transformations have to */
	    /* occur in the proper order, for example, rotation must come */
	    /* after slant transformation */

	    switch (p->u.i->opts->mirror) {
	    	case MIRROR_OFF:
		    break;
	    	case MIRROR_X:
		    xp->r22 *= -1.0;
		    break;
	    	case MIRROR_Y:
		    xp->r11 *= -1.0;
		    break;
	    	case MIRROR_XY:
		    xp->r11 *= -1.0;
		    xp->r22 *= -1.0;
		    break;
	    }

	    mat_scale(xp, p->u.i->opts->scale, p->u.i->opts->scale); 
	    mat_scale(xp, 1.0/p->u.i->opts->aspect_ratio, 1.0); 
	    mat_slant(xp,  p->u.i->opts->slant); 
	    mat_rotate(xp, p->u.i->opts->rotation);
	    xp->dx += p->u.i->x;
	    xp->dy += p->u.i->y;

	    save_transform = global_transform;
	    global_transform = compose(xp,global_transform);

	    if (nest >= nestlevel || !show_check_visible(INST,0)) {
		drawon = 0;
	    } else {
		drawon = 1;
	    }

	    /* instances are called by name, not by pointer */
	    /* otherwise, it is possible to PURGE a cell in */
	    /* memory and then reference a bad pointer, causing */
	    /* a crash... so instances are always accessed */
	    /* with a db_lookup() call */

	    childbb.init=0;
	    db_render(db_lookup(p->u.i->name), nest+1, &childbb, mode);

	    p->xmin = childbb.xmin;
	    p->xmax = childbb.xmax;
	    p->ymin = childbb.ymin;
	    p->ymax = childbb.ymax;

	    /* don't draw anything below nestlevel */
	    if (nest > nestlevel) {
		drawon = 0;
	    } else {
		drawon = 1;
	    }

            if (nest == nestlevel) { /* if at nestlevel, draw bounding box */
		set_layer(0,0);
		jump();
		    /* a square bounding box outline in white */
		    draw(childbb.xmax, childbb.ymax, &childbb, D_BB); 
		    draw(childbb.xmax, childbb.ymin, &childbb, D_BB);
		    draw(childbb.xmin, childbb.ymin, &childbb, D_BB);
		    draw(childbb.xmin, childbb.ymax, &childbb, D_BB);
		    draw(childbb.xmax, childbb.ymax, &childbb, D_BB);
		jump();
		    /* and diagonal lines from corner to corner */
		    draw(childbb.xmax, childbb.ymax, &childbb, D_BB);
		    draw(childbb.xmin, childbb.ymin, &childbb, D_BB);
		jump() ;
		    draw(childbb.xmin, childbb.ymax, &childbb, D_BB);
		    draw(childbb.xmax, childbb.ymin, &childbb, D_BB);
	    }

	    free(global_transform); free(xp);	
	    global_transform = save_transform;	/* set transform back */

	    break;
	default:
	    eprintf("unknown record type: %d in db_render\n", p->type);
	    return(1);
	    break;
	}

	p->xmin = childbb.xmin;
	p->xmax = childbb.xmax;
	p->ymin = childbb.ymin;
	p->ymax = childbb.ymax;

	/* now pass bounding box back */
	db_bounds_update(&mybb, &childbb);
    }

    db_bounds_update(bb, &mybb);
 
    if (nest == 0) { 
	jump();
	set_layer(12,0);
	draw(bb->xmin, bb->ymax, &mybb, D_BB);
	draw(bb->xmax, bb->ymax, &mybb, D_BB);
	draw(bb->xmax, bb->ymin, &mybb, D_BB);
	draw(bb->xmin, bb->ymin, &mybb, D_BB);
	draw(bb->xmin, bb->ymax, &mybb, D_BB);

	/* update cell and globals for db_bounds() */
	cell->minx =  bb->xmin;
	cell->maxx =  bb->xmax;
	cell->miny =  bb->ymin;
	cell->maxy =  bb->ymax;
    } 
}

/***************** low-level X, autoplot primitives ******************/

/* The "nseg" variable is used to convert draw(x,y) calls into 
 * x1,y1,x2,y2 line segments.  The rule is: jump() sets nseg to zero, and
 * draw() calls XDrawLine with the xold,yold,x,y only if nseg>0.  "nseg" is
 * declared global because it must be shared by both draw() and jump().
 * An alternative would have been to make all lines with a function like
 * drawline(x1,y1,x2,y2), but that would have been nearly 2x more 
 * inefficient when  drawing lines with more than two nodes (all shared nodes
 * get sent twice).  By using jump() and sending the points serially, we
 * have the option of improving the efficiency later by building a linked 
 * list and using XDrawLines() rather than XDrawLine().  
 * In addition, the draw()/jump() paradigm is used by
 * Bob Jewett's autoplot(1) program, so has a long history.
 */

static int nseg=0;

void draw(x, y, bb, mode)  /* draw x,y transformed by extern global xf */
NUM x,y;		   /* location in real coordinate space */
BOUNDS *bb;		   /* bounding box */
int mode;		   /* D_NORM=regular, D_RUBBER=rubberband, */
			   /* D_BB=bounding box, D_PICK=pick checking */
{
    extern int layer;
    extern XFORM *global_transform;	/* global for efficiency */
    NUM xx, yy;
    static NUM xxold, yyold;
    int debug=0;
    double x1, y1, x2, y2, xp, yp;

    if (mode == D_BB) {	
	/* skip screen transform to draw bounding */
	/* boxes in nested hierarchies, also don't */
	/* update bounding boxes (sort of a free */
	/* drawing mode in absolute coordinates) */
	if (debug) printf("in draw, mode=%d\n", mode);
	xx = x;
	yy = y;
    } else if (mode==D_NORM || mode==D_RUBBER || mode==D_PICK) {
	/* compute transformed coordinates */
	xx = x*global_transform->r11 + 
	    y*global_transform->r21 + global_transform->dx;
	yy = x*global_transform->r12 + 
	    y*global_transform->r22 + global_transform->dy;
	if (mode == D_PICK) {
	    /* transform the pick point into screen coords */
	    xp = bb->xmin;
	    yp = bb->ymin;
	    R_to_V(&xp, &yp);
	}
	if (mode == D_NORM) {
	    /* merge bounding boxes */
	    if(bb->init == 0) {
		bb->xmin=bb->xmax=xx;
		bb->ymin=bb->ymax=yy;
		bb->init++;
	    } else {
		bb->xmax = max(xx,bb->xmax);
		bb->xmin = min(xx,bb->xmin);
		bb->ymax = max(yy,bb->ymax);
		bb->ymin = min(yy,bb->ymin);
	    }
	} 
    } else {
    	printf("bad mode: %d in draw() function\n", mode);
	exit(1);
    }

    /* Before I added the clip() routine, there was a problem with  */
    /* extreme zooming on a large drawing.  When the transformed points */
    /* exceeded ~2^18, the X11 routines would overflow and the improperly */
    /* clipped lines would alias back onto the screen...  Adding the */
    /* Cohen-Sutherland clipper completely eliminated this problem. */
    /* I couldn't find the dynamic range of Xlib documented anywhere. */
    /* I was surprised that the range was not equal to the full 2^32 bit */
    /* range of an unsigned int (RCW) */

    if (X) {

	R_to_V(&xx, &yy);	/* convert to screen coordinates */ 

	if (nseg && clip(xxold, yyold, xx, yy, &x1, &y1, &x2, &y2)) {
	    if (mode == D_PICK) {   /* no drawing just pick checking */
		/* a bit of a hack, but we overload the bb structure */
		/* to transmit the pick points ... pick comes in on */
		/* xmin, ymin and a boolean 0,1 goes back on xmax */
		bb->xmax += (double) pickcheck(xxold, yyold, xx, yy, xp, yp, 3.0);
	    } else if (mode == D_RUBBER) { /* xor rubber band drawing */
		if (showon && drawon) {
		    xwin_xor_line((int)x1,(int)y1,(int)x2,(int)y2);
                }
	    } else {		/* regular drawing */
		if (showon && drawon) {
		    xwin_draw_line((int)x1,(int)y1,(int)x2,(int)y2);
		    /*
		    if (boundslevel) {
			draw_pick_bound(x1, y1, x2, y2, boundslevel);
		    }
		    */
 		}
	    }
	}
	xxold=xx;
	yyold=yy;
	nseg++;
    } else {
	/* autoplot output */
	if (showon && drawon) fprintf(PLOT_FD, "%4.6g %4.6g\n", xx,yy);	
    }
}

/* pickcheck(): called with a line segment L=(x1,y1),
 * (x2, y2) a point (x3, y3) and a fuzz factor eps, returns 1 if p3 is
 * within a rectangular bounding box eps larger than L, 0 otherwise. 
 *
 * ...a bit of a hack, but we overload the bb structure to transmit the
 * pick points x3,y3 ...  pick comes in on x3=bb->xmin, y3=bb->ymin and
 * a boolean 0,1 goes back on bb->xmax
 */

int pickcheck(x1,y1,x2,y2,x3,y3,eps)
double x1,y1,x2,y2,x3,y3;
double eps;
{
    double xn,yn, xp,yp, xr,yr;
    double d,s,c;

    /* normalize everything to x1,y1 */
    xn = x2-x1; yn = y2-y1;
    xp = x3-x1; yp = y3-y1;

    /* rotate pick point to vertical */
    d = sqrt(xn*xn+yn*yn);
    s = xn/d;		/* sin() */
    c = yn/d;		/* cos() */
    xr = xp*c - yp*s;

    /* test against a rectangle */
    if (xr >= -eps && xr <= eps) {
	/* save some time by computing */
	/* yr only after testing x */
	yr = yp*c + xp*s;
	if (yr >= -eps  && yr <= d+eps) {
	    return(1);
	}
    }
    return(0);
}

/* Cohen-Sutherland 2-D Clipping Algorithm */
/* See: Foley & Van Dam, p146-148 */
/* called with xy12 and rewrites xyc12 with clipped values, */
/* returning 0 if rejected and 1 if accepted */

int clip(x1, y1, x2, y2, xc1, yc1, xc2, yc2)
double x1, y1, x2, y2;
double *xc1, *yc1, *xc2, *yc2;
{

    double vp_xmin=0;
    double vp_ymin=0;
    double vp_xmax=(double) width;
    double vp_ymax=(double) height;
    int debug=0;
    int done=0;
    int accept=0;
    int code1=0;
    int code2=0;
    double tmp;

    if (debug) printf("canonicalized: %g,%g %g,%g\n", x1, y1, x2, y2);

    while (!done) {
        /* compute "outcodes" */
	code1=0;
	if((vp_ymax-y1) < 0) code1 += 1;
	if((y1-vp_ymin) < 0) code1 += 2;
	if((vp_xmax-x1) < 0) code1 += 4;
	if((x1-vp_xmin) < 0) code1 += 8;

	code2=0;
	if((vp_ymax-y2) < 0) code2 += 1;
	if((y2-vp_ymin) < 0) code2 += 2;
	if((vp_xmax-x2) < 0) code2 += 4;
	if((x2-vp_xmin) < 0) code2 += 8;

	if (debug) printf("code1: %d, code2: %d\n", code1, code2);

    	if (code1 & code2) {
	    if (debug) printf("trivial reject\n");
	    done++;	/* trivial reject */
	} else { 
	    if (accept = !((code1 | code2))) {
		if (debug) printf("trivial accept\n");
	    	done++;
	    } else {
	        if (!code1) { /* x1,y1 inside box, so SWAP */
		    if (debug) printf("swapping\n");
		    tmp=y1; y1=y2; y2=tmp;
		    tmp=x1; x1=x2; x2=tmp;
		    tmp=code1; code1=code2; code2=tmp;
		}

		if (debug) printf("preclip: %g,%g %g,%g\n", x1, y1, x2, y2);
		if (code1 & 1) {		/* divide line at top */
			x1 = x1 + (x2-x1)*(vp_ymax-y1)/(y2-y1);
                        y1 = vp_ymax;
		} else if (code1 & 2) {	/* divide line at bot */
			x1 = x1 + (x2-x1)*(vp_ymin-y1)/(y2-y1);
                        y1 = vp_ymin;
		} else if (code1 & 4) {	/* divide line at right */
			y1 = y1 + (y2-y1)*(vp_xmax-x1)/(x2-x1);
                        x1 = vp_xmax;
		} else if (code1 & 8) {	/* divide line at left */
			y1 = y1 + (y2-y1)*(vp_xmin-x1)/(x2-x1);
                        x1 = vp_xmin;
                }
		if (debug) printf("after: %g,%g %g,%g\n", x1, y1, x2, y2);
	    }
	}
    }

    if (accept) {
    	*xc1 = x1;
    	*yc1 = y1;
    	*xc2 = x2;
    	*yc2 = y2;
	return(1);
    }
    return(0);
}


int draw_pick_bound(NUM x1, NUM y1, NUM x2, NUM y2, int boundslevel) {
    NUM tmp;
    NUM e = boundslevel;	/* pixel fuzz width */

    if (x2 < x1) {		/* canonicalize the extent */
	tmp = x2; x2 = x1; x1 = tmp;
    }
    if (y2 < y1) {
	tmp = y2; y2 = y1; y1 = tmp;
    }

    xwin_draw_line((int)(x1-e),(int)(y1-e),(int)(x1-e),(int)(y2+e));
    xwin_draw_line((int)(x1-e),(int)(y2+e),(int)(x2+e),(int)(y2+e));
    xwin_draw_line((int)(x2+e),(int)(y2+e),(int)(x2+e),(int)(y1-e));
    xwin_draw_line((int)(x2+e),(int)(y1-e),(int)(x1-e),(int)(y1-e));

}


void jump(void) 
{
    if (X) {
	nseg=0;  		/* for X */
    } else {
	if (drawon) fprintf(PLOT_FD, "jump\n");  	/* autoplot */
    }
}

void set_layer(lnum, comp)
int lnum;	/* layer number */
int comp;	/* component type */
{
    extern int layer;
    extern int showon;
    layer=lnum;

    if (comp) {
	showon = show_check_visible(comp, lnum);
    } else {
        showon=1;
    }

    if (X) {
	xwin_set_pen((lnum%8));		/* for X */
    } else {
	if (drawon) fprintf(PLOT_FD, "pen %d\n", (lnum%8));	/* autoplot */
    }
    set_line((((int)(lnum/8))%5));
}

void set_line(lnum)
int lnum;
{
    if (X) {
	xwin_set_line((lnum%5));		/* for X */
    } else {
	if (drawon) fprintf(PLOT_FD, "line %d\n", (lnum%5)+1);	/* autoplot */
    }
}

double max(a,b) 
double a, b;
{
    return(a>=b?a:b);
}

double min(a,b) 
double a, b;
{
    return(a<=b?a:b);
}

