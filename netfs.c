/* netio - creates socket ports via the filesystem
   Copyright (C) 2001, 02 Moritz Schulte <moritz@duesseldorf.ccc.de>
 
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or * (at your option) any later version.
 
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA */

#include <hurd/netfs.h>
#include <hurd/socket.h>
#include <argz.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <dirent.h>
#include <sys/mman.h>

#include "netio.h"
#include "lib.h"
#include "node.h"
#include "protocol.h"

/* Returned directory entries are aligned to blocks this many bytes
   long.  Must be a power of two.  For netfs_get_dirents.  Taken from
   hostmux.  */
#define DIRENT_ALIGN 4
#define DIRENT_NAME_OFFS offsetof (struct dirent, d_name)
/* Length is structure before the name + the name + '\0', all
   padded to a four-byte alignment.  */
#define DIRENT_LEN(name_len)						      \
  ((DIRENT_NAME_OFFS + (name_len) + 1 + (DIRENT_ALIGN - 1))		      \
   & ~(DIRENT_ALIGN - 1))

/* The user must define this function.  Lookup NAME in DIR (which is
   locked) for USER; set *NP to the found name upon return.  If the
   name was not found, then return ENOENT.  On any error, clear *NP.
   (*NP, if found, should be locked and a reference to it generated.
   This call should unlock DIR no matter what.)  */
error_t
netfs_attempt_lookup (struct iouser *user, struct node *dir,
		      char *name, struct node **node)
{
  error_t err;
  err = fshelp_access (&dir->nn_stat, S_IEXEC, user);
  if (err)
    goto out;
  if (STRING_EQUAL (name, "."))
    {
      *node = dir;
      netfs_nref (*node);
    }
  else if (STRING_EQUAL (name, ".."))
    {
      *node = dir->nn->dir;
      netfs_nref (*node);
    }
  else
    {
      if (dir->nn->flags & PROTOCOL_NODE)
	err = node_make_host_node (user, dir, name, node);
      else if (dir->nn->flags & HOST_NODE)
	err = node_make_port_node (user, dir, name, node);
      else if (dir->nn->flags & ROOT_NODE)
	{
	  err = protocol_find_node (name, node);
	  if (! err)
	    netfs_nref (*node);
	}
      else
	err = EOPNOTSUPP; /* FIXME?  */
    }

 out:
  fshelp_touch (&dir->nn_stat, TOUCH_ATIME, netio_maptime);
  mutex_unlock (&dir->lock);
  if (err)
    *node = 0;
  else
    mutex_lock (&(*node)->lock);
  return err;
}

/* Return an argz string describing the current options.  Fill *ARGZ
   with a pointer to newly malloced storage holding the list and *LEN
   to the length of that storage.  */
error_t
netfs_append_args (char **argz, size_t *argz_len)
{
  return 0;
}

/* Make sure that NP->nn_stat is filled with current information.
   CRED identifies the user responsible for the operation. */
error_t
netfs_validate_stat (struct node *np, struct iouser *cred)
{
  return 0;
}

/* This should attempt a chmod call for the user specified by CRED on
   locked node NP, to change the owner to UID and the group to GID.  */
error_t
netfs_attempt_chown (struct iouser *cred, struct node *np,
		     uid_t uid, uid_t gid)
{
  return EOPNOTSUPP;
}

/* This should attempt a chauthor call for the user specified by CRED
   on locked node NP, thereby changing the author to AUTHOR.  */
error_t
netfs_attempt_chauthor (struct iouser *cred, struct node *np,
			uid_t author)
{
  return EOPNOTSUPP;
}

/* This should attempt a chmod call for the user specified by CRED on
   locked node NODE, to change the mode to MODE.  Unlike the normal
   Unix and Hurd meaning of chmod, this function is also used to
   attempt to change files into other types.  If such a transition is
   attempted which is impossible, then return EOPNOTSUPP.  */
error_t
netfs_attempt_chmod (struct iouser *cred, struct node *np,
		     mode_t mode)
{
  error_t err;

  /* We only support permission chmod's on the root node.  */
  if (np->nn->dir ||
      ((mode & S_IFMT) | (np->nn_stat.st_mode & S_IFMT))
      != (np->nn_stat.st_mode & S_IFMT))
    err = EOPNOTSUPP;
  else
    {
      if (! (err = fshelp_isowner (&np->nn_stat, cred)))
	{
	  np->nn_stat.st_mode = mode | (np->nn_stat.st_mode & S_IFMT);
	  fshelp_touch (&np->nn_stat, TOUCH_CTIME, netio_maptime);
	}
    }
  return err;
}

/* Attempt to turn locked node NP (user CRED) into a symlink with
   target NAME.  */
error_t
netfs_attempt_mksymlink (struct iouser *cred, struct node *np,
			 char *name)
{
  return EOPNOTSUPP;
}

/* Attempt to turn NODE (user CRED) into a device.  TYPE is either
   S_IFBLK or S_IFCHR.  NP is locked.  */
error_t
netfs_attempt_mkdev (struct iouser *cred, struct node *np,
		     mode_t type, dev_t indexes)
{
  return EOPNOTSUPP;
}

/* Attempt to set the passive translator record for FILE to ARGZ (of
   length ARGZLEN) for user CRED. NP is locked.  */
error_t
netfs_set_translator (struct iouser *cred, struct node *np,
		      char *argz, size_t argzlen)
{
  return EOPNOTSUPP;
}

/* For locked node NODE with S_IPTRANS set in its mode, look up the
   name of its translator.  Store the name into newly malloced
   storage, and return it in *ARGZ; set *ARGZ_LEN to the total length.  */
error_t
netfs_get_translator (struct node *node, char **argz,
		      size_t *argz_len)
{
  return EOPNOTSUPP;
}

/* This should attempt a chflags call for the user specified by CRED
   on locked node NP, to change the flags to FLAGS.  */
error_t
netfs_attempt_chflags (struct iouser *cred, struct node *np,
		       int flags)
{
  return EOPNOTSUPP;
}

/* This should attempt a utimes call for the user specified by CRED on
   locked node NP, to change the atime to ATIME and the mtime to
   MTIME.  If ATIME or MTIME is null, then set to the current time.  */
error_t
netfs_attempt_utimes (struct iouser *cred, struct node *np,
		      struct timespec *atime, struct timespec *mtime)
{
  return 0;
}

/* This should attempt to set the size of the locked file NP (for user
   CRED) to SIZE bytes long.  */
error_t
netfs_attempt_set_size (struct iouser *cred, struct node *np,
			off_t size)
{
  return 0;
}

/* This should attempt to fetch filesystem status information for the
   remote filesystem, for the user CRED. NP is locked.  */
error_t
netfs_attempt_statfs (struct iouser *cred, struct node *np,
		      struct statfs *st)
{
  return EOPNOTSUPP;
}

/* This should sync the locked file NP completely to disk, for the
   user CRED.  If WAIT is set, return only after the sync is
   completely finished.  */
error_t
netfs_attempt_sync (struct iouser *cred, struct node *np,
		    int wait)
{
  return EOPNOTSUPP;
}

/* This should sync the entire remote filesystem.  If WAIT is set,
   return only after the sync is completely finished.  */
error_t
netfs_attempt_syncfs (struct iouser *cred, int wait)
{
  return 0;
}

/* Delete NAME in DIR (which is locked) for USER.  */
error_t
netfs_attempt_unlink (struct iouser *user, struct node *dir,
		      char *name)
{
  return EOPNOTSUPP;
}

/* Attempt to rename the directory FROMDIR to TODIR. Note that neither
   of the specific nodes are locked.  */
error_t
netfs_attempt_rename (struct iouser *user, struct node *fromdir,
		      char *fromname, struct node *todir, 
		      char *toname, int excl)
{
  return EOPNOTSUPP;
}

/* Attempt to create a new directory named NAME in DIR (which is
   locked) for USER with mode MODE. */
error_t
netfs_attempt_mkdir (struct iouser *user, struct node *dir,
		     char *name, mode_t mode)
{
  return EOPNOTSUPP;
}

/* Attempt to remove directory named NAME in DIR (which is locked) for
   USER.  */
error_t
netfs_attempt_rmdir (struct iouser *user, 
		     struct node *dir, char *name)
{
  /* Let's allow unlinking of protocol directories.  */
  if (dir->nn->dir)
    return EOPNOTSUPP;
  return protocol_unregister (name);
}

/* Create a link in DIR with name NAME to FILE for USER. Note that
   neither DIR nor FILE are locked. If EXCL is set, do not delete the
   target.  Return EEXIST if NAME is already found in DIR.  */
error_t
netfs_attempt_link (struct iouser *user, struct node *dir,
		    struct node *file, char *name, int excl)
{
  return EOPNOTSUPP;
}

/* Attempt to create an anonymous file related to DIR (which is
   locked) for USER with MODE.  Set *NP to the returned file upon
   success. No matter what, unlock DIR.  */
error_t
netfs_attempt_mkfile (struct iouser *user, struct node *dir,
		      mode_t mode, struct node **np)
{
  return EOPNOTSUPP;
}

/* (We don't use this function!)  Attempt to create a file named NAME
   in DIR (which is locked) for USER with MODE.  Set *NP to the new
   node upon return.  On any error, clear *NP.  *NP should be locked
   on success; no matter what, unlock DIR before returning.  */
error_t
netfs_attempt_create_file (struct iouser *user, struct node *dir,
			   char *name, mode_t mode, struct node **np)
{
  return EOPNOTSUPP;
}

/* Read the contents of locked node NP (a symlink), for USER, into
   BUF.  */
error_t
netfs_attempt_readlink (struct iouser *user, struct node *np,
			char *buf)
{
  return EOPNOTSUPP;
}

/* libnetfs uses this functions once.  */
error_t
netfs_check_open_permissions (struct iouser *user, struct node *np,
			      int flags, int newnode)
{
  error_t err = 0;
  if (! err && (flags & O_READ))
    err = fshelp_access (&np->nn_stat, S_IREAD, user);
  if (! err && (flags & O_WRITE))
    err = fshelp_access (&np->nn_stat, S_IWRITE, user);
  if (! err && (flags & O_EXEC))
    err = fshelp_access (&np->nn_stat, S_IEXEC, user);
  return err;
}

/* Read from the locked file NP for user CRED starting at OFFSET and
   continuing for up to *LEN bytes.  Put the data at DATA.  Set *LEN
   to the amount successfully read upon return.  */
error_t
netfs_attempt_read (struct iouser *cred, struct node *np,
		    off_t offset, size_t *len, void *data)
{
  error_t err = 0;
  if (! (np->nn->flags & PORT_NODE))
    *len = 0;
  {
    addr_port_t addrport;
    char *bufp = (char *) data;
    mach_msg_type_number_t nread = *len;
    mach_port_t *ports;
    mach_msg_type_number_t nports;
    char *cdata = NULL;
    mach_msg_type_number_t clen = 0;
    int flags = 0;
    
    if (err)
      goto out;
    err = socket_recv (np->nn->sock, &addrport, flags, &bufp, &nread, 
		       &ports, &nports, &cdata, &clen, &flags,
		       nread);
    if (err)
      goto out;
    
    mach_port_deallocate (mach_task_self (), addrport);
    vm_deallocate (mach_task_self (), (vm_address_t) cdata, clen);
    if (bufp != (char *) data)
      {
	memcpy ((char *) data, bufp, nread);
	vm_deallocate (mach_task_self (), (vm_address_t) bufp, nread);
      }
  }
 out:
  return err;
}

/* Write to the locked file NP for user CRED starting at OFSET and
   continuing for up to *LEN bytes from DATA.  Set *LEN to the amount
   successfully written upon return.  */
error_t
netfs_attempt_write (struct iouser *cred, struct node *np,
		     off_t offset, size_t *len, void *data)
{
  error_t err = 0;
  if (! np->nn->connected)
    err = node_connect_socket (np);
  if (err)
    goto out;
  err = socket_send (np->nn->sock, MACH_PORT_NULL, 0, (char *) data,
		     *len, NULL, MACH_MSG_TYPE_COPY_SEND, 0, NULL, 0,
		     (int *) len);
 out:
  return err;
}

/* Return the valid access types (bitwise OR of O_READ, O_WRITE, and
   O_EXEC) in *TYPES for locked file NP and user CRED.  */
error_t
netfs_report_access (struct iouser *cred, struct node *np,
		     int *types)
{
  *types = 0;
  if (fshelp_access (&np->nn_stat, S_IREAD, cred) == 0)
    *types |= O_READ;
  if (fshelp_access (&np->nn_stat, S_IWRITE, cred) == 0)
    *types |= O_WRITE;
  if (fshelp_access (&np->nn_stat, S_IEXEC, cred) == 0)
    *types |= O_EXEC;
  return 0;
}

/* Create a new user from the specified UID and GID arrays.  */
struct iouser *
netfs_make_user (uid_t *uids, int nuids, uid_t *gids, int ngids)
{
  return NULL;
}

/* Node NP has no more references; free all its associated storage.  */
void
netfs_node_norefs (struct node *np)
{
  node_destroy (np);
}

/* Fill the array *DATA of size BUFSIZE with up to NENTRIES dirents
   from DIR (which is locked) starting with entry ENTRY for user CRED.
   The number of entries in the array is stored in *AMT and the number
   of bytes in *DATACNT.  If the supplied buffer is not large enough
   to hold the data, it should be grown.  Taken from hostmux.  */
error_t
netfs_get_dirents (struct iouser *cred, struct node *dir,
		   int first_entry, int num_entries, char **data,
		   mach_msg_type_number_t *data_len,
		   vm_size_t max_data_len, int *data_entries)
{
  error_t err;
  int count;
  size_t size = 0;		/* Total size of our return block.  */
  struct node *first_name, *nm;

  /* Add the length of a directory entry for NAME to SIZE and return true,
     unless it would overflow MAX_DATA_LEN or NUM_ENTRIES, in which case
     return false.  */
  int bump_size (const char *name)
    {
      if (num_entries == -1 || count < num_entries)
	{
	  size_t new_size = size + DIRENT_LEN (strlen (name));
	  if (max_data_len > 0 && new_size > max_data_len)
	    return 0;
	  size = new_size;
	  count++;
	  return 1;
	}
      else
	return 0;
    }

  if (dir->nn->flags & PORT_NODE)
    return ENOTDIR;

  /* Find the first entry.  */
  for (first_name = dir->nn->entries, count = 0;
       first_name && first_entry > count;
       first_name = first_name->next)
    count++;

  count = 0;

  /* Make space for the `.' and `..' entries.  */
  if (first_entry == 0)
    bump_size (".");
  if (first_entry <= 1)
    bump_size ("..");

  /* See how much space we need for the result.  */
  for (nm = first_name; nm; nm = nm->next)
    if (!bump_size (nm->nn->protocol->name))
      break;

  /* Allocate it.  */
  *data = mmap (0, size, PROT_READ|PROT_WRITE, MAP_ANON, 0, 0);
  err = ((void *) *data == (void *) -1) ? errno : 0;

  if (! err)
    /* Copy out the result.  */
    {
      char *p = *data;

      int add_dir_entry (const char *name, ino_t fileno, int type)
	{
	  if (num_entries == -1 || count < num_entries)
	    {
	      struct dirent hdr;
	      size_t name_len = strlen (name);
	      size_t sz = DIRENT_LEN (name_len);

	      if (sz > size)
		return 0;
	      else
		size -= sz;

	      hdr.d_fileno = fileno;
	      hdr.d_reclen = sz;
	      hdr.d_type = type;
	      hdr.d_namlen = name_len;

	      memcpy (p, &hdr, DIRENT_NAME_OFFS);
	      strcpy (p + DIRENT_NAME_OFFS, name);
	      p += sz;

	      count++;

	      return 1;
	    }
	  else
	    return 0;
	}

      *data_len = size;
      *data_entries = count;

      count = 0;

      /* Add `.' and `..' entries.  */
      if (first_entry == 0)
	add_dir_entry (".", 2, DT_DIR);
      if (first_entry <= 1)
	add_dir_entry ("..", 2, DT_DIR);

      /* Fill in the real directory entries.  */
      for (nm = first_name; nm; nm = nm->next)
	if (!add_dir_entry (nm->nn->protocol->name, nm->nn->protocol->id,
			    DT_DIR))
	  break;
    }

  fshelp_touch (&dir->nn_stat, TOUCH_ATIME, netio_maptime);
  return err;
}

/* We need our special version of netfs_S_io_read.  */

error_t
block_for_reading (socket_t sock)
{
  mach_port_t reply_port;
  int type = SELECT_READ;
  error_t err;
  do
    {
      reply_port = mach_reply_port ();
      if (reply_port == MACH_PORT_NULL)
	return EIEIO; /* FIXME?  */
      err = io_select (sock, reply_port, 1000, &type);
    }
  while (err == EMACH_RCV_TIMED_OUT);
  return err;
}

/* Implement the io_read interface.  This is the original
   implementation from libnetfs with one special hack at the
   beginning.  */
error_t
netfs_S_io_read (struct protid *user,
		 char **data,
		 mach_msg_type_number_t *datalen,
		 off_t offset,
		 mach_msg_type_number_t amount)
{
  error_t err;
  off_t start;
  struct node *node;
  int alloced = 0;

  if (!user)
    return EOPNOTSUPP;

  node = user->po->np;

  /* This is our special hack.  */
  if (! node->nn->connected)
    err = node_connect_socket (node);
  if (err || node->nn->flags & PORT_NODE)
    err = block_for_reading (node->nn->sock);
  if (err)
    return err;

  mutex_lock (&user->po->np->lock);

  if ((user->po->openstat & O_READ) == 0)
    {
      mutex_unlock (&node->lock);
      return EBADF;
    }

  if (amount > *datalen)
    {
      alloced = 1;
      *data = mmap (0, amount, PROT_READ|PROT_WRITE, MAP_ANON, 0, 0);
    }
  *datalen = amount;

  start = (offset == -1 ? user->po->filepointer : offset);

  if (start < 0)
    err = EINVAL;
  else if (S_ISLNK (node->nn_stat.st_mode))
    /* Read from a symlink.  */
    {
      off_t size = node->nn_stat.st_size;

      if (start + amount > size)
	amount = size - start;

      if (start >= size)
	{
	  *datalen = 0;
	  err = 0;
	}
      else if (amount < size || start > 0)
	{
	  char *whole_link = alloca (size);
	  err = netfs_attempt_readlink (user->user, node, *data);
	  if (! err)
	    {
	      memcpy (*data, whole_link + start, amount);
	      *datalen = amount;
	    }
	}
      else
	err = netfs_attempt_readlink (user->user, node, *data);
    }
  else
    /* Read from a normal file.  */
    err = netfs_attempt_read (user->user, node, start, datalen, *data);

  if (offset == -1 && !err)
    user->po->filepointer += *datalen;

  mutex_unlock (&node->lock);

  if (err && alloced)
    munmap (*data, amount);

  if (!err && alloced && (round_page (*datalen) < round_page (amount)))
    munmap (*data + round_page (*datalen),
	    round_page (amount) - round_page (*datalen));

  return err;
}
