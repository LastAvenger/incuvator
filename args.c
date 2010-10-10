/* Gopher filesystem. Argument handling routines.

   Copyright (C) 2001, 2002 James A. Morrison <ja2morri@uwaterloo.ca>
   Copyright (C) 2000 Igor Khavkine <igor@twu.net>

   Gopherfs is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   Gopherfs is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <error.h>
#include <stdio.h>
#include <argp.h>
#include <argz.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <hurd/netfs.h>

#include "gopherfs.h"

/* ARGP data */
const char *argp_program_version = GOPHER_SERVER_NAME " " GOPHER_SERVER_VERSION;
const char *argp_program_bug_address = "ja2morri@uwaterloo.ca";
char args_doc[] = "SERVER [REMOTE_FS]";
char doc[] = "Hurd gopher filesystem translator";
static const struct argp_option options[] = {
  {"port", 'P', "NUMBER", 0, "Specify a non-standard port"},
  {"debug", 'D', "FILE", 0, "Print debug output to FILE"},
  {0}
};
/* the function that groks the arguments and fills
 * global options
 */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct gopherfs_params *params = state->input;
  error_t err = 0;

  FILE *debug_stream = NULL;
  char *tail;
  switch (key)
    {
    case 'D':
      if (arg)
	{
	  debug_stream = fopen (arg, "w+");
	  if (! debug_stream)
	    {
	      err = errno;
	      argp_failure (state, 0, errno, "Cannot open debugging file %s", arg);
	    }
	}
      else
	debug_stream = stderr;

      if (!err)
	debug_init (debug_stream);
	
      break;

    case 'P':
      params->port = (unsigned short) strtol (arg, &tail, 10);
      if (tail == arg || params->port > USHRT_MAX)
        {
          /* XXX bad integer conversion */
          error (1, errno, "bad port number");
        }
      break;

    case ARGP_KEY_ARG:
      if (state->arg_num == 0)
        {
	  params->dir = "";
	  params->server = arg;
        }
      else if (state->arg_num == 1)
	{
	  params->dir = arg;
	}
      else
	return ARGP_ERR_UNKNOWN;
      break;

    case ARGP_KEY_SUCCESS:
      if (state->arg_num == 0)
	argp_error (state, "No remote filesystem specified");
      else
	{
	  /* No port was provided. Get gopher default port. */
	  if (params->port == 0)
	    {
	      struct servent *se = getservbyname ("gopher", "tcp");
	      if (! se)
		argp_error (state, "Couldn't get gopher port");

	      params->port = ntohs(se->s_port);
	    }

	  /* Check if the given address resolves. */
	  error_t err;
	  struct addrinfo *addr;
	  err = lookup_host (params->server, params->port, &addr);
	  if (err)
	    argp_failure (state, 10, 0, "%s: %s", params->server, gai_strerror (err));
	}
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp_child argp_children[] = { {&netfs_std_startup_argp}, {0} };
static struct argp parser =
  { options, parse_opt, args_doc, doc, argp_children };

/* Used by netfs_set_options to handle runtime option parsing. */
struct argp *netfs_runtime_argp = &parser;

/* maybe overwrite this some time later */
error_t netfs_append_args (char **argz, size_t * argz_len);

/* handle all initial parameter parsing */
error_t
gopherfs_parse_args (int argc, char **argv, struct gopherfs_params *params)
{
  return argp_parse (&parser, argc, argv, 0, 0, params);
}
