/**********************************************************
 * main.c
 *
 * Copyright 2004, Stefan Siegl <ssiegl@gmx.de>, Germany
 * 
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Publice License,
 * version 2 or any later. The license is contained in the COPYING
 * file that comes with the fuse4hurd distribution.
 *
 * translator startup code (and argp handling)
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <error.h>
#include <hurd/netfs.h>

#include "fuse_i.h"
#include "fuse.h"

/* global variables, needed for netfs */
char *netfs_server_name = PACKAGE;
char *netfs_server_version = VERSION;
int netfs_maxsymlinks = 12;

/* pointer to the fuse_operations structure of this translator process */
int fuse_use_ino = 0;
const struct fuse_operations *fuse_ops = NULL;
const struct fuse_operations_compat2 *fuse_ops_compat = NULL;

/* the port where to write out debug messages to, NULL to omit these */
FILE *debug_port = NULL;

static int
_fuse_main(int argc, char *argv[], const struct fuse_operations *op)
{
  (void) op; /* handled by wrapper function */

  mach_port_t bootstrap, ul_node;

  /* print debug messages out to standard error */
  debug_port = stderr;

  task_get_bootstrap_port(mach_task_self(), &bootstrap);
  if(bootstrap == MACH_PORT_NULL)
    {
      /* no assigned bootstrap port, i.e. we got called as a
       * common program, not using settrans
       */
      fprintf(stderr, "%s: must be started as a translator.\n", *argv);
      return EPERM;
    }

  /* we have got a bootstrap port, that is, we were set up
   * using settrans and may start with normal operation ... */
  netfs_init();
  ul_node = netfs_startup(bootstrap, 0);

  /* create our root node */
  {
    struct netnode *root = fuse_make_netnode(NULL, "/");
    netfs_root_node = fuse_make_node(root);
  }

  if(! netfs_root_node)
    {
      perror(PACKAGE ": cannot create rootnode");
      return -EAGAIN;
    }

  netfs_server_loop();
  return 0;
}


int
fuse_main_compat2(int argc, char *argv[],
		  const struct fuse_operations_compat2 *op)
{
  /* initialize global fuse4hurd variables ... */
  fuse_ops_compat = op;

  return _fuse_main(argc, argv, (const struct fuse_operations *) op);
}

int 
fuse_main_real(int argc, char *argv[],
	       const struct fuse_operations *op, size_t op_size)
{
  /* initialize global fuse4hurd variables ... */
  fuse_ops = op;

  return _fuse_main(argc, argv, op);
}
