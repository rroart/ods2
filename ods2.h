/*
 *  linux/fs/ods2/ods2.h
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 * Written 2003 by Jonas Lindholm <jlhm@usa.net>
 *
 */

#ifdef TWOSIX
#include <linux/statfs.h>
#endif

/*
  The followinf structures are defined in the book
  "VMS File System Internals"
*/

/*
  This is the home block on a ODS2 disk.
*/

typedef struct hm2def {
	u32		     hm2$l_homelbn;
	u32		     hm2$l_alhomelbn;
	u32		     hm2$l_altidxlbn;
	union {
		u16	     hm2$w_struclev;
		struct {
			u8   hm2$b_structlevv;
			u8   hm2$b_structlevl;
		} s1;
	} u1;
	u16		     hm2$w_cluster;
	u16		     hm2$w_homevbn;
	u16		     hm2$w_alhomevbn;
	u16		     hm2$w_altidxvbn;
	u16		     hm2$w_ibmapvbn;
	u32		     hm2$l_ibmaplbn;
	u32		     hm2$l_maxfiles;
	u16		     hm2$w_ibmapsize;
	u16		     hm2$w_resfiles;
	u16		     hm2$w_devtype;
	u16		     hm2$w_rvn;
	u16		     hm2$w_setcount;
	u16		     hm2$w_volchar;
	union {
		u32	     hm2$l_owner;
		struct {
			u16  hm2$w_mem;
			u16  hm2$w_grp;
		} s1;
	} u2;
	u32		     hm2$l_res1;
	u16		     hm2$w_protect;
	u16		     hm2$w_fileprot;
	u16		     hm2$w_res2;
	u16		     hm2$w_checksum1;
	u32		     hm2$q_credate[2];
	u8		     hm2$b_window;
	u8		     hm2$b_lru_lim;
	u16		     hm2$w_extend;
	u32		     hm2$q_retainmin[2];
	u32		     hm2$q_retainmax[2];
	u32		     hm2$q_revdate[2];
	u8		     hm2$r_min_class[20];
	u8		     hm2$r_max_class[20];
	u8		     hm2$b_res3[320];
	u32		     hm2$l_serialnum;
	char		     hm2$t_structname[12];
	char		     hm2$t_volname[12];
	char		     hm2$t_ownername[12];
	char		     hm2$t_format[12];
	u16		     hm2$w_res4;
	u16		     hm2$w_checksum2;
} HM2DEF;

/*
  This is the Storage Control Block.
  It is the first block in file BITMAP.SYS.
*/

typedef struct scbdef {
	union {
		u16	     scb$w_struclev;
		struct {
			u8   scb$b_structlevv;
			u8   scb$b_structlevl;
		} s1;
	} u1;
	u16		     scb$w_cluster;
	u32		     scb$l_volsize;
	u32		     scb$l_blksize;
	u32		     scb$l_sectors;
	u32		     scb$l_tracks;
	u32		     scb$l_cylinders;
	union {
		u32	     scb$l_status;
		struct {
			u32  scb$v_mapdirty:1;
			u32  scb$v_mapalloc:1;
			u32  scb$v_filalloc:1;
			u32  scb$v_quodirty:1;
			u32  scb$v_hdrwrite:1;
			u32  scb$v_corrupt:1;
		} s1;
	} u2;
	union {
		u32	     scb$l_status2;
		struct {
			u32  scb$v_mapdirty:1;
			u32  scb$v_mapalloc:1;
			u32  scb$v_filalloc:1;
			u32  scb$v_quodirty:1;
			u32  scb$v_hdrwrite:1;
			u32  scb$v_corrupt:1;
		} s1;
	} u3;
	u16		     scb$w_writecnt;
	char		     scb$t_volockname[12];
	u16		     scb$q_mounttime[4];
	u16		     scb$w_backrev;
	u64		     scb$q_genernum;
	u8		     scb$b_reserved[446];
	u16		     scb$w_checksum;
} SCBDEF;

/*
  This structure is part of the file header block and
  fives different tomes as well as the file name.
*/

typedef struct fi2def {
	char		     fi2$t_filename[20];
	u16		     fi2$w_revision;
	u16		     fi2$q_credate[4];
	u16		     fi2$q_revdate[4];
	u16		     fi2$q_expdate[4];
	u16		     fi2$q_bakdate[4];
	char		     fi_2_filenameext[66];
} FI2DEF;

/*
  This is the file header for any ODS2 file.
  It is located in file INDEXF.SYS.
*/

typedef struct fh2def {
	u8		     fh2$b_idoffset;
	u8		     fh2$b_mpoffset;
	u8		     fh2$b_acoffset;
	u8		     fh2$b_rsoffset;
	u16		     fh2$w_seg_num;
	union {
		u16	     fh2$w_struclev;
		struct {
			u8   fh2$b_structlevv;
			u8   fh2$b_structlevl;
		} s1;
	} u1;
	union {
		u16	     fh2$w_fid[3];
		struct {
			u16  fh2$w_fid_num;
			u16  fh2$w_fid_seq;
			u8   fh2$b_fid_rvn;
			u8   fh2$b_fid_nmx;
		} s1;
	} u2;
	union {
		u16	     fh2$w_ext_fid[3];
		struct {
			u16  fid$w_ex_fidnum;
			u16  fid$w_ex_fidseq;
			u8   fid$b_ex_fidrvn;
			u8   fid$b_ex_fidnmx;
		} s1;
	} u3;
	u32		     fh2$w_recattr[8];
	union {
		struct filechar {
			u32  fch$v_wascontig:1;
			u32  fch$v_nobackup:1;
			u32  fch$v_writeback:1;
			u32  fch$v_readcheck:1;
			u32  fch$v_writecheck:1;
			u32  fch$v_contigb:1;
			u32  fch$v_locked:1;
			u32  fch$v_contig:1;
			u32  fch$v_res1:3;
			u32  fch$v_badacl:1;
			u32  fch$v_spool:1;
			u32  fch$v_directory:1;
			u32  fch$v_badblock:1;
			u32  fch$v_markdel:1;
			u32  fch$v_nocharge:1;
			u32  fch$v_erase:1;
		} s1;
		u32	     fh2$l_filechar;
	} u4;
	u16		     fh2$w_res1;
	u8		     fh2$b_map_inuse;
	u8		     fh2$b_acc_mode;
	union {
		u32	     fh2$l_fileowner;
		struct {
			u16  fh2$w_mem;
			u16  fh2$w_grp;
		} s1;
	} u5;
	u16		     fh2$w_fileprot;
	union {
		u16	     fh2$w_backlink[3];
		struct {
			u16  fid$w_num;
			u16  fid$w_seq;
			u8   fid$b_rvn;
			u8   fid$b_nmx;
		} s1;
	} u6;
	u8		     fh2$b_journal;
	u8		     fh2$b_ru_active;
	u16		     fh2$w_res2;
	u32		     fh2$l_highwater;
	u8		     fh2$b_res3[8];
	u8		     fh2$r_class_prot[20];
	u8		     fh2$b_res4[402];
	u16		     fh2$w_checksum;
} FH2DEF;

/*
  This is the file attribute structure.
  It is part of the file header.
  It defines RMS attributes for any file.
*/

#define FAT$C_UNDEFINED	 0
#define FAT$C_FIXED	 1
#define FAT$C_VARIABLE	 2
#define FAT$C_VFC	 3
#define FAT$C_STREAM	 4
#define FAT$C_STREAMLF	 5
#define FAT$C_STREAMCR	 6

#define FAT$C_SEQUANTIAL 0
#define FAT$C_RELATIVE	 1
#define FAT$C_INDEXED	 2
#define FAT$C_DIRECT	 3

#define FAT$M_FORTRANCC	 0x01
#define FAT$M_IMPLIEDCC	 0x02
#define FAT$M_PRINTCC	 0x04
#define FAT$M_NOSPAN	 0x08
#define FAT$M_MSBRCW	 0x10

typedef struct fatdef {
	union {
		u8	     fat$b_rtype;
		struct {
			u8   fat$v_rtype:4;
			u8   fat$v_fileorg:4;
		} s0;
	} u0;
	u8		     fat$b_rattrib;
	u8		     fat$w_rsize;
	union {
		u32	     fat$l_hiblk;
		struct {
			u16  fat$w_hiblkh;
			u16  fat$w_hiblkl;
		} s1;
	} u1;
	union {
		u32	     fat$l_efblk;
		struct {
			u16  fat$w_efblkh;
			u16  fat$w_efblkl;
		} s1;
	} u2;
	u16		     fat$w_ffbyte;
	u8		     fat$b_bktsize;
	u8		     fat$b_vfcsize;
	u16		     fat$w_maxrec;
	u16		     fat$w_defext;
	u8		     fat$b_res1[6];
	u16		     fat$w_notused;
	u16		     fat$w_versions;
} FATDEF;

/*
  This is the structure used for mapping virtual block
  number, VBN, to logical block numbers, LBN.
  One or more of this structure is part of the file header.
*/

typedef struct fm2def {
	union {
		struct {
			u8   fm2$b_count1;
			u8   fm2$v_highlbn:6;
			u8   fm2$v_format:2;
			u16  fm2$w_lowlbn;
		} fm1;
		struct {
			u16  fm2$v_count2:14;
			u16  fm2$v_format:2;
			u16  fm2$l_lbn2[2];
		} fm2;
		struct {
			u16  fm2$v_count2:14;
			u16  fm2$v_format:2;
			u16  fm2$w_lowcount;
			u32  fm2$l_lbn3;
		} fm3;
	} u1;
} FM2DEF;

/*
  This structure define a directory entry in a directory file.
*/

#define DIR$C_FID	    0
#define DIR$C_LINKNAME	    1

typedef struct dirdef {
	union {
		struct {
			u16  dir$w_size;
			s16  dir$w_verlimit;
			union {
				u8	dir$b_flags;
				struct {
					u8 dir$v_type:3;
					u8 dir$v_res1:3;
					u8 dir$v_nextrec:1;
					u8 dir$v_prevrec:1;
				} s4;
			} u4;
			u8   dir$b_namecount;
			char dir$t_name;
		} s1;
		struct {
			u16	dir$w_version;
			union {
				u16	dir$w_fid[3];
				struct {
					u16 fid$w_num;
					u16 fid$w_seq;
					u8  fid$b_rvn;
					u8  fid$b_nmx;
				} s3;
			} u2;
		} s2;
	} u1;
} DIRDEF;


/*
  From here we have our own ODS2 specific structures
  and definitions.
*/

typedef struct ods2map {
	struct ods2map		  *nxt;
	struct {
		u32		  cnt;
		u32		  lbn;
	} s1[16];
} ODS2MAP;

/*
  Each block map 64Kbyte * 16 loff's.
  The number of bytes for this structure is 4 + 16 * 16 => 260.
  For a 1GB file we need a total of 1024 blocks. If each block is
  260 bytes the total amount of bytes is 1024 * 260 => 266240 bytes
  The linked list will contain no more than 8 blocks as the structure
  below has 128 pointers.
*/

typedef struct ods2var {
	struct ods2var		 *nxt;	   /* next block if needed */
	struct {
		u64		  recoffs; /* offset to start of record */
		loff_t		  loff;	   /* virtual offset to start of record */
	} s1[16];
} ODS2VAR;

/*
  Each file that is of variable record type has the following structure
  attached to it.
  This is the index for one or more ODS2VAR structures. By doing index as
  much as possible it is easy to calculate what structure to use by just
  doing some shifts and bit masking.
  Note that this structure and its sub structures are protected by a 
  semaphore because more than one process at the same time can use the inode
  structure to read the file contents.
  The number of bytes for this structure is 128 * 4 + 12 (or 16) => 528.
  The overhead for small files are big but 528 bytes allocated using kmalloc
  should not be to much.
*/

#define IDXVAR(a)     ((a >> 16) & 0x0f)
#define IDXVARI(a)    ((a >> 20) & 0x7f)
#define IDXBLOCK(a)   (a >> 27)

typedef struct ods2vari {
	ODS2VAR			 *ods2varp[128]; /* pointers to ods2var blocks */
	struct semaphore	  sem;		 /* This is the semaphore used to protect this structure */
	loff_t			  highidx;	 /* highest index so far... */
} ODS2VARI;

/*
  Each open file has the following structure attached to it.
  It add the extra variables needed to handle directories and
  RMS data.
*/

typedef struct ods2file {
	struct buffer_head	 *bhp;
	u8                       *data;    /* pointer to data portion in buffer */
	u64			  currec; /* byte offset to current record --- from start of file */
	u16			  curbyte; /* byte offset into current record */
	u16			  reclen; /* length of current record */
	union {
		u32		  flags;
		struct {
			u32	  v_raw:1; /* this file handler must return data in raw mode */
			u32	  v_res1:31;
		} s1;
	} u1;
} ODS2FILE;

/*
  Each inode has the following structure attached to it.
  It keep the file attributes and mapping information in memory.
*/

typedef struct ods2fh {
	ODS2MAP			 *map;	    /* mapping information from VBN to LBN */
	ODS2VARI		 *ods2vari; /* only used for variable record files */
	FATDEF			  fat;	    /* file attributes */
	u32			  parent;   /* ino of parent directory */
} ODS2FH;

/*
  The super block for an ODS2 disk has the following
  structure attached.
  It keep the home block and the inode for INDEXF.SYS;1
  in memory.
*/

#define SB$M_VERSALL	   0
#define SB$M_VERSHIGH	   1
#define SB$M_VERSNONE	   2
#define SB$M_RAW	   8
#define SB$M_LOWERCASE	   16

#ifdef TWOSIX
#define ODS2_SB(sb) (sb->s_fs_info)
#else
#define ODS2_SB(sb) ((struct ods2sb *)(sb->u.generic_sbp))
#endif
#define VMSSWAP(l) ((l & 0xffff) << 16 | l >> 16)
#define FAT$C_SEQUENTIAL FAT$C_SEQUANTIAL
#define fat$l_hiblk u1.fat$l_hiblk
#define fat$l_efblk u2.fat$l_efblk
#define fat$w_ffbyte fat$w_ffbyte
#define fat$b_rtype u0.fat$b_rtype
#define fat$w_maxrec fat$w_maxrec
#define fat$w_versions fat$w_versions
#define scb$l_volsize scb$l_volsize
#define scb$w_cluster scb$w_cluster
#define hm2$l_ibmaplbn hm2$l_ibmaplbn
#define hm2$w_ibmapsize hm2$w_ibmapsize
#define hm2$w_cluster hm2$w_cluster
#define fh2$b_idoffset fh2$b_idoffset
#define fh2$b_mpoffset fh2$b_mpoffset
#define fh2$b_map_inuse fh2$b_map_inuse
#define fh2$b_acoffset fh2$b_acoffset
#define fh2$b_rsoffset fh2$b_rsoffset
#define fh2$w_struclev u1.fh2$w_struclev
#define fh2$w_checksum fh2$w_checksum
#define fh2$w_fid_num u2.s1.fh2$w_fid_num
#define fh2$w_fid_seq u2.s1.fh2$w_fid_seq

#define SS$_NORMAL 1
#define SS$_BADPARAM    0x0014
#define SS$_BADFILENAME 0x0818
#define SS$_BADIRECTORY 0x0828
#define SS$_DUPFILENAME 0x0868
#define SS$_NOSUCHFILE  0x0910 
#define SS$_NOMOREFILES 0x0930

#ifdef VMS_BIG_ENDIAN
#define WORK_UNIT unsigned char
#define WORK_MASK 0xff
#else
#define WORK_UNIT unsigned int
#define WORK_MASK 0xffffffff
#endif
#define WORK_BITS (sizeof(WORK_UNIT) * 8)

#ifdef VMS_BIG_ENDIAN
#define VMSLONG(l) ((l & 0xff) << 24 | (l & 0xff00) << 8 | (l & 0xff0000) >> 8 |l >> 24)
#define VMSWORD(w) ((w & 0xff) << 8 | w >> 8)
#define VMSSWAP(l) ((l & 0xff0000) << 8 | (l & 0xff000000) >> 8 |(l & 0xff) << 8| (l & 0xff00) >> 8)
#else
#define VMSLONG(l) l
#define VMSWORD(w) w
#define VMSSWAP(l) ((l & 0xffff) << 16 | l >> 16)
#endif

typedef struct ods2sb {
	struct buffer_head * bh;
	HM2DEF			*  hm2;
	struct buffer_head * ibh;
	struct inode		 *indexf;  /* INDEXF.SYS */
	u8			 *ibitmap; /* index file header bitmap */
	struct buffer_head * sbh;
#ifndef TWOSIX
	struct statfs		  statfs;
#else
	struct kstatfs            statfs;
#endif
	struct {
		int		  v_version:3; /* what to do with file versions */
		int		  v_raw:1;     /* force all files as stream */
		int		  v_lowercase:1; /* force all file names to lowercase */
		int		  v_res:27;    /* reserved */
	} flags;
	char			  dollar;      /* character used for dollar */
	char			  semicolon;   /* character used for semicolon */
} ODS2SB;

/*
  These two macros are used to support media with a sector size of
  1024 or 2048 bytes.
  I.e. the RRD47 CDROM drive on my Alpha server 1200 report a sector
  size of 2048 even for an ODS2 CD.
*/

#define GETBLKNO(a, b)   ((b) >> (a->s_blocksize_bits - 9))
#define GETBLKP(a, b, c) (void *)&(((char *)c)[((b) & (a->s_blocksize_bits == 9 ? 0 : (a->s_blocksize_bits == 10 ? 1 : 3))) << 9])

/*
  This is our private ioctl operations for a file pointer.
*/

#define ODS2_IOC_FISETRAW  _IOW('f', 0x0d0, long) /* enable/disable raw file mode */
#define ODS2_IOC_FIGETRAW  _IOR('g', 0x0d0, long) /* get raw file mode */
#define ODS2_IOC_SBGETRAW  _IOR('g', 0x0d1, long) /* get raw mode for super block */

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

/*
  Ok, I give up :-) for some reason unknown to me the addition of 2 seconds
  is needed to get the correct time.
  It works for a file created 1-jan-1971 and for a file created 1-jan-2038
  as well as for files created 1992 and 2003.
*/

#define vms2unixtime(a) ((u64)div64((le64_to_cpu(*(u64 *)&(a)) - 0x007c953d63a19980L) >> 7, 78125) + 2);

/*
  util.c
*/

u64 div64(u64 a, u32 b0);
u32 vbn2lbn(struct super_block *sb, ODS2MAP *map, u32 vbn);
u32 ino2fhlbn(struct super_block *sb, u32 ino);
ODS2MAP *getmap(struct super_block *sb, FH2DEF *fh2p);
struct buffer_head *getfilebh(struct file *filp, u32 vbn);
int verify_fh(FH2DEF *fh2p, u32 ino);
int parse_options(struct super_block *sb, char *options);

/*
  inode.c
*/

struct dentry *ods2_lookup(struct inode *dir, struct dentry *dentry
#ifdef TWOSIX
, struct nameidata *nd
// but this is not used?
#endif
);
void ods2_read_inode(struct inode *inode);
void ods2_put_inode(struct inode *inode);
void ods2_clear_inode(struct inode *inode);
void ods2_delete_inode(struct inode *inode);
void ods2_write_inode (struct inode *inode, int wait);
void ods2_write_super (struct super_block * sb);
int ods2_create (struct inode * dir, struct dentry * dentry, int mode);
int ods2_link (struct dentry * old_dentry, struct inode * dir, struct dentry *dentry);
int ods2_mkdir(struct inode * dir, struct dentry * dentry, int mode);

/*
  dir.c
*/

int ods2_readdir(struct file *filp, void *dirent, filldir_t filldir);

/*
  file.c
*/

int ods2_file_ioctl(struct inode *inode, struct file *filp, int unsigned cmd, long unsigned arg);
ssize_t ods2_read(struct file *filp, char *buf, size_t buflen, loff_t *loff);
loff_t ods2_llseek(struct file *filp, loff_t loff, int seek);
int ods2_open_release(struct inode *inode, struct file *filp);

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
