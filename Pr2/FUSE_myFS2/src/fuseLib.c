// David Cantador Piedras
// David Davó Laviña
#include "fuseLib.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <linux/kdev_t.h>

/**
 * @brief Modifies the data size originally reserved by an inode, reserving or removing space if needed.
 *
 * @param idxNode inode number
 * @param newSize new size for the inode
 * @return int
 **/
int resizeNode(uint64_t idxNode, size_t newSize)
{
    NodeStruct *node = myFileSystem.nodes[idxNode];
    char block[BLOCK_SIZE_BYTES];
    int i, diff = newSize - node->fileSize;

    if(!diff)
        return 0;

    memset(block, 0, sizeof(char)*BLOCK_SIZE_BYTES);

    /// File size increases
    if(diff > 0) {

        /// Delete the extra conent of the last block if it exists and is not full
        if(node->numBlocks && node->fileSize % BLOCK_SIZE_BYTES) {
            int currentBlock = node->blocks[node->numBlocks - 1];

            if( readBlock(&myFileSystem, currentBlock, &block)==-1 ) {
                fprintf(stderr,"Error reading block in resizeNode\n");
                return -EIO;
            }

            int offBlock = node->fileSize % BLOCK_SIZE_BYTES;
            int bytes2Write = (diff > (BLOCK_SIZE_BYTES - offBlock)) ? BLOCK_SIZE_BYTES - offBlock : diff;
            for(i = 0; i < bytes2Write; i++) {
                block[offBlock++] = 0;
            }

            if( writeBlock(&myFileSystem, currentBlock, &block)==-1 ) {
                fprintf(stderr,"Error writing block in resizeNode\n");
                return -EIO;
            }
        }

        /// File size in blocks after the increment
        int newBlocks = (newSize + BLOCK_SIZE_BYTES - 1) / BLOCK_SIZE_BYTES - node->numBlocks;
        if(newBlocks) {
            memset(block, 0, sizeof(char)*BLOCK_SIZE_BYTES);

            // We check that there is enough space
            if(newBlocks > myFileSystem.superBlock.numOfFreeBlocks)
                return -ENOSPC;

            myFileSystem.superBlock.numOfFreeBlocks -= newBlocks;
            int currentBlock = node->numBlocks;
            node->numBlocks += newBlocks;

            for(i = 0; currentBlock != node->numBlocks; i++) {
                if(myFileSystem.bitMap[i] == 0) {
                    myFileSystem.bitMap[i] = 1;
                    node->blocks[currentBlock] = i;
                    currentBlock++;
                    // Clean disk (necessary for truncate)
                    if( writeBlock(&myFileSystem, i, &block)==-1 ) {
                        fprintf(stderr,"Error writing block in resizeNode\n");
                        return -EIO;
                    }
                }
            }
        }
        node->fileSize += diff;

    }
    /// File decreases
    else {
        // File size in blocks after truncation
        int numBlocks = (newSize + BLOCK_SIZE_BYTES - 1) / BLOCK_SIZE_BYTES;
        myFileSystem.superBlock.numOfFreeBlocks += (node->numBlocks - numBlocks);

        for(i = node->numBlocks; i > numBlocks; i--) {
            int nBloque = node->blocks[i - 1];
            myFileSystem.bitMap[nBloque] = 0;
            // Clean disk (it is not really necessary)
            if( writeBlock(&myFileSystem, nBloque, &block)==-1 ) {
                fprintf(stderr,"Error writing block in resizeNode\n");
                return -EIO;
            }
        }
        node->numBlocks = numBlocks;
        node->fileSize += diff;
    }
    node->modificationTime = time(NULL);

    sync();

    /// Update all the information in the backup file
    updateSuperBlock(&myFileSystem);
    updateBitmap(&myFileSystem);
    updateNode(&myFileSystem, idxNode, node);

    return 0;
}

/**
 * @brief It formats the access mode of a file so it is compatible on screen, when it is printed
 *
 * @param mode mode
 * @param str output with the access mode in string format
 * @return void
 **/
void mode_string(mode_t mode, char *str)
{
    str[0] = mode & S_IRUSR ? 'r' : '-';
    str[1] = mode & S_IWUSR ? 'w' : '-';
    str[2] = mode & S_IXUSR ? 'x' : '-';
    str[3] = mode & S_IRGRP ? 'r' : '-';
    str[4] = mode & S_IWGRP ? 'w' : '-';
    str[5] = mode & S_IXGRP ? 'x' : '-';
    str[6] = mode & S_IROTH ? 'r' : '-';
    str[7] = mode & S_IWOTH ? 'w' : '-';
    str[8] = mode & S_IXOTH ? 'x' : '-';
    str[9] = '\0';
}

/**
 * @brief Obtains the file attributes of a file just with the filename
 *
 * Help from FUSE:
 *
 * The 'st_dev' and 'st_blksize' fields are ignored. The 'st_ino' field is
 * ignored except if the 'use_ino' mount option is given.
 *
 *		struct stat {
 *			dev_t     st_dev;     // ID of device containing file
 *			ino_t     st_ino;     // inode number
 *			mode_t    st_mode;    // protection
 *			nlink_t   st_nlink;   // number of hard links
 *			uid_t     st_uid;     // user ID of owner
 *			gid_t     st_gid;     // group ID of owner
 *			dev_t     st_rdev;    // device ID (if special file)
 *			off_t     st_size;    // total size, in bytes
 *			blksize_t st_blksize; // blocksize for file system I/O
 *			blkcnt_t  st_blocks;  // number of 512B blocks allocated
 *			time_t    st_atime;   // time of last access
 *			time_t    st_mtime;   // time of last modification (file's content were changed)
 *			time_t    st_ctime;   // time of last status change (the file's inode was last changed)
 *		};
 *
 * @param path full file path
 * @param stbuf file attributes
 * @return 0 on success and <0 on error
 **/
static int my_getattr(const char *path, struct stat *stbuf)
{
    NodeStruct *node;
    int idxDir;

    fprintf(stderr, "--->>>my_getattr: path %s\n", path);

    memset(stbuf, 0, sizeof(struct stat));

    /// Directory attributes
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_mtime = stbuf->st_ctime = myFileSystem.superBlock.creationTime;
        return 0;
    }

    /// Rest of the world
    if((idxDir = findFileByName(&myFileSystem, (char *)path + 1)) != -1) {
        node = myFileSystem.nodes[myFileSystem.directory.files[idxDir].nodeIdx];
        stbuf->st_size = node->fileSize;
        switch (node->fileType) {
            case NODE_FILE:
                stbuf->st_mode = S_IFREG | 0644;
                break;
            case NODE_LINK:
                stbuf->st_mode = S_IFLNK | 0644;
                break;
            case NODE_DIR:
                stbuf->st_mode = S_IFDIR | 0755;
                break;
            default:
                return -ENOENT;
        }
        stbuf->st_nlink = node->nlinks;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_mtime = stbuf->st_ctime = node->modificationTime;
        return 0;
    }

    return -ENOENT;
}

/**
 * @brief Reads the content of the root directory
 *
 * Help from FUSE:
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and passes zero to the filler function's offset.
 *    The filler function will not return '1' (unless an error happens), so the
 *    whole directory is read in a single readdir operation.
 *
 * 2) The readdir implementation keeps track of the offsets of the directory entries.
 *    It uses the offset parameter and always passes non-zero offset to the filler function. When the buffer is full
 *    (or an error happens) the filler function will return '1'.
 *
 * Function to add an entry in a readdir() operation:
 *	typedef int(* fuse_fill_dir_t)(void *buf, const char *name, const struct stat *stbuf, off_t off)
 *
 *	*Parameters
 *		-buf: the buffer passed to the readdir() operation
 *		-name: the file name of the directory entry
 *		-stat: file attributes, can be NULL
 *		-off: offset of the next entry or zero
 *	*Returns 1 if buffer is full, zero otherwise
 *
 * @param path path to the root folder
 * @param buf buffer where all the forder entries will be stored
 * @param filler FUSE funtion used to fill the buffer
 * @param offset offset used by filler to fill the buffer
 * @param fi FUSE structure associated to the directory
 * @return 0 on success and <0 on error
 **/
static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler,  off_t offset, struct fuse_file_info *fi)
{
    int i, idxDir;
    char root = strcmp(path, "/");

    fprintf(stderr, "--->>>my_readdir: path %s, offset %jd\n", path, (intmax_t)offset);

    if ((idxDir = findFileByName(&myFileSystem, path + 1)) == -1 && root != 0) {
        return -ENOENT;
    }
    
    if (root != 0 && myFileSystem.nodes[myFileSystem.directory.files[idxDir].nodeIdx]->fileType != NODE_DIR)
        return -ENOTDIR;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if (root == 0) { 
        for(i = 0; i < MAX_FILES_PER_DIRECTORY; i++) {
            if(!(myFileSystem.directory.files[i].freeFile)) {
                if(filler(buf, myFileSystem.directory.files[i].fileName, NULL, 0) == 1)
                    return -ENOMEM;
            }
        } 
    }

    return 0;
}

/**
 * @brief File opening
 *
 * Help from FUSE:
 *
 * No creation (O_CREAT, O_EXCL) and by default also no truncation (O_TRUNC) flags will be passed to open().
 * If an application specifies O_TRUNC, fuse first calls truncate() and then open().
 *
 * Unless the 'default_permissions' mount option is given (limits access to the mounting user and root),
 * open should check if the operation is permitted for the given flags.
 *
 * Optionally open may also return an arbitrary filehandle in the fuse_file_info
 * structure, which will be passed to all file operations.
 *
 *	struct fuse_file_info{
 *		int				flags				Open flags. Available in open() and release()
 *		unsigned long 	fh_old				Old file handle, don't use
 *		int 			writepage			In case of a write operation indicates if this was caused by a writepage
 *		unsigned int 	direct_io: 1		Can be filled in by open, to use direct I/O on this file
 *		unsigned int 	keep_cache: 1		Can be filled in by open, to indicate,
 *		                                    that cached file data need not be invalidated.
 *		unsigned int 	flush: 1			Indicates a flush operation.
 *		unsigned int 	nonseekable: 1		Can be filled in by open, to indicate that the file is not seekable.
 *		unsigned int 	padding: 27			Padding. Do not use
 *		uint64_t 		fh					File handle. May be filled in by filesystem
 *		                                    in open(). Available in all other file operations
 *		uint64_t 		lock_owner			Lock owner id.
 *		uint32_t 		poll_events			Requested poll events.
 *	}
 *
 * @param path path to the root folder
 * @param fi FUSE structure associated to the file opened
 * @return 0 on success and <0 on error
 **/
static int my_open(const char *path, struct fuse_file_info *fi)
{
    int idxDir;

    fprintf(stderr, "--->>>my_open: path %s, flags %d, %"PRIu64"\n", path, fi->flags, fi->fh);

    //if(findFileByName(path, &idxNodoI)){
    if((idxDir = findFileByName(&myFileSystem, (char *)path + 1)) == -1) {
        return -ENOENT;
    }

    if (myFileSystem.nodes[myFileSystem.directory.files[idxDir].nodeIdx]->fileType == NODE_DIR)
        return -EISDIR;

    // Save the inode number in file handler to be used in the following calls
    fi->fh = myFileSystem.directory.files[idxDir].nodeIdx;

    return 0;
}

/**
 * @brief Read data from an open file
 *
 * Help from FUSE:
 *
 * The requested action is to read up to size bytes of the file or directory,
 * starting at offset. The bytes should be returned directly following the usual
 * reply header
 *
 * We suppose buf is not a null pointer
 *
 * @param path path to the file
 * @param buf buffer in which to return contents
 * @param bytes bytes to read
 * @param offset offset of file
 * @param fuse_file_info FUSE structure associated to the file opened
 * @return bytes read, distinct of bytes requested on error
 **/
static int my_read(const char* path, char *buf, size_t bytes, off_t offset,
        struct fuse_file_info *ffi) {
    char buffer[BLOCK_SIZE_BYTES];
    size_t bytes2Read = bytes, totalRead = 0;

    fprintf(stderr, "--->>my_read: path %s, bytes: %zu, offset: %jd, fh %"PRIu64"\n",
            path, bytes, offset, ffi->fh);

    if (myFileSystem.nodes[ffi->fh]->fileType == NODE_DIR)
        return -EISDIR;

    while (bytes2Read) {
        int i, b, ob;
        b = myFileSystem.nodes[ffi->fh]->blocks[offset/BLOCK_SIZE_BYTES];
        ob = offset % BLOCK_SIZE_BYTES;

        if (readBlock(&myFileSystem, b, &buffer) == -1) {
            fprintf(stderr, "Error reading block in my_read\n");
            return -EIO;
        }

        for (i = ob; i < BLOCK_SIZE_BYTES && totalRead < bytes; ++i) {
            buf[totalRead++] = buffer[i];
        }

        bytes2Read -= (i - ob);
        offset += (i -ob);
    }

    sync();

    return totalRead;
}


/**
 * @brief Write data on an opened file
 *
 * Help from FUSE
 *
 * Write should return exactly the number of bytes requested except on error.
 *
 * @param path file path
 * @param buf buffer where we have data to write
 * @param size quantity of bytes to write
 * @param offset offset over the writing
 * @param fi FUSE structure linked to the opened file
 * @return 0 on success and <0 on error
 **/
static int my_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    char buffer[BLOCK_SIZE_BYTES];
    int bytes2Write = size, totalWrite = 0;
    NodeStruct *node = myFileSystem.nodes[fi->fh];

    fprintf(stderr, "--->>>my_write: path %s, size %zu, offset %jd, fh %"PRIu64"\n",
            path, size, (intmax_t)offset, fi->fh);

    if (myFileSystem.nodes[fi->fh]->fileType == NODE_DIR)
        return -EISDIR;

    // Increase the file size if it is needed
    if(resizeNode(fi->fh, size + offset) < 0)
        return -EIO;

    // Write data
    while(bytes2Write) {
        int i;
        int currentBlock, offBlock;
        currentBlock = node->blocks[offset / BLOCK_SIZE_BYTES];
        offBlock = offset % BLOCK_SIZE_BYTES;

        if( readBlock(&myFileSystem, currentBlock, &buffer)==-1 ) {
            fprintf(stderr,"Error reading blocks in my_write\n");
            return -EIO;
        }

        for(i = offBlock; (i < BLOCK_SIZE_BYTES) && (totalWrite < size); i++) {
            buffer[i] = buf[totalWrite++];
        }

        if( writeBlock(&myFileSystem, currentBlock, &buffer)==-1 ) {
            fprintf(stderr,"Error writing block in my_write\n");
            return -EIO;
        }

        // Discount the written stuff
        bytes2Write -= (i - offBlock);
        offset += (i - offBlock);
    }
    sync();

    node->modificationTime = time(NULL);
    updateSuperBlock(&myFileSystem);
    updateBitmap(&myFileSystem);
    updateNode(&myFileSystem, fi->fh, node);

    return size;
}

/**
 * @brief Close the file
 *
 * Help from FUSE:
 *
 * Release is called when there are no more references to an open file: all file descriptors are
 * closed and all memory mappings are unmapped.
 *
 * For every open() call there will be exactly one release() call with the same flags and file descriptor.
 * It is possible to have a file opened more than once, in which case only the last release will mean,
 * that no more reads/writes will happen on the file. The return value of release is ignored.
 *
 * @param path file path
 * @param fi FUSE structure linked to the opened file
 * @return 0
 **/
static int my_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    (void) fi;

    fprintf(stderr, "--->>>my_release: path %s\n", path);

    return 0;
}

/**
 * @brief Create a file
 *
 * Help from FUSE:
 *
 * This is called for creation of all non-directory, non-symlink nodes.
 * If the filesystem defines a create() method, then for regular files that will be called instead.
 *
 * @param path file path
 * @param mode creation mode
 * @param device device where the device will be created (contains both major and minor numbers)
 * @return 0 on success and <0 on error
 **/
static int my_mknod(const char *path, mode_t mode, dev_t device)
{
    char modebuf[10];

    mode_string(mode, modebuf);
    fprintf(stderr, "--->>>my_mknod: path %s, mode %s, major %d, minor %d\n",
            path, modebuf, (int)MAJOR(device), (int)MINOR(device));

    // We check that the length of the file name is correct
    if(strlen(path + 1) > myFileSystem.superBlock.maxLenFileName) {
        return -ENAMETOOLONG;
    }

    // There exist an available inode
    if(myFileSystem.numFreeNodes <= 0) {
        return -ENOSPC;
    }

    // There is still space for a new file
    if(myFileSystem.directory.numFiles >= MAX_FILES_PER_DIRECTORY) {
        return -ENOSPC;
    }
    // The directory exists
    if(findFileByName(&myFileSystem, (char *)path + 1) != -1)
        return -EEXIST;

    /// Update all the information in the backup file:
    int idxNodoI, idxDir;
    if((idxNodoI = findFreeNode(&myFileSystem)) == -1 || (idxDir = findFreeFile(&myFileSystem)) == -1) {
        return -ENOSPC;
    }

    // Update root folder
    myFileSystem.directory.files[idxDir].freeFile = false;
    myFileSystem.directory.numFiles++;
    strcpy(myFileSystem.directory.files[idxDir].fileName, path + 1);
    myFileSystem.directory.files[idxDir].nodeIdx = idxNodoI;
    myFileSystem.numFreeNodes--;

    // Fill the fields of the new inode
    if(myFileSystem.nodes[idxNodoI] == NULL)
        myFileSystem.nodes[idxNodoI] = malloc(sizeof(NodeStruct));

    myFileSystem.nodes[idxNodoI]->fileSize = 0;
    myFileSystem.nodes[idxNodoI]->numBlocks = 0;
    myFileSystem.nodes[idxNodoI]->fileType = NODE_FILE;
    myFileSystem.nodes[idxNodoI]->modificationTime = time(NULL);
    myFileSystem.nodes[idxNodoI]->freeNode = false;
    myFileSystem.nodes[idxNodoI]->nlinks = 1;

    reserveBlocksForNodes(&myFileSystem, myFileSystem.nodes[idxNodoI]->blocks, 0);

    updateDirectory(&myFileSystem);
    updateNode(&myFileSystem, idxNodoI, myFileSystem.nodes[idxNodoI]);

    return 0;
}

/**
 * @brief Change the size of a file
 *
 * @param path file path
 * @param size new size
 * @return 0 on success and <0 on error
 **/
static int my_truncate(const char *path, off_t size)
{
    int idxDir;

    fprintf(stderr, "--->>>my_truncate: path %s, size %jd\n", path, size);

    if((idxDir = findFileByName(&myFileSystem, (char *)path + 1)) == -1) {
        return -ENOENT;
    }

    if(myFileSystem.nodes[myFileSystem.directory.files[idxDir].nodeIdx]->fileType == NODE_DIR)
        return -EISDIR;

    // Modify the size
    if(resizeNode(myFileSystem.directory.files[idxDir].nodeIdx, size) < 0)
        return -EIO;

    return 0;
}

static int my_unlink(const char *path)
{
    int idxDir;
    NodeStruct * node;
    int i, b;

    fprintf(stderr, "--->>>my_unlink: path %s\n", path);

    if ((idxDir = findFileByName(&myFileSystem, (char *)path + 1)) == -1) {
        return -EEXIST;
    }

    if (myFileSystem.nodes[myFileSystem.directory.files[idxDir].nodeIdx]->fileType == NODE_DIR)
        return -EISDIR;

    node = myFileSystem.nodes[myFileSystem.directory.files[idxDir].nodeIdx];

    // Free path (directory)
    myFileSystem.directory.files[idxDir].freeFile = 1;
    bzero(myFileSystem.directory.files[idxDir].fileName, MAX_LEN_FILE_NAME + 1);
    myFileSystem.directory.numFiles--;
    updateDirectory(&myFileSystem);

    // Free node
    node->nlinks--;
    if (node->nlinks <= 0) {
        // Free used blocks
        for (i = 0; i < node->numBlocks; ++i) {
            b = node->blocks[i];
            myFileSystem.bitMap[b] = 0;
            // We could hide info with 0's, 1's or whatever
            // But we will keep the file as if it were a real fs
        }
        updateBitmap(&myFileSystem);

        myFileSystem.nodes[idxDir]->freeNode = 1;
        updateNode(&myFileSystem, idxDir, myFileSystem.nodes[idxDir]);
        free(myFileSystem.nodes[idxDir]);
        myFileSystem.nodes[idxDir] = NULL;
        myFileSystem.numFreeNodes++;

        myFileSystem.superBlock.numOfFreeBlocks += i;

        updateSuperBlock(&myFileSystem);
    }

    return 0;
}

// See man 2 symlink for errors
static int my_symlink(const char* to, const char* filename) {
    int idxNodoI, idxDir;
    NodeStruct * node;

    fprintf(stderr, "--->>my_simlink: from: %s, to %s\n", filename, to);

    if (myFileSystem.numFreeNodes <= 0) {
        return -EDQUOT;
    }

    if (findFileByName(&myFileSystem, to+1) != -1) {
        return -EEXIST;
    }

    if (strlen(to)+1 > myFileSystem.superBlock.maxLenFileName) {
        return -ENAMETOOLONG;
    }

    if (myFileSystem.directory.numFiles >= MAX_FILES_PER_DIRECTORY) {
        return -ENOSPC;
    }

    if ( (idxNodoI = findFreeNode(&myFileSystem)) == -1) {
        // This shouldn't happen because we checked there are nodes available
        // we have an inconsistent filesystem :(
        return -EIO;
    }

    // Update nodes
    node = myFileSystem.nodes[idxNodoI];
    if (node == NULL) node = myFileSystem.nodes[idxNodoI] = malloc(sizeof(NodeStruct));
    node->numBlocks = 1;
    node->fileSize = strlen(to);
    node->modificationTime = time(NULL);
    node->freeNode = 0;
    node->fileType = NODE_LINK;
    myFileSystem.numFreeNodes--;

    if (reserveBlocksForNodes(&myFileSystem, node->blocks, 1) == -1) return -EIO;
    if (writeBlock(&myFileSystem, node->blocks[0], to) == -1) return -EIO;
    updateBitmap(&myFileSystem);

    // Update directory
    if ( (idxDir = findFreeFile(&myFileSystem)) == -1) {
        return -EIO;
    }
    myFileSystem.directory.files[idxDir].nodeIdx = idxNodoI;
    strcpy(myFileSystem.directory.files[idxDir].fileName, filename+1);
    myFileSystem.directory.files[idxDir].freeFile = 0;
    myFileSystem.directory.numFiles++;

    updateNode(&myFileSystem, idxNodoI, node);
    updateDirectory(&myFileSystem);

    sync();

    return 0;
}

static int my_readlink(const char *path, char*buf, size_t bytes) {
    int idxDir, idxNodeI;
    char block[BLOCK_SIZE_BYTES];
    NodeStruct * node;

    fprintf(stderr, "--->>>my_readlink: path: %s, size: %ld\n", path, bytes); 

    if ((idxDir = findFileByName(&myFileSystem, path+1)) == -1) {
        return -ENOENT;
    }

    idxNodeI = myFileSystem.directory.files[idxDir].nodeIdx;
    node = myFileSystem.nodes[idxNodeI];

    if (node->fileType != NODE_LINK)
        return -EINVAL;

    if (readBlock(&myFileSystem, node->blocks[0], &block) == -1)
        return -EIO;

    strncpy(buf, block, bytes);

    return 0;
}

// Crea un enlace lpath a un fichero path
static int my_link(const char * path, const char * lpath) {
    int idxDir;
    int nodeIdx;

    fprintf(stderr, "--->>>my_link: path %s, lpath: %s\n", path, lpath);

    if (findFileByName(&myFileSystem, lpath+1) != -1) {
        return -EEXIST;
    }

    if (strlen(lpath) > myFileSystem.superBlock.maxLenFileName) {
        return -ENAMETOOLONG;
    }

    if (myFileSystem.directory.numFiles >= MAX_FILES_PER_DIRECTORY) {
        return -ENOSPC;
    }

    if ((idxDir = findFileByName(&myFileSystem, path+1)) == -1) {
        return -ENOENT;
    }

    nodeIdx = myFileSystem.directory.files[idxDir].nodeIdx;

    if (myFileSystem.nodes[nodeIdx]->fileType == NODE_DIR)
        return -EISDIR;

    idxDir = findFreeFile(&myFileSystem);
    myFileSystem.directory.files[idxDir].freeFile = 0;
    myFileSystem.directory.files[idxDir].nodeIdx = nodeIdx;
    myFileSystem.nodes[nodeIdx]->nlinks++;
    strcpy(myFileSystem.directory.files[idxDir].fileName, lpath+1);

    return 0;
}

// We will ignore the mode, as our filesystem doesn't support "real" directories
static int my_mkdir(const char *pathname, mode_t mode) {
    int idxDir, idxNodoI;

    fprintf(stderr, "--->>>my_mkdir: path %s, mode: %o\n", pathname, mode);

    if (strlen(pathname+1) > myFileSystem.superBlock.maxLenFileName) {
        return -ENAMETOOLONG;
    }

    if (myFileSystem.numFreeNodes <= 0) {
        return -ENOSPC;
    }

    if (myFileSystem.directory.numFiles >= MAX_FILES_PER_DIRECTORY) {
        return -ENOSPC;
    }

    if (findFileByName(&myFileSystem, pathname+1) != -1) {
        return -EEXIST;
    }

    if ((idxNodoI = findFreeNode(&myFileSystem)) == -1 || (idxDir = findFreeFile(&myFileSystem)) == -1) {
        return -ENOSPC;
    }

    // Update root folder
    myFileSystem.directory.files[idxDir].freeFile = false;
    myFileSystem.directory.numFiles++;
    strcpy(myFileSystem.directory.files[idxDir].fileName, pathname+1);
    myFileSystem.directory.files[idxDir].nodeIdx = idxNodoI;

    // Fill new inode
    if(myFileSystem.nodes[idxNodoI] == NULL)
        myFileSystem.nodes[idxNodoI] = malloc(sizeof(NodeStruct));

    myFileSystem.nodes[idxNodoI]->fileSize = 0;
    myFileSystem.nodes[idxNodoI]->numBlocks = 0;
    myFileSystem.nodes[idxNodoI]->fileType = NODE_DIR;
    myFileSystem.nodes[idxNodoI]->modificationTime = time(NULL);
    myFileSystem.nodes[idxNodoI]->freeNode = false;
    myFileSystem.nodes[idxNodoI]->nlinks = 2;

    updateDirectory(&myFileSystem);
    updateNode(&myFileSystem, idxNodoI, myFileSystem.nodes[idxNodoI]);

    return 0;
}

// https://libfuse.github.io/doxygen/structfuse__operations.html
struct fuse_operations myFS_operations = {
    .getattr	= my_getattr,					// Obtain attributes from a file
    .readdir	= my_readdir,					// Read directory entries
    .truncate	= my_truncate,					// Modify the size of a file
    .open		= my_open,						// Oeen a file
    .write		= my_write,						// Write data into a file already opened
    .release	= my_release,					// Close an opened file
    .mknod		= my_mknod,						// Create a new file
    .unlink     = my_unlink,                    // Unlinks a file
    .read       = my_read,                      // Reads the contents of a file
    .symlink    = my_symlink,                   // Symlinks two files
    .readlink   = my_readlink,                  // Reads symlink
    .link       = my_link,                      // Hardlinks a dir to inode
    .mkdir      = my_mkdir                      // Creates an empty directory
};

