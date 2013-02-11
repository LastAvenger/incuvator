/*
  Copyright (C) 1997, 2002, 2004, 2007, 2009 Free Software Foundation, Inc.
  Copyright (C) 2004, 2007, 2009 Giuseppe Scrivano.
  Copyright (C) 2012 Ludovic Courtès <ludo@gnu.org>

  Written by Giuseppe Scrivano <gscrivano@gnu.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2, or (at
  your option) any later version.
  
  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "smb.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <hurd/fsys.h>
#include <dirent.h>
#include <sys/time.h>
#include <maptime.h>
#include <pthread.h>

/* Returned directory entries are aligned to blocks this many bytes long.
   Must be a power of two.  */
#define DIRENT_ALIGN 4
#define DIRENT_NAME_OFFS offsetof (struct dirent, d_name)
/* Length is structure before the name + the name + '\0', all
   padded to a four-byte alignment.  */
#define DIRENT_LEN(name_len)    ((DIRENT_NAME_OFFS + (name_len) + 1 \
                               + (DIRENT_ALIGN - 1)) & ~(DIRENT_ALIGN - 1))

struct smb_credentials credentials =
  {
    .workgroup = "WORKGROUP",
    .password = ""
  };

static volatile struct mapped_time_value *maptime;
static pthread_mutex_t smb_mutex;

char *netfs_server_name = "smbfs";
char *netfs_server_version = "0.1";
int netfs_maxsymlinks = 0;

/* A node in the SMB file system.  */
struct netnode
{
  struct node *node;				/* corresponding node */
  char *file_name;				/* base name */
  char *abs_file_name;				/* absolute SMB path */
  struct node *dir;				/* parent directory */
  struct node *entries;				/* entries, if a directory */
};

/* Return a zeroed stat buffer for CRED.  */
static struct stat
empty_stat (void)
{
  struct stat st;

  memset (&st, 0, sizeof st);

  st.st_fstype = FSTYPE_MISC;
  st.st_fsid = getpid ();

  return st;
}

/* Initialize *NODE with a new node within directory DIR, and for user
   CRED.  */
static error_t
create_node (struct node *dir, struct node **node)
{
  struct netnode *n;

  n = calloc (1, sizeof *n);
  if (!n)
    return ENOMEM;

  n->dir = dir;
  *node = n->node = netfs_make_node (n);
  if (!(*node))
    {
      free (n);
      return ENOMEM;
    }

  if (dir != NULL)
    {
      /* Increase DIR's reference counter.  */
      netfs_nref (dir);

      (*node)->next = NULL;

      if (dir->nn->entries != NULL)
	{
	  /* Insert *NODE at the end.  */
	  struct node *e;

	  for (e = dir->nn->entries;
	       e->next != NULL;
	       e = e->next);

	  (*node)->prevp = &e->next;
	  e->next = *node;
	}
      else
	{
	  /* *NODE is the first entry in DIR.  */
	  (*node)->prevp = &dir->nn->entries;
	  dir->nn->entries = *node;
	}
    }

  (*node)->nn_stat = empty_stat ();

  return 0;
}

/* Return the node named FILENAME in DIR, or NULL on failure.  */
static struct netnode *
search_node (const char *filename, struct node *dir)
{
  struct node *entry;

  for (entry = dir->nn->entries;
       entry != NULL;
       entry = entry->next)
    {
      if (strcmp (entry->nn->file_name, filename) == 0)
	return entry->nn;
    }

  return 0;
}

/* Remove NODE and free any associated resources.  */
static void
remove_node (struct node *node)
{
  *node->prevp = node->next;
  node->next = NULL;
  node->prevp = NULL;
  if (node->nn->file_name != NULL)
    free (node->nn->file_name);
  if (node->nn->abs_file_name != NULL)
    free (node->nn->abs_file_name);
  free (node->nn);
  node->nn = NULL;
}

/* Create the file system's root node.  */
static void
create_root_node ()
{
  struct node *node;
  int err = create_node (NULL, &node);
  if (err)
    return;

  netfs_root_node = node;
  node->nn->abs_file_name = strdup (credentials.share);

  netfs_validate_stat (node, 0);
}


/* Add FILENAME in directory TOP and set *NN to the resulting node.  */
static error_t
add_node (char *filename, struct node *top, struct netnode **nn)
{
  int err;
  struct netnode *n;
  struct node *newnode;

  n = search_node (filename, top);
  if (n == NULL)
    {
      /* Nothing known about FILENAME, so create a new node.  */
      err = create_node (top, &newnode);
      if (err)
	return err;

      n = newnode->nn;
      n->node = newnode;
      n->dir = top;
      n->file_name = strdup (filename);
      asprintf (&n->abs_file_name, "%s/%s",
		top->nn->abs_file_name, filename);
      if (n->file_name == NULL || n->abs_file_name == NULL)
	{
	  netfs_nput (newnode);
	  return ENOMEM;
	}
    }
  else
    /* A node already exists for FILENAME.  */
    newnode = n->node;

  /* Make sure FILENAME actually exists.  */
  pthread_mutex_lock (&smb_mutex);
  err = smbc_stat (n->abs_file_name, &n->node->nn_stat);
  pthread_mutex_unlock (&smb_mutex);

  if (err != 0)
    {
      /* FILENAME cannot be accessed, so forget about NEWNODE.  */
      err = errno;
      netfs_nput (newnode);
    }
  else
    *nn = n;

  return err;
}


error_t
netfs_validate_stat (struct node *np, struct iouser *cred)
{
  np->nn_stat = empty_stat ();
  np->nn_stat.st_ino = (uintptr_t) np >> 3UL;

  pthread_mutex_lock (&smb_mutex);
  int err = smbc_stat (np->nn->abs_file_name, &np->nn_stat);
  pthread_mutex_unlock (&smb_mutex);
  if (err)
    return errno;

  np->nn_stat.st_author = np->nn_stat.st_uid;

  return 0;
}

error_t
netfs_attempt_chown (struct iouser * cred, struct node * np, uid_t uid,
		     uid_t gid)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_chauthor (struct iouser * cred, struct node * np, uid_t author)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_chmod (struct iouser * cred, struct node * np, mode_t mode)
{
  int err;
  pthread_mutex_lock (&smb_mutex);
  err = smbc_chmod (np->nn->abs_file_name, mode);
  pthread_mutex_unlock (&smb_mutex);

  if (err)
    return errno;
  else 
    return 0;
}

error_t
netfs_attempt_mksymlink (struct iouser * cred, struct node * np, char *name)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_mkdev (struct iouser * cred, struct node * np, mode_t type,dev_t indexes)
{
  return EOPNOTSUPP;
}

error_t
netfs_set_translator (struct iouser * cred, struct node * np, char *argz,size_t argzlen)
{
  return 0;
}

error_t
netfs_get_translator (struct node * node, char **argz, size_t * argz_len)
{
  return 0;
}

error_t
netfs_attempt_chflags (struct iouser * cred, struct node * np, int flags)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_utimes (struct iouser * cred, struct node * np,
		      struct timespec * atime, struct timespec * mtime)
{
  int err;
  struct timeval tv;
  tv.tv_usec = 0;/* Not used by samba.  */
  if (mtime)
    tv.tv_sec = mtime->tv_sec;
  else
    maptime_read (maptime, &tv);

  pthread_mutex_lock (&smb_mutex);
  err = smbc_utimes (np->nn->abs_file_name, &tv);
  pthread_mutex_unlock (&smb_mutex);

  if(err)
    return errno;
  else 
    return 0;
}

error_t
netfs_attempt_set_size (struct iouser *cred, struct node *np, loff_t size)
{
  int fd, ret, saved_errno;

  pthread_mutex_lock (&smb_mutex);
  fd = smbc_open (np->nn->abs_file_name, O_WRONLY, 0);
  pthread_mutex_unlock (&smb_mutex);

  if (fd < 0)
    return errno;

  pthread_mutex_lock (&smb_mutex);
  ret = smbc_ftruncate (fd, size);
  saved_errno = ret != 0 ? errno : 0;
  smbc_close (fd);
  pthread_mutex_unlock (&smb_mutex);

  return saved_errno;
}

error_t
netfs_attempt_statfs (struct iouser * cred, struct node * np,
		      fsys_statfsbuf_t * st)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_sync (struct iouser * cred, struct node * np, int wait)
{
  return 0;
}

error_t
netfs_attempt_syncfs (struct iouser * cred, int wait)
{
  return 0;
}

error_t
netfs_attempt_lookup (struct iouser * user, struct node * dir, char *name,
		      struct node ** np)
{
  struct netnode *n;
  error_t err = 0;

  if (*name == '\0' || strcmp (name, ".") == 0)	/*Current directory */
    {
      netfs_nref (dir);/*Add a reference to current directory */
      *np = dir;
      pthread_mutex_unlock (&dir->lock);
      return 0;
    }
  else if (strcmp (name, "..") == 0)	/*Parent directory */
    {
      if (dir->nn->dir)
        {
          *np = dir->nn->dir;
          if (*np)
            {
              netfs_nref (*np);
              err = 0;
            }
          else
            err = ENOENT;
        }
      else
        {
          err = ENOENT;
          *np = 0;
        }

      pthread_mutex_unlock (&dir->lock);
      return err;    
    }

  pthread_mutex_unlock (&dir->lock);
  err = add_node (name, dir, &n);

  if(err)
    return err;

  *np = n->node;
  netfs_nref (*np);

  if (*np)
    err = 0;

  return err;
}

error_t
netfs_attempt_unlink (struct iouser * user, struct node * dir, char *name)
{
  char *filename;

  asprintf (&filename, "%s/%s", dir->nn->abs_file_name, name);

  if (!filename)
    return ENOMEM;

  pthread_mutex_lock (&smb_mutex);
  error_t err = smbc_unlink (filename);
  pthread_mutex_unlock (&smb_mutex);  
  
  free (filename);

  if (err)
    return errno;
  else
    return 0;
}

error_t
netfs_attempt_rename (struct iouser * user, struct node * fromdir,
		      char *fromname, struct node * todir, char *toname,
		      int excl)
{
  char *filename;		/* Origin file name.  */
  char *filename2;		/* Destination file name.  */

  asprintf (&filename, "%s/%s", fromdir->nn->abs_file_name, fromname);
  if (!filename)
    return ENOMEM;

  asprintf (&filename2, "%s/%s", todir->nn->abs_file_name, toname);
  if (!filename2)
    {
      free (filename);
      return ENOMEM;
    }

  pthread_mutex_lock (&smb_mutex);
  error_t err = smbc_rename (filename, filename2);
  pthread_mutex_unlock (&smb_mutex);

  free (filename);
  free (filename2);
  return err ? errno : 0;
}

error_t
netfs_attempt_mkdir (struct iouser * user, struct node * dir, char *name,
		     mode_t mode)
{
  char *filename;
  error_t err;

  asprintf (&filename, "%s/%s", dir->nn->abs_file_name,name);
  if (!filename)
    return ENOMEM;

  pthread_mutex_lock (&smb_mutex);
  err = smbc_mkdir (filename, mode);
  pthread_mutex_unlock (&smb_mutex);  
  
  free (filename);
  return err ? errno : 0;
}

error_t
netfs_attempt_rmdir (struct iouser * user, struct node * dir, char *name)
{
  char *filename;
  error_t err;

  asprintf (&filename, "%s/%s", dir->nn->abs_file_name, name);
  if (!filename)
    return ENOMEM;

  pthread_mutex_lock (&smb_mutex);
  err = smbc_rmdir (filename);
  pthread_mutex_unlock (&smb_mutex);  

  free(filename);
  return err ? errno : 0;
}

error_t
netfs_attempt_link (struct iouser * user, struct node * dir,
		    struct node * file, char *name, int excl)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_mkfile (struct iouser * user, struct node * dir, mode_t mode,
		      struct node ** np)
{
  pthread_mutex_unlock (&dir->lock);
  return EOPNOTSUPP;
}

error_t
netfs_attempt_create_file (struct iouser * user, struct node * dir,
			   char *name, mode_t mode, struct node ** np)
{
  error_t err = 0;
  char *filename;
  struct netnode *nn;
  int fd;

  *np = 0;

  asprintf (&filename, "%s/%s", dir->nn->abs_file_name, name);
  if (!filename)
    return ENOMEM;

  pthread_mutex_lock (&smb_mutex);
  fd = smbc_open (filename, O_WRONLY | O_CREAT, mode);
  if (fd < 0)
    {
      pthread_mutex_unlock (&smb_mutex);  
      pthread_mutex_unlock (&dir->lock);
      return errno;
    }
  smbc_close (fd);
  pthread_mutex_unlock (&smb_mutex);    

  err = add_node (name, dir, &nn);

  if (err)
    {
      pthread_mutex_unlock (&dir->lock);
      return err;
    }

  *np = nn->node;

  pthread_mutex_unlock (&dir->lock);
  return 0;
}

error_t
netfs_attempt_readlink (struct iouser * user, struct node * np, char *buf)
{
  return EOPNOTSUPP;
}

error_t
netfs_check_open_permissions (struct iouser * user, struct node * np,
			      int flags, int newnode)
{
  error_t err;
  io_statbuf_t  nn_stat;
  
  pthread_mutex_lock (&smb_mutex);
  err = smbc_stat (np->nn->abs_file_name, &nn_stat);
  pthread_mutex_unlock (&smb_mutex);
   
  if (err)
    return errno;

  if (flags & O_READ)
    err = !(S_IREAD & nn_stat.st_mode);
  if (flags & O_WRITE)
    err |= !(S_IWRITE & nn_stat.st_mode);
  if (flags & O_EXEC)
    err |= !(S_IEXEC & nn_stat.st_mode);

  return err?EPERM:0;
}

error_t
netfs_attempt_read (struct iouser * cred, struct node * np, loff_t offset,
		    size_t * len, void *data)
{
  int fd;
  int ret = 0;

  pthread_mutex_lock (&smb_mutex);
  fd = smbc_open (np->nn->abs_file_name, O_RDONLY, 0);
  pthread_mutex_unlock (&smb_mutex);

  if (fd < 0)
    {
      *len = 0;
      return errno;
    }

  pthread_mutex_lock (&smb_mutex);
  ret = smbc_lseek (fd, offset, SEEK_SET);
  pthread_mutex_unlock (&smb_mutex);
  
  if ((ret < 0) || (ret != offset))
    {
      *len = 0;
      pthread_mutex_lock (&smb_mutex);
      smbc_close (fd);
      pthread_mutex_unlock (&smb_mutex);
      return errno;
    }

  pthread_mutex_lock (&smb_mutex);
  ret = smbc_read (fd, data, *len);
  pthread_mutex_unlock (&smb_mutex);

  if (ret < 0)
    {
      *len = 0;
      pthread_mutex_lock (&smb_mutex);
      smbc_close (fd);
      pthread_mutex_unlock (&smb_mutex);
      return errno;
    }

  *len = ret;
  pthread_mutex_lock (&smb_mutex);
  smbc_close (fd);
  pthread_mutex_unlock (&smb_mutex);
  return 0;
}

error_t
netfs_attempt_write (struct iouser * cred, struct node * np, loff_t offset,
		     size_t * len, void *data)
{
  int ret = 0;
  int fd;

  pthread_mutex_lock (&smb_mutex);
  fd = smbc_open (np->nn->abs_file_name, O_WRONLY, 0);
  pthread_mutex_unlock (&smb_mutex);

  if (fd < 0)
    {
      *len = 0;
      return errno;
    }
  pthread_mutex_lock (&smb_mutex);
  ret = smbc_lseek (fd, offset, SEEK_SET);
  pthread_mutex_unlock (&smb_mutex);
  
  if ((ret < 0) || (ret != offset))
    {
      *len = 0;
      pthread_mutex_lock (&smb_mutex);
      smbc_close (fd);
      pthread_mutex_unlock (&smb_mutex);
      return errno;
    }
  pthread_mutex_lock (&smb_mutex);
  ret = smbc_write (fd, data, *len);
  pthread_mutex_unlock (&smb_mutex);
  
  if (ret < 0)
    {
      *len = 0;
      pthread_mutex_lock (&smb_mutex);
      smbc_close (fd);
      pthread_mutex_unlock (&smb_mutex);      
      return errno;
    }

  *len = ret;
  pthread_mutex_lock (&smb_mutex);
  smbc_close (fd);
  pthread_mutex_unlock (&smb_mutex);      

  return 0;
}

error_t
netfs_report_access (struct iouser * cred, struct node * np, int *types)
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

struct iouser *
netfs_make_user (uid_t * uids, int nuids, uid_t * gids, int ngids)
{
  return 0;
}

void
netfs_node_norefs (struct node *np)
{
  remove_node (np);
}

error_t
netfs_get_dirents (struct iouser *cred, struct node *dir, int entry,
                   int nentries, char **data,
                   mach_msg_type_number_t * datacnt, vm_size_t bufsize,
                   int *amt)
{
  io_statbuf_t  st; 
  struct smbc_dirent * dirent;  
  int size = 0,  dd;
  int nreturningentries = 0;
  int err = 0;
  int add_dir_entry_size = 0;
  char *p = 0;

  if (!dir)
    return ENOTDIR;

  pthread_mutex_lock (&smb_mutex);
  dd = smbc_opendir (dir->nn->abs_file_name);
  pthread_mutex_unlock (&smb_mutex);
  
  if (dd < 0)
    return ENOTDIR;  
  
  pthread_mutex_lock (&smb_mutex);
  err = smbc_lseekdir (dd, entry);
  pthread_mutex_unlock (&smb_mutex);
  
  if(err)
    {
      if(errno == EINVAL)
        {
          *datacnt = 0;
          *amt = 0;
          pthread_mutex_lock (&smb_mutex);
          smbc_closedir (dd);
          pthread_mutex_unlock (&smb_mutex);
          return 0;
        }
      return errno;
    }
   
  int
  addSize (char *filename)
    {
      if (nentries == -1 || nreturningentries < nentries)
        {
          size_t new_size = size + DIRENT_LEN (strlen (filename));
          if (bufsize > 0 && new_size > bufsize)
            return 1;
          size = new_size;
          nreturningentries++;
          return 0;
        }
      else
        return 1;
    }

  for(;;)
    {
      pthread_mutex_lock (&smb_mutex);      
      dirent = smbc_readdir (dd);
      pthread_mutex_unlock (&smb_mutex);

      if(!dirent)
        break;

      /* Add only files and directories.  */
      if ((dirent->smbc_type == SMBC_DIR) || (dirent->smbc_type == SMBC_FILE) ) 
        if (addSize(dirent->name))/* bufsize or nentries reached.  */
          break;
      
    }

  if(size > *datacnt) /* if the supplied buffer isn't large enough.  */
    *data = mmap (0, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS, 0, 0);

  if (!(*data) ||  (*data == (void *) -1))
    {
      pthread_mutex_lock (&smb_mutex);
      smbc_closedir(dd);
      pthread_mutex_unlock (&smb_mutex);
      return ENOMEM;    
    }

  pthread_mutex_lock (&smb_mutex);
  err=smbc_lseekdir (dd, entry);
  pthread_mutex_unlock (&smb_mutex);

  if (err)
    {
      if (errno == EINVAL)
        {
          *datacnt = 0;
          *amt = 0;
          pthread_mutex_lock (&smb_mutex);
          smbc_closedir (dd);
          pthread_mutex_unlock (&smb_mutex);
          return 0;
        }

      return errno;
    }
      
  add_dir_entry_size = size;
  p = *data;
  int count = 0;
  
  int
  add_dir_entry (const char *name, ino_t fileno, int type)
    {
      if(count < nreturningentries)
        {
          struct dirent hdr;
          size_t name_len = strlen (name);
          size_t sz = DIRENT_LEN (name_len);
  
          if (sz > add_dir_entry_size)
            return 1;
          
          add_dir_entry_size -= sz;
 
          hdr.d_fileno = fileno;
          hdr.d_reclen = sz;
          hdr.d_type = type;
          hdr.d_namlen = name_len;
 
          memcpy (p, &hdr, DIRENT_NAME_OFFS);
          strcpy (p + DIRENT_NAME_OFFS, name);
          p += sz;
          ++count;
          return 0;
      }
    else
      return 1;
    }
 
    for(;;)
      {
        pthread_mutex_lock (&smb_mutex);
        dirent = smbc_readdir (dd);
        pthread_mutex_unlock (&smb_mutex);
        if (!dirent)
          break;
        int type = 0;
        if (dirent->smbc_type == SMBC_DIR)
          type = DT_DIR;
        else if (dirent->smbc_type == SMBC_FILE)
          type = DT_REG;
        else
          continue;

        if (!strcmp (dirent->name, "."))
          {
            pthread_mutex_lock (&smb_mutex);
            err = smbc_stat (dir->nn->abs_file_name, &st);
            pthread_mutex_unlock (&smb_mutex);
          }
        else if (!strcmp (dirent->name, ".."))
          {
	    st = empty_stat ();
	    st.st_mode |= S_IFDIR;
          }
        else
          {
	    char *stat_file_name;

            asprintf (&stat_file_name, "%s/%s",
		      dir->nn->abs_file_name, dirent->name);
	    if (stat_file_name == NULL)
	      return ENOMEM;

            pthread_mutex_lock (&smb_mutex);
            err = smbc_stat (stat_file_name, &st);
            pthread_mutex_unlock (&smb_mutex);

	    if (err)
	      {
		/* STAT_FILE_NAME is not accessible but ought to be listed.  */
		st = empty_stat ();
		err = 0;
	      }
          }

        if (err)
          {
            pthread_mutex_lock (&smb_mutex);
            smbc_closedir(dd);
            pthread_mutex_unlock (&smb_mutex);
            return errno;
          }

        err = add_dir_entry (dirent->name, st.st_ino, type);
        if (err)
          break;
      } 

  *datacnt = size;
  *amt = nreturningentries;
  pthread_mutex_lock (&smb_mutex);
  smbc_closedir (dd);
  pthread_mutex_unlock (&smb_mutex);
  return 0;
}

void
smbfs_init ()
{
  int err;
  err = maptime_map (0, 0, &maptime);

  if(err)
    return;  
   
  create_root_node ();
}

void
smbfs_terminate ()
{
  pthread_mutex_init (&smb_mutex, NULL);
}
