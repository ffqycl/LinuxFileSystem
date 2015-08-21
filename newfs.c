#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#define NEWFS_MAGIC     0x06376786 


struct inode *newfs_get_inode(struct super_block *sb, int mode, dev_t dev);

// simple address space ops
struct address_space_operations newfs_aops = {
	.readpage	= simple_readpage,
	.write_begin	= simple_write_begin,
	.write_end	= simple_write_end,
};

// all default & simple file ops
struct file_operations newfs_file_operations = {
	.read           = do_sync_read,
	.aio_read	= generic_file_aio_read,
	.write          = do_sync_write,
	.aio_write	= generic_file_aio_write,
	.mmap           = generic_file_mmap,
	.fsync          = simple_sync_file,
	.llseek         = generic_file_llseek,
	.splice_read	= generic_file_splice_read,
};

// get an inode and link with dentry
static int newfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	struct inode * inode = newfs_get_inode(dir->i_sb, mode, dev);
	int error = -ENOSPC;

	if (inode) {
		if (dir->i_mode & S_ISGID) {
			inode->i_gid = dir->i_gid;
			if (S_ISDIR(mode))
				inode->i_mode |= S_ISGID;
		}
		d_instantiate(dentry, inode);
		dget(dentry);
		error = 0;
		dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	}
	return error;
}

// make a directory
static int newfs_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
	
	int retval = newfs_mknod(dir, dentry, mode | S_IFDIR, 0);
	if (!retval)
		inc_nlink(dir);
	return retval;
}

// start to creat a root
static int newfs_create(struct inode *dir, struct dentry *dentry, int mode,struct nameidata *nd)
{
	return newfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

struct inode_operations newfs_dir_inode_operations = {
	.create         = newfs_create,
	.mknod          = newfs_mknod,
	.lookup         = simple_lookup,
	.unlink         = simple_unlink,
	.rename         = simple_rename,
	.mkdir          = newfs_mkdir,
	.rmdir          = simple_rmdir,


};

static const struct super_operations newfs_ops = {
	.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
};

static struct backing_dev_info newfs_backing_dev_info = {
	.ra_pages       = 0, 
	.capabilities   = BDI_CAP_NO_ACCT_DIRTY | BDI_CAP_NO_WRITEBACK |
			  BDI_CAP_MAP_DIRECT | BDI_CAP_MAP_COPY |
			  BDI_CAP_READ_MAP | BDI_CAP_WRITE_MAP |
			  BDI_CAP_EXEC_MAP,
};

struct inode *newfs_get_inode(struct super_block *sb, int mode, dev_t dev)
{
	struct inode * root = new_inode(sb);

	if (root) {
		root->i_mapping->a_ops = &newfs_aops;
		root->i_mode = mode;
		root->i_gid = current->fsgid;
		root->i_blocks = 1024; // block size
		root->i_uid = current->fsuid;
		root->i_atime = root->i_mtime = root->i_ctime = CURRENT_TIME;
		root->i_mapping->backing_dev_info = &newfs_backing_dev_info;
		
		// based on user mode, assign different ops
		switch (mode & S_IFMT) {
		default:
			init_special_inode(root, mode, dev);
			break;
		case S_IFREG:
			root->i_op = &simple_dir_inode_operations;
			root->i_fop = &newfs_file_operations;
			break;
		case S_IFDIR:
			root->i_op = &newfs_dir_inode_operations;
			root->i_fop = &simple_dir_operations;

			inc_nlink(root);
			break;
		}
	}
	return root;
}


static int newfs_fill_super(struct super_block * sb, void * data, int silent)
{
	struct inode * inode;
	struct dentry * root;

	sb->s_magic = NEWFS_MAGIC;
	sb->s_maxbytes = MAX_LFS_FILESIZE;
	sb->s_blocksize = PAGE_CACHE_SIZE; // block size
	sb->s_op = &newfs_ops;
	sb->s_time_gran = 1;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;

	inode = newfs_get_inode(sb, S_IFDIR | 0755, 0);;
	root = d_alloc_root(inode);
	sb->s_root = root;
	return 0;
}

int newfs_get_sb(struct file_system_type *fs_type,
        int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_nodev(fs_type, flags, data, newfs_fill_super, mnt);
}


static struct file_system_type newfs_type = {
	.owner = THIS_MODULE,
	.name = "newfs",
	.get_sb = newfs_get_sb,
	.kill_sb = kill_litter_super,
	.fs_flags = FS_REQUIRES_DEV,
};


static int __init init_newfs_fs(void)
{
	return register_filesystem(&newfs_type);
}

static void __exit exit_newfs_fs(void)
{
	unregister_filesystem(&newfs_type);
}

module_init(init_newfs_fs)
module_exit(exit_newfs_fs)
