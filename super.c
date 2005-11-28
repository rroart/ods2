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

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/locks.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h>

#include "ods2.h"

/*
  This routine is executed when the ODS2 file system is unmounted.
  The only thing we need to do is to release file INDEXF.SYS;1 and
  deallocate memory used for index file header bitmap.
*/

static void ods2_put_super(struct super_block *sb) {
	ODS2SB			   *ods2p = (ODS2SB *)sb->u.generic_sbp;
	
	if (ods2p != NULL) {
		iput(ods2p->indexf); /* release INDEXF.SYS;1 */
#if 0
// not yet
		kfree(ods2p->ibitmap);
		kfree(sb->u.generic_sbp);
#endif
	}
}

/*
  This routine is executed when the user want to get information
  about the ODS2 file system. As we are read only we can just copy
  the information we were gathering during the mount into the buffer.
*/

int ods2_statfs(struct super_block *sb, struct statfs *buf) {
	ODS2SB			   *ods2p = (ODS2SB *)sb->u.generic_sbp;

	memcpy(buf, &ods2p->statfs, sizeof(struct statfs));
	return 0;
}


static struct super_operations ods2_sops = {
	read_inode:	ods2_read_inode,
	write_inode:	ods2_write_inode,
	put_inode:	ods2_put_inode,
	delete_inode:	ods2_delete_inode,
	clear_inode:	ods2_clear_inode,
	put_super:	ods2_put_super,
	write_super:	ods2_write_super,
	statfs:		ods2_statfs,
	remount_fs:	NULL,
};


/*
  This array is used to get the number of bits set for a nibble value.
*/

static char unsigned nibble2bits[] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

/*
  This routine open and read the BITMAP.SYS;1 file.
*/

int ods2_read_bitmap(struct super_block *sb) {
	ODS2SB			   *ods2p = (ODS2SB *)sb->u.generic_sbp;
	struct buffer_head	   *bh;
	struct inode		   *inode;
	
	if ((inode = iget(sb, 2)) != NULL) { /* this is BITMAP.SYS */
		ODS2FH			 *ods2fhp = (ODS2FH *)(inode->u.generic_ip);
		u32			  lbn;

		if ((lbn = vbn2lbn(sb, ods2fhp->map, 1)) > 0 &&
		    (bh = sb_bread(sb, GETBLKNO(sb, lbn))) != NULL && bh->b_data != NULL) {
			
			struct scbdef	       *scb = (SCBDEF *)(GETBLKP(sb, lbn, bh->b_data));
			short unsigned	       *p;
			short unsigned		chksum = 0;
			
			for (p = (short unsigned *)scb ; p < (short unsigned *)&(scb->scb_w_checksum) ; chksum += *p++);
			
			if (scb->u1.s1.scb_b_structlevl == 2 && scb->u1.s1.scb_b_structlevv >= 1 &&
			    scb->scb_w_cluster == ods2p->hm2->hm2_w_cluster &&
			    scb->scb_w_checksum == chksum) {
				
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
				bitset *= scb->scb_w_cluster; /* each bit represent 1 or more blocks (cluster factor) */
				ods2p->statfs.f_blocks = scb->scb_l_volsize;
				ods2p->statfs.f_bfree = bitset;
				ods2p->statfs.f_bavail = bitset;
				brelse(bh);
				iput(inode);
				lbn = vbn2lbn(sb, ods2fhp->map, 2);
				ods2p->sbh = bread(sb->s_dev, lbn, (scb->scb$l_volsize/(512*8*scb->scb$w_cluster)+1)<<9);
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
	ODS2SB			   *ods2p = (ODS2SB *)sb->u.generic_sbp;
	struct buffer_head	   *bh;
	int			    idx;

	ods2p->statfs.f_ffree = 0;
	if ((ods2p->ibitmap = kmalloc(ods2p->hm2->hm2_w_ibmapsize << 9, GFP_KERNEL)) != NULL) {
		memset(ods2p->ibitmap, 0, (ods2p->hm2->hm2_w_ibmapsize << 9));
		for (idx = 0 ; idx < ods2p->hm2->hm2_w_ibmapsize ; idx++) {
			if ((bh = sb_bread(sb, GETBLKNO(sb, ods2p->hm2->hm2_l_ibmaplbn + idx))) != NULL && bh->b_data != NULL) {
				u8      	   *bp = (GETBLKP(sb, ods2p->hm2->hm2_l_ibmaplbn + idx, bh->b_data));
				int		    cnt;

				memcpy((ods2p->ibitmap + (idx << 9)), GETBLKP(sb, ods2p->hm2->hm2_l_ibmaplbn + idx, bh->b_data), 512);
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

static struct super_block * ods2_read_super(struct super_block *sb, void *data, int silent) {
	struct buffer_head	   *bh;
	ODS2SB			   *ods2p;

	sb_set_blocksize(sb, get_hardsect_size(sb->s_dev));
	if ((bh = sb_bread(sb, GETBLKNO(sb, 1))) != NULL && bh->b_data != NULL) {

		u16     	   *p;
		u16		    chksum1 = 0;
		u16                 chksum2 = 0;

		if ((sb->u.generic_sbp = kmalloc(sizeof(ODS2SB), GFP_KERNEL)) == NULL) {
			printk("ODS2-fs kmalloc failed for sb generic\n");
			return NULL;
		}
		ods2p = (ODS2SB *)sb->u.generic_sbp;
		//memcpy(&ods2p->hm2, GETBLKP(sb, 1, bh->b_data), sizeof(HM2DEF));
		ods2p->bh = bh;
		ods2p->hm2 = bh->b_data;
		//brelse(bh);
		
		for (p = (u16 *)(ods2p->hm2) ; p < (u16 *)&(ods2p->hm2->hm2_w_checksum1) ; chksum1 += *p++);
		for (p = (u16 *)(ods2p->hm2) ; p < (u16 *)&(ods2p->hm2->hm2_w_checksum2) ; chksum2 += *p++);

		/*
		  This is the way to check for a valid home block.
		*/

		if (ods2p->hm2->hm2_l_homelbn != 0 && ods2p->hm2->hm2_l_alhomelbn != 0 &&
		    ods2p->hm2->hm2_l_altidxlbn != 0 && ods2p->hm2->hm2_w_cluster != 0 &&
		    ods2p->hm2->u1.s1.hm2_b_structlevl == 2 && ods2p->hm2->u1.s1.hm2_b_structlevv >= 1 &&
		    ods2p->hm2->hm2_w_homevbn != 0 && ods2p->hm2->hm2_l_ibmaplbn != 0 &&
		    ods2p->hm2->hm2_l_maxfiles > ods2p->hm2->hm2_w_resfiles && ods2p->hm2->hm2_w_resfiles >= 5 &&
		    chksum1 == ods2p->hm2->hm2_w_checksum1 && chksum2 == ods2p->hm2->hm2_w_checksum2) {
			

			ods2p->flags.v_raw = 0;
			ods2p->flags.v_lowercase = 0;
			ods2p->flags.v_version = SB_M_VERSALL;
			ods2p->dollar = '_';
			ods2p->semicolon = '.';
			if (data != NULL) { parse_options(sb, data); }

			sb->s_op = &ods2_sops;

			ods2p->indexf = iget(sb, 1); /* read INDEXF.SYS. */

			extend_map(((ODS2FH *)ods2p->indexf->u.generic_ip)->map); // gross hack
			
			sb->s_root = d_alloc_root(iget(sb, 4)); /* this is 000000.DIR;1 */
			
			ods2p->ibh = bread(sb->s_dev, ods2p->hm2->hm2$l_ibmaplbn, ods2p->hm2->hm2$w_ibmapsize << 9);;

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
					ods2p->statfs.f_files = ods2p->hm2->hm2_l_maxfiles;
					ods2p->statfs.f_namelen = 80;
		
					memcpy(format, ods2p->hm2->hm2_t_format, 12);
					format[12] = 0;
					memcpy(volname, ods2p->hm2->hm2_t_volname, 12);
					volname[12] = 0;
					memcpy(volowner, ods2p->hm2->hm2_t_ownername, 12);
					volowner[12] = 0;
					printk("ODS2-fs This is a valid ODS2 file system with format /%s/ and volume name /%s/ and owner /%s/\n", format, volname, volowner);
					return sb;
				}
#if 0
// not yet
				kfree(ods2p->ibitmap);
#endif
			}
		}
#if 0
// not yet
		kfree(sb->u.generic_sbp);
#endif
	}
	return NULL;
}

static void ods2_commit_super (struct super_block * sb,
                               struct ods2sb * es)
{
        //es->s_wtime = cpu_to_le32(CURRENT_TIME);
        mark_buffer_dirty(((struct ods2sb *)sb->u.generic_sbp)->bh);
        sb->s_dirt = 0;
}

static void ods2_sync_super(struct super_block *sb, struct hm2def *es)
{
        //es->s_wtime = cpu_to_le32(CURRENT_TIME);
        mark_buffer_dirty(ODS2_SB(sb)->bh);
        ll_rw_block(WRITE, 1, &ODS2_SB(sb)->bh);
        wait_on_buffer(ODS2_SB(sb)->bh);
        sb->s_dirt = 0;
}

void ods2_write_super (struct super_block * sb)
{
        struct hm2def * es;
	struct ods2sb * osb = sb -> u.generic_sbp;

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
                        ods2_commit_super (sb, es);
        }
        sb->s_dirt = 0;
}

static DECLARE_FSTYPE_DEV(ods2_fs_type, "ods2", ods2_read_super);

static int __init init_ods2_fs(void)
{
        return register_filesystem(&ods2_fs_type);
}

static void __exit exit_ods2_fs(void)
{
	unregister_filesystem(&ods2_fs_type);
}

EXPORT_NO_SYMBOLS;

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
