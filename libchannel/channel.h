/* Channel I/O

   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2001, 2002, 2004, 2005, 2007
     Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.org>
   Reworked for libchannel by Carl Fredrik Hammar <hammy.lite@gmail.com>

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111, USA. */

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <cthreads.h>
#include <mach.h>
#include <hurd/hurd_types.h>

struct channel
{
  int flags;
  struct channel_hub *hub;

  /* These hooks are not touched by libchannel.  */
  void *class_hook; /* For class' use.  */
  void *user_hook;  /* For user's use.  */
};

struct channel_hub
{
   /* Should be held when before access to fields of this hub or call to
      any function operating on it.  */
  struct mutex lock;
  
  /*  The name of this hub.  Its meaning is class-specific.  May be null
      and is freed by channel_free_hub.  */
  char *name;

  int flags;
  const struct channel_class *class;

  /* A list of sub-hubs.  Their usage is type-specific.  */
  struct channel_hub **children;
  size_t num_children;

  void *hook; /* For class' use.  */
};

struct channel_class
{
  /* Name of the class.  */
  const char *name;
 
  /* Read at most AMOUNT bytes from CHANNEL into BUF and LEN with the
     usual return buf semantics.  Blocks until data is available or return
     0 bytes on EOF.  May not be null.  See channel_read.  */
  error_t (*read) (struct channel *channel,
		   size_t amount, void **buf, size_t *len);

  /* Write LEN bytes from BUF to CHANNEL, AMOUNT is set to the amount
     actually witten.  Should block until data can be written.  May not be
     null.  See channel_write.  */
  error_t (*write) (struct channel *channel,
		    void *buf, size_t len, size_t *amount);
 
  /* Write out any pending data held by CHANNEL for buffering purposes.
     It should *not* flush data held to implement specific behavior.  May
     be null.  See channel_flush.  */
  error_t (*flush) (struct channel *channel);

  /* Set any backend handled flags in CHANNEL specified in FLAGS.  May be
     null.  See channel_set_flags.  */
  error_t (*set_flags) (struct channel *channel, int flags);

  /* Clear any backend handled flags in CHANNEL specified in FLAGS.  May
     be null.  See channel_clear_flags.  */
  error_t (*clear_flags) (struct channel *channel, int flags);

  /* Open CHANNEL and set FLAGS.  May be null.  See channel_open.  */
  error_t (*open) (struct channel *channel, int flags);

  /* Free any resources allocated for CHANNEL.  May be null.  See
     channel_close.  */
  void (*close) (struct channel *channel);

  /*  This demuxer should be called whenever an unknown RPC is received by
      the translator.  See channel_control_demuxer.  */
  int (*control_demuxer) (mach_msg_header_t *in, mach_msg_header_t *out);

  /* Create new hub specified by NAME with given FLAGS and return it in
     HUB.  The interpretation of NAME is class-specific.  CLASSES are
     provided in order to open any sub-hubs.  */
  error_t (*create_hub) (const char *name, int flags,
                         const struct channel_class *const *classes,
                         struct channel_hub **hub);

  /* Free any class-specific resources allocated for HUB.  May be null.
     See channel_free_hub.  */
  void (*clear_hub) (struct channel_hub *hub);

  /* Set any backend handled flags in HUB specified in FLAGS.  May be
     null.  See channel_set_hub_flags.  */
  error_t (*set_hub_flags) (struct channel_hub *hub, int flags);

  /* Clear any backend handled flags in HUB specified in FLAGS.  May be
     null.  See channel_clear_hub_flags.  */
  error_t (*clear_hub_flags) (struct channel_hub *hub, int flags);

  /* Return EINVAL if NAME is an invalid name for a hub.  It should accept
     0 if it accepts an empty name.  May be null.  */
  error_t (*validate_name) (const char *name,
			    const struct channel_class *const *classes);
};

/* Flags implemented by generic channel code.  */
#define CHANNEL_READONLY   0x1 /* No writing allowed. */
#define CHANNEL_WRITEONLY  0x2 /* No reading allowed. */
#define CHANNEL_NO_FILEIO  0x4 /* Don't consider file io as an option.  */
#define CHANNEL_ENFORCED   0x8 /* Don't transfer hubs over IPC.  */

#define CHANNEL_GENERIC_FLAGS (CHANNEL_READONLY | CHANNEL_WRITEONLY	\
			       | CHANNEL_NO_FILEIO | CHANNEL_ENFORCED)

/* Flags implemented by each backend.  */
#define CHANNEL_HARD_READONLY     0x010 /* Can't be made writable.  */
#define CHANNEL_HARD_WRITEONLY    0x020 /* Can't be made readable.  */

#define CHANNEL_BACKEND_SPEC_BASE 0x100 /* Here up are backend-specific */
#define CHANNEL_BACKEND_FLAGS	(CHANNEL_HARD_READONLY			\
				 | CHANNEL_HARD_WRITEONLY		\
				 | ~(CHANNEL_BACKEND_SPEC_BASE - 1))

/* Allocate a new channel of hub HUB, with FLAGS set, then return it in
   CHANNEL.  Return ENOMEM if memory for the hub couldn't be allocated.  */
error_t channel_alloc (struct channel_hub *hub, int flags,
		       struct channel **channel);

/* Free CHANNEL and any generic resources allocated for it.  */
void channel_free (struct channel *channel);

/* Allocate a new channel, open it, and return it in CHANNEL.  Uses
   HUB's open method and passes FLAGS to it, unless it's null.  */
error_t channel_open (struct channel_hub *hub, int flags,
		      struct channel **channel);

/* Call CHANNEL's close method, unless it's null, then free it
   (regardless.)  */
void channel_close (struct channel *channel);

/* Set the flags FLAGS in CHANNEL.  Remove any already set flags in FLAGS,
   if FLAGS contain any backend flags call set_flags method or if
   set_flags is null return EINVAL.  Lastly generic flags get set.  */
error_t channel_set_flags (struct channel *channel, int flags);

/* Clear the flags FLAGS in CHANNEL.  Remove any already cleared flags in
   FLAGS, if FLAGS contain any backend flags call clear_flags method or if
   clear_flags is null return EINVAL.  Lastly generic flags get
   cleared.  */
error_t channel_clear_flags (struct channel *channel, int flags);

/* Reads at most AMOUNT bytes from CHANNEL into BUF and LEN with the usual
   return buf semantics.  Block until data is available and return 0 bytes
   on EOF.  If channel is write-only return EPERM, otherwise forward call
   to CHANNEL's read method.  */
error_t channel_read (struct channel *channel, size_t amount,
		      void **buf, size_t *len);

/* Write LEN bytes of BUF to CHANNEL, AMOUNT is set to the amount actually
   witten.  Block until data can be written.  If channel is read-only
   return EPERM, otherwise forward call to CHANNEL's write method.  */
error_t channel_write (struct channel *channel, void *buf, size_t len,
		       size_t *amount);

/* Write out any pending data held by CHANNEL in buffers, by forwarding
   call to flush method, unless it's null.  */
error_t channel_flush (struct channel *channel);

/* This demuxer will get the channel IN is intended for using
   channel_begin_using_channel and call channel_end_using_channel, then
   call the channel's control_demuxer method.  Return true, unless channel
   or its method is null or if the method couldn't handle IN.  */
int channel_control_demuxer (mach_msg_header_t *in, mach_msg_header_t *out);


/* The following two functions are provided for use in the MiG .defs file
   as a type translator, destructor pair.  They are to be implemented in
   the translator using libchannel.  */

/* Find and return the channel associated with PORT or return null if
   there is no such channel.  This function should be overridden by the
   translator using libchannel, as the default implementation will
   always return null.  */
struct channel *channel_begin_using_channel (mach_port_t port);

/* Reverses any side-effects of using channel_begin_using_channel and
   should also be implemented by the translator using libchannel, as
   the default implementation does nothing.  */
void channel_end_using_channel (struct channel *channel);

/* Allocate a new hub of CLASS, named NAME and with FLAGS set, then return
   it in HUB.  Return ENOMEM if memory for the hub couldn't be
   allocated.  */
error_t channel_alloc_hub (const struct channel_class *class,
			   const char *name, int flags,
			   struct channel_hub **hub);

/* If not null call method clear_hub to deallocate class-specific bits of
   HUB, then (regardless) free any generic resources used by it and
   itself.  */
void channel_free_hub (struct channel_hub *hub);

/* Set name of HUB to copy of NAME.  Return ENOMEM if no memory if
   available.  */
error_t channel_set_hub_name (struct channel_hub *hub, const char *name);

/* Set the HUB's list of children to a copy of CHILDREN and NUM_CHILDREN,
   or if allocation fails return ENOMEM.  */
error_t channel_set_children (struct channel_hub *hub,
			      struct channel_hub *const *children,
			      size_t num_children);

/* Generate a name for the children of HUB into NAME.  It done by
   combining the name of each child in a way that the name can be parsed
   by channel_create_hub_children.  This is done heuristically, and it may
   fail and return EGRATUITOUS.  If a child does not have a name, return
   EINVAL.  If memory is exausted, return ENOMEM.  */
error_t channel_children_name (const struct channel_hub *hub,
			       char **name);

/* Set the flags FLAGS in HUB.  Remove any already set flags in FLAGS, if
   FLAGS then contain backend flags call set_hub_flags method with with
   FLAGS or if set_hub_flags is null return EINVAL.  Lastly generic flags
   get set.  */
error_t channel_set_hub_flags (struct channel_hub *hub, int flags);

/* Clear the flags FLAGS in HUB.  Remove any already cleared flags in
   FLAGS, if FLAGS then contain backend flags call clear_hub_flags method
   with with FLAGS or if clear_hub_flags is null return EINVAL.  Lastly
   generic flags get clear.  */
error_t channel_clear_hub_flags (struct channel_hub *hub, int flags);

/* Set FLAGS in all the children of HUB, and if successful, set them in
   HUB also.  Flags are set using channel_set_hub_flags.  Propagate any
   error on failure.  */
error_t channel_set_child_flags (struct channel_hub *hub, int flags);

/* Clear FLAGS in all the children of HUB, and if successful, clear them
   in HUB also.  Flags are cleared as if using channel_clear_hub_flags.
   Propagate any error on failure.  */
error_t channel_clear_child_flags (struct channel_hub *hub, int flags);

/* Return a new hub in HUB, which is a copy of the hub underlying SOURCE.
   The class of hub is found in CLASSES or CHANNEL_STD_CLASSES, if CLASSES
   is null.  FLAGS is set with channel_set_hub_flags.  Keeps the SOURCE
   reference, which may be closed using channel_close_hub_source.  */
error_t channel_fetch_hub (file_t source, int flags,
			   const struct channel_class *const *classes,
			   struct channel_hub **hub);

/* Open the file NAME and return a new hub in HUB, which is either a copy
   of the file's underlying hub or a hub using it through file io, unless
   CHANNEL_NO_FILEIO flag is given.  The class of HUB is found in CLASSES
   or CHANNEL_STD_CLASSES, if CLASSES is null.  FLAGS is set with
   channel_set_hub_flags.  Keeps the SOURCE reference, which may be closed
   using channel_close_hub_source.  */
error_t
channel_create_query_hub (const char *name, int flags,
			  const struct channel_class *const *classes,
			  struct channel_hub **hub);

/* Create the channel hub indicated by NAME, which should consist of a
   channel type name followed by a ':' and any type-specific name,
   returning the new hub in HUB.  If NAME doesn't contain a `:', then it
   will be interpreted as either a class name, if such a class occurs in
   CLASSES, or a filename, which is opened by calling
   channel_create_query_hub on NAME; a `:' at the end or the beginning of
   NAME unambiguously causes the remainder to be treated as a class-name
   or a filename, respectively.  CLASSES is used to select classes
   specified by the type name; if it is 0, CHANNEL_STD_CLASSES is
   used.  */
error_t channel_create_typed_hub (const char *name, int flags,
				  const struct channel_class *const *classes,
				  struct channel_hub **hub);

/* Create and return in HUB, the hub specified by NAME, which should
   consist of a hub type name followed by a `:' and any type-specific
   name.  Its class is loaded dynamically and CLASSES is only passed to
   the class' create_hub method (which usually use it for creating
   sub-hubs.)  */
error_t
channel_create_module_hub (const char *name, int flags,
			   const struct channel_class *const *classes,
			   struct channel_hub **hub);

/* Create and return in HUB a hub that creates channels that does file i/o
   with the file specified by NAME.  */
error_t channel_create_file_hub (const char *name, int flags,
				 const struct channel_class *const *classes,
				 struct channel_hub **hub);

error_t
channel_create_broadcast_hub (const char *name, int flags,
			      const struct channel_class *const *classes,
			      struct channel_hub **hub);

error_t
channel_create_tee_hub (const char *name, int flags,
			const struct channel_class *const *classes,
			struct channel_hub **hub);

/* Parse multiple hub names in NAME, creating and returning each in HUBS
   and NUM_HUBS.  The syntax of name is a single non-alpha-numeric
   character followed by each child hub's name, seperated by the same
   separator.  Each child name is in TYPE:NAME notation as parsed by
   channel_create_typed_hub.  If all children has the same TYPE: prefix,
   then it may be factored out and put before the child list instead.  */
error_t
channel_create_hub_children (const char *name, int flags,
			     const struct channel_class *const *classes,
			     struct channel_hub ***hubs, size_t *num_hubs);

/* Find and return a class by name in CLASSES if not null, otherwise in
   the `channel_std_class' section and already loaded modules.  NAME_END
   points to the character after the class name in NAME; if null, then
   NAME is the null-terminated class name.  */
const struct channel_class *
channel_find_class (const char *name, const char *clname_end,
		    const struct channel_class *const *classes);

/* Load the module that defines the class NAME (upto NAME_END) and return
   it in CLASS.  Return ENOENT if module isn't available, or any other
   error code from dlopen and friends if module couldn't be loaded.  */
error_t
channel_module_find_class (const char *name, const char *clname_end,
			   const struct channel_class **classp);

/* Standard channel classes implemented by channel.  */
extern const struct channel_class channel_query_class;
extern const struct channel_class channel_typed_class;
extern const struct channel_class channel_module_class;

extern const struct channel_class channel_file_class;
extern const struct channel_class channel_broadcast_class;
extern const struct channel_class channel_tee_class;

/* The macro CHANNEL_STD_CLASS produces a reference in the
   `channel_std_classes' section for channel_NAME_class.  */
#define CHANNEL_STD_CLASS(name) \
  static const struct channel_class *const channel_std_classes_##name[]  \
  __attribute_used__ __attribute__ ((section ("channel_std_classes")))	\
       = { &channel_##name##_class }

/* These are pointers to the start and end of the channel_std_classes
   section of the executable.  */
extern const struct channel_class *const __start_channel_std_classes[]
  __attribute__ ((weak));
extern const struct channel_class *const __stop_channel_std_classes[]
  __attribute__ ((weak));

/* An argument parser that may be used for parsing a simple command line
   specification for channels.  The accompanying input parameter must be a
   pointer to a struct channel_argp_params.  */
extern struct argp channel_argp;

/* The structure used to pass args back and forth from CHANNEL_ARGP.  */
struct channel_argp_params
{
  /* The resulting parsed result.  */
  struct channel_parsed *result;

  /* If --channel-type isn't specified use this; null is equivalent to
     `query'.  */
  const char *default_type;

  /* The set of classes used to validate channel-types and argument
     syntax.  */
  const struct channel_class *const *classes;

  /* This controls the behavior when no channel arguments are specified.
     If zero, the parser fails with the error message ``No channel
     specified''.  If nonzero, the parser succeeds and sets `result' to
     null.  */
  int channel_optional;
};

/* The result of parsing a channel, which should be enough information to
   open it, or return the arguments.  */
struct channel_parsed;

/* Free all resources used by PARSED.  */
void channel_parsed_free (struct channel_parsed *parsed);

/* Create and return in HUB the channel hub specified in PARSED.  */
error_t channel_create_parsed_hub (const struct channel_parsed *parsed,
				   int flags, struct channel_hub **hub);

/* Add the arguments used to create PARSED to ARGZ & ARGZ_LEN.  */
error_t channel_parsed_append_args (const struct channel_parsed *parsed,
				    char **argz, size_t *argz_len);

/* Make a string describing PARSED, and return it in malloced storage in
   NAME.  */
error_t channel_parsed_name (const struct channel_parsed *parsed,
			     char **name);


#endif /* __CHANNEL_H__ */
