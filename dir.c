/*
 *  linux/fs/ods2/dir.c
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

/*
  This routine return one or more file names for a directory file.
  For an ODS2 file structure each file name can have one or more
  versions of a file, each file must be treated as a unique file.
*/

int ods2_readdir(struct file *filp, void *dirent, filldir_t filldir) {
	struct inode		   *inode = filp->f_dentry->d_inode;
	struct super_block	   *sb = inode->i_sb;
	struct buffer_head	   *bh = NULL;
	ODS2SB			   *ods2p = (ODS2SB *)sb->u.generic_sbp;
	ODS2FH			   *ods2fhp = (ODS2FH *)inode->u.generic_ip;
	ODS2FILE		   *ods2filep = (ODS2FILE *)filp->private_data;
	loff_t			    pos = filp->f_pos;
	u32			    vbn = (ods2filep->currec >> 9); /* get current VBN-1 to use */
	u32			    lbn;
	char			    cdirname[256];

	memset(cdirname, ' ', sizeof(cdirname));
	/*
	  When there are no more files to return the file position in file
	  is set to -1.
	*/
	
	if (pos == -1) return 0;
	
	/*
	  When we get called the first time for a directory file the file
	  position is set to 0. We must then return two fake entries,
	  . for the current directory and .. for the parent directory.
	*/
	
	if (pos == 0) {
		filldir(dirent, ".", 1, 0, inode->i_ino, DT_DIR);
		filldir(dirent, "..", 2, 1, ods2fhp->parent, DT_DIR);
		ods2filep->currec = 0;
		ods2filep->curbyte = 0;
		vbn = 0;
	}
	
	/*
	  As long we can translate the virtual block number, VBN, to a
	  logical block number, LBN, and read the block we continue to loop.
	*/
	
	while (vbn * 512 < inode->i_size && (lbn = vbn2lbn(sb, ods2fhp->map, vbn + 1)) > 0 &&
	       (bh = sb_bread(sb, GETBLKNO(sb, lbn))) != NULL && bh->b_data != NULL) {

		u16      	   *recp = (short unsigned *)((char *)(GETBLKP(sb, lbn, bh->b_data)) + (ods2filep->currec & 511));
		
		/*
		  For a ODS2 directory each block contains 1 to 62 directory entries.
		  Note that a directory entry can not span between two or more blocks.
		  We should be able to use the routine to read variable block size but
		  because directory file is so specific we do our own block decoding here.
		  When there are no more directory entries in the current block the record
		  length -1 is inserted as the last record.
		*/
		
		while (*recp != 65535 && *recp <= 512 && ods2filep->currec < inode->i_size) {
			DIRDEF	   *dire = (DIRDEF *)recp;
			char	    dirname[dire->u1.s1.dir_b_namecount + 1];
			
			memcpy(dirname, &dire->u1.s1.dir_t_name, dire->u1.s1.dir_b_namecount);
			dirname[dire->u1.s1.dir_b_namecount] = 0;
			if (ods2p->dollar != '$' || ods2p->flags.v_lowercase) {
				char	       *p = dirname;
				char		cnt = dire->u1.s1.dir_b_namecount;

				while (*p && cnt-- > 0) { if (*p == '$') { *p = ods2p->dollar; } if (ods2p->flags.v_lowercase) { *p = tolower(*p); } p++; }
			}
			if (ods2filep->curbyte == 0) { ods2filep->curbyte = ((dire->u1.s1.dir_b_namecount + 1) & ~1) + 6; }
			filp->f_pos = ods2filep->currec + ods2filep->curbyte;
			
			while (ods2filep->curbyte < dire->u1.s1.dir_w_size &&
			       !(ods2p->flags.v_version != SB_M_VERSALL && strlen(dirname) == strlen(cdirname) && strncmp(dirname, cdirname, strlen(dirname)) == 0)) {

				DIRDEF		     *dirv = (DIRDEF *)((char *)dire + ods2filep->curbyte);
				u32		      ino = (dirv->u1.s2.u2.s3.fid_b_nmx << 16) | le16_to_cpu(dirv->u1.s2.u2.s3.fid_w_num);
				char		      dirnamev[dire->u1.s1.dir_b_namecount + 1 + 5 + 1];
				
				if (ino != 4) { /* we must ignore 000000.DIR as it is the same as . */
					if (ods2p->flags.v_version == SB_M_VERSNONE) {
						sprintf(dirnamev, "%s", dirname);
					} else {
						sprintf(dirnamev, "%s%c%d", dirname, ods2p->semicolon, dirv->u1.s2.dir_w_version);
					}

					/*
					  We don't really know if the file is a directory by just checking
					  the file extension but it is the best we can do.
					  Should the file have extension .DIR but be a regular file the mistake
					  will be detected later on when the user try to walk down into
					  the false directory.
					*/

					if (filldir(dirent, dirnamev, strlen(dirnamev), filp->f_pos, ino, 
						    (strstr(dirnamev, (ods2p->flags.v_lowercase ? ".dir." : ".DIR")) == NULL ? DT_REG : DT_DIR))) {
						/*
						  We come here when filldir is unable to handle more entries.
						*/

						brelse(bh);
						return 0;
					}
					if (ods2p->flags.v_version != SB_M_VERSALL) { strcpy(cdirname, dirname); }
				}
				if (ods2p->flags.v_version == SB_M_VERSALL) {
					ods2filep->curbyte += 8;
					filp->f_pos += 8;
				} else {
					ods2filep->curbyte = le16_to_cpu(dire->u1.s1.dir_w_size);
					filp->f_pos += dire->u1.s1.dir_w_size;
				}
			}
			
			/*
			  When we come here there are no more versions for the file name.
			  We then reset our current byte offset and set current record offset
			  to the next directory entry.
			*/
			
			ods2filep->curbyte = 0;
			ods2filep->currec += le16_to_cpu(dire->u1.s1.dir_w_size) + 2;
			recp = (u16 *)((char *)recp + le16_to_cpu(dire->u1.s1.dir_w_size) + 2);
		}
		
		/*
		  When we come here there are no more directory entries in the current block
		  and we just release the buffer and increase the VBN counter.
		*/
		
		brelse(bh);
		vbn++;
		ods2filep->currec = vbn * 512;
	}
	filp->f_pos = -1; /* this mark that we have no more files to return */
	return 0;
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
