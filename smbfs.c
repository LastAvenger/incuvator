/*
  Copyright (C) 2004, 2007, 2009 Free Software Foundation, Inc.
  Copyright (C) 2004, 2007, 2009 Giuseppe Scrivano.
  Copyright (C) 2012 Ludovic Courtès <ludo@gnu.org>

  Written by Giuseppe Scrivano <gscrivano@gnu.org>
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 3, or (at
  your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "smb.h"
#include <stdio.h>

static char doc[] = "smbfs - SMB file system translator" \
  "\vSHARE specifies an SMB directory in the form "
  "`smb://[WORKGROUP/]HOST/SHARE'.";
static char args_doc[] = "SHARE";



extern void smbfs_init ();
extern void smbfs_terminate ();
static struct argp_option options[] = 
{
	{"server",'s',"SERVER",0, "SMB server"},
	{"resource",'r',"RESOURCE",0, "directory to access"},
	{"password",'p',"PWD",0, "password to use (default: empty password)"},
	{"username",'u',"USR",0, "user name to use (default: `$USER')"},
	{"workgroup",'w',"WKG",0, "workgroup to use (default: `WORKGROUP')"},
	{0}
};
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{	
  switch(key)
    {
      case 's':
        credentials.server = arg;        
        break;
      case 'r':
        credentials.share = arg;        
        break;
      case 'w':
        credentials.workgroup = arg;
        break;
      case 'u':
        credentials.username = arg;
        break;
      case 'p':
        credentials.password = arg;
        break;
      case ARGP_KEY_ARG:
        break;
      case ARGP_KEY_END:
        break;
      default:
        return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp smb_argp = {options, parse_opt, args_doc, doc};

int
main (int argc, char *argv[])
{
  mach_port_t bootstrap;
  int err;

  /* Default user name.  */
  credentials.username = getenv ("USER");

  argp_parse (&smb_argp, argc, argv, 0, 0, &credentials);

  if(!credentials.server  || !credentials.share || !credentials.workgroup
     || !credentials.username || !credentials.password)
    error (EXIT_FAILURE, 0, "Please specify a server, share, workgroup, "
	   "user name, and password.");

  err = init_smb ();

  if (err < 0)
    error (EXIT_FAILURE, errno, "failed to initialize SMB client");

  task_get_bootstrap_port (mach_task_self (), &bootstrap);
  if (bootstrap == MACH_PORT_NULL)
    error (EXIT_FAILURE, 0, "Must be started as a translator.");

  netfs_init();
  netfs_startup(bootstrap, 0);
  smbfs_init();

  for(;;)
    netfs_server_loop ();

  smbfs_terminate ();
  return 0;
}
