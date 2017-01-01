/* Arianne Butler
 * alb153, 11068223
 * Kristen Mercier
 * kmm732, 11182729
 */
 
#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"


int main (void) {
	
	int parentpid;
	int oldPrio;
	int newPrio;
	int incr;

	incr = 2;
	
	fork();
	
	parentpid = getpid();
	
	printf(1, "TESTING nice() and getpriority()\n");
	
	oldPrio = getpriority(parentpid);
	
	nice(incr);
	
	newPrio = getpriority(parentpid);	
	
	if (oldPrio + incr == newPrio) {
		
		printf(1, "Call to nice() successful\n");		
	}
	else {
		printf(1, "Call to nice() failed\n");
	}
	
	printf(1, "The old priority was %d\n", oldPrio);
	printf(1, "The new priority is %d\n", newPrio);
	
	/*printf(1, "TESTING setpriority()\n");
	setpriority(parentpid, 3);*/
	
	exit();
}
