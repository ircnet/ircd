
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "irc.h"


#define	CTCP_CHAR	0x1

void check_ctcp(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	char	*front = NULL, *back = NULL;

	if (parc < 3)
		return;

	if (!(front = index(parv[2], CTCP_CHAR)))
		return;
	if (!(back = index(++front, CTCP_CHAR)))
		return;
	*back = '\0';
	if (!strcmp(front, "VERSION"))
		sendto_one(sptr, "NOTICE %s :%cVERSION %s%c", parv[0],
			  CTCP_CHAR, version, CTCP_CHAR);
	*back = CTCP_CHAR;
}
