/*
 * linux/fs/ods2/file.c
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 * Written 2003 by Jonas Lindholm <jlhm@usa.net>
 *
 * Changes:	0.9.2 - A lot of bug fixes for keeping track of
 *			virtual position for variable record files.
 *
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < 0x20612
#include <linux/config.h>
#endif
#ifdef TWOSIX
#include <linux/module.h>
#endif
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#ifndef TWOSIX
#include <linux/locks.h>
#endif
#include <linux/blkdev.h>
#ifndef TWOSIX
#include <asm/uaccess.h>
#else
#include <linux/buffer_head.h>
#endif

#include "ods2.h"

/*
  FUNCTION:

  This routine take care of ioctl command for an open file.
  It is possible to put a file into raw mode independing if raw mode was
  selected or not during the file system mount. This is used by the rms library.

  INPUT:

  *inode          pointer to inode structure for the open file.

  *filp           pointer to file structure for the open file.

  cmd             ODS2 specific command.

  arg             argument for the command.

  OUTPUT:

  0 if everything went ok.

  -ENOTTY for invalid cmmand.

  Other negativ values for different errors.

  IMPLICIT:

  None. 

*/

int ods2_file_ioctl(struct inode *inode, struct file *filp, int unsigned cmd, long unsigned arg) {
	struct super_block	   *sb = inode->i_sb;
	ODS2SB			   *ods2p = ODS2_SB(sb);
	ODS2FILE		   *ods2filep = (ODS2FILE *)filp->private_data;
	int			    error = -ENOTTY;
	int			    onoff;

	switch (cmd) {
	case ODS2_IOC_FISETRAW:
		if ((error = get_user(onoff, (int *)arg)) == 0) {
			ods2filep->u1.s1.v_raw = (onoff == 1);
		}
		break;
	case ODS2_IOC_FIGETRAW:
		onoff = ods2filep->u1.s1.v_raw;
		error = put_user(onoff, (int *)arg);
		break;
	case ODS2_IOC_SBGETRAW:
		onoff = ods2p->flags.v_raw;
		error = put_user(onoff, (int *)arg);
		break;
	}
	return error;
}

/*
  FUNCTION:

  This routine update the memory structure used to keep track of the virtual
  position in a variable record file.

  INPUT:

  loff            virtual position.

  *ods2vari       pointer to memory structure used to keep tracj of position.

  currec          current record position in file. This is the offset in bytes
                  from the start of the file.

  OUTPUT:

  1 if the update was successful.

  0 if something went wrong such as memory allocaion.

  IMPLICIT:

  The only requirement is that a linked list of varp structures are at least
  the number of entries allocated by macro IDXBLOCK.

 */

int update_virtual_file_pos(loff_t loff, ODS2VARI *ods2vari, u64 currec) {
	ODS2VAR		     *ods2varp = NULL;
	int		      idxvar = IDXVAR(loff);
	int		      idxvari = IDXVARI(loff);
	int		      idxblock = IDXBLOCK(loff);
	
	if (ods2vari->ods2varp[idxvari] == NULL) {
		if ((ods2vari->ods2varp[idxvari] = (ODS2VAR *)kmalloc(sizeof(ODS2VAR), GFP_KERNEL)) != NULL) {
			memset(ods2vari->ods2varp[idxvari], 0, sizeof(ODS2VAR));
		} else {
			printk("ODS2-fs kmalloc failed for new varp (1)\n");
			return 0;
		}
	}
	ods2varp = ods2vari->ods2varp[idxvari];
	for (; idxblock > 0; idxblock--) {
		if (ods2varp->nxt == NULL) {
			if ((ods2varp->nxt = (ODS2VAR *)kmalloc(sizeof(ODS2VAR), GFP_KERNEL)) != NULL) {
				memset(ods2varp->nxt, 0, sizeof(ODS2VAR));
			} else {
				printk("ODS2-fs kmalloc failed for new varp (2)\n");
				return 0;
			}
		}
		ods2varp = ods2varp->nxt;
	}
	if (ods2varp != NULL && ods2varp->s1[idxvar].loff == 0) {
		ods2varp->s1[idxvar].recoffs = currec;
		ods2varp->s1[idxvar].loff = loff;
		ods2vari->highidx = loff;
	}
	return 1;
}

/*
  FUNCTION:

  This routine take care of reading of variable record files.
  This routine will add a LF after each record if one of the following
  record attributes are set: FAT$M_FORTRANCC, FAT$M_IMPLIEDCC, FAT$M_PRINTCC.

  Note that a correct handling of all record structures should be able
  to handle form feed and insertion of more than one LF after each record.
  All this extra functionality must be handled outside of this driver.

  It will also handle the FAT$M_NOSPAN. This attributes indicates that no
  record must span between blocks. In each block a record with length
  65535 (-1) is inserted to indicate that there are no more records in the
  current block.

  INPUT:

  *filp            pointer to the file.

  *buf             buffer where to return data.

  buflen           size of buf area.

  *loff            virtual position in file where to read from.

  OUTPUT:

  The number of bytes read.
  0 if no bytes was read.

  IMPLICIT:

  The ODS2 specific part of the file header must have a ODS2VARI structure
  attached to it.


*/

ssize_t ods2_read_variable(struct file *filp, char *buf, size_t buflen, loff_t *loff) {
	struct inode		   *inode = filp->f_inode;
	char			   *buforg = buf;
#if LINUX_VERSION_CODE < 0x2061A
	ODS2FH			   *ods2fhp = (ODS2FH *)inode->u.generic_ip;
#else
	ODS2FH			   *ods2fhp = (ODS2FH *)inode->i_private;
#endif
	ODS2FILE		   *ods2filep = (ODS2FILE *)filp->private_data;
	ODS2VARI		   *ods2vari = ods2fhp->ods2vari;
	FATDEF			   *fatp = (FATDEF *)&(ods2fhp->fat);
	u32			    vbn = 0;
	u16     		    cpylen;
	

	if (*loff == 0) {
		ods2filep->currec = 0;
		ods2filep->curbyte = 0;
		ods2filep->reclen = 0;
	}
	
	if (ods2filep->reclen == 65535) {
		brelse(ods2filep->bhp);
		ods2filep->bhp = NULL;
		return 0;
	}
	
	while (1) {
		
		/*
		  We need to loop until the calculated value of currec offset plus currect byte offset from currec give
		  the same VBN as the last one we fetched.
		  There is one case when we will loop. That case is when a record start is at the last two bytes of the
		  block. In that case the length will be fetched from current block but all data will start on next block.
		*/

		do {
			vbn = (ods2filep->currec + ods2filep->curbyte) >> 9;
			if (!(getfilebh(filp, vbn + 1))) {
				ods2filep->reclen = 65535;
				return (buf - buforg);
			}
			
			/*
			  If curbyte is zero we will start on a new record.
			*/
			
			if (ods2filep->curbyte == 0) {
				ods2filep->reclen = le16_to_cpu(*((u16 *)((char *)ods2filep->data + (ods2filep->currec & 511))));

				if ((*loff >> 16) != 0) {
					down(&(ods2vari->sem));
					update_virtual_file_pos(*loff, ods2vari, ods2filep->currec);
					up(&(ods2vari->sem));
				}
				
				if ((ods2filep->reclen == 65535 && !(fatp->fat$b_rattrib & FAT$M_NOSPAN)) ||
				    (ods2filep->currec >= inode->i_size)) { /* end of records */
					
					ods2filep->reclen = 65535;
					return (buf - buforg);
				}

				if (ods2filep->reclen == 65535 && (fatp->fat$b_rattrib & FAT$M_NOSPAN)) {
					ods2filep->currec = (vbn + 1) * 512; /* could be a new record at next block */
				} else {
					ods2filep->curbyte = 2;
					ods2filep->curbyte += (fatp->u0.s0.fat$v_rtype == FAT$C_VFC ? fatp->fat$b_vfcsize : 0);
				}
			}
		} while (((ods2filep->currec + ods2filep->curbyte) >> 9) != vbn);
		
		cpylen = MIN(MIN((ods2filep->reclen - ods2filep->curbyte + 2), buflen), (512 - ((ods2filep->currec + ods2filep->curbyte) & 511)));
		
		if (cpylen > 0) {
			u8       		 *recp = (u8 *)((char *)ods2filep->data + ((ods2filep->currec + ods2filep->curbyte) & 511));
			
			memcpy(buf, recp, cpylen);
			*loff += cpylen; /* loff will always be a virtual offset for a variable record file */
			buf += cpylen;
			buflen -= cpylen;
			ods2filep->curbyte += cpylen;
		}
		
		if (ods2filep->curbyte - 2 == ods2filep->reclen) {
			if (buflen > 0) {
				if (fatp->fat$b_rattrib & FAT$M_FORTRANCC || fatp->fat$b_rattrib & FAT$M_IMPLIEDCC || fatp->fat$b_rattrib & FAT$M_PRINTCC) {
					buflen--;
					*buf++ = '\n';
					*loff += 1;
				}
				ods2filep->currec = ((ods2filep->currec + ods2filep->reclen + 1) & ~1) + 2; /* each record is always even aligned */
				ods2filep->curbyte = 0;
			}
		}
		
		if (buflen == 0) { return (buf - buforg); }
	}
}

/*
  FUNCTION:

  This routine is invoked when the file type is one of STREAM, STREAMLF or STREAMCR.
  For a non-RMS machine that doesn't know anything about records these three formats
  are the same.
  For RMS the different between these formats is the following:
  
  STREAM:     Records are delimited by FF, VT, LF, or CRLF.
  STREAMLF:   Records are delimited by LF.
  STREAMCR:   Records are delimited by CR.
  
  Note that we can not use generic read routines even if we treat the data as just a 
  stream of bytes because the way we need to translate from VBN to LBN.

  INPUT:

  *filp            pointer to the file.

  *buf             buffer where to return data.

  buflen           size of buf area.

  *loff            virtual position in file where to read from.

  OUTPUT:

  The number of bytes read.
  0 if no bytes was read.

  IMPLICIT:

  None.
*/

ssize_t ods2_read_stream(struct file *filp, char *buf, size_t buflen, loff_t *loff) {
	struct inode		   *inode = filp->f_inode;
	char			   *buforg = buf;
	ODS2FILE		   *ods2filep = (ODS2FILE *)filp->private_data;
	u32			    vbn = 0;
	u16       		    cpylen;
	
	while (*loff < inode->i_size) {
		vbn = *loff >> 9;
		if (!(getfilebh(filp, vbn + 1))) {
			*loff = inode->i_size;
			return (buf - buforg);
		}
		if ((cpylen = MIN(MIN(inode->i_size - *loff, buflen), 512 - (*loff & 511))) > 0) {
			u8      	       *recp = (u8 *)((char *)ods2filep->data + (*loff & 511));
			
			memcpy(buf, recp, cpylen);
			*loff += cpylen;
			buf += cpylen;
			buflen -= cpylen;
			if (buflen == 0) {
				return (buf - buforg);
			}
		}
	}
	brelse(ods2filep->bhp);
	ods2filep->bhp = NULL;
	return (buf - buforg);
}

/*
  FUNCTION:

  This routine is called when a read request is done for any file.
  The routine will invoke one of two functions. One function is for
  files of STREAM types.
  The other routine is for VARIABLE record files.
  File of type RELATIVE or INDEXED are not supported by this module.

  Should the file system be mounted by option raw or if the file has
  been set to raw mode the routine to hamdle STREAM format is invoked
  for ALL file types including RELATIVE and INDEXED files.

  *filp            pointer to the file.

  *buf             buffer where to return data.

  buflen           size of buf area.

  *loff            virtual position in file where to read from.

  OUTPUT:

  The number of bytes read.
  0 if no bytes was read.

  IMPLICIT:

  None.
*/

ssize_t ods2_read(struct file *filp, char *buf, size_t buflen, loff_t *loff) {
	struct inode		   *inode = filp->f_inode;
	struct super_block	   *sb = inode->i_sb;
	ODS2SB			   *ods2p = ODS2_SB(sb);
#if LINUX_VERSION_CODE < 0x2061A
	ODS2FH			   *ods2fhp = (ODS2FH *)inode->u.generic_ip;
#else
	ODS2FH			   *ods2fhp = (ODS2FH *)inode->i_private;
#endif
	ODS2FILE		   *ods2filep = (ODS2FILE *)filp->private_data;
	FATDEF			   *fatp = (FATDEF *)&(ods2fhp->fat);
	

	if (ods2p->flags.v_raw || ods2filep->u1.s1.v_raw) {
		return ods2_read_stream(filp, buf, buflen, loff);
	} else {
		switch (fatp->u0.s0.fat$v_fileorg) {
		case FAT$C_SEQUANTIAL: {
			switch (fatp->u0.s0.fat$v_rtype) {
			case FAT$C_VFC:
			case FAT$C_VARIABLE: return ods2_read_variable(filp, buf, buflen, loff);
			case FAT$C_FIXED:
			case FAT$C_STREAMLF:
			case FAT$C_STREAMCR:
			case FAT$C_STREAM: return ods2_read_stream(filp, buf, buflen, loff);
			default: return 0;
			}
		}
		default: return 0;
		}
	}
}


/*
  FUNCTION:

  This routine return a valid file offset for STREAM files.
  Note that the current ODS2 driver does not support an offset that
  is larger then file size.

  INPUT:

  *filp           pointer to the file.

  loff            virtual position in file where to read from.

  seek            how loff should be calculated for the file.
                  0 = absolute position.
		  1 = offset from current file position.
		  2 = offset from end of file.

  OUTPUT:

  The new position in the file is returned.

  IMPLICIT:

  This routine will not allow the current position to be beyond
  the end of file position.
*/

loff_t ods2_llseek_stream(struct file *filp, loff_t loff, int seek) {
	struct inode		   *inode = filp->f_inode;
	loff_t			    offs;
	
	if (seek == 0) { /* SEEK_SET */
		offs = MIN(loff, inode->i_size);
	} else {
		if (seek == 1) { /* SEEK_CUR */
			if (loff > 0) {
				offs = MIN(filp->f_pos + loff, inode->i_size);
			} else {
				offs = MAX(filp->f_pos + loff, 0);
			}
		} else {
			offs = MIN(inode->i_size + loff, inode->i_size);
		}
	}
	filp->f_pos = offs;
#ifndef TWOSIX
// check this?
	filp->f_reada = 0;
#endif
	filp->f_version++;
	return offs;
}

/*
  FUNCTION:

  This routine return a valid file offset for VARIABLE files.
  Note that the current ODS2 driver does not support an offset that
  is larger then file size.
  This routine will take care of the fact that Linux doesn't know
  anything about records in a file so all routines and utilities
  believe the file offset is the exact position in the file.
  For a variable record file each record consists not only of data
  but also of the record length (2 bytes). An additional fix part
  of the record can contain meta data for the record such as print
  control information.
  All this make it complicated to calculate the record offset into
  the file from a given offset.
  To avoid to be forced to read from the start of the file to find
  the correct position for a given offset checkpoints are stored
  together with the inode for each 64K blocks of data.
  By using these checkpoints this routine can calculate the record
  position for a given offset by starting reading records from the
  closest checkpoint.
  If the requested position is within a part of the file already
  read no more than 128 blocks of data must be read to find the
  position.
  On the other hand if no reading has been done for the requested
  position before we could ending up to read all records for the
  remaining of the file but that is no other good solution to the
  problem.

  INPUT:

  *filp           pointer to the file.

  loff            virtual position in file where to read from.

  seek            how loff should be calculated for the file.
                  0 = absolute position.
		  1 = offset from current file position.
		  2 = offset from end of file.

  OUTPUT:

  The new position in the file is returned.

  IMPLICIT:

  This routine will not allow the current position to be beyond
  the end of file position.
*/


loff_t ods2_llseek_variable(struct file *filp, loff_t loff, int seek) {
	struct inode		   *inode = filp->f_inode;
	ODS2VAR			   *ods2varp = NULL;
#if LINUX_VERSION_CODE < 0x2061A
	ODS2FH			   *ods2fhp = (ODS2FH *)inode->u.generic_ip;
#else
	ODS2FH			   *ods2fhp = (ODS2FH *)inode->i_private;
#endif
	ODS2VARI		   *ods2vari = ods2fhp->ods2vari;
	ODS2FILE		   *ods2filep = (ODS2FILE *)filp->private_data;
	FATDEF			   *fatp = (FATDEF *)&(ods2fhp->fat);
	int			    idxblock = 0;
	loff_t			    offs = 0;
	loff_t			    coffs = 0;
	loff_t			    currec = 0;
	u32			    vbn = 0;
	u16      		    reclen = 0;

	offs = loff;
	if (seek == 0) { /* SEEK_SET */
		offs = MIN(offs, inode->i_size);
	} else {
		if (seek == 1) { /* SEEK_CUR */
			if (offs > 0) {
				offs = MIN(filp->f_pos + offs, inode->i_size);
			} else {
				offs = MAX(filp->f_pos + offs, 0);
			}
		} else {
			offs = MIN(inode->i_size + offs, inode->i_size);
		}
	}

	/*
	  offs - the absolute virtual offset into the file we want to find.
	  coffs - offset counter.
	*/

	down(&(ods2vari->sem));
	if (offs > 65535) {
		coffs = offs;
		if ((coffs >> 16) > (ods2vari->highidx >> 16)) {
			coffs = ods2vari->highidx;
		}
		coffs += 65536;
		do {
			coffs -= 65536;
			idxblock = IDXBLOCK(coffs);
			ods2varp = ods2vari->ods2varp[IDXVARI(coffs)];
			for (; idxblock > 0; idxblock--) {
				ods2varp = ods2varp->nxt;
			}
		} while (coffs > 65535 && ods2varp->s1[IDXVAR(coffs)].loff > offs);
		if (coffs > 65535) {
			currec = ods2varp->s1[IDXVAR(coffs)].recoffs;
			coffs = ods2varp->s1[IDXVAR(coffs)].loff;
		} else {
			coffs = 0;
		}
	}

	while (1) {
		
		do {
			vbn = currec >> 9;
			if (!(getfilebh(filp, vbn + 1))) {
				ods2filep->reclen = 65535;
				up(&(ods2vari->sem));
				filp->f_pos = coffs;
#ifndef TWOSIX
// check this?
				filp->f_reada = 0;
#endif
				filp->f_version++;
				return offs;
			}
			reclen = le16_to_cpu(*((u16 *)((char *)ods2filep->data + (currec & 511))));

			if ((coffs >> 16) != 0) {
				update_virtual_file_pos(coffs, ods2vari, currec);
			}

			if ((reclen == 65535 && !(fatp->fat$b_rattrib & FAT$M_NOSPAN)) || currec > inode->i_size) { /* end of records */
				ods2filep->reclen = 65535;
				up(&(ods2vari->sem));
				filp->f_pos = coffs;
#ifndef TWOSIX
// check this?
				filp->f_reada = 0;
#endif
				filp->f_version++;
				return offs;
			}
			if (reclen == 65535 && (fatp->fat$b_rattrib & FAT$M_NOSPAN)) {
				currec = (vbn + 1) * 512; /* next block... */
			}
		} while (reclen == 65535);
		
		if (coffs <= offs && (coffs + reclen - (fatp->u0.s0.fat$v_rtype == FAT$C_VFC ? fatp->fat$b_vfcsize : 0)) >= offs) { /* we have found our location */
			ods2filep->currec = currec;
			ods2filep->curbyte = (offs - coffs) + 2 + (fatp->u0.s0.fat$v_rtype == FAT$C_VFC ? fatp->fat$b_vfcsize : 0);
			ods2filep->reclen = reclen;
			up(&(ods2vari->sem));
			filp->f_pos = coffs;
#ifndef TWOSIX
// check this?
			filp->f_reada = 0;
#endif
			filp->f_version++;
			return offs;
		}
		coffs += (reclen - (fatp->u0.s0.fat$v_rtype == FAT$C_VFC ? fatp->fat$b_vfcsize : 0));
		if (fatp->fat$b_rattrib & FAT$M_FORTRANCC || fatp->fat$b_rattrib & FAT$M_IMPLIEDCC || fatp->fat$b_rattrib & FAT$M_PRINTCC) {
			coffs++; /* need to add one byte for LF */
		}
		currec = ((currec + reclen + 1) & ~1) + 2; /* all records are even aligned */
	}
}

  
loff_t ods2_llseek(struct file *filp, loff_t loff, int seek) {
	struct inode		   *inode = filp->f_inode;
	struct super_block	   *sb = inode->i_sb;
	ODS2SB			   *ods2p = ODS2_SB(sb);
#if LINUX_VERSION_CODE < 0x2061A
	ODS2FH			   *ods2fhp = (ODS2FH *)inode->u.generic_ip;
#else
	ODS2FH			   *ods2fhp = (ODS2FH *)inode->i_private;
#endif
	ODS2FILE		   *ods2filep = (ODS2FILE *)filp->private_data;
	FATDEF			   *fatp = (FATDEF *)&(ods2fhp->fat);

	if (ods2p->flags.v_raw || ods2filep->u1.s1.v_raw) {
		return ods2_llseek_stream(filp, loff, seek);
	} else {
		switch (fatp->u0.s0.fat$v_fileorg) {
		case FAT$C_SEQUANTIAL: {
			switch (fatp->u0.s0.fat$v_rtype) {
			case FAT$C_VFC:
			case FAT$C_VARIABLE: return ods2_llseek_variable(filp, loff, seek);
			case FAT$C_FIXED:
			case FAT$C_STREAMLF:
			case FAT$C_STREAMCR:
			case FAT$C_STREAM: return ods2_llseek_stream(filp, loff, seek);
			default: return loff;
			}
		}
		default: return loff;
		}
	}
}


int ods2_open_release(struct inode *inode, struct file *filp) {
	printk("priv %x \n",filp->private_data);
	if (filp->private_data==0x246) {
		unsigned long * c=(void *) filp;
		int i;
		for(i=0;i<32;i++) printk("%8x ",c[i]);
		filp->private_data = NULL;
		return 0;
	}
	if (filp->private_data == NULL) {
		if ((filp->private_data = kmalloc(sizeof(ODS2FILE), GFP_KERNEL)) != NULL) {
			memset(filp->private_data, 0, sizeof(ODS2FILE));
		} else {
			printk("ODS2-fs kmalloc failed for open_release\n");
			return 0;
		}
	} else {
		ODS2FILE		   *ods2filep = (ODS2FILE *)filp->private_data;
		
		if (ods2filep != NULL) {
#if 0
// not yet?
			brelse(ods2filep->bhp);
			kfree(filp->private_data);
#endif
		}
		filp->private_data = NULL;
	}
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
