/* gopherfs interface to libnetfs

   Copyright (C) 2000 Free Software Foundation, Inc.
   Written by Igor Khavkine <igor@twu.net>
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <maptime.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <mach.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "gopherfs.h"

#include <hurd/hurd_types.h>
#include <hurd/netfs.h>


/* Attempt to create a file named NAME in DIR for USER with MODE.  Set *NODE
   to the new node upon return.  On any error, clear *NODE.  *NODE should be
   locked on success; no matter what, unlock DIR before returning.  */
error_t
netfs_attempt_create_file (struct iouser *user, struct node *dir,
													 char *name, mode_t mode, struct node **node)
{
	*node = NULL;
	mutex_unlock (&dir->lock);
	return EROFS;
}

/* Node NODE is being opened by USER, with FLAGS.  NEWNODE is nonzero if we
   just created this node.  Return an error if we should not permit the open
   to complete because of a permission restriction. */
error_t
netfs_check_open_permissions (struct iouser *user, struct node *node,
															int flags, int newnode)
{
	error_t err = 0;
	if (!err && (flags & O_READ))
		err = fshelp_access (&node->nn_stat, S_IREAD, user);
	if (!err && (flags & O_WRITE))
		err = fshelp_access (&node->nn_stat, S_IWRITE, user);
	if (!err && (flags & O_EXEC))
		err = fshelp_access (&node->nn_stat, S_IEXEC, user);
	return err;
}

/* This should attempt a utimes call for the user specified by CRED on node
   NODE, to change the atime to ATIME and the mtime to MTIME. */
error_t
netfs_attempt_utimes (struct iouser *cred, struct node *node,
											struct timespec *atime, struct timespec *mtime)
{
	error_t err = 0;
	int flags = TOUCH_CTIME;

	if (!err)
		err = fshelp_isowner (&node->nn_stat, cred);

	if (!err) {
		if (atime) {
			node->nn_stat.st_atime = atime->tv_sec;
			node->nn_stat.st_atime_usec = atime->tv_nsec / 1000;
		} else
			flags |= TOUCH_ATIME;

		if (mtime) {
			node->nn_stat.st_mtime = mtime->tv_sec;
			node->nn_stat.st_mtime_usec = mtime->tv_nsec / 1000;
		} else
			flags |= TOUCH_MTIME;

		fshelp_touch (&node->nn_stat, flags, gopherfs_maptime);
	}

	return err;
}

/* Return the valid access types (bitwise OR of O_READ, O_WRITE, and O_EXEC)
   in *TYPES for file NODE and user CRED.  */
error_t
netfs_report_access (struct iouser *cred, struct node *node, int *types)
{
	*types = 0;
	if (fshelp_access (&node->nn_stat, S_IREAD, cred) == 0)
		*types |= O_READ;
	if (fshelp_access (&node->nn_stat, S_IWRITE, cred) == 0)
		*types |= O_WRITE;
	if (fshelp_access (&node->nn_stat, S_IEXEC, cred) == 0)
		*types |= O_EXEC;

	return 0;
}

/* Trivial definitions.  */

/* Make sure that NP->nn_stat is filled with current information.  CRED
   identifies the user responsible for the operation.  */
error_t netfs_validate_stat (struct node *node, struct iouser *cred)
{
	/* Assume that gopher data is static and will not change for however
	 * long we keep the cached node. Hence nn_stat is always valid.
	 * XXX: would be a good idea to introduce a forced refresh call */
	return 0;
}

/* This should sync the file NODE completely to disk, for the user CRED.  If
   WAIT is set, return only after sync is completely finished.  */
error_t
netfs_attempt_sync (struct iouser *cred, struct node *node, int wait)
{
	return 0;
}

#if 0
/* The granularity with which we allocate space to return our result.  */
#define DIRENTS_CHUNK_SIZE	(8*1024)

/* Returned directory entries are aligned to blocks this many bytes long.
   Must be a power of two.  */
#define DIRENT_ALIGN 4
#define DIRENT_NAME_OFFS (offsetof (struct dirent, d_name))

/* Length is structure before the name + the name + '\0', all
   padded to a four-byte alignment.  */
#define DIRENT_LEN(name_len)						      \
  ((DIRENT_NAME_OFFS + (name_len) + 1 + (DIRENT_ALIGN - 1))		      \
   & ~(DIRENT_ALIGN - 1))
#endif

/* Fetch a directory  */
error_t
netfs_get_dirents (struct iouser *cred, struct node *dir,
									 int first_entry, int max_entries, char **data,
									 mach_msg_type_number_t *data_len,
									 vm_size_t max_data_len, int *data_entries)
{
	error_t err = 0;
	struct node *nd;
	int count;
	size_t size;
	char *p;

	if (!dir || dir->nn->type != GPHR_DIR)
		return ENOTDIR;

	if (!dir->nn->ents && !dir->nn->noents) {
		mutex_lock (&dir->lock);
		err = fill_dirnode (dir->nn);
		mutex_unlock (&dir->lock);
		if (err)
			return err;
	}

	for (nd = dir->nn->ents; first_entry > 0 && nd; first_entry--, nd=nd->next)
		;
	if (!nd)
		max_entries = 0;

	if (max_entries == 0) {
		*data_len = 0;
		*data_entries = 0;
		*data = NULL;
		return 0;
	}

#define DIRENTS_CHUNK_SIZE  (8*1024)
	size = (max_data_len == 0 || max_data_len > DIRENTS_CHUNK_SIZE)
			? DIRENTS_CHUNK_SIZE
			: max_data_len;
	errno = 0;
	*data = NULL;
	err = vm_allocate (mach_task_self (), (vm_address_t *)data,
			size, /*anywhere*/TRUE);
	if (err)
		return err;
	err = vm_protect (mach_task_self (), (vm_address_t)*data, size,
			/*set_maximum*/FALSE, VM_PROT_READ | VM_PROT_WRITE);
	if (err)
		return err;

	p = *data;
	for (count = 0; nd && (max_entries == -1 || count < max_entries); count++) {
#define DIRENT_NAME_OFFS offsetof (struct dirent, d_name)
		vm_address_t addr;
		struct dirent ent;

		ent.d_fileno = nd->nn_stat.st_ino;
		ent.d_type = IFTODT (nd->nn_stat.st_mode);
		ent.d_namlen = strlen (nd->nn->name);
		ent.d_reclen = DIRENT_NAME_OFFS + ent.d_namlen + 1;
		if (p - *data + ent.d_reclen > size) {
			size_t extra = ((p - *data + ent.d_reclen - size)/DIRENTS_CHUNK_SIZE + 1)*DIRENTS_CHUNK_SIZE;
			if (extra + size > max_data_len)
				break;
			addr = (vm_address_t) (*data + size);
			err = vm_allocate (mach_task_self (), &addr, extra, /*anywhere*/FALSE);
			if (err)
				break;
			err = vm_protect (mach_task_self (), (vm_address_t)*data, size,
					/*set_maximum*/FALSE, VM_PROT_READ | VM_PROT_WRITE);
			if (err)
				break;
			size += extra;
		}
		/* copy the dirent structure */
		memcpy (p, &ent, DIRENT_NAME_OFFS);
		/* copy ent.d_name */
		strncpy (p + DIRENT_NAME_OFFS, nd->nn->name, ent.d_namlen);
		p += ent.d_reclen;

		nd = nd->next;
	}
	if (err) {
		vm_deallocate (mach_task_self (), (vm_address_t)*data, size);
		*data_len = 0;
		*data_entries = 0;
		*data = NULL;
		return 0;
	} else {
		vm_address_t alloc_end = (vm_address_t) (*data + size);
		vm_address_t real_end = round_page (p);

		if (alloc_end > real_end)
			vm_deallocate (mach_task_self (), real_end, alloc_end - real_end);
		*data_len = p - *data;
		*data_entries = count;
	}
	
	return 0;
}

/* Lookup NAME in DIR for USER; set *NODE to the found name upon return.  If
   the name was not found, then return ENOENT.  On any error, clear *NODE.
   (*NODE, if found, should be locked, this call should unlock DIR no matter
   what.) */
error_t netfs_attempt_lookup (struct iouser *user, struct node *dir,
															char *name, struct node ** node)
{
	error_t err;
	struct node *nd;

	if (dir->nn->type != GPHR_DIR)
		err = ENOTDIR;
	for (nd = dir->nn->ents; nd && strcmp (name, nd->nn->name); nd = nd->next)
		;
	if (nd) {
		mutex_lock (&nd->lock);
		*node = nd;
		err = 0;
	} else
		err = ENOENT;

	mutex_unlock (&dir->lock);
	return 0;
}

/* Delete NAME in DIR for USER. */
error_t netfs_attempt_unlink (struct iouser *user, struct node *dir,
															char *name)
{
	return EROFS;
}

/* Note that in this one call, neither of the specific nodes are locked. */
error_t netfs_attempt_rename (struct iouser *user, struct node *fromdir,
		char *fromname, struct node *todir,
		char *toname, int excl)
{
	return EROFS;
}

/* Attempt to create a new directory named NAME in DIR for USER with mode
   MODE.  */
error_t netfs_attempt_mkdir (struct iouser *user, struct node *dir,
														 char *name, mode_t mode)
{
	return EROFS;
}

/* Attempt to remove directory named NAME in DIR for USER. */
error_t netfs_attempt_rmdir (struct iouser *user,
														 struct node *dir, char *name)
{
	return EROFS;
}

/* This should attempt a chmod call for the user specified by CRED on node
   NODE, to change the owner to UID and the group to GID. */
error_t netfs_attempt_chown (struct iouser *cred, struct node *node,
														 uid_t uid, uid_t gid)
{
	return EROFS;
}

/* This should attempt a chauthor call for the user specified by CRED on node
   NODE, to change the author to AUTHOR. */
error_t netfs_attempt_chauthor (struct iouser *cred, struct node *node,
																uid_t author)
{
	return EROFS;
}

/* This should attempt a chmod call for the user specified by CRED on node
   NODE, to change the mode to MODE.  Unlike the normal Unix and Hurd meaning
   of chmod, this function is also used to attempt to change files into other
   types.  If such a transition is attempted which is impossible, then return
   EOPNOTSUPP.  */
error_t netfs_attempt_chmod (struct iouser *cred, struct node *node,
														 mode_t mode)
{
	return EROFS;
}

/* Attempt to turn NODE (user CRED) into a symlink with target NAME. */
error_t netfs_attempt_mksymlink (struct iouser *cred, struct node *node,
																 char *name)
{
	return EROFS;
}

/* Attempt to turn NODE (user CRED) into a device.  TYPE is either S_IFBLK or
   S_IFCHR. */
error_t netfs_attempt_mkdev (struct iouser *cred, struct node *node,
														 mode_t type, dev_t indexes)
{
	return EROFS;
}

/* Attempt to set the passive translator record for FILE to ARGZ (of length
   ARGZLEN) for user CRED. */
error_t netfs_set_translator (struct iouser *cred, struct node *node,
															char *argz, size_t argzlen)
{
	return EROFS;
}

#if 0
/* The user may define this function (but should define it together with
   netfs_set_translator).  For locked node NODE with S_IPTRANS set in its
   mode, look up the name of its translator.  Store the name into newly
   malloced storage, and return it in *ARGZ; set *ARGZ_LEN to the total
   length.  */
error_t netfs_get_translator (struct node *node, char **argz, size_t *argz_len)
{
}
#endif

/* This should attempt a chflags call for the user specified by CRED on node
   NODE, to change the flags to FLAGS. */
error_t netfs_attempt_chflags (struct iouser *cred, struct node *node,
															 int flags)
{
	return EROFS;
}

/* This should attempt to set the size of the file NODE (for user CRED) to
   SIZE bytes long. */
error_t netfs_attempt_set_size (struct iouser *cred, struct node *node,
																off_t size)
{
	return EROFS;
}

/* This should attempt to fetch filesystem status information for the remote
   filesystem, for the user CRED. */
error_t netfs_attempt_statfs (struct iouser *cred, struct node *node,
															struct statfs *st)
{
	return EOPNOTSUPP;
}

/* This should sync the entire remote filesystem.  If WAIT is set, return
   only after sync is completely finished.  */
error_t netfs_attempt_syncfs (struct iouser *cred, int wait)
{
	return 0;
}

/* Create a link in DIR with name NAME to FILE for USER.  Note that neither
   DIR nor FILE are locked.  If EXCL is set, do not delete the target, but
   return EEXIST if NAME is already found in DIR.  */
error_t netfs_attempt_link (struct iouser *user, struct node *dir,
														struct node *file, char *name, int excl)
{
	return EROFS;
}

/* Attempt to create an anonymous file related to DIR for USER with MODE.
   Set *NODE to the returned file upon success.  No matter what, unlock DIR. */
error_t netfs_attempt_mkfile (struct iouser *user, struct node *dir,
															mode_t mode, struct node ** node)
{
	*node = NULL;
	mutex_unlock (&dir->lock);
	return EROFS;
}

/* maximum numer of symlinks, does not really apply, so set to 0 */
int netfs_maxsymlinks = 0;
/* Read the contents of NODE (a symlink), for USER, into BUF. */
error_t netfs_attempt_readlink (struct iouser *user, struct node *node,
																char *buf)
{
	return EINVAL;
}

/* Read from the file NODE for user CRED starting at OFFSET and continuing for
   up to *LEN bytes.  Put the data at DATA.  Set *LEN to the amount
   successfully read upon return.  */
error_t netfs_attempt_read (struct iouser *cred, struct node *node,
														off_t offset, size_t *len, void *data)
{
	error_t err;
	int remote_fd;
	ssize_t read_len;

	err = open_selector (node->nn, &remote_fd);
	if (err)
		return err;

	read_len = pread (remote_fd, data, *len, offset);
	if (read_len < 0)
		err = errno;
	else {
		*len = (size_t)read_len;
		err = 0;
	}
	
	return err;
}

/* Write to the file NODE for user CRED starting at OFSET and continuing for up
   to *LEN bytes from DATA.  Set *LEN to the amount seccessfully written upon
   return. */
error_t netfs_attempt_write (struct iouser *cred, struct node *node,
														 off_t offset, size_t *len, void *data)
{
	return EROFS;
}

/* XXX doesn't say anywhere what this must do */
#if 0  
/* The user must define this function.  Create a new user
   from the specified UID and GID arrays. */
struct iouser *netfs_make_user (uid_t *uids, int nuids,
				       uid_t *gids, int ngids)
{
}
#endif

/* The user must define this function.  Node NP is all done; free
   all its associated storage. */
void netfs_node_norefs (struct node *np)
{
	mutex_lock (&np->lock);
	*np->prevp = np->next;
	np->next->prevp = np->prevp;
	free_node (np);
	/* XXX: remove node from tree and delete the cache entry */
}
