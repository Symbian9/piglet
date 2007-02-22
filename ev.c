#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "ev.h"

#define HASHSIZE 101

// Environment variable handling for SET and SHELL commands.
//
// This code follows the basic outline given in
// "Advanced UNIX Programming", by Marc J. Rochkind, 
// 1985 Prentice-Hall.  I've changed the static array used
// in the book to the hashed symbol table described in 
// "The C Programming Language", Kernighan and Ritchie, Second
// Edition, 1988, Prentice-Hall to eliminate any hard-wired
// limitations on number of ENV variables (RCW).

extern char **environ;
void *malloc(), *realloc();
BOOLEAN assign(char **p, char *s);

static struct varslot {
    char *name;		/* variable name */
    char *defn;		/* variable value */
    BOOLEAN exported;	/* is it to be exported */
    struct varslot *next;
} *hashtab[HASHSIZE];	/* pointer table */

unsigned int hash(char *s)
{
    unsigned int hashval;
    for (hashval = 0; *s != '\0'; s++) 
    	hashval = *s+31*hashval;
    return (hashval%HASHSIZE);
}

static struct varslot *lookup(char *s) 	/* find symbol table entry */
{
    struct varslot *vp;
    for (vp=hashtab[hash(s)]; vp!=NULL; vp=vp->next) {
        if (strcmp(s,vp->name) == 0) {
	   return(vp); 	/* found */
	} 
    }
    return NULL;	/* not found */
}

BOOLEAN assign(char **p, char *s) /* initialize name or value */
{
    int size;

    size = strlen(s) + 1;
    if (*p == NULL) {
        if ((*p = malloc(size)) == NULL) {
	    return(FALSE);
	}
    } else if ((*p = realloc(*p,size)) == NULL) {
        return(FALSE);
    }
    strcpy(*p, s);
    return(TRUE);
}

struct varslot *EVset(char *name, char *defn)
{
    struct varslot *vp;
    unsigned hashval;

    if((vp=lookup(name)) == NULL) {	/* not found */
        vp = (struct varslot *) malloc(sizeof(*vp));
	vp->exported = FALSE;
	vp->defn = NULL;
	if ((vp == NULL) || (vp->name = strdup(name)) == NULL) {
	    return(NULL);
	}
	hashval = hash(name);
	vp->next = hashtab[hashval];
	hashtab[hashval] = vp;
    } 
    
    assign(&(vp->defn), defn);
    
    return(vp);
}

BOOLEAN EVexport(name) /* set variable to be exported */
char *name;
{
    struct varslot *vp;
    
    if ((vp=lookup(name)) == NULL) {
        return(FALSE);
    }
    vp->exported = TRUE;
    return(TRUE);
}

char *EVget(name) /* get value of variable */
char *name;
{
    struct varslot *vp;

    if ((vp=lookup(name)) == NULL) {
        return(NULL);
    }
    return(vp->defn);
}

BOOLEAN EVinit() /* initialize symbol table from environment */
{
    int i, namelen;
    char name[200];	/* FIXME: can overflow */

    for (i=0; environ[i] != NULL; i++) {
        namelen = strcspn(environ[i], "=");
	strncpy(name,environ[i],namelen);
	name[namelen] = '\0';
	if (!EVset(name, &environ[i][namelen+1]) || !EVexport(name)) {
	    return(FALSE);
	}
    }
    return(TRUE);
}

BOOLEAN EVupdate() /* build environment from symbol table */
{
    int i, envi, nvlen;
    struct varslot *vp;
    static BOOLEAN updated = FALSE;
    int count;

    if (!updated) {

	count = 0;
	for (i=0; i<HASHSIZE; i++) { 	/* count how many to export */
	    for (vp = hashtab[i]; vp != NULL; vp = vp->next) {
		if (vp->exported == TRUE) {
		    count++;
		}
	    }
	}

        if ((environ = (char **) malloc((count + 1) * sizeof(char *))) == NULL) {
	    return(FALSE);
	}
    }

    envi = 0;
    for (i=0; i<HASHSIZE; i++) { 
	for (vp = hashtab[i]; vp != NULL; vp = vp->next) {
	    if (!vp->exported) {
		continue;
	    }
	    nvlen = strlen(vp->name) + strlen(vp->defn) + 2;
	    if (!updated) {
		if ((environ[envi] = malloc(nvlen)) == NULL) {
		    return(FALSE);
		}
	    } else if ((environ[envi] = realloc(environ[envi], nvlen)) == NULL) {
		return(FALSE);
	    }
	    sprintf(environ[envi], "%s=%s", vp->name, vp->defn);
	    envi++;
	}
    }

   environ[envi]=NULL;
   updated = TRUE;
   return(TRUE);
}

void EVprint() /* print the environment */
{
    struct varslot *vp;
    int i;

    for (i=0; i<HASHSIZE; i++) {
	for (vp = hashtab[i]; vp != NULL; vp = vp->next) {
	    printf("%3s %s=%s\n", 
	        vp->exported ? "[E]":"[P]",
	        vp->name, vp->defn);
	}
    }
}

//int main()
//{
//    if (!EVinit()) {
//       printf("can't initialize environment\n");
//    }
//
//    printf("before update:\n");
//    EVprint();
//
//    if (!EVset("count", "0"))
//       printf("error: EVset\n");
//    if (!EVset("BOOK", "/home/walker/book") || !EVexport("BOOK"))
//       printf("error: EVset, EVexport\n");
//    if (!EVset("FOO", "BAR"))
//       printf("error: EVset, EVexport\n");
//
//   printf("after update:\n");
//    EVprint();
//
//    exit(0);
//}
