
#include "db.h"		/* hierarchical database routines */
#include "eprintf.h"	/* error reporting functions */

#include <stdio.h>
#include <math.h>
#include "token.h"
#include "xwin.h"       /* for snapxy() */

#define EPS 1e-6

#define CELL 0		/* modes for db_def_print() */
#define ARCHIVE 1

/* master symbol table pointers */
static DB_TAB *HEAD = 0;
static DB_TAB *TAIL = 0;

COORDS *first_pair, *last_pair; 
OPTS   *first_opt, *last_opt;

/* global drawing variables */
XFORM unity_transform;
XFORM *global_transform = &unity_transform;  /* global xform matrix */
NUM xmax, ymax; 	   /* globals for finding bounding boxes */
NUM xmin, ymin;
double max(), min();
int drawon=1;		  /* 0 = dont draw, 1 = draw */
int nestlevel=9;
int X=1;		  /* 1 = draw to X, 0 = emit autoplot commands */

/* routines for expanding db entries into vectors */
void do_arc(),  do_circ(), do_line();
void do_oval(), do_poly(), do_rect(), do_text();

/* matrix routines */
XFORM *compose();
void mat_rotate();
void mat_scale();
void mat_slant();
void mat_print();

/* primitives for outputting autoplot,X primitives */
void draw();
void jump();
void set_pen();
void set_line();

/********************************************************/

DB_TAB *db_lookup(char *name)           /* find name in db */
{
    DB_TAB *sp;
    int debug=0;

    if (debug) printf("calling db_lookup with %s: ", name);
    if (HEAD != (DB_TAB *)0) {
	for (sp=HEAD; sp!=(DB_TAB *)0; sp=sp->next) {
	    if (strcmp(sp->name, name)==0) {
		if (debug) printf("found!\n");
		return(sp);
	    }
	}
    }
    if (debug) printf("not found!\n");
    return(0);               	/* 0 ==> not found */
} 

DB_TAB *db_install(s)		/* install s in db */
char *s;
{
    DB_TAB *sp;

    /* this is written to ensure that a db_print is */
    /* not in reversed order...  New structures are */
    /* added at the end of the list using TAIL      */

    sp = (DB_TAB *) emalloc(sizeof(struct db_tab));
    sp->name = emalloc(strlen(s)+1); /* +1 for '\0' */
    strcpy(sp->name, s);

    sp->next   = (DB_TAB *) 0; 
    sp->prev   = (DB_TAB *) 0; 
    sp->dbhead = (struct db_deflist *) 0; 
    sp->dbtail = (struct db_deflist *) 0; 

    sp->minx = 0.0; 
    sp->miny = 0.0; 
    sp->maxx = 0.0; 
    sp->maxy = 0.0; 

    sp->grid_xd = 10.0;
    sp->grid_yd = 10.0;
    sp->grid_xs = 1.0;
    sp->grid_ys = 1.0;
    sp->grid_xo = 0.0;
    sp->grid_yo = 0.0;

    sp->logical_level=1;
    sp->lock_angle=0.0;
    sp->modified = 0;
    sp->flag = 0;

    if (HEAD ==(DB_TAB *) 0) {	/* first element */
	HEAD = TAIL = sp;
    } else {
	sp->prev = TAIL;
	if (TAIL != (DB_TAB *) 0) {
	    TAIL->next = sp;		/* current tail points to new */
	}
	TAIL = sp;			/* and tail becomes new */
    }

    return (sp);
}

int db_purge(s)			/* remove all definitions for s */
char *s;
{
    DB_TAB *sp;
    DB_DEFLIST *p;
    COORDS *coords;
    COORDS *ncoords;
    int debug=0;

    if ((sp=db_lookup(s)) == 0) { 	/* not in memory */
    	return(1);
    } else {				/* unlink s */
        if (debug) printf("db_purge: unlinking %s from db\n", s);

        if (currep == sp) {
	    currep = NULL;
	    need_redraw++;
	}

	if (HEAD==sp) {
	    HEAD=sp->next;
	}

	if (TAIL==sp) {
	    TAIL=sp->prev;
	}

	if (sp->prev != (DB_TAB *) 0) {
	    sp->prev->next = sp->next;
        }
	if (sp->next != (DB_TAB *) 0) {
	    sp->next->prev = sp->prev;
	}
    }

    for (p=sp->dbhead; p!=(struct db_deflist *)0; p=p->next) {
	switch (p->type) {

        case ARC:  /* arc definition */
	    if (debug) printf("db_purge: freeing arc\n");
	    free(p->u.a->opts);
	    free(p->u.a);
	    free(p);
	    break;

        case CIRC:  /* circle definition */
	    if (debug) printf("db_purge: freeing circle\n");
	    free(p->u.c->opts);
	    free(p->u.c);
	    free(p);
	    break;

        case LINE:  /* line definition */

	    if (debug) printf("db_purge: freeing line\n");
	    coords = p->u.l->coords;
	    while(coords != NULL) {
		ncoords = coords->next;
		free(coords);	
		coords=ncoords;
	    }
	    free(p->u.l->opts);
	    free(p->u.l);
	    free(p);
	    break;

        case OVAL:  /* oval definition */

	    if (debug) printf("db_purge: freeing oval\n");
	    free(p->u.o->opts);
	    free(p->u.o);
	    free(p);
	    break;

        case POLY:  /* polygon definition */

	    if (debug) printf("db_purge: freeing poly\n");
	    coords = p->u.p->coords;
	    while(coords != NULL) {
		ncoords = coords->next;
		free(coords);
		coords=ncoords;
	    }
	    free(p->u.p->opts);
	    free(p->u.p);
	    free(p);
	    break;

	case RECT:  /* rectangle definition */

	    if (debug) printf("db_purge: freeing rect\n");
	    free(p->u.r->opts);
	    free(p->u.r);
	    free(p);
	    break;

        case TEXT:  /* text definition */

	    if (debug) printf("db_purge: freeing text\n");
	    free(p->u.t->opts);
	    free(p->u.t->text);
	    free(p->u.t);
	    free(p);
	    break;

        case INST:  /* instance call */

	    if (debug) printf("db_purge: freeing instance call: %s\n",
	    	p->u.i->name);
	    free(p->u.i->name);
	    free(p->u.i->opts);
	    free(p->u.i);
	    free(p);
	    break;

	default:
	    eprintf("unknown record type (%d) in db_def_print\n", p->type );
	    break;
	}

    }
    
    free(sp);

}

/* save a non-recursive archive of single cell to a file called "cell.d" */

int db_save(sp)           	/* print db */
DB_TAB *sp;
{

    FILE *fp;
    char buf[MAXFILENAME];
    int err=0;

    mkdir("./cells", 0777);
    snprintf(buf, MAXFILENAME, "./cells/%s.d", sp->name);
    err+=((fp = fopen(buf, "w+")) == 0); 

    db_def_print(fp, sp, CELL); 
    err+=(fclose(fp) != 0);

    return(err);
}


int db_def_archive(sp) 
DB_TAB *sp;
{
    FILE *fp;
    char buf[MAXFILENAME];
    int err=0;

    mkdir("./cells", 0777);
    snprintf(buf, MAXFILENAME, "./cells/%s_I", sp->name);
    err+=((fp = fopen(buf, "w+")) == 0); 

    fp = fopen(buf, "w+"); 

    db_def_arch_recurse(fp,sp);
    db_def_print(fp, sp, ARCHIVE);

    err+=(fclose(fp) != 0);

    return(err);
}

int db_def_arch_recurse(fp,sp) 
FILE *fp;
DB_TAB *sp;
{
    DB_TAB *dp;
    DB_DEFLIST *p; 

    /* clear out all flag bits in cell definition headers */
    for (dp=HEAD; dp!=(DB_TAB *)0; dp=dp->next) {
	sp->flag=0;
    }

    for (p=sp->dbhead; p!=(struct db_deflist *)0; p=p->next) {

	if (p->type == INST && !(db_lookup(p->u.i->name)->flag)) {

	    db_def_arch_recurse(fp, db_lookup(p->u.i->name)); 	/* recurse */
	    ((db_lookup(p->u.i->name))->flag)++;
	    db_def_print(fp, db_lookup(p->u.i->name), ARCHIVE);
	}
    }
    return(0); 	
}


int db_def_print(fp, dp, mode) 
FILE *fp;
DB_TAB *dp;
int mode;
{
    COORDS *coords;
    DB_DEFLIST *p; 
    int i;

    if (mode == ARCHIVE) {
	fprintf(fp, "PURGE %s;\n", dp->name); 
	fprintf(fp, "EDIT %s;\n", dp->name);
    }

    fprintf(fp, "LOCK %g;\n", dp->lock_angle);
    fprintf(fp, "LEVEL %d;\n", dp->logical_level);
    fprintf(fp, "GRID %g %g %g %g %g %g;\n", 
    	dp->grid_xd, dp->grid_yd,
	dp->grid_xs, dp->grid_ys,
	dp->grid_xo, dp->grid_yo);

    fprintf(fp, "WIN %g,%g %g,%g;\n",
    	dp->minx, dp->miny, dp->maxx, dp->maxy);

    for (p=dp->dbhead; p!=(struct db_deflist *)0; p=p->next) {
	switch (p->type) {
        case ARC:  /* arc definition */
	    fprintf(fp, "#add ARC not inplemented yet\n");
	    break;

        case CIRC:  /* circle definition */

	    fprintf(fp, "ADD C%d ", p->u.c->layer);
	    db_print_opts(fp, p->u.c->opts, "WR");

	    fprintf(fp, "%g,%g %g,%g;\n",
		p->u.c->x1,
		p->u.c->y1,
		p->u.c->x2,
		p->u.c->y2
	    );
	    break;

        case LINE:  /* line definition */

	    fprintf(fp, "ADD L%d ", p->u.l->layer);
	    db_print_opts(fp, p->u.l->opts, "W");

	    i=1;
	    coords = p->u.l->coords;
	    while(coords != NULL) {
		fprintf(fp, " %g,%g", coords->coord.x, coords->coord.y);
		if ((coords = coords->next) != NULL && (++i)%7 == 0) {
		    fprintf(fp,"\n");
		}
	    }
	    fprintf(fp, ";\n");

	    break;

        case OVAL:  /* oval definition */

	    fprintf(fp, "#add OVAL not inplemented yet\n");
	    break;

        case POLY:  /* polygon definition */

	    fprintf(fp, "ADD P%d", p->u.p->layer);
	    db_print_opts(fp, p->u.p->opts, "W");

	    i=1;
	    coords = p->u.p->coords;
	    while(coords != NULL) {
		fprintf(fp, " %g,%g", coords->coord.x, coords->coord.y);
		if ((coords = coords->next) != NULL && (++i)%7 == 0) {
		    fprintf(fp,"\n");
		}
	    }
	    fprintf(fp, ";\n");

	    break;

	case RECT:  /* rectangle definition */

	    fprintf(fp, "ADD R%d ", p->u.r->layer);
	    db_print_opts(fp, p->u.r->opts, "W");

	    fprintf(fp, "%g,%g %g,%g;\n",
		p->u.r->x1,
		p->u.r->y1,
		p->u.r->x2,
		p->u.r->y2
	    );
	    break;

        case TEXT:  /* text and note definition */

	    if (p->u.t->opts->font_num%2) {
		fprintf(fp, "ADD T%d ", p->u.t->layer); 	/* 1,3,5.. = text */
		db_print_opts(fp, p->u.t->opts, "MNRYZF");
	    } else {
		fprintf(fp, "ADD N%d ", p->u.t->layer); 	/* 0,2,4.. = note */
		db_print_opts(fp, p->u.t->opts, "MNRYZF");
	    }

	    fprintf(fp, "\"%s\" %g,%g;\n",
		p->u.t->text,
		p->u.t->x,
		p->u.t->y
	    );
	    break;

        case INST:  /* instance call */
	    fprintf(fp, "ADD %s ", p->u.i->name);
	    db_print_opts(fp, p->u.i->opts, "MRXYZ");
	    fprintf(fp, "%g,%g;\n",
		p->u.i->x,
		p->u.i->y
	    );
	    break;

	default:
	    eprintf("unknown record type (%d) in db_def_print\n", p->type );
	    break;
	}

    }

    if (mode == ARCHIVE) {
	fprintf(fp, "SAVE;\n");
	fprintf(fp, "EXIT;\n\n");
    }
}


int db_add_arc(cell, layer, opts, x1,y1,x2,y2,x3,y3) 
DB_TAB *cell;
int layer;
OPTS *opts;
NUM x1,y1,x2,y2,x3,y3;
{
    struct db_arc *ap;
    struct db_deflist *dp;

    cell->modified++;
 
    ap = (struct db_arc *) emalloc(sizeof(struct db_arc));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;

    /* add definition at *end* of list */
    if (cell->dbhead == NULL) {
	cell->dbhead = cell->dbtail = dp;
    } else {
	cell->dbtail->next = dp;
	cell->dbtail = dp;
    }

    dp->u.a = ap;
    dp->type = ARC;

    ap->layer=layer;
    ap->opts=opts;
    ap->x1=x1;
    ap->y1=y1;
    ap->x2=x2;
    ap->y2=y2;
    ap->x3=x3;
    ap->y3=y3;

    return(0);
}

int db_add_circ(cell, layer, opts, x1,y1,x2,y2)
DB_TAB *cell;
int layer;
OPTS *opts;
NUM x1,y1,x2,y2;
{
    struct db_circ *cp;
    struct db_deflist *dp;
 
    cp = (struct db_circ *) emalloc(sizeof(struct db_circ));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;

    /* add definition at *end* of list */
    if (cell->dbhead == NULL) {
	cell->dbhead = cell->dbtail = dp;
    } else {
	cell->dbtail->next = dp;
	cell->dbtail = dp;
    }

    dp->u.c = cp;
    dp->type = CIRC;

    cp->layer=layer;
    cp->opts=opts;
    cp->x1=x1;
    cp->y1=y1;
    cp->x2=x2;
    cp->y2=y2;

    return(0);
}

int db_add_line(cell, layer, opts, coords)
DB_TAB *cell;
int layer;
OPTS *opts;
COORDS *coords;
{
    struct db_line *lp;
    struct db_deflist *dp;

    cell->modified++;
 
    lp = (struct db_line *) emalloc(sizeof(struct db_line));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;

    /* add definition at *end* of list */
    if (cell->dbhead == NULL) {
	cell->dbhead = cell->dbtail = dp;
    } else {
	cell->dbtail->next = dp;
	cell->dbtail = dp;
    }

    dp->u.l = lp;
    dp->type = LINE;

    lp->layer=layer;
    lp->opts=opts;
    lp->coords=coords;

    return(0);
}

int db_add_oval(cell, layer, opts, x1,y1,x2,y2,x3,y3) 
DB_TAB *cell;
int layer;
OPTS *opts;
NUM x1,y1, x2,y2, x3,y3;
{
    struct db_oval *op;
    struct db_deflist *dp;

    cell->modified++;
 
    op = (struct db_oval *) emalloc(sizeof(struct db_oval));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;

    /* add definition at *end* of list */
    if (cell->dbhead == NULL) {
	cell->dbhead = cell->dbtail = dp;
    } else {
	cell->dbtail->next = dp;
	cell->dbtail = dp;
    }

    dp->u.o = op;
    dp->type = OVAL;

    op->layer=layer;
    op->opts=opts;
    op->x1=x1;
    op->y1=y1;
    op->x2=x2;
    op->y2=y2;
    op->x3=x3;
    op->y3=y3;

    return(0);
}

int db_add_poly(cell, layer, opts, coords)
DB_TAB *cell;
int layer;
OPTS *opts;
COORDS *coords;
{
    struct db_poly *pp;
    struct db_deflist *dp;
    int debug = 0;

    cell->modified++;
 
    pp = (struct db_poly *) emalloc(sizeof(struct db_poly));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;

    /* add definition at *end* of list */
    if (cell->dbhead == NULL) {
	cell->dbhead = cell->dbtail = dp;
    } else {
	cell->dbtail->next = dp;
	cell->dbtail = dp;
    }

    dp->u.p = pp;
    dp->type = POLY;

    if (debug) printf("db_add_poly: setting poly layer %d\n", layer);
    pp->layer=layer;
    pp->opts=opts;
    pp->coords=coords;

    return(0);
}

int db_add_rect(cell, layer, opts, x1,y1,x2,y2)
DB_TAB *cell;
int layer;
OPTS *opts;
NUM x1,y1,x2,y2;
{
    struct db_rect *rp;
    struct db_deflist *dp;

    cell->modified++;
 
    rp = (struct db_rect *) emalloc(sizeof(struct db_rect));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;

    /* add definition at *end* of list */
    if (cell->dbhead == NULL) {
	cell->dbhead = cell->dbtail = dp;
    } else {
	cell->dbtail->next = dp;
	cell->dbtail = dp;
    }

    dp->u.r = rp;
    dp->type = RECT;

    rp->layer=layer;
    rp->x1=x1;
    rp->y1=y1;
    rp->x2=x2;
    rp->y2=y2;
    rp->opts = opts;

    return(0);
}

int db_add_text(cell, layer, opts, string ,x,y) 
DB_TAB *cell;
int layer;
OPTS *opts;
char *string;
NUM x,y;
{
    struct db_text *tp;
    struct db_deflist *dp;

    cell->modified++;
 
    tp = (struct db_text *) emalloc(sizeof(struct db_text));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;

    /* add definition at *end* of list */
    if (cell->dbhead == NULL) {
	cell->dbhead = cell->dbtail = dp;
    } else {
	cell->dbtail->next = dp;
	cell->dbtail = dp;
    }

    dp->u.t = tp;
    dp->type = TEXT;

    tp->layer=layer;
    tp->text=string;
    tp->opts=opts;
    tp->x=x;
    tp->y=y;

    return(0);
}

int db_add_inst(cell, subcell, opts, x, y)
DB_TAB *cell;
DB_TAB *subcell;
OPTS *opts;
NUM x,y;
{
    struct db_inst *ip;
    struct db_deflist *dp;
 
    cell->modified++;

    ip = (struct db_inst *) emalloc(sizeof(struct db_inst));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;

    /* add definition at *end* of list */
    if (cell->dbhead == NULL) {
	cell->dbhead = cell->dbtail = dp;
    } else {
	cell->dbtail->next = dp;
	cell->dbtail = dp;
    }

    dp->u.i = ip;
    dp->type = INST;

    /* ip->def=subcell; */
    ip->name=strsave(subcell->name);

    ip->x=x;
    ip->y=y;
    ip->opts=opts;
}

/*
main () { 	

    DB_TAB *currep, *oldrep;

    currep = db_install("1foo");
    currep = db_install("2xyz");
	db_add_rect(currep,1,2.0,3.0,4.0,5.0);
	db_add_rect(currep,6,7.0,8.0,9.0,10.0);
    oldrep = currep;
    currep = db_install("3abc");
	db_add_rect(currep,11,12.0,13.0,14.0,15.0);
	db_add_inst(currep,oldrep,16.0,17.0);
    db_print();
}
*/


COORDS *pair_alloc(p)
PAIR p;
{	
    COORDS *temp;
    temp = (COORDS *) emalloc(sizeof(COORDS));
    temp->coord = p;
    temp->next = NULL;
    return(temp);
}


void append_pair(p)
PAIR p;
{
    last_pair->next = pair_alloc(p);
    last_pair = last_pair->next;
}

void discard_pairs()
{	
    COORDS *temp;

    while(first_pair != NULL) {
	temp = first_pair;
	first_pair = first_pair->next;
	free((char *)temp);
    }
    last_pair = NULL;
}		

OPTS *opt_copy(OPTS *opts)
{
    OPTS *tmp;
    tmp = (OPTS *) emalloc(sizeof(OPTS));
    
    /* set defaults */
    tmp->font_size = opts->font_size;
    tmp->font_num = opts->font_num;
    tmp->mirror = opts->mirror;
    tmp->rotation = opts->rotation;
    tmp->width = opts->width;
    tmp->aspect_ratio = opts->aspect_ratio;
    tmp->scale = opts->scale;
    tmp->slant = opts->slant;
    tmp->cname = opts->cname;
    tmp->sname = opts->sname;

    return(tmp);
}     

OPTS *opt_set_defaults(OPTS *opts)
{
    opts->font_size = 10.0;       /* :F<font_size> */
    opts->mirror = MIRROR_OFF;    /* :M<x,xy,y>    */
    opts->font_num=0;		  /* :N<font_num> */
    opts->rotation = 0.0;         /* :R<rotation,resolution> */
    opts->width = 0.0;            /* :W<width> */
    opts->aspect_ratio = 1.0;     /* :Y<yx_aspect_ratio> */
    opts->scale=1.0;              /* :X<scale> */
    opts->slant=0.0;              /* :Z<slant_degrees> */
    opts->cname= (char *) NULL;   /* component name */
    opts->sname= (char *) NULL;   /* signal name */
}

OPTS *opt_create()
{
    OPTS *tmp;
    tmp = (OPTS *) emalloc(sizeof(OPTS));
    
    opt_set_defaults(tmp);

    return(tmp);
}     

int db_print_opts(fp, popt, validopts) /* print options */
FILE *fp;
OPTS *popt;
/* validopts is a string which sets which options are printed out */
/* an example for a line is "W", text uses "MNRYZF" */
char *validopts;
{
    char *p;

    if (popt->cname != NULL) {
    	fprintf(fp, "%s ", popt->cname);
    }

    if (popt->sname != NULL) {
    	fprintf(fp, "%s ", popt->sname);
    }

    for (p=validopts; *p != '\0'; p++) {
    	switch(toupper(*p)) {
	    case 'F':
		fprintf(fp, ":F%g ", popt->font_size);
	    	break;
	    case 'M':
		switch(popt->mirror) {
		    case MIRROR_OFF:
		    	break;
		    case MIRROR_X:
	    		fprintf(fp, ":MX ");
			break;
		    case MIRROR_Y:
	    		fprintf(fp, ":MY ");
			break;
		    case MIRROR_XY:
	    		fprintf(fp, ":MXY ");
			break;
		    default:
	    		weprintf("invalid mirror case\n");
		    	break;	
		}
	    	break;
	    case 'N':
		if (popt->font_num != 0.0 && popt->font_num != 1.0) {
		    fprintf(fp, ":N%g ", popt->font_num);
		}
	    	break;
	    case 'R':
		if (popt->rotation != 0.0) {
		    fprintf(fp, ":R%g ", popt->rotation);
		}
	    	break;
	    case 'W':
		if (popt->width != 0.0) {
		    fprintf(fp, ":W%g ", popt->width);
		}
	    	break;
	    case 'X':
		if (popt->scale != 1.0) {
		    fprintf(fp, ":X%g ", popt->scale);
		}
	    	break;
	    case 'Y':
		if (popt->aspect_ratio != 1.0) {
		    fprintf(fp, ":Y%g ", popt->aspect_ratio);
		}
	    	break;
	    case 'Z':
		if (popt->slant != 0.0) {
		    fprintf(fp, ":Z%g ", popt->slant);
		}
	    	break;
	    default:
		weprintf("invalid option %c\n", *p); 
	    	break;
	}
    }
}


char *strsave(s)   /* save string s somewhere */
char *s;
{
    char *p;

    if ((p = (char *) emalloc(strlen(s)+1)) != NULL)
	strcpy(p,s);
    return(p);
}

/********************************************************/
/* rendering routine */
/* db_render() sets globals xmax, ymax, xmin, ymin;
/* db_bounds() accesses them for use in com_win();
/********************************************************/

/*

	Here's the logic:

	xwin.c exports a function R_to_V which converts real coordinates
	into pixel coordinates.  

	in xwin: db_render();
	...

	db_render() {
	     switch(INST_TYPE) {
		 ...
		 case PRIMITIVES:
		     draw() - using global_transform
		     break;
		 ...
		 case INST:  - a recursive instance call

                     !! SAVE = global_xform
		     global_transform = compose(inst_xform, transform)
		     db_render();
		     free(global_transform);
		     free(inst_xform);
		     global_transform = SAVE;
		     break;
	    }
	}

	draw(x,y) {
	    R_to_V(&x, &y);
	    call XWIN_draw (x,y);
	}
*/

void db_bounds(xxmin, yymin, xxmax, yymax)
NUM *xxmin;
NUM *yymin;
NUM *xxmax;
NUM *yymax;
{
    *xxmin = xmin;
    *yymin = ymin;
    *xxmax = xmax;
    *yymax = ymax;
    /* snapxy(xxmin, yymin); */
    /* snapxy(xxmax, yymax); */

}

void db_set_nest(nest) 
int nest;
{
    extern int nestlevel;
    nestlevel = nest;
}


int db_render(cell, nest, mode)
DB_TAB *cell;
int nest;	/* nesting level */
int mode; 	/* 0=regular rendering, 1=xor rubberband */
{
    DB_DEFLIST *p;
    OPTS *op;
    XFORM *xp, *xa;
    extern XFORM *global_transform;
    extern XFORM unity_transform;
    XFORM *save_transform;
    extern int nestlevel;
    double optval;
    int debug=0;
    double xminsave;
    double xmaxsave;
    double yminsave;
    double ymaxsave;

    if (nest == 0) {
	unity_transform.r11 = 1.0;
	unity_transform.r12 = 0.0;
	unity_transform.r21 = 0.0;
	unity_transform.r22 = 1.0;
	unity_transform.dx  = 0.0;
	unity_transform.dy  = 0.0;
        global_transform = &unity_transform;
    }

    if (debug) printf ("#rendering %s at level %d\n", cell->name, nest); 

    if (!X && (nest == 0)) {	/* autoplot output */
	printf("nogrid\n");
	printf("isotropic\n");
	printf("back\n");
    }

    if (nest == 0) {
        if (debug) printf("initializing global bounding box to 0\n");
	xmax=0.0;		/* initialize globals to 0,0 location*/
	xmin=0.0;		/* draw() routine will modify these */
	ymax=0.0;
	ymin=0.0;
    }

    if (debug) printf("bounds = %g %g %g %g\n", xmin, xmax, ymin, ymax);

    for (p=cell->dbhead; p!=(DB_DEFLIST *)0; p=p->next) {
	switch (p->type) {
        case ARC:  /* arc definition */
	    do_arc(p, mode);
	    break;
        case CIRC:  /* circle definition */
	    do_circ(p, mode);
	    break;
        case LINE:  /* line definition */
	    do_line(p, mode);
	    break;
        case OVAL:  /* oval definition */
	    do_oval(p, mode);
	    break;
        case POLY:  /* polygon definition */
	    do_poly(p, mode);
	    break;
	case RECT:  /* rectangle definition */
	    do_rect(p, mode);
	    break;
        case TEXT:  /* text and note definition */
	    do_text(p, mode);
	    break;
        case INST:  /* recursive instance call */

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
	    /* after slant transformation or else it wont work properly */

	    /* FIXME: inst calls should be by name, not by pointer */
	    /* it is possible to PURGE a cell in memory and then try */
	    /* to reference a bad pointer, causing a crash... For this */
	    /* reason, instances are called by name, and the pointer */
	    /* is always accessed with a db_lookup() call */

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

            /* save old min/max values for currep */

	    xminsave=xmin;
	    xmaxsave=xmax;
	    yminsave=ymin;
	    ymaxsave=ymax;

	    /* find bounding box on screen for instance */

	    xmax=0.0;		/* initialize globals to 0,0 location*/
	    xmin=0.0;		/* draw() routine will modify these */
	    ymax=0.0;
	    ymin=0.0;

	    if (nest >= nestlevel) { 
		drawon = 0;
	    } else {
		drawon = 1;
	    }

	    /* render instance */
	    if (debug) printf("rendering %s, %d\n",
	    	p->u.i->name, db_lookup(p->u.i->name));
	    db_render(db_lookup(p->u.i->name), nest+1, mode);
    

	    /* don't draw anything below nestlevel */
	    if (nest > nestlevel) { 
		drawon = 0;
	    } else {
		drawon = 1;
	    }

	    /* if at nestlevel, draw bounding box */
	    /* running through same transformations */
	    /* as original draw */

	    if (nest == nestlevel) { 
		set_pen(0);
		jump();
		    /* a square bounding box outline in white */
		    draw(xmax,ymax,mode); draw(xmax,ymin,mode);
		    draw(xmin,ymin,mode); draw(xmin,ymax,mode);
		    draw(xmax,ymax,mode);
		jump();
		    /* and diagonal lines from corner to corner */
		    draw(xmax,ymax,mode);
		    draw(xmin,ymin,mode);
		jump();
		    draw(xmin,ymax,mode);
		    draw(xmax,ymin,mode);
	    }


/*
	xmin=min(xminsave,xmin*xp->r11 + ymin*xp->r21 + xp->dx);
	xmin=min(xminsave,xmax*xp->r11 + ymax*xp->r21 + xp->dx);
	xmin=min(xminsave,xmin*xp->r11 + ymax*xp->r21 + xp->dx);
	xmin=min(xminsave,xmax*xp->r11 + ymin*xp->r21 + xp->dx);

	xmax=max(xmaxsave,xmin*xp->r11 + ymin*xp->r21 + xp->dx);
	xmax=max(xmaxsave,xmax*xp->r11 + ymax*xp->r21 + xp->dx);
	xmax=max(xmaxsave,xmin*xp->r11 + ymax*xp->r21 + xp->dx);
	xmax=max(xmaxsave,xmax*xp->r11 + ymin*xp->r21 + xp->dx);

	ymin=min(yminsave,xmin*xp->r12 + ymin*xp->r22 + xp->dy);
	ymin=min(yminsave,xmax*xp->r12 + ymax*xp->r22 + xp->dy);
	ymin=min(yminsave,xmax*xp->r12 + ymin*xp->r22 + xp->dy);
	ymin=min(yminsave,xmin*xp->r12 + ymax*xp->r22 + xp->dy);

	ymax=max(ymaxsave,xmin*xp->r12 + ymin*xp->r22 + xp->dy);
	ymax=max(ymaxsave,xmax*xp->r12 + ymax*xp->r22 + xp->dy);
	ymax=max(ymaxsave,xmax*xp->r12 + ymin*xp->r22 + xp->dy);
	ymax=max(ymaxsave,xmin*xp->r12 + ymax*xp->r22 + xp->dy);

*/

	    free(global_transform); free(xp);	
	    global_transform = save_transform;	/* set transform back */

	    /* now pass bounding box back */

	    xmin=min(xminsave, xmin+p->u.i->x);
	    xmax=max(xmaxsave, xmax+p->u.i->x);
	    ymin=min(yminsave, ymin+p->u.i->y);
	    ymax=max(ymaxsave, ymax+p->u.i->y);

	    break;
	default:
	    eprintf("unknown record type: %d in db_render\n", p->type);
	    return(1);
	    break;
	}
    }


    /* snapxy(&xmin, &ymin); */
    /* snapxy(&xmax, &ymax); */

    if (debug) printf("setting bounds %g %g %g %g\n",
    	xmin, xmax, ymin, ymax);

    if (nest == 0) {
	jump();
	set_pen(12);
	draw(xmin,ymax,mode);
	draw(xmax,ymax,mode);
	draw(xmax,ymin,mode);
	draw(xmin,ymin,mode);
	draw(xmin,ymax,mode);
    }

    cell->minx = xmin;
    cell->maxx = xmax;
    cell->miny = ymin;
    cell->maxy = ymax;
}


/******************** plot geometries *********************/

/* ADD Emask [.cname] [@sname] [:Wwidth] [:Rres] coord coord coord */ 
void do_arc(def, mode)
DB_DEFLIST *def;
int mode;
{
    NUM x1,y1,x2,y2,x3,y3;

    /* printf("# rendering arc  (not implemented)\n"); */

    x1=def->u.a->x1;	/* #1 end point */
    y1=def->u.a->y1;

    x2=def->u.a->x2;	/* #2 end point */
    y2=def->u.a->y2;

    x3=def->u.a->x3;	/* point on curve */
    y3=def->u.a->y3;
}

/* ADD Cmask [.cname] [@sname] [:Wwidth] [:Rres] coord coord */
void do_circ(def,mode)
DB_DEFLIST *def;
int mode;		/* drawing mode */
{
    int i;
    double r,theta,x,y,x1,y1,x2,y2;
    double theta_start;
    int res;


    if ((def->u.c->opts->rotation) != 0.0) {
	res = (int) (360.0/def->u.c->opts->rotation);	/* resolution */
    } else {
    	res = 64;
    }

    /* printf("# rendering circle\n"); */

    x1 = (double) def->u.c->x1; 	/* center point */
    y1 = (double) def->u.c->y1;

    x2 = (double) def->u.c->x2;	/* point on circumference */
    y2 = (double) def->u.c->y2;
    
    jump();
    set_pen(def->u.c->layer);

    x = (double) (x2-x1);
    y = (double) (y2-y1);
    r = sqrt(x*x+y*y);
    theta_start = atan2(x,y);
    for (i=0; i<=res; i++) {
	theta = theta_start+((double) i)*2.0*M_PI/((double) res);
	x = r*sin(theta);
	y = r*cos(theta);
	draw(x1+x, y1+y,mode);
    }
}

/* ADD Lmask [.cname] [@sname] [:Wwidth] coord coord [coord ...] */
  

void do_line(def,mode)
DB_DEFLIST *def;
int mode; 	/* drawing mode */
{
    COORDS *temp;
    OPTS *op;

    double x1,y1,x2,y2,x3,y3,dx,dy,d,a;
    double xa,ya,xb,yb,xc,yc,xd,yd;
    double k,dxn,dyn;
    double width=0.0;
    int segment;
    int debug=0;	/* set to 1 for copious debug output */

    width = def->u.l->opts->width;

    /* there are four cases for rendering lines with miters 
     *
     * "===" is rendered segment, "|" is a square end,
     * "/" is a mitered end, "----" is a non-rendered segment.
     * 
     * 1) single segment: 
     *      (temp->next == NULL)
     * 
     *	    |=====|
     *
     * 2) first segment, but more than one
     *      (temp->next != NULL)
     *
     *	    |=====/-----/...
     *
     * 3) middle segment bracketed by active segments:
     *      (temp->next != NULL)
     *
     *      -----/======/-----
     *
     * 4) final segment, preceeded by an active segment:
     *      (temp->next == NULL)
     *      -----/======|
     */

    /* printf("# rendering line\n"); */
    set_pen(def->u.l->layer);

    temp = def->u.l->coords;
    x2=temp->coord.x;
    y2=temp->coord.y;
    temp = temp->next;

    segment = 0;
    do {
	x1=x2; x2=temp->coord.x;
	y1=y2; y2=temp->coord.y;

	if (width != 0) {
	    if (segment == 0) {
		dx = x2-x1; 
		dy = y2-y1;
		a = 1.0/(2.0*sqrt(dx*dx+dy*dy));
		if (debug) printf("# in 1, a=%g\n",a);
		dx *= a; 
		dy *= a;
	    } else {
		if (debug) printf("# in 2\n"); 
		dx=dxn;
		dy=dyn;
	    }

	    if ( temp->next != NULL ) {
		x3=temp->next->coord.x;
		y3=temp->next->coord.y;
		dxn = x3-x2; 
		dyn = y3-y2;

		/* prevent a singularity if the last */
		/* rubber band segment has zero length */
		if (dxn==0 && dyn==0) {
		    a = 0;
		} else {
		    a = 1.0/(2.0*sqrt(dxn*dxn+dyn*dyn));
		}

		if (debug) printf("# in 3, a=%g\n",a); 
		dxn *= a; 
		dyn *= a;
	    }
	    
	    if ( temp->next == NULL && segment == 0) {

		if (debug) printf("# in 4\n"); 
		xa = x1+dy*width; ya = y1-dx*width;
		xb = x2+dy*width; yb = y2-dx*width;
		xc = x2-dy*width; yc = y2+dx*width;
		xd = x1-dy*width; yd = y1+dx*width;

	    } else if (temp->next != NULL && segment == 0) {  

		if (debug) printf("# in 5\n"); 
		xa = x1+dy*width; ya = y1-dx*width;
		xd = x1-dy*width; yd = y1+dx*width;

		if (fabs(dx+dxn) < EPS) {
		    k = (dx - dxn)/(dyn + dy); 
		    if (debug) printf("# in 6, k=%g\n",k);
		} else {
		    k = (dyn - dy)/(dx + dxn);
		    if (debug) printf("# in 7, k=%g\n",k);
		}

		/* check for co-linearity of segments */
		if ((fabs(dx-dxn)<EPS && fabs(dy-dyn)<EPS) ||
		    (fabs(dx+dxn)<EPS && fabs(dy+dyn)<EPS)) {
		    if (debug) printf("# in 8\n"); 
		    xb = x2+dy*width; yb = y2-dx*width;
		    xc = x2-dy*width; yc = y2+dx*width;
		} else { 
		    if (debug) printf("# in 19\n");
		    xb = x2+dy*width + k*dx*width;
		    yb = y2-dx*width + k*dy*width;
		    xc = x2-dy*width - k*dx*width; 
		    yc = y2+dx*width - k*dy*width;
		}

	    } else if ((temp->next == NULL && segment >= 0) ||
	              ( temp->next->coord.x == x2 && 
		        temp->next->coord.y == y2)) {

		if (debug) printf("# in 10\n");
		xb = x2+dy*width; yb = y2-dx*width;
		xc = x2-dy*width; yc = y2+dx*width;

	    } else if (temp->next != NULL && segment >= 0) {  

		if (debug) printf("# in 11\n");
		if (fabs(dx+dxn) < EPS) {
		    k = (dx - dxn)/(dyn + dy); 
		    if (debug) printf("# in 12, k=%g\n",k);
		} else {
		    if (debug) printf("# in 13, k=%g\n",k); 
		    k = (dyn - dy)/(dx + dxn);
		}

		/* check for co-linearity of segments */
		if ((fabs(dx-dxn)<EPS && fabs(dy-dyn)<EPS) ||
		    (fabs(dx+dxn)<EPS && fabs(dy+dyn)<EPS)) {
		    if (debug) printf("# in 8\n"); 
		    xb = x2+dy*width; yb = y2-dx*width;
		    xc = x2-dy*width; yc = y2+dx*width;
		} else { 
		    if (debug) printf("# in 9\n"); 
		    xb = x2+dy*width + k*dx*width;
		    yb = y2-dx*width + k*dy*width;
		    xc = x2-dy*width - k*dx*width; 
		    yc = y2+dx*width - k*dy*width;
		}
	    }

	    jump();
	
	    draw(xa, ya,mode);
	    draw(xb, yb,mode);

	    if( width != 0) {
		draw(xc, yc,mode);
		draw(xd, yd,mode);
		draw(xa, ya,mode);
	    }

	    /* printf("#dx=%g dy=%g dxn=%g dyn=%g\n",dx,dy,dxn,dyn); */
	    /* check for co-linear reversal of path */
	    if ((fabs(dx+dxn)<EPS && fabs(dy+dyn)<EPS)) {
		if (debug) printf("# in 20\n"); 
		xa = xc; ya = yc;
		xd = xb; yd = yb;
	    } else {
		if (debug) printf("# in 21\n"); 
		xa = xb; ya = yb;
		xd = xc; yd = yc;
	    }
	} else {		/* width == 0 */
	    jump();
	    draw(x1,y1,mode);
	    draw(x2,y2,mode);
	    jump();
	}

	temp = temp->next;
	segment++;

    } while(temp != NULL);
}

/* ADD Omask [.cname] [@sname] [:Wwidth] [:Rres] coord coord coord   */

void do_oval(def, mode)
DB_DEFLIST *def;
int mode;
{
    NUM x1,y1,x2,y2,x3,y3;

    /* printf("# rendering oval (not implemented)\n"); */

    x1=def->u.o->x1;	/* focii #1 */
    y1=def->u.o->y1;

    x2=def->u.o->x2;	/* focii #2 */
    y2=def->u.o->y2;

    x3=def->u.o->x3;	/* point on curve */
    y3=def->u.o->y3;

}

/* ADD Pmask [.cname] [@sname] [:Wwidth] coord coord coord [coord ...]  */ 

void do_poly(def, mode)
DB_DEFLIST *def;
int mode;
{
    COORDS *temp;
    int debug=0;
    int npts=0;
    double x1, y1;
    double x2, y2;
    
    if (debug) printf("# rendering polygon\n"); 
    
    if (debug) printf("# setting pen %d\n", def->u.p->layer); 
    set_pen(def->u.p->layer);

    
    /* FIXME: should eventually check for closure here and probably */
    /* at initial entry of points into db.  If last point != first */
    /* point, then the polygon should be closed automatically and an */
    /* error message generated */

    temp = def->u.p->coords;
    x1=temp->coord.x; y1=temp->coord.y;

    jump();
    while(temp != NULL) {
	x2=temp->coord.x; y2=temp->coord.y;
	if (temp->prev->coord.x == x2 && temp->prev->coord.y == y2) {
	    ;
	} else {
	    npts++;
	    draw(x2, y2, mode);
	}
	temp = temp->next;
    }
    
    /* when rubber banding, it is important not to redraw over a */
    /* line because it will erase it.  This can occur when only */
    /* two points have been drawn so far.  In this case, we should */
    /* not automatically close the polygon.  So we only execute the  */
    /* next bit of code when npts > 2 */

    if ((x1 != x2 || y1 != y2) && npts > 2) {
	draw(x1, y1, mode);
    }

}


/* ADD Rmask [.cname] [@sname] [:Wwidth] coord coord  */

void do_rect(def, mode)
DB_DEFLIST *def;
int mode;	/* drawing mode */
{
    double x1,y1,x2,y2;

    /* printf("# rendering rectangle\n"); */

    x1 = (double) def->u.r->x1;
    y1 = (double) def->u.r->y1;
    x2 = (double) def->u.r->x2;
    y2 = (double) def->u.r->y2;
    
    jump();
    set_pen(def->u.c->layer);

    draw(x1, y1, mode); draw(x1, y2, mode);
    draw(x2, y2, mode); draw(x2, y1, mode);
    draw(x1, y1, mode);
}



/* ADD [TN]mask [.cname] [:Mmirror] [:Rrot] [:Yyxratio]
 *             [:Zslant] [:Ffontsize] [:Nfontnum] "string" coord  
 */

void do_text(def, mode)
DB_DEFLIST *def;
int mode;
{

    extern XFORM *transform;
    XFORM *xp, *xf;
    NUM x,y;
    OPTS  *op;
    double optval;

    /* create a unit xform matrix */

    xp = (XFORM *) emalloc(sizeof(XFORM));  
    xp->r11 = 1.0; xp->r12 = 0.0; xp->r21 = 0.0;
    xp->r22 = 1.0; xp->dx  = 0.0; xp->dy  = 0.0;

    /* NOTE: To work properly, these transformations have to */
    /* occur in the proper order, for example, rotation must come */
    /* after slant transformation or else it wont work properly */

    switch (def->u.t->opts->mirror) {
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

    mat_scale(xp, def->u.t->opts->font_size, def->u.t->opts->font_size);
    mat_scale(xp, 1.0/def->u.t->opts->aspect_ratio, 1.0);
    mat_slant(xp, def->u.t->opts->slant);
    mat_rotate(xp, def->u.t->opts->rotation);

    jump(); set_pen(def->u.t->layer);

    xp->dx += def->u.t->x;
    xp->dy += def->u.t->y;

    writestring(def->u.t->text, xp, def->u.t->opts->font_num, mode);

    free(xp);
}

/***************** coordinate transformation utilities ************/

XFORM *compose(xf1, xf2)
XFORM *xf1, *xf2;
{
     XFORM *xp;
     xp = (XFORM *) emalloc(sizeof(XFORM)); 

     xp->r11 = (xf1->r11 * xf2->r11) + (xf1->r12 * xf2->r21);
     xp->r12 = (xf1->r11 * xf2->r12) + (xf1->r12 * xf2->r22);
     xp->r21 = (xf1->r21 * xf2->r11) + (xf1->r22 * xf2->r21);
     xp->r22 = (xf1->r21 * xf2->r12) + (xf1->r22 * xf2->r22);
     xp->dx  = (xf1->dx  * xf2->r11) + (xf1->dy  * xf2->r21) + xf2->dx;
     xp->dy  = (xf1->dx  * xf2->r12) + (xf1->dy  * xf2->r22) + xf2->dy;

     return (xp);
}

/* in-place rotate a transform matrix by theta degrees */
void mat_rotate(xp, theta)
XFORM *xp;
double theta;
{
    double s,c,t;
    s=sin(2.0*M_PI*theta/360.0);
    c=cos(2.0*M_PI*theta/360.0);

    t = c*xp->r11 - s*xp->r12;
    xp->r12 = c*xp->r12 + s*xp->r11;
    xp->r11 = t;

    t = c*xp->r21 - s*xp->r22;
    xp->r22 = c*xp->r22 + s*xp->r21;
    xp->r21 = t;

    t = c*xp->dx - s*xp->dx;
    xp->dy = c*xp->dy + s*xp->dx;
    xp->dx = t;
}

/* in-place scale transform */
void mat_scale(xp, sx, sy) 
XFORM *xp;
double sx, sy;
{
    double t;

    xp->r11 *= sx;
    xp->r12 *= sy;
    xp->r21 *= sx;
    xp->r22 *= sy;
    xp->dx  *= sx;
    xp->dy  *= sy;
}

/* in-place slant transform (for italics) */
void mat_slant(xp, theta) 
XFORM *xp;
double theta;
{
    double s,c,t;
    double a;

    int debug = 0;
    if (debug) printf("in mat_slant with theta=%g\n", theta);
    if (debug) mat_print(xp);

    s=sin(2.0*M_PI*theta/360.0);
    c=cos(2.0*M_PI*theta/360.0);
    a=s/c;

    xp->r21 += a*xp->r11;
    xp->r22 += a*xp->r12;

    if (debug) mat_print(xp);
}

void mat_print(xa)
XFORM *xa;
{
     printf("\n");
     printf("\t%g\t%g\t%g\n", xa->r11, xa->r12, 0.0);
     printf("\t%g\t%g\t%g\n", xa->r21, xa->r22, 0.0);
     printf("\t%g\t%g\t%g\n", xa->dx, xa->dy, 1.0);
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
 * have the option of building a linked list and using XDrawLines() rather
 * than XDrawLine().  In addition, the draw()/jump() paradigm is used by
 * autoplot, so has a long history.
 */


/* FIXME: currently transform gets seeded at the top of the render
tree in db_render by XWIN based on real->screen relationship.  This makes the
whole pipeline tangled up with graphical window size.  This was done to make
the transform maximally efficient.  It would be cleaner to always seed the
render pipeline with the unit transform and pass transform as an explicit
argument down the pipe.  That way, drawing is always done in real-world coords
with no admixture of screen transformations.  That requires draw to
concatenate the screen transform at the last possible moment just before
calling the X plotting funcions.  */

static int nseg=0;

void draw(x, y, mode)	/* draw x,y transformed by extern global xf */
NUM x,y;		/* location in real coordinate space */
int mode;
{
    extern XFORM *global_transform;	/* global for efficiency */
    NUM xx, yy;
    static NUM xxold, yyold;
    int debug=0;

    /* now compute transformed coordinates */

    xx = x*global_transform->r11 + 
         y*global_transform->r21 + global_transform->dx;

    yy = x*global_transform->r12 + 
         y*global_transform->r22 + global_transform->dy;

    /* globals for computing bounding boxes */
    /* NOTE: This must be done prior to conversion into screen space */
    xmax = max(x,xmax);
    xmin = min(x,xmin);
    ymax = max(y,ymax);
    ymin = min(y,ymin);

    if (X) {
	R_to_V(&xx, &yy);	/* convert to screen coordinates */ 
	if (nseg) {
	    if (mode == 0) { 	/* regular drawing */
		if (drawon) {
		    xwin_draw_line((int)xxold,(int)yyold,(int)xx,(int)yy);
                }
	    } else {		/* rubber band drawing */
		if (drawon) {
		    xwin_xor_line((int)xxold,(int)yyold,(int)xx,(int)yy);
 		}
	    }
	}
	xxold=xx;
	yyold=yy;
	nseg++;
    } else {
	if (drawon) printf("%4.6g %4.6g\n",xx,yy);	/* autoplot output */
    }
}

void jump(void) 
{
    if (X) {
	nseg=0;  		/* for X */
    } else {
	if (drawon) printf("jump\n");  	/* autoplot */
    }
}

void set_pen(pnum)
int pnum;
{
    if (X) {
	xwin_set_pen((pnum%8));		/* for X */
    } else {
	if (drawon) printf("pen %d\n", (pnum%8));	/* autoplot */
    }
    set_line((((int)(pnum/8))%5));
}

void set_line(lnum)
int lnum;
{
    if (X) {
	xwin_set_line((lnum%5));		/* for X */
    } else {
	if (drawon) printf("line %d\n", (lnum%5)+1);	/* autoplot */
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

