#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* save useful constants */
#define SECTOR_SIZE 512
#define BOOT_MAGIC 0x80
#define BOOT_MINIX 0x81
#define NUM_PARTITIONS 4
#define PARTITION_TABLE_LOC 0x1BE
#define PARTITION_MAGIC_LOC1 510
#define PARTITION_MAGIC_VAL1 0x55
#define PARTITION_MAGIC_LOC2 511
#define PARTITION_MAGIC_VAL2 0xAA
#define SUPERBLOCK_OFF 1024
#define MINIX_MAGIC 0x4D5A
#define INODE_SIZE 64
#define DIR_ENTRY_SIZE 64
#define DIRECT_ZONES 7
#define FILE_NAME_SIZE 60
#define FILE_TYPE_MASK 0170000
#define IS_REG_FILE 0100000
#define IS_DIR 0040000
#define OWN_RD 0000400
#define OWN_WR 0000200
#define OWN_EX 0000100
#define GRP_RD 0000040
#define GRP_WR 0000020
#define GRP_EX 0000010
#define OTH_RD 0000004
#define OTH_WR 0000002
#define OTH_EX 0000001

/* define necessary structures */
typedef struct partition {
  uint8_t bootind;
  uint8_t start_head;
  uint8_t start_sec;
  uint8_t start_cyl;
  uint8_t type;
  uint8_t end_head;
  uint8_t end_sec;
  uint8_t end_cyl;
  uint32_t lFirst;
  uint32_t size;
} partition;

typedef struct superblock {
  uint32_t ninodes;
  uint16_t pad;
  int16_t iblocks;
  int16_t zblocks;
  uint16_t firstdata;
  int16_t log_zone_size;
  int16_t pad2;
  uint32_t max_file;
  uint32_t zones;
  int16_t magic;
  int16_t pad3;
  uint16_t blocksize;
  uint8_t subversion;
} superblock;

typedef struct inode {
  uint16_t mode;
  uint16_t links;
  uint16_t uid;
  uint16_t gid;
  uint32_t size;
  int32_t atime;
  int32_t mtime;
  int32_t ctime;
  uint32_t zone[DIRECT_ZONES];
  uint32_t indirect;
  uint32_t two_indirect;
  uint32_t unused;
} inode;

typedef struct dir_entry {
  uint32_t inode;
  unsigned char name[FILE_NAME_SIZE];
} dir_entry;

/* Globals */
partition ptable[NUM_PARTITIONS];
superblock super;
unsigned int zone_size = 0;
int verbose = 0;
unsigned int first_addr = 0, last_addr = 0, root_addr = 0;
inode root_inode;
dir_entry root_dir;

void usage() {
  printf("usage: minget [ -v ] [ -p num [ -s num ] ] imagefile srcpath [\
  dstpath ]\n");
  printf("Options:\n");
  printf("-p part --- select partition for filesystem (default: none)\n");
  printf("-s sub --- select subpartition for filesystem (default: none)\n");
  printf("-h help --- print usage information and exit\n");
  printf("-v verbose --- increase verbosity level\n");
  exit(1);
}

int main(int argc, char **argv) {
  int ret;
  char *imagefile = NULL, *srcpath = NULL, *dstpath = NULL;
  uint8_t magic1, magic2;
  int part_num = -1, subpart_num = -1, fd;

  if (argc > 9) {
    usage();
  }
  while(++argv && *argv) {
    if (**argv == '-') {
      switch (*((*argv)+1)) {
        case 'v':
          verbose = 1;
          break;
        case 'p':
          part_num = atoi(*(++argv));
          break;
        case 's':
          subpart_num = atoi(*(++argv));
          break;
        default:
          usage();
      }
    }
    else if (!imagefile) {
      imagefile = *argv;
    }
    else if (!srcpath) {
      srcpath = *argv;
    }
    else if (!dstpath) {
      dstpath = *argv;
    }
    else {
      usage();
    }
  }

  if (!imagefile || !srcpath || (subpart_num != -1 && part_num == -1)) {
    usage();
  }

  /* open the imagefile */
  fd = open(imagefile, O_RDONLY);
  if (fd < 0) {
    perror("Error: could not open image file.\n");
    exit(1);
  }

  /* read partition table */
  if (part_num != -1) { /* user specified a partition */
    if (part_num < 0 || part_num > NUM_PARTITIONS - 1) {
      perror("Error: invalid partition number.\n");
      exit(1);
    }

    /* check for valid partition table */
    ret = lseek(fd, PARTITION_MAGIC_LOC1, SEEK_SET);
    if (ret != PARTITION_MAGIC_LOC1) {
      perror("Error: error seeking.\n");
      exit(1);
    }
    ret = read(fd, &magic1, 1);
    if (ret < 1) {
      perror("Error: error reading.\n");
      exit(1);
    }
    ret = read(fd, &magic2, 1);
    if (ret < 1) {
      perror("Error: error reading.\n");
      exit(1);
    }
    if (magic1 != PARTITION_MAGIC_VAL1 || magic2 != PARTITION_MAGIC_VAL2) {
      fprintf(stderr, "magic1: %x, magic2: %x\n", magic1, magic2);
      perror("Error: invalid partition table.\n");
      exit(1);
    }

    partition part;
    ret = lseek(fd, PARTITION_TABLE_LOC, SEEK_SET);
    if (ret != PARTITION_TABLE_LOC) {
      perror("Error: error seeking.\n");
      exit(1);
    }

    int i;
    for (i = 0; i < NUM_PARTITIONS; i++) {
      ret = read(fd, &part, sizeof(partition));
      if (ret < sizeof(partition)) {
        perror("Error: error reading.\n");
        exit(1);
      }
      ptable[i] = part;
    }

    /* check if requested partition is valid *
    if (ptable[part_num].bootind != BOOT_MAGIC) {
      fprintf(stderr, "bootind: %d\n", part_num, ptable[part_num].bootind);
      perror("Error: unbootable partition.\n");
      exit(1);
    } */
    if (ptable[part_num].type != BOOT_MINIX) {
      perror("Error: partition is not Minix.\n");
      exit(1);
    }

    /* get the first and last addresses of partition */
    first_addr = ptable[part_num].lFirst * SECTOR_SIZE;
    last_addr = (ptable[part_num].lFirst + ptable[part_num].size) * SECTOR_SIZE;

    if (subpart_num != -1) { /* user specified subpartition */
      if (subpart_num < 0 || subpart_num > NUM_PARTITIONS - 1) {
        perror("Error: invalid subpartition number.\n");
        exit(1);
      }
      /* check for valid partition table */
      ret = lseek(fd, first_addr + PARTITION_MAGIC_LOC1, SEEK_SET);
      if (ret != first_addr + PARTITION_MAGIC_LOC1) {
        perror("Error: error seeking.\n");
        exit(1);
      }
      ret = read(fd, &magic1, 1);
      if (ret < 1) {
        perror("Error: error reading.\n");
        exit(1);
      }
      ret = read(fd, &magic2, 1);
      if (ret < 1) {
        perror("Error: error reading.\n");
        exit(1);
      }
      if (magic1 != PARTITION_MAGIC_VAL1 || magic2 != PARTITION_MAGIC_VAL2) {
        perror("Error: invalid subpartition table.\n");
        exit(1);
      }

      ret = lseek(fd, first_addr + PARTITION_TABLE_LOC, SEEK_SET);
      if (ret != first_addr + PARTITION_TABLE_LOC) {
        perror("Error: error seeking.\n");
        exit(1);
      }

      for (i = 0; i < NUM_PARTITIONS; i++) {
        ret = read(fd, &part, sizeof(partition));
        if (ret < sizeof(partition)) {
          perror("Error: error reading.\n");
          exit(1);
        }
        ptable[i] = part;
      }

      /* check if requested partition is valid *
      if (subpart_num >= NUM_PARTITIONS || ptable[subpart_num].bootind !=
       BOOT_MAGIC) {
        perror("Error: unbootable subpartition.\n");
        exit(1);
      }*/
      if (ptable[subpart_num].type != BOOT_MINIX) {
        perror("Error: subpartition is not Minix.\n");
        exit(1);
      }

      /* get the first and last addresses of subpartition */
      first_addr = ptable[subpart_num].lFirst * SECTOR_SIZE;
      last_addr = (ptable[subpart_num].lFirst + ptable[subpart_num].size)
       * SECTOR_SIZE;
    }
  }

  /* read the superblock */
  ret = lseek(fd, first_addr + SUPERBLOCK_OFF, SEEK_SET);
  if (ret != first_addr + SUPERBLOCK_OFF) {
    perror("Error: error seeking.\n");
    exit(1);
  }
  ret = read(fd, &super, sizeof(superblock));
  if (ret < sizeof(superblock)) {
    perror("Error: error reading.\n");
    exit(1);
  }

  /* check for valid Minix filesystem */
  if (super.magic != MINIX_MAGIC) {
    perror("Error: invalid Minix superblock.\n");
    exit(1);
  }

  zone_size = super.blocksize << super.log_zone_size;

  if (verbose) {
    printf("\nSuperblock Contents:\n");
    printf("Stored Fields:\n");
    printf("  ninodes %12d\n", super.ninodes);
    printf("  i_blocks %11d\n", super.iblocks);
    printf("  z_blocks %11d\n", super.zblocks);
    printf("  firstdata %10d\n", super.firstdata);
    printf("  log_zone_size %6d (zone size: %d)\n", super.log_zone_size,
     zone_size);
    printf("  max_file %11d\n", super.max_file);
    printf("  magic %14x\n", super.magic);
    printf("  zones %14d\n", super.zones);
    printf("  blocksize %10d\n", super.blocksize);
    printf("  subversion %9d\n", super.subversion);
  }

  /* get root address */
  root_addr = first_addr + (1 + 1 + super.iblocks + super.zblocks) *
   super.blocksize;

  /* read in root inode */
  ret = lseek(fd, root_addr, SEEK_SET);
  if (ret != root_addr) {
    perror("Error: error seeking.\n");
    exit(1);
  }
  ret = read(fd, &root_inode, sizeof(inode));
  if (ret < sizeof(inode)) {
    perror("Error: error reading.\n");
    exit(1);
  }

  /* read in first root directory entry */
  dir_entry temp_dir;
  inode temp = root_inode;
  int zone_ndx = 0, zone_num = 0, ind_zone_ndx = 0, ind2_zone_ndx = 0;
  int max_ind_zone_ndx = zone_size - sizeof(uint32_t);
  int double_zone_ndx = 0;
  uint32_t double_zone_num = 0;
  ret = lseek(fd, first_addr + root_inode.zone[zone_num] * zone_size, SEEK_SET);
  if (ret != first_addr + root_inode.zone[zone_num] * zone_size) {
    perror("Error: error seeking.\n");
    exit(1);
  }
  ret = read(fd, &temp_dir, sizeof(dir_entry));
  if (ret < sizeof(dir_entry)) {
    perror("Error: error reading.\n");
    exit(1);
  }

  /* go through directory(s) using srcpath */
  int name_size = 0, byte_count = sizeof(dir_entry);
  if (*srcpath == '/') {
    srcpath++;
  }
  while (*(srcpath + name_size) && *(srcpath + name_size) != '/') {
    name_size++;
  }

  while (1) {
    /* check if we need to move zones */
    if (byte_count > zone_size - sizeof(dir_entry)) {
      if (zone_ndx < DIRECT_ZONES - 1) {
        zone_ndx++;
        if (temp.zone[zone_ndx]) {
          zone_num = temp.zone[zone_ndx];
          byte_count = 0;
        }
        else {
          continue;
        }
      }
      else {
        perror("Error: file not found in direct zones\n");
        exit(1);
      }
    }

    if (strncmp(srcpath, temp_dir.name, name_size)) {
      /* wrong directory keep checking*/
      ret = read(fd, &temp_dir, sizeof(dir_entry));
      if (ret < sizeof(dir_entry)) {
        perror("Error: error reading.\n");
        exit(1);
      }
      byte_count += sizeof(dir_entry);
    }
    else {
      /* we got a hit */
      srcpath += name_size;
      if (*srcpath == '/' && *(srcpath + 1)) {
        /* it's a directory */
        name_size = 0;
        srcpath++;
        while (*(srcpath + name_size) != '/' && *(srcpath + name_size)) {
          name_size++;
        }

        ret = lseek(fd, root_addr + (temp_dir.inode-1) * INODE_SIZE, SEEK_SET);
        if (ret != root_addr + (temp_dir.inode-1) * INODE_SIZE) {
          perror("Error: error seeking.\n");
          exit(1);
        }
        ret = read(fd, &temp, INODE_SIZE);
        if (ret < INODE_SIZE) {
          perror("Error: error reading.\n");
          exit(1);
        }

        /* make sure it's actually a directory */
        if (!(temp.mode & IS_DIR)) {
          perror("Error: directory not found.\n");
          exit(1);
        }

        zone_num = 0;
        ret = lseek(fd, first_addr + temp.zone[0] * zone_size, SEEK_SET);
        if (ret != first_addr + temp.zone[0] * zone_size) {
          perror("Error: error seeking.\n");
          exit(1);
        }
        ret = read(fd, &temp_dir, sizeof(dir_entry));
        if (ret < sizeof(dir_entry)) {
          perror("Error: error reading.\n");
          exit(1);
        }
        byte_count = sizeof(dir_entry);
      }
      else if (*srcpath == '/' && !*(srcpath+1)) {
        /* specified file is a dir */
        perror("Error: cannot use a directory.\n");
        exit(1);
      }
      else if (*srcpath) {
        /* name_size chars matched, wrong file */
        ret = read(fd, &temp_dir, sizeof(dir_entry));
        if (ret < sizeof(dir_entry)) {
          perror("Error: error reading.\n");
          exit(1);
        }
        byte_count += sizeof(dir_entry);
      }
      else {
        /* we got the file! (inode number at least) */
        ret = lseek(fd, root_addr+(temp_dir.inode-1)*sizeof(inode), SEEK_SET);
        if (ret != root_addr+(temp_dir.inode-1)*sizeof(inode)) {
          perror("Error: error seeking.\n");
          exit(1);
        }
        ret = read(fd, &temp, sizeof(inode));
        if (ret < sizeof(inode)) {
          perror("Error: error reading.\n");
          exit(1);
        }
        break;
      }
    }
  }

  /* check if file isn't regular or directory */
  if (temp.mode & IS_DIR || !(temp.mode & IS_REG_FILE)) {
    perror("Error: expected a regular file.\n");
    exit(1);
  }

  if (verbose) {
    char permission[11];
    permission[0] = '-';
    permission[1] = temp.mode & OWN_RD ? 'r' : '-';
    permission[2] = temp.mode & OWN_WR ? 'w' : '-';
    permission[3] = temp.mode & OWN_EX ? 'x' : '-';
    permission[4] = temp.mode & GRP_RD ? 'r' : '-';
    permission[5] = temp.mode & GRP_WR ? 'w' : '-';
    permission[6] = temp.mode & GRP_EX ? 'x' : '-';
    permission[7] = temp.mode & OTH_RD ? 'r' : '-';
    permission[8] = temp.mode & OTH_WR ? 'w' : '-';
    permission[9] = temp.mode & OTH_EX ? 'x' : '-';
    permission[10] = 0;
    printf("\nFile inode:\n");
    printf("  uint16_t mode %16x (%s)\n", temp.mode, permission);
    printf("  uint16_t links %15d\n", temp.links);
    printf("  uint16_t uid %17d\n", temp.uid);
    printf("  uint16_t gid %17d\n", temp.gid);
    printf("  uint32_t size %16d\n", temp.size);
    printf("  uint32_t atime %15d --- %s\n", temp.atime, ctime(&temp.atime));
    printf("  uint32_t mtime %15d --- %s\n", temp.mtime, ctime(&temp.mtime));
    printf("  uint32_t ctime %15d --- %s\n", temp.ctime, ctime(&temp.ctime));
    printf("\n  Direct zones:\n");
    printf("              zone[0]   = %12d\n", temp.zone[0]);
    printf("              zone[1]   = %12d\n", temp.zone[1]);
    printf("              zone[2]   = %12d\n", temp.zone[2]);
    printf("              zone[3]   = %12d\n", temp.zone[3]);
    printf("              zone[4]   = %12d\n", temp.zone[4]);
    printf("              zone[5]   = %12d\n", temp.zone[5]);
    printf("              zone[6]   = %12d\n", temp.zone[6]);
    printf("  uint32_t    indirect %15d\n", temp.indirect);
    printf("  uint32_t    double   %15d\n", temp.two_indirect);
  }

  /* get output file descriptor */
  int outfd;
  if (!dstpath) {
    outfd = STDOUT_FILENO;
  }
  else {
    outfd = open(dstpath, O_CREAT | O_TRUNC | O_WRONLY);
    if (outfd < 0) {
      perror("Error: could not open destination file.\n");
      exit(1);
    }
  }

  /* read from each zone and write */
  int num_zones = temp.size / zone_size + 1;
  /* todo determine if we need to check indirect zones etc. Nico said he
   wouldn't use them... */
  int i, j = 0;
  char buf[zone_size];
  for (i = 0; i < num_zones; i++) {
    if (i > DIRECT_ZONES - 1) {
      break;
    }


    /* check for zone 0, we don't want that */
    if (!temp.zone[i]) {
      continue;
    }
    ret = lseek(fd, first_addr + temp.zone[i] * zone_size, SEEK_SET);
    if (ret != first_addr + temp.zone[i] * zone_size) {
      perror("Error: error seeking.\n");
      exit(1);
    }
    if (i == num_zones - 1) {
      zone_size = zone_size % temp.size ? temp.size % zone_size : zone_size;
    }
    ret = read(fd, buf, zone_size);
    if (ret < zone_size) {
      perror("Error: error reading.\n");
      exit(1);
    }
    ret = write(outfd, buf, zone_size);
    if (ret < zone_size) {
      perror("Error: error writing.\n");
      exit(1);
    }
  }

  return 0;
}
