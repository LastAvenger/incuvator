/* Gopher filesystem

   Copyright (C) 2002 James A. Morrison <ja2morri@uwaterloo.ca>
   Copyright (C) 2000 Igor Khavkine <igor@twu.net>
   This file is part of the Gopherfs translator.

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
#include <stdio.h>
#include <unistd.h>
#include <maptime.h>
#include <errno.h>
#include <error.h>
#include <argp.h>
#include <argz.h>

#include <hurd/netfs.h>

#include "gopherfs.h"

/* definition of global config parapeters */
char *gopherfs_root_server;
unsigned short gopherfs_root_port;
char *gopherfs_server_dir;

int debug_flag;

struct gopherfs *gopherfs;	/* filesystem global pointer */
volatile struct mapped_time_value *gopherfs_maptime;

int
main (int argc, char **argv)
{
  error_t err;
  mach_port_t bootstrap;

  gopherfs_parse_args (argc, argv);
  if (debug_flag)
    fprintf (stderr, "pid %d\n", getpid ());

  task_get_bootstrap_port (mach_task_self (), &bootstrap);
  if (bootstrap == MACH_PORT_NULL)
    error (1, 0, "Must be started as a translator");

  netfs_init ();

  err = maptime_map (0, 0, &gopherfs_maptime);
  if (err)
    error (1, err, "Error mapping time.");


  /* err = gopherfs_create (...); */
  /* XXX */
  gopherfs = (struct gopherfs *) malloc (sizeof (struct gopherfs));
  if (! gopherfs)
    error (1, errno, "Cannot allocate gopherfs.");

  gopherfs->umask = 0;
  gopherfs->uid = getuid ();
  gopherfs->gid = getgid ();
  gopherfs->next_inode = 0;
  gopherfs->root =
    gopherfs_make_node (GPHR_DIR, "dir", "", gopherfs_root_server,
			gopherfs_root_port);
  fprintf (stderr, "attaching to %s\n", gopherfs_root_server);
  /* XXX */
  netfs_root_node = gopherfs->root;
  netfs_startup (bootstrap, 0);

  if (debug_flag)
    fprintf (stderr, "entering the main loop\n");
  for (;;)
    fprintf (stderr, "loop\n");
  netfs_server_loop ();

  /*NOT REACHED */
  fprintf (stderr, "Reached, now it will die");

  /*      free (gopherfs); */
  return 0;
}


/*
 * vim:ts=2:sw=2:autoindent:cindent:
 */
