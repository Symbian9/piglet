
/* a facility for saving pointers in a push pop stack */

#include <stdio.h>
#include <stdlib.h>
#include "stack.h"

/* private functions */
STACK *stack_new();
int stack_print();

STACK *stack_new()
{
    STACK *tmp;
    tmp = (STACK *) malloc(sizeof(STACK));
    tmp->saved = NULL;
    tmp->next = NULL;
    return(tmp);
}

void stack_push(stack, pointer)
STACK **stack;
char *pointer;
{
    STACK *tmp;
    tmp = stack_new();
    tmp->saved = pointer;

    if (*stack == NULL) {	/* first call */
	tmp->next = NULL;
    } else {			/* subsequent calls */
        tmp->next = *stack;	
    }
    *stack = tmp;
    /* stack_print(stack);  */
}

char *stack_pop(stack)
STACK **stack;
{

    char *s;
    STACK *tmp;

    if (*stack == NULL) {	/* empty stack */
	return(NULL);
    } else {			/* subsequent calls */
	tmp = *stack;
	s = (*stack)->saved;
	*stack = (*stack)->next;
	free(tmp);
	return(s);
    }
}

int stack_print(stack) 
STACK **stack;
{
     int i=0;
     STACK *sp;

     if (stack == NULL) {
     	return(0);
     }
     sp = *stack;
     while(sp != NULL) {
	 if (sp->saved != NULL) {
	     printf("stack #%d: %s\n", i++, sp->saved);
	 }
	 sp = sp->next;
     }
     return(0);
}

/*
int main() {

    STACK *stack;
    char *s;

    stack=NULL;
    stack_push(&stack, "hello");
    stack_push(&stack, "world");
    stack_push(&stack, "this");
    stack_push(&stack, "is");
    stack_push(&stack, "a");
    stack_push(&stack, "test");

    while ((s=stack_pop(&stack))!=NULL) {
	    printf("%s\n",s);
    }
    return(1);
}
*/
