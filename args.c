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
#include <argp.h>
#include <argz.h>

#include <hurd/netfs.h>

#include "gopherfs.h"

/* ARGP data */
const char *argp_program_version = "gopherfs 0.1.2";
const char *argp_program_bug_address = "ja2morri@uwaterloo.ca";
char args_doc[] = "REMOTE_FS [SERVER]";
char doc[] = "Hurd gopher filesystem translator";
static const struct argp_option options[] = {
  {"debug", 'D', 0, 0, "enable debug output"},
  {0}
};
/* the function that groks the arguments and fills
 * global options
 */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'D':
      debug_flag = 1;
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num == 0)
	gopherfs_root_server = arg;
      else if (state->arg_num == 1)
	{
	  gopherfs_root_port = 70;
	  gopherfs_server_dir = arg;
	}
      else if (state->arg_num == 2)
	{
	  char *tail;
	  gopherfs_root_port = (unsigned short) strtol (arg, &tail, 10);
	  if (tail == arg || gopherfs_root_port > USHRT_MAX)
	    {
	      /* XXX bad integer conversion */
	      error (1, errno, "bad port number");
	    }
	}
      else
	return ARGP_ERR_UNKNOWN;
      break;
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
gopherfs_parse_args (int argc, char **argv)
{
  /* XXX: handle command line arguments properly */
  return argp_parse (&parser, argc, argv, 0, 0, /*&conf */ 0);
}
