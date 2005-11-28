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
	llseek:         generic_file_llseek,
	mmap:           generic_file_mmap,
};


struct file_operations ods2_file_operations = {
	read:		ods2_read,
	write:          generic_file_write,
	readdir:	NULL,
	llseek:		ods2_llseek,
	open:		ods2_open_release,
	release:	ods2_open_release,
	ioctl:		ods2_file_ioctl,
	fsync:		NULL,
};


struct inode_operations ods2_dir_inode_operations = {
	create:		ods2_create,
	lookup:		ods2_lookup,
	link:		ods2_link,
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

static int ods2_update_inode(struct inode * inode, int do_sync)
{
	struct buffer_head * bh;
	struct fh2def * fh2p;
	int err = 0;
	signed int fhlbn;

	if ((fhlbn = ino2fhlbn(inode->i_sb, inode->i_ino)) > 0 &&
	    (!(bh = sb_bread(inode->i_sb, GETBLKNO(inode->i_sb, fhlbn))) != NULL && bh->b_data != NULL)) {
#if 0
		ods2_error (inode->i_sb, "ods2_write_inode",
			    "unable to read inode block - "
			    "inode=%lu, block=%lu", inode->i_ino, fhlbn);
#endif
		return -EIO;
	}

	fh2p = bh->b_data;

	
	
	ODS2FH		       *ods2fhp;
	FI2DEF		       *fi2p;
	FATDEF		       *fatp;

	ods2fhp = (ODS2FH *)inode->u.generic_ip;
	fh2p->fh2$b_idoffset=0x28;
	ods2_write_map(fh2p,ods2fhp->map);
	fi2p = (FI2DEF *)((short unsigned *)fh2p + fh2p->fh2_b_idoffset);
	fatp = (FATDEF *)&(fh2p->fh2_w_recattr);

	memcpy(fatp, &ods2fhp->fat, sizeof(FATDEF));
	//ods2fhp->map = getmap(sb, fh2p);

	switch (inode->i_mode) {
	case S_IFDIR:
		fh2p->u4.s1.fch_v_directory=1;
		break;
	case S_IFREG:
		break;
	default:
		panic("unknown mode\n");
		break;
	}

	fh2p->u5.s1.fh2_w_mem=cpu_to_le16(inode->i_uid);
	fh2p->u5.s1.fh2_w_grp=cpu_to_le16(inode->i_gid);
		
#if 0		
	fi2p->fi2_q_credate=unix2vmstime(inode->i_ctime);
	fi2p->fi2_q_revdate=unix2vmstime(inode->i_mtime);
	fi2p->fi2_q_revdate=unix2vmstime(inode->i_atime);
#endif
				
	/*
	  Note that we don't use the system protection bits for ODS2.
	*/

#if 0
// not yet				
	inode->i_mode |= vms2unixprot[(le16_to_cpu(fh2p->fh2_w_fileprot) >> 4) & 0x0f] << 6; /* owner */
	inode->i_mode |= vms2unixprot[(le16_to_cpu(fh2p->fh2_w_fileprot) >> 8) & 0x0f] << 3; /* group */
	inode->i_mode |= vms2unixprot[(le16_to_cpu(fh2p->fh2_w_fileprot) >> 12) & 0x0f];     /* world => other */
#endif		
		
	fatp->fat$w_maxrec = 512;
	fatp->fat$b_rtype=(FAT$C_SEQUENTIAL << 4) | FAT$C_FIXED;

	fatp->fat$l_hiblk=VMSSWAP((1+inode->i_blocks));
	fatp->fat$l_efblk=VMSSWAP((1+inode->i_size/512));
	fatp->fat$w_ffbyte=inode->i_size%512;

#if 0
	inode->i_blocks = ((le16_to_cpu(fatp->u1.s1.fat_w_hiblkh) << 16) | le16_to_cpu(fatp->u1.s1.fat_w_hiblkl));
	inode->i_size = ((le16_to_cpu(fatp->u2.s1.fat_w_efblkh) << 16) | le16_to_cpu(fatp->u2.s1.fat_w_efblkl)) << 9;
	if (inode->i_size > 0) { inode->i_size -= 512; }
#endif
	fatp->fat$w_versions=1;
				
	ods2fhp->parent = (fh2p->u6.s1.fid_b_nmx << 16) |  le16_to_cpu(fh2p->u6.s1.fid_w_num);

	ods2_write_super(inode->i_sb);
	
	mark_buffer_dirty(bh);
	if (do_sync) {
		ll_rw_block (WRITE, 1, &bh);
		wait_on_buffer (bh);
		if (buffer_req(bh) && !buffer_uptodate(bh)) {
			printk ("IO error syncing ods2 inode ["
				"%s:%08lx]\n",
				bdevname(inode->i_dev), inode->i_ino);
			err = -EIO;
		}
	}
	brelse (bh);
	return err;
}

void ods2_write_inode (struct inode * inode, int wait)
{
        //lock_kernel();
        ods2_update_inode (inode, wait);
        //unlock_kernel();
}

int ods2_sync_inode (struct inode *inode)
{
        return ods2_update_inode (inode, 1);
}

static int ods2_link (struct dentry * old_dentry, struct inode * dir,
	struct dentry *dentry)
{
	struct inode *inode = old_dentry->d_inode;

	if (S_ISDIR(inode->i_mode))
		return -EPERM;

	if (inode->i_nlink >= 42 /*ODS2_LINK_MAX not yet? */)
		return -EMLINK;

	inode->i_ctime = CURRENT_TIME;
	ods2_inc_count(inode);
	atomic_inc(&inode->i_count);

	return ods2_add_nondir(dentry, inode);
}

static inline void ods2_inc_count(struct inode *inode)
{
        inode->i_nlink++;
        mark_inode_dirty(inode);
}

static inline void ods2_dec_count(struct inode *inode)
{
        inode->i_nlink--;
        mark_inode_dirty(inode);
}

/*
 * Set the first fragment of directory.
 */
int ods2_make_empty(struct inode *inode, struct inode *parent)
{
	panic("not yet\n");
#if 0
	short buf[256];

	memset(buf, 0, 512);
	buf[0]=0xffff;


	int err=0;

	struct page     *page, *cached_page = 0;
	struct address_space *mapping = inode->i_mapping;


	page = __grab_cache_page(mapping, index, &cached_page);
	if (!page)
		break;

	/* We have exclusive IO access to the page.. */
	if (!PageLocked(page)) {
		PAGE_BUG(page);
	}

	kaddr = kmap(page);
	status = mapping->a_ops->prepare_write(file, page, offset, offs\
					       et+bytes);
	if (status)
		goto sync_failure;
	page_fault = __copy_from_user(kaddr+offset, buf, bytes);
	flush_dcache_page(page);
	status = mapping->a_ops->commit_write(file, page, offset, offse\
					      t+bytes);



	if (err)
		goto fail;

fail:
	return err;
#endif
}

static int ods2_get_block(struct inode *inode, long iblock, struct buffer_head *
			  bh_result, int create)
{
	int lbn;
	struct super_block * sb=inode->i_sb;
	ODS2SB * ods2p;
	ODS2FH                   *ods2fhp=(ODS2FH *)inode->u.generic_ip;
	ODS2MAP * map, * newmap;
	int pos;
	struct hm2def * hm2;
	int vbn=iblock;
	int i;

	lbn = vbn2lbn(sb, ods2fhp->map, vbn);

	if (lbn > 0) {
		bh_result->b_dev = inode->i_dev;
                bh_result->b_blocknr = lbn;
                bh_result->b_state |= (1UL << BH_Mapped);
		return 0;
	}

	if (create == 0)
		return -1;

	sb = inode->i_sb;
	ods2p = ODS2_SB(sb);
	ods2fhp = inode->u.generic_ip;

	bitmap_search(sb, &pos, 1);
	bitmap_modify(sb, pos, 1, 0);

	//new_map = kmalloc(sizeof(ODS2MAP), GFP_KERNEL);
	
	hm2=ods2p->hm2;
	lbn=hm2->hm2$w_cluster*pos;

	//new_map->lbn=lbn;
	//new_map->cnt=1 * hm2->hm2$w_cluster;

	map = ods2fhp->map;

	for(i=0;i<16;i++) {
		if (map->s1[i].lbn==0) {
			map->s1[i].lbn=lbn;
			map->s1[i].lbn=1 * hm2->hm2$w_cluster;
			goto map_set;
		}
	}

	map->nxt = kmalloc(sizeof(ODS2MAP), GFP_KERNEL);
	map = map->nxt;
	memset(map, 0, sizeof(ODS2MAP));

	i=0;
	map->s1[i].lbn=lbn;
	map->s1[i].lbn=1 * hm2->hm2$w_cluster;

 map_set:

	bh_result->b_dev = inode->i_dev;
	bh_result->b_blocknr = lbn;
	bh_result->b_state |= (1UL << BH_New) | (1UL << BH_Mapped);
	return 0;
}

static int ods2_writepage(struct page *page)
{
        return block_write_full_page(page,ods2_get_block);
}
static int ods2_readpage(struct file *file, struct page *page)
{
        return block_read_full_page(page,ods2_get_block);
}
static int ods2_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
        return block_prepare_write(page,from,to,ods2_get_block);
}
static int ods2_bmap(struct address_space *mapping, long block)
{
        return generic_block_bmap(mapping,block,ods2_get_block);
}
static int ods2_direct_IO(int rw, struct inode * inode, struct kiobuf * iobuf, unsigned long blocknr, int blocksize)
{
        return generic_direct_IO(rw, inode, iobuf, blocknr, blocksize, ods2_get_block);
}
struct address_space_operations ods2_aops = {
        readpage: ods2_readpage,
        writepage: ods2_writepage,
        sync_page: block_sync_page,
        prepare_write: ods2_prepare_write,
        commit_write: generic_commit_write,
        bmap: ods2_bmap,
        direct_IO: ods2_direct_IO,
};

static int ods2_create (struct inode * dir, struct dentry * dentry, int mode)
{
	struct inode * inode = ods2_new_inode (dir, mode);
	int err = PTR_ERR(inode);
	if (!IS_ERR(inode)) {
		inode->i_op = &ods2_file_operations;
		inode->i_fop = &ods2_file_operations;
		inode->i_mapping->a_ops = &ods2_aops;
		mark_inode_dirty(inode);
		err = ods2_add_nondir(dentry, inode);
	}
	return err;
}

static int ods2_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
	struct inode * inode;
	int err = -EMLINK;

#if 0
	if (dir->i_nlink >= ODS2_LINK_MAX)
		goto out;
#endif

	ods2_inc_count(dir);

	inode = ods2_new_inode (dir, S_IFDIR | mode);
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out_dir;

	inode->i_op = &ods2_dir_inode_operations;
	inode->i_fop = &ods2_dir_operations;
	inode->i_mapping->a_ops = &ods2_aops;

	ods2_inc_count(inode);

	err = ods2_make_empty(inode, dir);
	if (err)
		goto out_fail;

	err = ods2_add_link(dentry, inode);
	if (err)
		goto out_fail;

	d_instantiate(dentry, inode);
out:
	return err;

out_fail:
	ods2_dec_count(inode);
	ods2_dec_count(inode);
	iput(inode);
out_dir:
	ods2_dec_count(dir);
	goto out;
}

int ods2_add_nondir(struct dentry *dentry, struct inode *inode)
{
	int err = ods2_add_link(dentry, inode);
	if (!err) {
		d_instantiate(dentry, inode);
		return 0;
	}
	ods2_dec_count(inode);
	iput(inode);
	return err;
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
