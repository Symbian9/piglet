#include <stdio.h>
#include "db.h"
#include "opt_parse.h"

#define MIRROR_OFF 0
#define MIRROR_X   1
#define MIRROR_Y   2
#define MIRROR_XY  3

#define ERR -1

/* called with OPTION in optstring.  Compares optstring against
validopts and returns ERR if invalid option, or if invalid
scan of option value.  Updates opt structure if OK */

int opt_parse(optstring, validopts, popt) 
char *optstring;
char *validopts;	/* a string like "WSRMYZF" */
OPTS *popt;
{

    double optval;

    if (optstring[0] == '.') {
	/* got a component name */
    	popt->cname = strsave(optstring);
    } else if (optstring[0] == '@') {
	/* got a signal name */
    	popt->sname = strsave(optstring);
    } else if (optstring[0] == ':') {
        /* got a regular option */

	if (index(validopts, toupper(optstring[1])) == NULL) {
	    weprintf("invalid option (2) %s\n", optstring); 
	    return(ERR);
	}

	switch (toupper(optstring[1])) {
	    case 'F': 		/* :F(fontsize) */
		if(sscanf(optstring+2, "%lf", &optval) != 1) {
		    weprintf("invalid option argument: %s\n", optstring+2); 
		    return(ERR);
		}
		if (optval < 0.0 ) {
		    weprintf("fontsize must be positive: %s\n", optstring+2); 
		    return(ERR);
		}
		popt->font_size = optval;
		break;
	    case 'M':		/* mirror X,Y,XY */
		/* Note: these comparisons must be done in this order */
		if (strncasecmp(optstring+1,"MXY",3) == 0) {
		    popt->mirror = MIRROR_XY;
		} else if (strncasecmp(optstring+1,"MY",2) == 0) {
		    popt->mirror = MIRROR_Y;
		} else if (strncasecmp(optstring+1,"MX",2) == 0) {
		    popt->mirror = MIRROR_X;
		} else {
		    weprintf("unknown mirror option %s\n", optstring); 
		    return(ERR);
		}
		break;
	    case 'R':		/* resolution or rotation angle +/- 180 */
		if(sscanf(optstring+2, "%lf", &optval) != 1) {
		    weprintf("invalid option argument: %s\n", optstring+2); 
		    return(ERR);
		}
		if (optval < -180.0 || optval > 180.0 ) {
		    weprintf("option out of range: %s\n", optstring+2); 
		    return(ERR);
		}
		popt->rotation = optval;
		break;
	    case 'S':		/* step an instance */
		weprintf("option ignored: instance stepping not implemented\n");
		return(ERR);
		break;
	    case 'W':		/* width */
		if(sscanf(optstring+2, "%lf", &optval) != 1) {
		    weprintf("invalid option argument: %s\n", optstring+2); 
		    return(ERR);
		}
		if (optval < 0.0 ) {
		    weprintf("width must be positive: %s\n", optstring+2); 
		    return(ERR);
		}
		popt->width = optval;
		break;
	    case 'X':		/* scale */
		if(sscanf(optstring+2, "%lf", &optval) != 1) {
		    weprintf("invalid option argument: %s\n", optstring+2); 
		    return(ERR);
		}
		if (optval < 0.0 ) {
		    weprintf("scale must be positive: %s\n", optstring+2); 
		    return(ERR);
		}
		popt->scale = optval;
		break;
	    case 'Y':		/* yx ratio */
		if(sscanf(optstring+2, "%lf", &optval) != 1) {
		    weprintf("invalid option argument: %s\n", optstring+2); 
		    return(ERR);
		}
		popt->aspect_ratio = optval;
		break;
	    case 'Z':		/* slant +/-45 */
		if(sscanf(optstring+2, "%lf", &optval) != 1) {
		    weprintf("invalid option argument: %s\n", optstring+2); 
		    return(ERR);
		}
		if (optval < -45.0 || optval > 45.0 ) {
		    weprintf("out of range (+/-45 degrees): %s\n", optstring+2); 
		    return(ERR);
		}
		popt->slant = optval;
		break;
	    default:
		weprintf("unknown option ignored: %s\n", optstring);
		return(ERR);
		break;
	}
    } else {
	weprintf("unknown option ignored: %s\n", optstring);
    } 
    return(1);
}

/*
  ADD Elayer [:Wwidth] [:Rres] xy1 xy2 xy3 EOC
x ADD Clayer [:Wwidth] [:Rres] xy1 xy2 EOC
x ADD Llayer [:Wwidth] xy1 xy2 [xy3 ...]  EOC
x ADD Nlayer [:Mmirror] [:Rrot] [:Yyxratio] [:Zslant] [:Fsize] "string" xy EOC
  ADD Olayer [:Wwidth] [:Rres] xy1 xy2 xy3 EOC
x ADD Player [:Wwidth] xy1 xy2 xy3 [xy4 ...] EOC
x ADD Rlayer [:Wwidth] xy1 xy2 EOC
x ADD Tlayer [:Mmirror] [:Rrot] [:Yyxratio] [:Zslant] [:Fsize] "string"  xy1 EOC
x ADD [I] devicename [:M] [:R] [:X] [:Y] [:Z] [:Snx ny xstep ystep] xy1 EOC
*/

