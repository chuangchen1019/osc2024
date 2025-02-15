#include "vfs.h"
#include "tmpfs.h"
#include "memory.h"
#include "utils.h"
#include "uart1.h"
#include "initramfs.h"
#include "dev_uart.h"
#include "dev_framebuffer.h"

struct mount *rootfs;
struct filesystem reg_fs[MAX_FS_REG];
struct file_operations reg_dev[MAX_DEV_REG];

// Register the file system to the kernel. Initialize memory pool of the file system.
int register_filesystem(struct filesystem *fs)
{
    for (int i = 0; i < MAX_FS_REG; i++)
    {
        if (!reg_fs[i].name)
        {
            reg_fs[i].name = fs->name;
            reg_fs[i].setup_mount = fs->setup_mount;
            return i;
        }
    }
    return -1;
}

int register_dev(struct file_operations *fo)
{
    for (int i = 0; i < MAX_FS_REG; i++)
    {
        if (!reg_dev[i].open)
        {
            // return unique id for the assigned device
            reg_dev[i] = *fo;
            return i;
        }
    }
    return -1;
}

struct filesystem *find_filesystem(const char *fs_name)
{
    for (int i = 0; i < MAX_FS_REG; i++)
    {
        if (strcmp(reg_fs[i].name, fs_name) == 0)
        {
            return &reg_fs[i];
        }
    }
    return 0;
}

// file ops
int vfs_open(const char *pathname, int flags, struct file **target)
{
    // 1. Lookup pathname
    // 2. Create a new file if O_CREAT is specified in flags and vnode not found
    struct vnode *node;
    // If the file does not exist, create it
    if (vfs_lookup(pathname, &node) != 0 && (flags & O_CREAT))
    {
        // uart_sendline("O_CREAT\r\n");
        // grep all of the directory path
        int last_slash_idx = 0;
        for (int i = 0; i < strlen(pathname); i++)
        {
            if (pathname[i] == '/')
            {
                last_slash_idx = i;
            }
        }

        char dirname[MAX_PATH_NAME + 1];
        strcpy(dirname, pathname);
        dirname[last_slash_idx] = 0;
        // update dirname to node
        // If the upper directory does not exist, the creation fails.
        if (vfs_lookup(dirname, &node) != 0)
        {
            uart_sendline("cannot ocreate no dir name\r\n");
            return -1;
        }
        // create a new file node on node, &node is new file, 3rd arg is filename
        node->v_ops->create(node, &node, pathname + last_slash_idx + 1);
        *target = kmalloc(sizeof(struct file));
        // attach opened file on the new node
        node->f_ops->open(node, target);
        (*target)->flags = flags;
        return 0;
    }
    else // 2. Create a new file handle for this vnode if found.
    {
        // attach opened file on the node
        *target = kmalloc(sizeof(struct file));
        node->f_ops->open(node, target);
        (*target)->flags = flags;
        return 0;
    }

    // lookup error code shows if file exist or not or other error occurs
    // 4. Return error code if fails
    return -1;
}

// file ops
int vfs_close(struct file *file)
{
    // 1. release the file handle
    // 2. Return error code if fails
    return file->f_ops->close(file);
}

// file ops
int vfs_write(struct file *file, const void *buf, size_t len)
{
    // 1. write len byte from buf to the opened file.
    // 2. return written size or error code if an error occurs.
    return file->f_ops->write(file, buf, len);
}

// file ops
int vfs_read(struct file *file, void *buf, size_t len)
{
    // 1. read min(len, readable size) byte to buf from the opened file.
    // 2. block if nothing to read for FIFO type
    // 2. return read size or error code if an error occurs.
    return file->f_ops->read(file, buf, len);
}

// file ops
int vfs_mkdir(const char *pathname)
{
    char dirname[MAX_PATH_NAME] = {};    // before add folder
    char newdirname[MAX_PATH_NAME] = {}; // after  add folder

    // search for last directory
    int last_slash_idx = 0;
    for (int i = 0; i < strlen(pathname); i++)
    {
        if (pathname[i] == '/')
        {
            last_slash_idx = i;
        }
    }

    memcpy(dirname, pathname, last_slash_idx);
    strcpy(newdirname, pathname + last_slash_idx + 1);

    // create new directory if upper directory is found
    struct vnode *node;
    if (vfs_lookup(dirname, &node) == 0)
    {
        // node is the old dir, &node is new dir
        node->v_ops->mkdir(node, &node, newdirname);
        return 0;
    }

    uart_sendline("vfs_mkdir cannot find upper directory");
    return -1;
}

int vfs_mount(const char *target, const char *filesystem)
{
    struct vnode *dirnode;
    // search for the target filesystem
    struct filesystem *fs = find_filesystem(filesystem);
    if (!fs)
    {
        uart_sendline("vfs_mount cannot find filesystem\r\n");
        return -1;
    }

    if (vfs_lookup(target, &dirnode) == -1)
    {
        uart_sendline("vfs_mount cannot find dir\r\n");
        return -2;
    }
    else
    {
        // mount fs on dirnode
        dirnode->mount = kmalloc(sizeof(struct mount));
        fs->setup_mount(fs, dirnode->mount);
    }
    return 0;
}

int vfs_lookup(const char *pathname, struct vnode **target)
{
    // if no path input, return root
    if (strlen(pathname) == 0)
    {
        *target = rootfs->root;
        return 0;
    }

    struct vnode *dirnode = rootfs->root;
    char component_name[MAX_FILE_NAME + 1] = {};
    int c_idx = 0;
    // deal with directory
    for (int i = 1; i < strlen(pathname); i++)
    {
        if (pathname[i] == '/')
        {
            component_name[c_idx] = 0;
            // if fs's v_ops error, return -1
            if (dirnode->v_ops->lookup(dirnode, &dirnode, component_name) != 0)
            {
                // uart_sendline("vfs_lookup cannot find directory %s\r\n", component_name);
                return -1;
            }
            // redirect to mounted filesystem
            while (dirnode->mount)
            {
                dirnode = dirnode->mount->root;
            }
            c_idx = 0;
        }
        else
        {
            component_name[c_idx++] = pathname[i];
        }
    }

    // deal with file
    component_name[c_idx++] = 0;
    // if fs's v_ops error, return -1
    if (dirnode->v_ops->lookup(dirnode, &dirnode, component_name) != 0)
    {
        // uart_sendline("vfs_lookup cannot find file %s\r\n", component_name);
        return -1;
    }
    // redirect to mounted filesystem
    while (dirnode->mount)
    {
        dirnode = dirnode->mount->root;
    }
    // return file's vnode
    *target = dirnode;

    return 0;
}

// for device operations only
int vfs_mknod(char* pathname, int id)
{
    struct file* f = kmalloc(sizeof(struct file));
    // create leaf and its file operations
    vfs_open(pathname, O_CREAT, &f);
    f->vnode->f_ops = &reg_dev[id];
    vfs_close(f);
    return 0;
}

void init_rootfs()
{
    // tmpfs
    int idx = register_tmpfs();
    rootfs = kmalloc(sizeof(struct mount));
    reg_fs[idx].setup_mount(&reg_fs[idx], rootfs);

    // initramfs
    vfs_mkdir("/initramfs");
    register_initramfs();
    vfs_mount("/initramfs","initramfs");
    
    // dev_fs
    vfs_mkdir("/dev");
    int uart_id = init_dev_uart();
    vfs_mknod("/dev/uart", uart_id);

    int framebuffer_id = init_dev_framebuffer();
    vfs_mknod("/dev/framebuffer", framebuffer_id);

    // vfs_test();
}

void vfs_test()
{
    // Lab7- Basic Exercise 1 & 2
    uart_sendline("rootfs name: %s\n", rootfs->fs->name);
    // test read/write
    if (vfs_mkdir("/dir1") == -1)
        uart_sendline("mkdir1 failed\n");
    if (vfs_mkdir("/dir1/dir2") == -1)
        uart_sendline("mkdir2 failed\n");
    // test mount
    if (vfs_mount("/dir1/dir2", "tmpfs") < 0)
        uart_sendline("mount failed\n");

    struct file *testfilew;
    struct file *testfiler;
    char testbufw[0x30] = "0123456789abcdef";
    char testbufr[0x30] = {};
    if (vfs_open("/dir1/dir2/file1.txt\n", O_CREAT, &testfilew) == -1)
        uart_sendline("open failed\n");
    if (vfs_open("/dir1/dir2/file1.txt\n", O_CREAT, &testfiler) == -1)
        uart_sendline("open failed\n");
    if (vfs_write(testfilew, testbufw, 10) != 10)
        uart_sendline("write failed\n");
    if (vfs_read(testfiler, testbufr, 10) != 10)
        uart_sendline("read failed\n");
    uart_sendline("testbufr: %s\n", testbufr);
    if (vfs_close(testfilew) != 0)
        uart_sendline("close testfilew failed\n");
    if (vfs_close(testfiler) != 0)
        uart_sendline("close testfiler failed\n");

    // Lab7- Basic Exercise 4
    struct file *testfile_initramfs;
    vfs_open("/initramfs/file0.txt", O_CREAT, &testfile_initramfs);
    vfs_read(testfile_initramfs, testbufr, 0x20);
    uart_sendline("Read from testfile_initramfs testbufr: %s\n", testbufr);
}

char *get_absolute_path(char *path, char *curr_working_dir)
{
    // if relative path -> add root path
    if (path[0] != '/')
    {
        char tmp[MAX_PATH_NAME];
        strcpy(tmp, curr_working_dir);
        if (strcmp(curr_working_dir, "/") != 0)
            strcat(tmp, "/");
        strcat(tmp, path); // "/curr_working_dir/path"
        strcpy(path, tmp);
    }

    char absolute_path[MAX_PATH_NAME + 1] = {};
    int idx = 0;
    for (int i = 0; i < strlen(path); i++)
    {
        // meet /.. -> delete last level directory
        if (path[i] == '/' && path[i + 1] == '.' && path[i + 2] == '.')
        {
            for (int j = idx; j >= 0; j--)
            {
                if (absolute_path[j] == '/')
                {
                    absolute_path[j] = 0;
                    idx = j;
                }
            }
            i += 2;
            continue;
        }

        // ignore /.
        if (path[i] == '/' && path[i + 1] == '.')
        {
            i++;
            continue;
        }

        absolute_path[idx++] = path[i];
    }
    absolute_path[idx] = 0;

    return strcpy(path, absolute_path);
}
