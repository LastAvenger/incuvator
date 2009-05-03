#ifndef	_io_server_
#define	_io_server_

/* Module io */

#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/message.h>

#include <mach/std_types.h>
#include <mach/mach_types.h>
#include <device/device_types.h>
#include <device/net_status.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <hurd/hurd_types.h>

/* Routine io_write */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_write
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, data, dataCnt, offset, amount)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	data_t data;
	mach_msg_type_number_t dataCnt;
	loff_t offset;
	vm_size_t *amount;
{ return S_io_write(io_object, reply, replyPoly, data, dataCnt, offset, amount); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	data_t data,
	mach_msg_type_number_t dataCnt,
	loff_t offset,
	vm_size_t *amount
);
#endif

/* Routine io_read */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_read
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, data, dataCnt, offset, amount)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	data_t *data;
	mach_msg_type_number_t *dataCnt;
	loff_t offset;
	vm_size_t amount;
{ return S_io_read(io_object, reply, replyPoly, data, dataCnt, offset, amount); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	data_t *data,
	mach_msg_type_number_t *dataCnt,
	loff_t offset,
	vm_size_t amount
);
#endif

/* Routine io_seek */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_seek
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, offset, whence, newp)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	loff_t offset;
	int whence;
	loff_t *newp;
{ return S_io_seek(io_object, reply, replyPoly, offset, whence, newp); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	loff_t offset,
	int whence,
	loff_t *newp
);
#endif

/* Routine io_readable */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_readable
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, amount)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	vm_size_t *amount;
{ return S_io_readable(io_object, reply, replyPoly, amount); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	vm_size_t *amount
);
#endif

/* Routine io_set_all_openmodes */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_set_all_openmodes
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, newbits)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	int newbits;
{ return S_io_set_all_openmodes(io_object, reply, replyPoly, newbits); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	int newbits
);
#endif

/* Routine io_get_openmodes */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_get_openmodes
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, bits)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	int *bits;
{ return S_io_get_openmodes(io_object, reply, replyPoly, bits); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	int *bits
);
#endif

/* Routine io_set_some_openmodes */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_set_some_openmodes
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, bits_to_set)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	int bits_to_set;
{ return S_io_set_some_openmodes(io_object, reply, replyPoly, bits_to_set); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	int bits_to_set
);
#endif

/* Routine io_clear_some_openmodes */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_clear_some_openmodes
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, bits_to_clear)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	int bits_to_clear;
{ return S_io_clear_some_openmodes(io_object, reply, replyPoly, bits_to_clear); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	int bits_to_clear
);
#endif

/* Routine io_async */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_async
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, notify_port, async_id_port, async_id_portPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	mach_port_t notify_port;
	mach_port_t *async_id_port;
	mach_msg_type_name_t *async_id_portPoly;
{ return S_io_async(io_object, reply, replyPoly, notify_port, async_id_port, async_id_portPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	mach_port_t notify_port,
	mach_port_t *async_id_port,
	mach_msg_type_name_t *async_id_portPoly
);
#endif

/* Routine io_mod_owner */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_mod_owner
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, owner)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	pid_t owner;
{ return S_io_mod_owner(io_object, reply, replyPoly, owner); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	pid_t owner
);
#endif

/* Routine io_get_owner */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_get_owner
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, owner)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	pid_t *owner;
{ return S_io_get_owner(io_object, reply, replyPoly, owner); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	pid_t *owner
);
#endif

/* Routine io_get_icky_async_id */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_get_icky_async_id
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, icky_async_id_port, icky_async_id_portPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	mach_port_t *icky_async_id_port;
	mach_msg_type_name_t *icky_async_id_portPoly;
{ return S_io_get_icky_async_id(io_object, reply, replyPoly, icky_async_id_port, icky_async_id_portPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	mach_port_t *icky_async_id_port,
	mach_msg_type_name_t *icky_async_id_portPoly
);
#endif

/* Routine io_select */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_select
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, select_type)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	int *select_type;
{ return S_io_select(io_object, reply, replyPoly, select_type); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	int *select_type
);
#endif

/* Routine io_stat */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_stat
#if	defined(LINTLIBRARY)
    (stat_object, reply, replyPoly, stat_info)
	io_t stat_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	io_statbuf_t *stat_info;
{ return S_io_stat(stat_object, reply, replyPoly, stat_info); }
#else
(
	io_t stat_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	io_statbuf_t *stat_info
);
#endif

/* SimpleRoutine io_reauthenticate */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_reauthenticate
#if	defined(LINTLIBRARY)
    (auth_object, reply, replyPoly, rendezvous2)
	io_t auth_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	mach_port_t rendezvous2;
{ return S_io_reauthenticate(auth_object, reply, replyPoly, rendezvous2); }
#else
(
	io_t auth_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	mach_port_t rendezvous2
);
#endif

/* Routine io_restrict_auth */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_restrict_auth
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, new_object, new_objectPoly, uids, uidsCnt, gids, gidsCnt)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	mach_port_t *new_object;
	mach_msg_type_name_t *new_objectPoly;
	idarray_t uids;
	mach_msg_type_number_t uidsCnt;
	idarray_t gids;
	mach_msg_type_number_t gidsCnt;
{ return S_io_restrict_auth(io_object, reply, replyPoly, new_object, new_objectPoly, uids, uidsCnt, gids, gidsCnt); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	mach_port_t *new_object,
	mach_msg_type_name_t *new_objectPoly,
	idarray_t uids,
	mach_msg_type_number_t uidsCnt,
	idarray_t gids,
	mach_msg_type_number_t gidsCnt
);
#endif

/* Routine io_duplicate */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_duplicate
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, newport, newportPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	mach_port_t *newport;
	mach_msg_type_name_t *newportPoly;
{ return S_io_duplicate(io_object, reply, replyPoly, newport, newportPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	mach_port_t *newport,
	mach_msg_type_name_t *newportPoly
);
#endif

/* Routine io_server_version */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_server_version
#if	defined(LINTLIBRARY)
    (vers_object, reply, replyPoly, server_name, server_major_version, server_minor_version, server_edit_level)
	io_t vers_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	string_t server_name;
	int *server_major_version;
	int *server_minor_version;
	int *server_edit_level;
{ return S_io_server_version(vers_object, reply, replyPoly, server_name, server_major_version, server_minor_version, server_edit_level); }
#else
(
	io_t vers_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	string_t server_name,
	int *server_major_version,
	int *server_minor_version,
	int *server_edit_level
);
#endif

/* Routine io_map */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_map
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, memobjrd, memobjrdPoly, memobjwt, memobjwtPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	mach_port_t *memobjrd;
	mach_msg_type_name_t *memobjrdPoly;
	mach_port_t *memobjwt;
	mach_msg_type_name_t *memobjwtPoly;
{ return S_io_map(io_object, reply, replyPoly, memobjrd, memobjrdPoly, memobjwt, memobjwtPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	mach_port_t *memobjrd,
	mach_msg_type_name_t *memobjrdPoly,
	mach_port_t *memobjwt,
	mach_msg_type_name_t *memobjwtPoly
);
#endif

/* Routine io_map_cntl */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_map_cntl
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, memobj, memobjPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	mach_port_t *memobj;
	mach_msg_type_name_t *memobjPoly;
{ return S_io_map_cntl(io_object, reply, replyPoly, memobj, memobjPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	mach_port_t *memobj,
	mach_msg_type_name_t *memobjPoly
);
#endif

/* Routine io_get_conch */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_get_conch
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
{ return S_io_get_conch(io_object, reply, replyPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly
);
#endif

/* Routine io_release_conch */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_release_conch
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
{ return S_io_release_conch(io_object, reply, replyPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly
);
#endif

/* Routine io_eofnotify */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_eofnotify
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
{ return S_io_eofnotify(io_object, reply, replyPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly
);
#endif

/* Routine io_prenotify */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_prenotify
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, write_start, write_end)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	vm_offset_t write_start;
	vm_offset_t write_end;
{ return S_io_prenotify(io_object, reply, replyPoly, write_start, write_end); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	vm_offset_t write_start,
	vm_offset_t write_end
);
#endif

/* Routine io_postnotify */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_postnotify
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, write_start, write_end)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	vm_offset_t write_start;
	vm_offset_t write_end;
{ return S_io_postnotify(io_object, reply, replyPoly, write_start, write_end); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	vm_offset_t write_start,
	vm_offset_t write_end
);
#endif

/* Routine io_readnotify */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_readnotify
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
{ return S_io_readnotify(io_object, reply, replyPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly
);
#endif

/* Routine io_readsleep */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_readsleep
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
{ return S_io_readsleep(io_object, reply, replyPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly
);
#endif

/* Routine io_sigio */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_sigio
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
{ return S_io_sigio(io_object, reply, replyPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly
);
#endif

/* Routine io_pathconf */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_pathconf
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, name, value)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	int name;
	int *value;
{ return S_io_pathconf(io_object, reply, replyPoly, name, value); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	int name,
	int *value
);
#endif

/* Routine io_identity */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_identity
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly, idport, idportPoly, fsidport, fsidportPoly, fileno)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
	mach_port_t *idport;
	mach_msg_type_name_t *idportPoly;
	mach_port_t *fsidport;
	mach_msg_type_name_t *fsidportPoly;
	ino64_t *fileno;
{ return S_io_identity(io_object, reply, replyPoly, idport, idportPoly, fsidport, fsidportPoly, fileno); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly,
	mach_port_t *idport,
	mach_msg_type_name_t *idportPoly,
	mach_port_t *fsidport,
	mach_msg_type_name_t *fsidportPoly,
	ino64_t *fileno
);
#endif

/* Routine io_revoke */
#ifdef	mig_external
mig_external
#else
extern
#endif
kern_return_t S_io_revoke
#if	defined(LINTLIBRARY)
    (io_object, reply, replyPoly)
	io_t io_object;
	mach_port_t reply;
	mach_msg_type_name_t replyPoly;
{ return S_io_revoke(io_object, reply, replyPoly); }
#else
(
	io_t io_object,
	mach_port_t reply,
	mach_msg_type_name_t replyPoly
);
#endif

#endif	/* not defined(_io_server_) */
