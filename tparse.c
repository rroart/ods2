/*
 *  linux/fs/ods2/file.c
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 * Written 2003 by Jonas Lindholm <jlhm@usa.net>
 *
 */

#include <linux/string.h>
#include <linux/ctype.h>

#include "tparse.h"


int tparse(ARGBLK *argblk, TPARSE *tpa) {
  int		      idx;
  int		      found;
  
  while (1) {
    char	     *str = argblk->str;

    while (*str && (*str == ' ' || *str == '\t')) { str++; };
    found = 0;
    idx = -1;
    argblk->number = 0;
    argblk->str = str;
    argblk->token = str;
    do {
      str = argblk->str;
      idx++;
      switch ((long unsigned)tpa[idx].type) {
      case lu(TPA_ANY):	  { if (*str) { str++; found = 1; } break; }
      case lu(TPA_ALPHA): { if (*str && ((*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z'))) { str++; found = 1; } break; }
      case lu(TPA_DIGIT): { if (*str && (*str >= '0' && *str <= '9')) { argblk->number = *str++ - '0'; found = 1; } break; }
      case lu(TPA_HEX):
	{
	  while (*str && ((*str >= '0' && *str <= '9') || (tolower(*str) >= 'a' && tolower(*str) <= 'f'))) {
	    argblk->number = (argblk->number * 16) + (*str <= '9' ? *str - '0' : tolower(*str) - 'a' + 10);
	    str++;
	  }
	  found = 1;
	  break;
	}
      case lu(TPA_OCTAL): { while (*str && (*str >= '0' && *str <= '7')) { argblk->number = argblk->number * 8 + (*str++ - '0'); } found = 1; break; }
      case lu(TPA_DECIMAL): { while (*str && (*str >= '0' && *str <= '9')) { argblk->number = argblk->number * 10 + (*str++ - '0'); } found = 1; break; }
      case lu(TPA_STRING):
	{
	  while (*str && ((*str >= '0' && *str <= '9') || (*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z'))) { str++; }
	  found = 1;
	  break;
	}
      case lu(TPA_SYMBOL):
	{
	  while (*str && ((*str >= '0' && *str <= '9') || (*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z') || *str == '_' || *str == '$')) { str++; }
	  found = 1;
	  break;
	}
      case lu(TPA_EOS):	{ found = !*str; break;	}
      case lu(TPA_LAMBDA): { found = 1; }
      case 0: /* no more entries in table */
	break;
      default:
	{
	  if (((void **)tpa[idx].type)[0] == (void *)244) {
	    TPARSE            *tmptpa = ((TPARSE **)tpa[idx].type)[1];
	    ARGBLK             tmpargblk;
	    
	    tmpargblk.options = argblk->options;
	    tmpargblk.arg = argblk->arg;
	    tmpargblk.str = str;
	    tmpargblk.token = NULL;
	    tmpargblk.number = 0;
	    tmpargblk.param = tpa[idx].param;
	    if ((found = tparse(&tmpargblk, tmptpa))) {
	      str = tmpargblk.str;
	    }
	    break;
	  } else {
	    if (strlen(tpa[idx].type) == 1) {
	      if (*str++ == *tpa[idx].type) { found = 1; }
	    } else {
	      while (*str && ((*str >= '0' && *str <= '9') || (*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z') || *str == '_' || *str == '$')) { str++; }
	      if (strlen(tpa[idx].type) == (str - argblk->token) && strncmp(argblk->token, tpa[idx].type, (str - argblk->token)) == 0) { found = 1; }
	    }
	  }
	}
      }
      if (found) {
	if (tpa[idx].action != NULL) {
	  char        tmp = *str;

	  *str = 0;
	  argblk->param = tpa[idx].param;
	  argblk->mask = tpa[idx].mask;
	  argblk->mskadr = tpa[idx].mskadr;
	  found = tpa[idx].action(argblk);
	  *str = tmp;
	}
	argblk->str = str;
	argblk->token = str;
      }
    } while (!found && tpa[idx].type != NULL);
    if (found) {
      if (tpa[idx].mskadr != NULL) { *tpa[idx].mskadr |= tpa[idx].mask; }
      if ((void *)tpa[idx].label == (void *)0) { return 0; }
      if ((void *)tpa[idx].label == (void *)1) { return 1; }
      tpa = tpa[idx].label;
    } else {
      return 0;
    }
  }
}
