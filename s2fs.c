/*
 * Demonstrate a trivial filesystem using libfs.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pagemap.h> 	/* PAGE_CACHE_SIZE */
#include <linux/fs.h>     	/* This is where libfs stuff is declared */
#include <asm/atomic.h>
#include <asm/uaccess.h>	/* copy_to_user */

/*
 * Boilerplate stuff.
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anuj Verma");

#define LFS_MAGIC 0x19920342

#define TMPSIZE 20
#define BUFSIZE  100



/*
 * Anytime we make a file or directory in our filesystem we need to
 * come up with an inode to represent it internally.  This is
 * the function that does that job.  All that's really interesting
 * is the "mode" parameter, which says whether this is a directory
 * or file, and gives the permissions.
 */

static struct inode *s2fs_make_inode(struct super_block *sb, umode_t mode, const struct file_operations* fops)
{
	struct inode* inode;            
        inode = new_inode(sb);
       if (!inode) {
                return NULL;
        }
        inode->i_mode = mode;
        inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
        inode->i_fop = fops;
        inode->i_ino = get_next_ino();
	return inode;

}

/*
 * Open a file. 
 */
static int s2fs_open(struct inode *inode, struct file *filp)
{
	return 0;
}

/*
 * Read a file.  Here, we return "Hello World!"
 * string to the user. 
 */
static ssize_t s2fs_read_file(struct file *filp, char *ubuf,
		size_t count, loff_t *offset)
{
	char buf[TMPSIZE];
	int len=0;
	char *temp = "Hello World!\n";
	struct inode *inode = file_inode(filp);
	if(*offset > 0 || count < TMPSIZE)
		return 0;
	
	len = sprintf(buf,"%s",temp);
	printk(KERN_DEBUG "s2fs : read file handler\n");
	if(copy_to_user(ubuf,buf,len))
		return -EFAULT;
	printk(KERN_DEBUG "s2fs : copied to user space\n");
	*offset = len;
	/* inode->i_size = len; */
	printk(KERN_DEBUG "s2fs : File is %s\n", filp->f_path.dentry->d_name.name);
	
	i_size_write(inode, len);
	
	return len;
}

/*
 * Write a file.
 */
static ssize_t s2fs_write_file(struct file *filp, const char *buf,
		size_t count, loff_t *offset)
{
	return 0;
}

static struct file_operations s2fs_fops = {
	.open	= s2fs_open,
	.read 	= s2fs_read_file,
	.write  = s2fs_write_file,
};

const struct inode_operations s2fs_inode_operations = {
        .setattr        = simple_setattr,
        .getattr        = simple_getattr,
};

static struct dentry *s2fs_create_file (struct super_block *sb,
		struct dentry *dir, const char *name)
{
	struct dentry *dentry;
	struct inode *inode;
	umode_t mode = S_IFREG | 0644;
/*
 * Now we can create our dentry and the inode to go with it.
 */
	dentry = d_alloc_name(dir, name);
	if (! dentry)
		goto out;
	
	inode = s2fs_make_inode(sb, mode, &s2fs_fops);
	if (! inode)
		goto out_dput;

/*
 * Put it all into the dentry cache and we're done.
 */
	d_add(dentry, inode);
	return dentry;
/*
 * Then again, maybe it didn't work.
 */
  out_dput:
	dput(dentry);
  out:
	return 0;
}

static struct dentry *s2fs_create_dir (struct super_block *sb,
		struct dentry *parent, const char *dir_name)
{
	struct dentry *dentry;
	struct inode *inode;
	umode_t mode = S_IFDIR | 0755;
	
	dentry = d_alloc_name(parent, dir_name);
	if (! dentry)
		goto out;
	
	inode = s2fs_make_inode(sb, mode , &simple_dir_operations);
	if (! inode)
		goto out_dput;
	inode->i_op = &simple_dir_inode_operations;
	printk(KERN_DEBUG "s2fs : s2fs_create_dir\n");
	d_add(dentry, inode);
	return dentry;

  out_dput:
	dput(dentry);
  out:
	return 0;
}

static void s2fs_create_files (struct super_block *sb, struct dentry *root)
{
	struct dentry *subdir;
	struct dentry *file_created;
	subdir = s2fs_create_dir(sb, root, "foo");
	if (subdir){
		printk(KERN_DEBUG "s2fs : s2fs foo directory created\n");
		file_created = s2fs_create_file(sb, subdir, "bar");
		if (file_created){
			printk(KERN_DEBUG "s2fs : s2fs bar file created\n");
		}
	}
}


/*
 * Superblock stuff.  This is all boilerplate to give the vfs something
 * that looks like a filesystem to work with.
 */

/*
 * Our superblock operations, both of which are generic kernel ops
 * that we don't have to write ourselves.
 */
static struct super_operations lfs_s_ops = {
	.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
};

/*
 * "Fill" a superblock with mundane stuff.
 */
static int s2fs_fill_super (struct super_block *sb, void *data, int silent)
{
	struct inode *root;
	struct dentry *root_dentry;
	umode_t mode = S_IFDIR | 0755;
/*
 * Basic parameters.
 */
	sb->s_blocksize = VMACACHE_SIZE;
	sb->s_blocksize_bits = VMACACHE_SIZE;
	sb->s_magic = LFS_MAGIC;
	sb->s_op = &lfs_s_ops;
/*
 * We need to conjure up an inode to represent the root directory
 * of this filesystem.  Its operations all come from libfs, so we
 * don't have to mess with actually *doing* things inside this
 * directory.
 */
	
	root = s2fs_make_inode(sb, mode, &simple_dir_operations);
	inode_init_owner(root, NULL, mode);
	if (!root)
		goto out;
	root->i_op = &simple_dir_inode_operations;
	/*root->i_fop = &simple_dir_operations;*/
/*
 * Get a dentry to represent the directory in core.
 */
	set_nlink(root, 2);
	root_dentry = d_make_root(root);
	if (! root_dentry)
		goto out_iput;
/*
 * Make up the files which will be in this filesystem, and we're done.
 */
	s2fs_create_files(sb, root_dentry);
	sb->s_root = root_dentry;
	return 0;
	
  out_iput:
	iput(root);
  out:
	return -ENOMEM;
}


/*
 * Stuff to pass in when registering the filesystem.
 */
static struct dentry *s2fs_mount(struct file_system_type *fst,
		int flags, const char *devname, void *data)
{
	return mount_nodev(fst, flags, data, s2fs_fill_super);
}

static struct file_system_type lfs_type = {
	.owner 		= THIS_MODULE,
	.name		= "s2fs",
	.mount		= s2fs_mount,
	.kill_sb	= kill_litter_super,
};

MODULE_ALIAS_FS("s2fs");


/*
 * Get things set up.
 */
static int __init lfs_init(void)
{
	return register_filesystem(&lfs_type);
}

static void __exit lfs_exit(void)
{
	unregister_filesystem(&lfs_type);
}

module_init(lfs_init);
module_exit(lfs_exit);