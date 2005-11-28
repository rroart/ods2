/*
 *  linux/fs/ods2/inode.c
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

#include <linux/config.h>
/*
#include <linux/module.h>
*/
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/locks.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h>

#include "ods2.h"

struct file_operations ods2_dir_operations = {
	read:		NULL,
	readdir:	ods2_readdir,
	open:		ods2_open_release,
	release:	ods2_open_release,
	ioctl:		NULL,
	fsync:		NULL,
};


struct file_operations ods2_file_operations = {
	read:		ods2_read,
	readdir:	NULL,
	llseek:		ods2_llseek,
	open:		ods2_open_release,
	release:	ods2_open_release,
	ioctl:		ods2_file_ioctl,
	fsync:		NULL,
};


struct inode_operations ods2_dir_inode_operations = {
	create:		NULL,
	lookup:		ods2_lookup,
	link:		NULL,
	unlink:		NULL,
	symlink:	NULL,
	mkdir:		NULL,
	rmdir:		NULL,
	mknod:		NULL,
	rename:		NULL,
};



struct dentry *ods2_lookup(struct inode *dir, struct dentry *dentry)
{
	struct super_block	   *sb = dir->i_sb;
	ODS2SB			   *ods2p = (ODS2SB *)sb->u.generic_sbp;
	struct buffer_head	   *bh = NULL;
	char			   *vp;
	u16     		   *rec;
	ODS2FH			   *ods2fhp = (ODS2FH *)dir->u.generic_ip;
	u32			    vbn = 1;
	u32			    lbn;
	int			    vers = 0;
	char			    name[dentry->d_name.len + 1];
	
	memcpy(name, dentry->d_name.name, dentry->d_name.len);
	name[dentry->d_name.len] = 0;
	
	/*
	  We need to extract any version number and terminate the file name with file type
	  at the ; character because in the directory file only the file name and type
	  is stored as text without the ; character. The version number for the file is
	  stored together with each FID.
	*/

	if (( vp = strrchr(name, ods2p->semicolon)) != NULL) {
		*vp++ = 0;
		
		if (sscanf(vp, "%d", &vers) != 1) {
			*--vp = ods2p->semicolon;
		} else if (vers > 32767) {
			printk("ODS2-fs error with version number for %s (%s)\n", name, vp);
			return ERR_PTR(-EBADF);
		}
	}

	while ((lbn = vbn2lbn(sb, ods2fhp->map, vbn)) > 0 &&
	       (bh = sb_bread(sb, GETBLKNO(sb, lbn))) != NULL && bh->b_data != NULL) {
		
		rec = (u16 *)(GETBLKP(sb, lbn, bh->b_data));
		
		while (*rec != 65535 && *rec != 0) {
			DIRDEF		       *dire = (DIRDEF *)rec;
			
			if (dire->u1.s1.dir_b_namecount == strlen(name)) {
				char		      dirname[dire->u1.s1.dir_b_namecount + 1];
				
				memcpy(dirname, &dire->u1.s1.dir_t_name, dire->u1.s1.dir_b_namecount);
				dirname[dire->u1.s1.dir_b_namecount] = 0;
				if (ods2p->dollar != '$' || ods2p->flags.v_lowercase) {
					char	       *p = dirname;
					char		cnt = dire->u1.s1.dir_b_namecount;
					
					while (*p && cnt-- > 0) { if (*p == '$') { *p = ods2p->dollar; } if (ods2p->flags.v_lowercase) { *p = tolower(*p); } p++; }
				}

				if (strcmp(dirname, name) == 0) {
					int		    curbyte = 0;
					
					while (curbyte < dire->u1.s1.dir_w_size) {
						u32		  ino;
						DIRDEF		 *dirv = (DIRDEF *)((char *)dire + ((dire->u1.s1.dir_b_namecount + 1) & ~1) + 6 + curbyte);
						
						if (dirv->u1.s2.dir_w_version == vers || vers == 0) {
							struct inode   *inode;
							
							ino = (dirv->u1.s2.u2.s3.fid_b_nmx << 16) | le16_to_cpu(dirv->u1.s2.u2.s3.fid_w_num);
							brelse(bh);
							if ((inode = iget(dir->i_sb, ino)) != NULL) {
								d_add(dentry, inode);
								return NULL;
							}
							printk("ODS2-fs error when iget for file %s\n", name);
							return ERR_PTR(-EACCES);
						}
						curbyte += 8;
					}
				}
			}
			rec = (u16 *)((char *)rec + le16_to_cpu(dire->u1.s1.dir_w_size) + 2);
		}
		brelse(bh);
		vbn++;
	}
	d_add(dentry, NULL);
	return NULL;
}


/*
  The array is used to map ODS2 protection bits to Unix protection bits.
  There are two problems when doing the mapping.
  The first one is that ODS2 have four types of classes, system, owner, group
  and world. As you know Unix has only three, owner, group and other.
  We solve that by mapping owner to owner, group to group and world to other.
  The system class is ignored.
  The other problem is that ODS2 have four different protection bits, read,
  write, execute and delete. The read, write and execute can be mapped directly
  to Unix bits but the delete bit must be mapped to something else.
  As write access give the user delete access on Unix we map the delete bit to
  write access.
  Please note that on an ODS2 disk a set bit mean deny access where on Unix a
  set bit mean granted access.
*/

char unsigned vms2unixprot[] = {       /* ODS2 prot */
	S_IROTH | S_IWOTH | S_IXOTH ,	/* D E W R */
	    0	| S_IWOTH | S_IXOTH ,	/* D E W   */
	S_IROTH | S_IWOTH | S_IXOTH ,	/* D E	 R */
	    0	| S_IWOTH | S_IXOTH ,	/* D E	   */
	S_IROTH | S_IWOTH |	0   ,	/* D   W R */
	    0	| S_IWOTH |	0   ,	/* D   W   */
	S_IROTH | S_IWOTH |	0   ,	/* D	 R */
	    0	| S_IWOTH |	0   ,	/* D	   */
	S_IROTH | S_IWOTH | S_IXOTH ,	/*   E W R */
	    0	| S_IWOTH | S_IXOTH ,	/*   E W   */
	S_IROTH |     0	  | S_IXOTH ,	/*   E	 R */
	    0	|     0	  | S_IXOTH ,	/*   E	   */
	S_IROTH | S_IWOTH |	0   ,	/*     W R */
	    0	| S_IWOTH |	0   ,	/*     W   */
	S_IROTH |     0	  |	0   ,	/*	 R */
	    0	|     0	  |	0   ,	/*	   */
};

void ods2_read_inode(struct inode *inode) {
	struct super_block	   *sb = inode->i_sb;
	struct buffer_head	   *bh;
	ODS2SB			   *ods2p = (ODS2SB *)sb->u.generic_sbp;
	u32			    fhlbn;

	if ((fhlbn = ino2fhlbn(sb, inode->i_ino)) > 0 &&
	    (bh = sb_bread(sb, GETBLKNO(sb, fhlbn))) != NULL && bh->b_data != NULL) {

		FH2DEF		       *fh2p = (FH2DEF *)(GETBLKP(sb, fhlbn, bh->b_data));

		if ((inode->u.generic_ip = kmalloc(sizeof(ODS2FH), GFP_KERNEL)) != NULL) {

			ODS2FH		       *ods2fhp;
			FI2DEF		       *fi2p;
			FATDEF		       *fatp;

			ods2fhp = (ODS2FH *)inode->u.generic_ip;
			ods2fhp->map = NULL;
			ods2fhp->ods2vari = NULL;
			fi2p = (FI2DEF *)((short unsigned *)fh2p + fh2p->fh2_b_idoffset);
			fatp = (FATDEF *)&(fh2p->fh2_w_recattr);

			if (verify_fh(fh2p, inode->i_ino)) {

				memcpy(&ods2fhp->fat, fatp, sizeof(FATDEF));
				ods2fhp->map = getmap(sb, fh2p);

				if (fh2p->u4.s1.fch_v_directory) {
					inode->i_mode = S_IFDIR;
					inode->i_op = &ods2_dir_inode_operations;
					inode->i_fop = &ods2_dir_operations;
				} else {
					inode->i_mode = S_IFREG;
					inode->i_fop = &ods2_file_operations;
				}

				inode->i_uid = le16_to_cpu(fh2p->u5.s1.fh2_w_mem);
				inode->i_gid = le16_to_cpu(fh2p->u5.s1.fh2_w_grp);
				
				inode->i_ctime = vms2unixtime(fi2p->fi2_q_credate);
				inode->i_mtime = vms2unixtime(fi2p->fi2_q_revdate);
				inode->i_atime = vms2unixtime(fi2p->fi2_q_revdate);
				
				/*
				  Note that we don't use the system protection bits for ODS2.
				*/
				
				inode->i_mode |= vms2unixprot[(le16_to_cpu(fh2p->fh2_w_fileprot) >> 4) & 0x0f] << 6; /* owner */
				inode->i_mode |= vms2unixprot[(le16_to_cpu(fh2p->fh2_w_fileprot) >> 8) & 0x0f] << 3; /* group */
				inode->i_mode |= vms2unixprot[(le16_to_cpu(fh2p->fh2_w_fileprot) >> 12) & 0x0f];     /* world => other */
				
				inode->i_blksize = 512;
				inode->i_blocks = ((le16_to_cpu(fatp->u1.s1.fat_w_hiblkh) << 16) | le16_to_cpu(fatp->u1.s1.fat_w_hiblkl));
				inode->i_size = ((le16_to_cpu(fatp->u2.s1.fat_w_efblkh) << 16) | le16_to_cpu(fatp->u2.s1.fat_w_efblkl)) << 9;
				if (inode->i_size > 0) { inode->i_size -= 512; }
				inode->i_size += le16_to_cpu(fatp->fat_w_ffbyte);
				
				if ((fatp->u0.s0.fat_v_rtype == FAT_C_VFC || fatp->u0.s0.fat_v_rtype == FAT_C_VARIABLE) && !ods2p->flags.v_raw) {
					if ((ods2fhp->ods2vari = (ODS2VARI *)kmalloc(sizeof(ODS2VARI), GFP_KERNEL)) != NULL) {
						memset(ods2fhp->ods2vari, 0 , sizeof(ODS2VARI));
						sema_init(&(ods2fhp->ods2vari->sem), 1);
					} else {
						printk("ODS2-fs kmalloc failed for vari data\n");
					}
				}
				
				ods2fhp->parent = (fh2p->u6.s1.fid_b_nmx << 16) |  le16_to_cpu(fh2p->u6.s1.fid_w_num);
				inode->i_version = ++event;
				bforget(bh);
				return;
			}
			printk("ODS2-fs not a valid file header\n");
		} else {
			bforget(bh);
			printk("ODS2-fs kmalloc failed for extension inode\n");
			kfree(inode->u.generic_ip);
		}
	}
	printk("ODS2-fs error reading inode\n");
	make_bad_inode(inode);
}

/*
  For a read only file system there is nothing to do for put_inode.
*/

void ods2_put_inode(struct inode *inode) {
}


void ods2_clear_inode(struct inode *inode) {
	ODS2FH			 *ods2fhp = (ODS2FH *)inode->u.generic_ip;
	
	if (ods2fhp != NULL) {
		ODS2MAP		       *map = ods2fhp->map;
		
		while (map != NULL) {
			ODS2MAP		     *nxt = map->nxt;
			
			kfree(map);
			map = nxt;
		}
		ods2fhp->map = NULL;
		
		if (ods2fhp->ods2vari != NULL) { /* in case the file was of variable record type */
			int			idx;
			
			for (idx = 0; idx < 128; idx++) {
				ODS2VAR		     *ods2varp = ods2fhp->ods2vari->ods2varp[idx];
				
				while (ods2varp != NULL) {
					ODS2VAR		   *nxt = ods2varp->nxt;
					
					kfree(ods2varp);
					ods2varp = nxt;
				}
			}
			kfree(ods2fhp->ods2vari);
			ods2fhp->ods2vari = NULL;
		}
		kfree(inode->u.generic_ip);
		inode->u.generic_ip = NULL;
	}
}


/*
  This routine doesn't need to be defined for a read only filesystem
  but we do it for fun so remember to call clear_inode otherwise you
  will run out of memory...
*/

void ods2_delete_inode(struct inode *inode) {
	clear_inode(inode);
}

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
