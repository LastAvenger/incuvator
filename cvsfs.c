/**********************************************************
 * cvsfs.c
 *
 * Copyright 2004, Stefan Siegl <ssiegl@gmx.de>, Germany
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Publice License,
 * version 2 or any later. The license is contained in the COPYING
 * file that comes with the cvsfs4hurd distribution.
 *
 * translator startup code (netfs startup & argp handling)
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#define PACKAGE "cvsfs"
#define VERSION "0.1"

#include <stdio.h>
#include <argp.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <error.h>
#include <netdb.h>
#include <sys/stat.h>
#include <hurd/netfs.h>

#include "cvsfs.h"
#include "cvs_connect.h"
#include "cvs_tree.h"
#include "node.h"

/* cvsfs configuration: cvs pserver hostname, rootdir, modulename, etc. */
cvsfs_config config;

/* cvsfs entry stat template */
cvsfs_stat_template stat_template;

/* global variables, needed for netfs */
char *netfs_server_name = PACKAGE;
char *netfs_server_version = VERSION;
int netfs_maxsymlinks = 12;



static error_t parse_cvsfs_opt(int key, char *arg, struct argp_state *state);

static const struct argp_child argp_children[] =
  {
    {&netfs_std_startup_argp, 0, NULL, 0}, 
    {NULL, 0, NULL, 0} 
  };



/* documentation, written out when called with either --usage or --help */
const char *argp_program_version = VERSION;
static char *args_doc = "HOSTNAME CVSROOT MODULE";
static char *doc = "cvs filesystem translator for the Hurd.\v"
"Please mind the cvsfs is currently very much alpha-state code, therefore "
"please do not expect a translator working perfectly from the first "
"second on.";



/* options our translator understands, to be used by libc argp */
enum 
  {
    OPT_PORT = 'p',
    OPT_USER = 'u',
    OPT_HOMEDIR = 'h',
  };

static const struct argp_option cvsfs_options[] =
  { 
    { "port", OPT_PORT, "PORT", 0,
      "port to connect to on given host (if not using standard port)", 0 },
    { "user", OPT_USER, "USERNAME", 0,
      "username to supply to cvs host, when logging in", 0 },
    { "homedir", OPT_HOMEDIR, "PATH", 0,
      "path of your home directory (= path to .cvspass file)", 0 },
    /* terminate list */
    { NULL, 0, NULL, 0, NULL, 0 }
  };



volatile struct mapped_time_value *cvsfs_maptime;

/* rwlock from cvs_connect.c we've got to initialize */
extern spin_lock_t cvs_cached_conn_lock;

int
main(int argc, char **argv)
{
  struct netnode *rootdir;
  io_statbuf_t ul_stat;
  mach_port_t bootstrap, ul_node;
  FILE *handle;
  struct argp argp =
    {
      cvsfs_options, parse_cvsfs_opt, 
      args_doc, doc, argp_children, 
      NULL, NULL
    };

  /* first things first: initialize global locks we use */
  spin_lock_init(&cvs_cached_conn_lock);

  /* stderr = fopen("cvsfs.log", "w");
   * setvbuf(stderr, NULL, _IONBF, 0);
   */

  /* reset configuration structure to sane defaults */
  memset(&config, 0, sizeof(config));
  config.cvs_mode = PSERVER;
  config.cvs_port = 2401;
  config.cvs_username = "anonymous";
  config.cvs_password = NULL;

  /* parse command line parameters, first things first. */
  argp_parse(&argp, argc, argv, 0, 0, 0);

  /* set up our translator now ... */
  task_get_bootstrap_port(mach_task_self(), &bootstrap);
  netfs_init();

  /* okay, now try connecting our cvs server for the very first time */
  handle = cvs_connect(&config);
  if(! handle)
    {
      fprintf(stderr, PACKAGE ": cannot establish connection to '%s'.\n",
	      config.cvs_hostname);
      return 1;
    }

  if(! (rootdir = cvs_tree_read(handle, config.cvs_module)))
    {
      fprintf(stderr, PACKAGE ": unable to get initial cvs tree, stop.\n");
      return 1;
    }

  /* release, aka cache our connection */
  cvs_connection_release(handle);

  /* map time */
  if(maptime_map(0, 0, &cvsfs_maptime))
    {
      perror(PACKAGE ": cannot map time");
      return 1;
    }

  /* start up netfs stuff */
  ul_node = netfs_startup(bootstrap, 0);
  
  if(io_stat(ul_node, &ul_stat))
    {
      perror(PACKAGE ": cannot stat underlying node");
      return 1;
    }

  /* initialize our stat_template structure */
  stat_template.uid = ul_stat.st_uid;
  stat_template.gid = ul_stat.st_gid;
  stat_template.author = ul_stat.st_author;
  stat_template.fsid = getpid();
  stat_template.mode = ul_stat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);

  /* create our root node */
  netfs_root_node = cvsfs_make_node(rootdir);
  if(! netfs_root_node)
    {
      perror(PACKAGE ": cannot create rootnode");
      return 1;
    }

  netfs_server_loop();
  return 1; /* netfs_server_loop doesn't return, exit with status 1, if
	     * it returns anyways ...
	     */
}
    

/* parse_cvsfs_opt
 *
 * argp parser function for cvsfs'es command line args 
 */  
static error_t 
parse_cvsfs_opt(int key, char *arg, struct argp_state *state)
{
  switch(key) 
    {
    case OPT_PORT:
      config.cvs_port = atoi(arg);
      break;

    case OPT_USER:
      config.cvs_username = strdup(arg);
      break;

    case OPT_HOMEDIR:
      config.homedir = strdup(arg);
      break;

    case ARGP_KEY_ARGS:
      if(state->argc - state->next != 3)
	argp_usage(state);
      else
	{
	  config.cvs_hostname = strdup(state->argv[state->next ++]);
	  config.cvs_root = strdup(state->argv[state->next ++]);
	  config.cvs_module = strdup(state->argv[state->next ++]);
	}
      break;

    case ARGP_KEY_NO_ARGS:
      argp_usage(state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}
