/*
 *  linux/fs/ods2/super.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/statfs.h>
#include <linux/parser.h>
#include <linux/version.h>
#include <linux/mount.h>

#include "ods2.h"

static int ods2_sync_fs(struct super_block *sb, int wait);

/*
  This routine is executed when the ODS2 file system is unmounted.
  The only thing we need to do is to release file INDEXF.SYS;1 and
  deallocate memory used for index file header bitmap.
*/

static void ods2_put_super(struct super_block *sb) {
	kfree(sb->s_fs_info);
}

/*
  This routine is executed when the user want to get information
  about the ODS2 file system. As we are read only we can just copy
  the information we were gathering during the mount into the buffer.
*/

int ods2_statfs(struct dentry * dentry, struct kstatfs *buf) {
	// TODO ODS2SB			   *ods2p = ODS2_SB (sb);

	//memcpy(buf, &ods2p->statfs, sizeof(struct kstatfs));
	return 0;
}

static int ods2_sync_fs(struct super_block *sb, int wait)
{
	// TODO
	/*
        struct ext2_sb_info *sbi = EXT2_SB(sb);
        struct ext2_super_block *es = EXT2_SB(sb)->s_es;
	*/

        /*                                                                      
         * Write quota structures to quota file, sync_blockdev() will write     
         * them to disk later                                                   
         */

	/*
        dquot_writeback_dquots(sb, -1);

        spin_lock(&sbi->s_lock);
        if (es->s_state & cpu_to_le16(EXT2_VALID_FS)) {
                ext2_debug("setting valid to 0\n");
                es->s_state &= cpu_to_le16(~EXT2_VALID_FS);
        }
        spin_unlock(&sbi->s_lock);
        ext2_sync_super(sb, es, wait);
	*/
        return 0;
}

static void ods2_evict_inode (struct inode *inode) {
        void * fh = inode->i_private;
	if (!fh)
		return;
	kfree (fh);
	clear_inode(inode);	
}

static int ods2_remount_fs (struct super_block *sb, int *flags, char *data)  {
	// TODO
	return 0;
}

static struct super_operations ods2_sops = {
	.write_inode=	ods2_write_inode,
	.evict_inode=	ods2_evict_inode,
	.put_super=	ods2_put_super,
	.sync_fs        = ods2_sync_fs,
	.statfs=		ods2_statfs,
	.remount_fs=		ods2_remount_fs,
	// TODO .delete_inode=	ods2_delete_inode,
	// TODO .clear_inode=	ods2_clear_inode,
	// TODO .write_super=	ods2_write_super,
	//remount_fs=	NULL,
};

/*
  This array is used to get the number of bits set for a nibble value.
*/

static char unsigned nibble2bits[] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

/*
  This routine open and read the BITMAP.SYS;1 file.
*/

int ods2_read_bitmap(struct super_block *sb) {
	ODS2SB			   *ods2p = ODS2_SB (sb);
	struct buffer_head	   *bh;
	struct inode		   *inode;
	
	if ((inode = ods2_iget(sb, 2)) != NULL) { /* this is BITMAP.SYS */
		ODS2FH			 *ods2fhp = (ODS2FH *)(inode->i_private);
		u32			  lbn;

		if ((lbn = vbn2lbn(sb, ods2fhp->map, 1)) > 0 &&
		    (bh = sb_bread(sb, GETBLKNO(sb, lbn))) != NULL && bh->b_data != NULL) {
			
			struct scbdef	       *scb = (SCBDEF *)(GETBLKP(sb, lbn, bh->b_data));
			short unsigned	       *p;
			short unsigned		chksum = 0;
			
			for (p = (short unsigned *)scb ; p < (short unsigned *)&(scb->scb$w_checksum) ; chksum += *p++);
			
			if (scb->u1.s1.scb$b_structlevl == 2 && scb->u1.s1.scb$b_structlevv >= 1 &&
			    scb->scb$w_cluster == ods2p->hm2->hm2$w_cluster &&
			    scb->scb$w_checksum == chksum) {
				
				struct buffer_head   *bh2;
				u32		      vbn = 1;
				u32		      bitset = 0;
				
				/*
				  We need to loop through all bytes that make up the bitmap.
				  The fastest way to count the number of bits set in the byte
				  is to have a nibble table that has the number of bits for the
				  values of 0 to 15. By adding the number of bits for the low
				  and high nibble we can get the total amount of bits set.
				*/

				while (vbn * 512 < inode->i_size && (lbn = vbn2lbn(sb, ods2fhp->map, vbn + 1)) > 0 &&
				       (bh2 = sb_bread(sb, GETBLKNO(sb, lbn))) != NULL && bh->b_data != NULL) {
					
					u8      	   *bp = (char unsigned *)(GETBLKP(sb, lbn, bh2->b_data));
					int		    cnt;
					
					for (cnt = 0; cnt < 512; cnt++, bp++) { bitset += (nibble2bits[*bp & 0x0f] + nibble2bits[*bp >> 4]); }
					brelse(bh2);
					vbn++;
				}
				bitset *= scb->scb$w_cluster; /* each bit represent 1 or more blocks (cluster factor) */
				ods2p->statfs.f_blocks = scb->scb$l_volsize;
				ods2p->statfs.f_bfree = bitset;
				ods2p->statfs.f_bavail = bitset;
				brelse(bh);
				iput(inode);
				lbn = vbn2lbn(sb, ods2fhp->map, 2);
				ods2p->sbh = __bread(sb->s_bdev, lbn, (scb->scb$l_volsize/(512*8*scb->scb$w_cluster)+1)<<9);
				return 1; /* everything went ok */
			}
			brelse(bh); /* invalid data in VBN 1 */
		}
		iput(inode); /* could not read VBN 1 */
	}
	return 0; /* unable to get inode 2 OR some other problem */
}

/*
  This routine allocate memory for the index file header bitmap
  and copy data from the INDEXF.SYS file. At the same time the
  number of free file headers are counted.
*/

int ods2_read_ibitmap(struct super_block *sb) {
	ODS2SB			   *ods2p = ODS2_SB(sb);
	struct buffer_head	   *bh;
	int			    idx;

	ods2p->statfs.f_ffree = 0;
	if ((ods2p->ibitmap = kmalloc(ods2p->hm2->hm2$w_ibmapsize << 9, GFP_KERNEL)) != NULL) {
		memset(ods2p->ibitmap, 0, (ods2p->hm2->hm2$w_ibmapsize << 9));
		for (idx = 0 ; idx < ods2p->hm2->hm2$w_ibmapsize ; idx++) {
			if ((bh = sb_bread(sb, GETBLKNO(sb, ods2p->hm2->hm2$l_ibmaplbn + idx))) != NULL && bh->b_data != NULL) {
				u8      	   *bp = (GETBLKP(sb, ods2p->hm2->hm2$l_ibmaplbn + idx, bh->b_data));
				int		    cnt;

				memcpy((ods2p->ibitmap + (idx << 9)), GETBLKP(sb, ods2p->hm2->hm2$l_ibmaplbn + idx, bh->b_data), 512);
				for (cnt = 0; cnt < 512; cnt++, bp++) { ods2p->statfs.f_ffree += (nibble2bits[(*bp & 0x0f) ^ 0xf] + nibble2bits[(*bp >> 4) ^ 0xf]); }
#if 0
// not yet
				bforget(bh);
#endif
			}
		}
		return 1;
	}
	printk("ODS2-fs error when allocating memory for index file header bitmap\n");
	return 0;
}

/*
  This is the routine that is invoked when an ODS2 file system
  is mounted.
*/

static int ods2_fill_super(struct super_block *sb, void *data, int silent)
{
	struct buffer_head	   *bh;
	ODS2SB			   *ods2p;

	sb_min_blocksize(sb, 512);
	if ((bh = sb_bread(sb, GETBLKNO(sb, 1))) != NULL && bh->b_data != NULL) {

		u16     	   *p;
		u16		    chksum1 = 0;
		u16                 chksum2 = 0;

		if ((ODS2_SB(sb) = kmalloc(sizeof(ODS2SB), GFP_KERNEL)) == NULL) {
			printk("ODS2-fs kmalloc failed for sb generic\n");
			return NULL;
		}
		ods2p = ODS2_SB(sb);
		//memcpy(&ods2p->hm2, GETBLKP(sb, 1, bh->b_data), sizeof(HM2DEF));
		ods2p->bh = bh;
		ods2p->hm2 = (void *) bh->b_data;
		//brelse(bh);
		
		for (p = (u16 *)(ods2p->hm2) ; p < (u16 *)&(ods2p->hm2->hm2$w_checksum1) ; chksum1 += *p++);
		for (p = (u16 *)(ods2p->hm2) ; p < (u16 *)&(ods2p->hm2->hm2$w_checksum2) ; chksum2 += *p++);

		/*
		  This is the way to check for a valid home block.
		*/

		printk("O %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",ods2p->hm2->hm2$l_homelbn,ods2p->hm2->hm2$l_alhomelbn,ods2p->hm2->hm2$l_altidxlbn,ods2p->hm2->hm2$w_cluster,ods2p->hm2->u1.s1.hm2$b_structlevl,ods2p->hm2->u1.s1.hm2$b_structlevv,ods2p->hm2->hm2$w_homevbn,ods2p->hm2->hm2$l_ibmaplbn,ods2p->hm2->hm2$l_maxfiles,ods2p->hm2->hm2$w_resfiles,chksum1,chksum2,ods2p->hm2->hm2$w_checksum1,ods2p->hm2->hm2$w_checksum2);
		if (ods2p->hm2->hm2$l_homelbn != 0 && ods2p->hm2->hm2$l_alhomelbn != 0 &&
		    ods2p->hm2->hm2$l_altidxlbn != 0 && ods2p->hm2->hm2$w_cluster != 0 &&
		    ods2p->hm2->u1.s1.hm2$b_structlevl == 2 && ods2p->hm2->u1.s1.hm2$b_structlevv >= 1 &&
		    ods2p->hm2->hm2$w_homevbn != 0 && ods2p->hm2->hm2$l_ibmaplbn != 0 &&
		    ods2p->hm2->hm2$l_maxfiles > ods2p->hm2->hm2$w_resfiles && ods2p->hm2->hm2$w_resfiles >= 5 &&
		    chksum1 == ods2p->hm2->hm2$w_checksum1 && chksum2 == ods2p->hm2->hm2$w_checksum2) {

			ods2p->flags.v_raw = 0;
			ods2p->flags.v_lowercase = 0;
			ods2p->flags.v_version = SB$M_VERSALL;
			ods2p->dollar = '$';
			ods2p->semicolon = ';';
			if (data != NULL) { parse_options(sb, data); }

			sb->s_op = &ods2_sops;

			ods2p->indexf = ods2_iget(sb, 1); /* read INDEXF.SYS. */

			sb->s_root = d_make_root(ods2_iget(sb, 4)); /* this is 000000.DIR;1 */
			
			ods2p->ibh = __bread(sb->s_bdev, ods2p->hm2->hm2$l_ibmaplbn, ods2p->hm2->hm2$w_ibmapsize << 9);;

			/*
			  We need to be able to read the index file header bitmap.
			*/

			if (ods2_read_ibitmap(sb)) {
				
				/*
				  We need to be able to read BITMAP.SYS as it contains the bitmap for allocated blocks.
				  Without this file we need to rebuild it by reading ALL file mapping pointers for ALL
				  files and create the file. That will be in a later release...
				*/
				
				if (ods2_read_bitmap(sb)) {
					char			format[13];
					char			volname[13];
					char			volowner[13];
					
					/*
					  We need to fill in statfs structure used when any user want to get information about
					  the mounted ODS2 file system.
					  Some of the information is static and other is found in BITMAP.SYS.
					*/
					
					ods2p->statfs.f_type = 0x3253444f; /* 2SDO */
					ods2p->statfs.f_bsize = 512;
					ods2p->statfs.f_files = ods2p->hm2->hm2$l_maxfiles;
					ods2p->statfs.f_namelen = 80;
		
					memcpy(format, ods2p->hm2->hm2$t_format, 12);
					format[12] = 0;
					memcpy(volname, ods2p->hm2->hm2$t_volname, 12);
					volname[12] = 0;
					memcpy(volowner, ods2p->hm2->hm2$t_ownername, 12);
					volowner[12] = 0;
					printk("ODS2-fs This is a valid ODS2 file system with format /%s/ and volume name /%s/ and owner /%s/\n", format, volname, volowner);
					return 0;
				}
#if 0
// not yet
				kfree(ods2p->ibitmap);
#endif
			}
		}
#if 0
// not yet
		kfree(ODS2_SB(sb));
#endif
	}
	return -EINVAL;
}

static void ods2_commit_super (struct super_block * sb,
                               struct ods2sb * es)
{
        //es->s_wtime = cpu_to_le32(CURRENT_TIME);
        mark_buffer_dirty(((struct ods2sb *)ODS2_SB(sb))->bh);
        // TODO sb->s_dirt = 0;
}

static void ods2_sync_super(struct super_block *sb, struct hm2def *es)
{
        //es->s_wtime = cpu_to_le32(CURRENT_TIME);
        mark_buffer_dirty(((struct ods2sb *)ODS2_SB(sb))->bh);
        // TODO ll_rw_block(WRITE, 1, &((struct ods2sb *)ODS2_SB(sb))->bh);
        wait_on_buffer(((struct ods2sb *)ODS2_SB(sb))->bh);
        //sb->s_dirt = 0;
}

void ods2_write_super (struct super_block * sb)
{
        struct hm2def * es;
	struct ods2sb * osb = ODS2_SB(sb);

	#define MS_RDONLY     1
        if (!(sb->s_flags & MS_RDONLY)) {
                es = osb->hm2;

#if 0
                if (le16_to_cpu(es->s_state) & EXT2_VALID_FS) {
                        ext2_debug ("setting valid to 0\n");
                        es->s_state = cpu_to_le16(le16_to_cpu(es->s_state) &
                                                  ~EXT2_VALID_FS);
                        es->s_mtime = cpu_to_le32(CURRENT_TIME);
                        ods2_sync_super(sb, es);
                } else
#endif
                        ods2_commit_super (sb, 0);
        }
        //sb->s_dirt = 0;
	if (!sb_rdonly(sb))
                ods2_sync_fs(sb, 1);
}

static  struct dentry * ods2_get_sb(struct file_system_type *fs_type,
				       int flags, const char *dev_name, void *data)
{
        return mount_bdev(fs_type, flags, dev_name, data, ods2_fill_super);
}

static struct file_system_type ods2_fs_type = {
#if 1
	.owner          = THIS_MODULE,
#endif
	.name           = "ods2",
	.mount          = ods2_get_sb,
	.kill_sb        = kill_block_super,
	.fs_flags       = FS_REQUIRES_DEV,
};

static int __init init_ods2_fs(void)
{
	printk("ods2 reg\n");
        return register_filesystem(&ods2_fs_type);
}

static void __exit exit_ods2_fs(void)
{
	unregister_filesystem(&ods2_fs_type);
}

// TODO EXPORT_NO_SYMBOLS;

module_init(init_ods2_fs);
module_exit(exit_ods2_fs);

MODULE_AUTHOR("Jonas Lindholm - <jlhm@usa.net>");
MODULE_DESCRIPTION("ODS2 Filesystem");
MODULE_LICENSE("GPL");

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
