#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define USAGE "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile\
  [ path ]\n"
#define OPTIONS "Options:\n"
#define USAGE_PART "-p part --- select partition for filesystem \
  (default: none)\n"
#define USAGE_SUB "-s sub --- select subpartition for filesystem \
  (default: none)\n"
#define USAGE_HELP "-h help --- print usage information and exit\n"
#define USAGE_VERBOSE "-v verbose --- increase verbosity level\n"

#define _V "-v"
#define _P "-p"
#define _S "-s"
#define _H "-h"

#define PATH_MAX 70
#define END -1
#define BAD -1
#define PARTITION_ENTRY 0x1BE
#define TYPE_OFFSET 32
#define ONE_K 1024
#define MINIX_TYPE 0x81
#define MAGIC_MINIX 0x4D5A
#define MAGIC_MINIX_REV 0x5A4D
#define SECTOR_SIZE 512
#define DIRECT_ZONES 7
#define DIR_CONST 60
#define PART_TABLE_TO_VALID 64
#define BYTE_510_511_MASK 0x0000AA55
#define PARTITION_END_TO_SUPER 558
#define NOT_VALID_PARTITION -5
#define PARTITION_COUNT 4
#define DIRECTORY_MAX_ENTRIES 500
#define INODE_SIZE 64
#define DIRECTORY_FILE_MASK 0x4000
#define REGULAR_FILE_MASK 0x8000
#define MAX_STRING_LEN 60
#define OWNER_READ 0x100
#define OWNER_WRITE 0x80
#define OWNER_EXECUTE 0x40
#define GROUP_READ 0x20
#define GROUP_WRITE 0x10
#define GROUP_EXECUTE 0x8
#define OTHER_READ 0x4
#define OTHER_WRITE 0x2
#define OTHER_EXECUTE 0x1
#define MAX_PARTITIONS 4
#define MIN_PARTITIONS 0


typedef struct part_table {
    uint8_t bootind;
    uint8_t starthead;
    uint8_t start_sec;
    uint8_t start_cyl;
    uint8_t type;
    uint8_t end_head;
    uint8_t end_sec;
    uint8_t end_cyl;
    uint32_t lFirst;
    uint32_t size;
} part_table;

typedef struct superblock { 
    /* Minix Version 3 Superblock
    * this structure found in fs/super.h
    * in minix 3.1.1
    */
    /* on disk. These fields and orientation are non–negotiable */
    uint32_t ninodes; /* number of inodes in this filesystem */
    uint16_t pad1; /* make things line up properly */
    int16_t i_blocks; /* # of blocks used by inode bit map */
    int16_t z_blocks; /* # of blocks used by zone bit map */
    uint16_t firstdata; /* number of first data zone */
    int16_t log_zone_size; /* log2 of blocks per zone */
    int16_t pad2; /* make things line up again */
    uint32_t max_file; /* maximum file size */
    uint32_t zones; /* number of zones on disk */
    int16_t magic; /* magic number */
    int16_t pad3; /* make things line up again */
    uint16_t blocksize; /* block size in bytes */
    uint8_t subversion; /* filesystem sub–version */
} Superblock;

typedef struct inode {
    uint16_t mode; /* mode */
    uint16_t links; /* number or links */
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

typedef struct directory {
    uint32_t inode;
    unsigned char name[DIR_CONST];
} dir;


/*argument check*/
int verbose_sel;
int p_input;
int sp_input;
char source_path[PATH_MAX];
char source_path_copy[PATH_MAX];
char filename[PATH_MAX];
FILE *fp;
/*partition table info*/
part_table pt;
part_table parts[PARTITION_COUNT];
/*superblock info*/
Superblock sb;
unsigned int zonesize;
/*directory*/
dir dr;
dir drs[DIRECTORY_MAX_ENTRIES];
int drs_count;
/*inode*/
inode ino;
/*others*/
long start_addr;
long inode_start_addr;


/* Prototype Declarations */
void print_usage();
int argument_check(int, char const *[]);
int partition_peek();
int subpartition_peek();
int superblock_peek();
void print_partition_header();
void print_partition();
void print_subpartition_header();
int read_and_traverse_inodes();
void print_inode();
int print_dir(dir);
int search_directory_for_name(char *);
int print_regular_file();
int print_directory_file();
void print_permissions(inode);


int main(int argc, char const *argv[])
{
    int res = 0;
    p_input = -1;
    sp_input = 0;
    fpos_t pos = {0};

    if (argc < 2) {
        print_usage();
        return 0;
    }
    /************************************************/
    res = argument_check(argc, argv);
    if (res) {
        return res;
    }


    /******************* read first partition **********************/
    res = fseek(fp, PARTITION_ENTRY, SEEK_SET);
    if (res) {
        printf("Bap partition read. 2\n");
        return BAD;
    }

    res = partition_peek();
    if (res == BAD) {
        return BAD;
    }
    /** make sure partition is valid **/
    if (res != NOT_VALID_PARTITION) {
        if (p_input < MIN_PARTITIONS || p_input >= MAX_PARTITIONS) {
            printf("File is parttioned but no partition was selected.\n");
            return BAD;
        }
        pt = parts[p_input]; 
        if (pt.type != MINIX_TYPE) {
            printf("This doesn't look like a minix filesystem.\n");
            return BAD;
        }
        /******************* read subpartition *******************/
        res = fseek(fp, pt.lFirst * SECTOR_SIZE + PARTITION_ENTRY, SEEK_SET);
        if (res) {
            printf("Bap partition read. 2\n");
            return BAD;
        }

        res = subpartition_peek();
        if (res == BAD) {
            return BAD;
        }

        pt = parts[sp_input];
        if (pt.type != MINIX_TYPE) {
            printf("This doesn't look like a minix filesystem.\n");
            return BAD;
        }
        /*************** read superblock ***********/
        if (verbose_sel) {
            printf("Current pt info: \n");
            print_partition();
            printf("\n");
        }

        res = fseek(fp, pt.lFirst * SECTOR_SIZE + ONE_K, SEEK_SET);
        if (res) {
            printf("Bad superblock seek. 1\n");
            return BAD;
        }
    }   
    else { 
        if (verbose_sel) {
            printf("Partition Table not found. Reading superblock.\n");
        }
        res = fseek(fp, ONE_K, SEEK_SET);
        if (res) {
            printf("Bad superblock seek. 1\n");
            return BAD;
        }
    }

	res = superblock_peek();
	if (res == BAD) {
        return BAD;
	}

	/***************** rewind to start of partition block ********/
	res = fseek(fp, (sizeof(Superblock) + ONE_K) * -1, SEEK_CUR);
	if (res) {
        printf("Bad superblock seek. (1)\n");
        return BAD;
	}

	res = fgetpos(fp, &pos);
    if (res) {
        printf("Unexpected Error. (72)\n");
        return res;
    }
	start_addr = pos.__pos;
    /*if (verbose_sel) {
        printf("position is %li\n", start_addr);
    }*/
    inode_start_addr = (2 + sb.i_blocks + sb.z_blocks) * 
      sb.blocksize + start_addr;

    /* go to inodes! */
    res = fseek(fp, (2 + sb.i_blocks + sb.z_blocks) * 
      sb.blocksize, SEEK_CUR);
    if (res) {
        printf("Bad superblock seek. (9)\n");
        return BAD;
    }

    /*********************** inode traversal *********************/
    res = read_and_traverse_inodes();
    if (res == BAD) {
        return BAD;
    }

    /***********************************************/
    res = fclose(fp);
    if (res == EOF) {
        printf("Bad file close.\n");
        return BAD;
    }
    return 0;
}

void print_usage() {
    printf(USAGE);
    printf(OPTIONS);
    printf(USAGE_PART);
    printf(USAGE_SUB);
    printf(USAGE_HELP);
    printf(USAGE_VERBOSE);
}

int argument_check(int argc, char const *argv[]) {
    char *ptr;
    int count = 0;

    /*skip over unnecessary data*/
    argv++;

    while (*argv != NULL) {
        if (!strcmp(*argv, _V)) {    /*verbose arg input*/
            verbose_sel = 1;
        }
        else if (!strcmp(*argv, _P)) {  /*partition arg input*/
            argv++;
            if (*argv == NULL) {
                printf("Incorrect Usage.\n");
                print_usage();
                return END;
            }
            p_input = strtol(*argv, &ptr, 10);
            if (p_input < MIN_PARTITIONS || p_input >= MAX_PARTITIONS) {
                printf("Bad partition value. Input must be (0 - 3).\n");
                return END;
            }
            if (verbose_sel) {
                printf("partition selected is %d\n", p_input);
            }
        }
        else if (!strcmp(*argv, _S)) {  /*subpartition arg input*/
            argv++;
            if (*argv == NULL) {
                printf("Incorrect Usage.\n");
                print_usage();
                return END;
            }
            sp_input = strtol(*argv, &ptr, 10);
            if (sp_input < 0 || sp_input > 3) {
                printf("Bad subpartition value. Input must be (0 - 3).\n");
                return END;
            }
            if (verbose_sel) {
                printf("subpartition selected is %d\n", sp_input);
            }
        }
        else if (!strcmp(*argv, _H)) {  /*help arg input*/
            print_usage();
            return BAD;
        }
        else {
            if (count == 0) {
                fp = fopen(*argv, "r");
                if (fp == NULL) {
                    printf("Imagefile argument could not be opened\n");
                    print_usage();
                    return END;
                }

                strncpy(filename, *argv, PATH_MAX);
                /*strncpy null-ending assertion*/
                filename[PATH_MAX - 1] = '\0';

                count++;

                if (verbose_sel) {
                    printf("Imagefile is %s\n", filename);
                }
            }
            else if (count == 1) {
                strncpy(source_path, *argv, PATH_MAX);
                strncpy(source_path_copy, *argv, PATH_MAX);

                /*strncpy null-ending assertion*/
                source_path[PATH_MAX - 1] = '\0';

                count++;

                if (verbose_sel) {
                    printf("Source Path is %s\n", source_path);
                }
            }
            else {
                printf("Too many arguments.\n");
                print_usage();
                return END;
            }
            
    	}

    	argv++;
    }
    return 0;
}
/** Assumes: we are already at seek location we need to be **/
/** Does: read partition table and store it in global variable pt **/
int partition_peek() {
    int ret = 0;
    int temp = 0;
    int count = 0;


    if (fp == NULL) {
        printf("Bad File Pointer. Received NULL.\n");
        return BAD;	
    }
    /**********************************************************/
    /* seek from where we are to check the valid bits*/
    ret = fseek(fp, PART_TABLE_TO_VALID, SEEK_CUR);
    if (ret) {
        printf("Bad seek to valid. 1\n");
        return BAD;
    }	

    ret = fread(&temp, 2, 1, fp);
    if (ret < 1) {
        printf("Bad read of valid. 1\n");
        return BAD;
    }

    if (temp != BYTE_510_511_MASK) {
        return NOT_VALID_PARTITION;
    }

    /*seek back to prev location with 2 extra read bits included*/
    ret = fseek(fp, (PART_TABLE_TO_VALID + 2) * -1, SEEK_CUR);
    if (ret) {
        printf("Bad seek from valid. 1\n");
        return BAD;
    }

    /**********************************************************/
    /* back at beginning. read partitions*/
    if (verbose_sel) {
        print_partition_header();
    }

    while (count < PARTITION_COUNT) {
        ret = fread(&pt, sizeof(part_table), 1, fp);
        if (ret < 1) {
            printf("Bad partition read.\n");
            return BAD;
        }
        else if (verbose_sel) {
            print_partition();
        }
        parts[count] = pt;
        count++;
    }

    return 0;	
}

/** Assumes: we are already at seek location we need to be **/
/** Does: read partition table and store it in global variable pt **/
int subpartition_peek() {
    int ret = 0;
    int temp = 0;
    int count = 0;


    if (fp == NULL) {
        printf("Bad File Pointer. Received NULL.\n");
        return BAD;	
    }
    /**********************************************************/
    /* seek from where we are to check the valid bits*/
    ret = fseek(fp, PART_TABLE_TO_VALID, SEEK_CUR);
    if (ret) {
        printf("Bad seek to valid. 1\n");
        return BAD;
    }	

    ret = fread(&temp, 2, 1, fp);
    if (ret < 1) {
        printf("Bad read of valid. 1\n");
        return BAD;
    }

    if (temp != BYTE_510_511_MASK) {
        return NOT_VALID_PARTITION;
    }

    /*seek back to prev location with 2 extra read bits included*/
    ret = fseek(fp, (PART_TABLE_TO_VALID + 2) * -1, SEEK_CUR);
    if (ret) {
        printf("Bad seek from valid. 1\n");
        return BAD;
    }

    /**********************************************************/
    /* back at beginning. read partitions*/
    if (verbose_sel) {
        print_subpartition_header();
    }

    while (count < PARTITION_COUNT) {
        ret = fread(&pt, sizeof(part_table), 1, fp);
        if (ret < 1) {
            printf("Bad partition read.\n");
            return BAD;
        }
        else if (verbose_sel) {
            print_partition();
        }
        parts[count] = pt;
        count++;
}

	return 0;	
}

int superblock_peek() {
    int ret = 0;

    ret = fread(&sb, sizeof(struct superblock), 1, fp);
    if (ret != 1) {
        printf("Bad Superblock read. 3\n");
        return BAD;
    }

    if (verbose_sel) {
        printf("\nSuperblock Contents:\n");
        printf("  Ninodes : %u\n", sb.ninodes);
        printf("  Pad1 : %hu\n", sb.pad1);
        printf("  I_blocks : %hu\n", sb.i_blocks);
        printf("  z_blocks: %hu\n", sb.z_blocks);
        printf("  First data : %hi\n", sb.firstdata);
        printf("  Log_zone_size : %hi\n", sb.log_zone_size);
        printf("  Pad2 : %hi\n", sb.pad2);
        printf("  Max_file : %u\n", sb.max_file);
        printf("  Zones : %u\n", sb.zones);
        printf("  Magic: %04X\n", sb.magic);
        printf("  Pad3 : %hu\n", sb.pad3);
        printf("  Blocksize : %hi\n", sb.blocksize);
        printf("  Subversion : %02X\n", sb.subversion);
        printf("  Zonesize : %d (Calculated field)\n", sb.blocksize <<\
 sb.log_zone_size);
        
        
    }

    zonesize = sb.blocksize << sb.log_zone_size;

    return 0;
}

int read_and_traverse_inodes() {
    int ret = 0;
    int count = 0;
    int d_ind = 0;
    int i = 0;
    int k = 0;
    unsigned int inode_data_size = 0;
    dir d = {0};
    char next_str[MAX_STRING_LEN];
    char curr_str[MAX_STRING_LEN];
    char *temp;
    int no_src = 0;

    *curr_str = '1';
    *(curr_str + 1) = '\0'; 


    while (strlen(curr_str)) {
    	/*************************************************/
        /** get next string to search for ****************/
        if (i == 0) {
            if (source_path != NULL) {
            	temp = strtok(source_path, "/");
                if (temp == NULL) {
                    no_src = 1;
                }
                else {
                    memcpy(curr_str, temp, strlen(temp));
                    *(curr_str + strlen(temp)) = '\0';
                }
            }
            temp = strtok(NULL, "/");
            if (temp != NULL) {
            	memcpy(next_str, temp, strlen(temp));
                *(next_str + strlen(temp)) = '\0';
            }
            else {
                *next_str = '\0';
            }

            /*if (verbose_sel) {
                printf("Going to data zone. First run. \
                  Searching for: %s\n", curr_str);
            }*/
            i++;
        }
        else {
        	strncpy(curr_str, next_str, MAX_STRING_LEN);
            temp = strtok(NULL, "/");
            if (temp != NULL) {
            	memcpy(next_str, temp, strlen(temp));
                *(next_str + strlen(temp)) = '\0';
            }
            if (!strcmp(curr_str, next_str)) {
                *next_str = '\0';
            }

            /*if (verbose_sel) {
                printf("Going to data zone. After first run. \
                  Searching for: \'%s\'\n", curr_str);
            }*/
        }
        /** if we've gotten here, there has been an error **/
        if (curr_str == NULL && !no_src) {
            printf("\nUnexpected Error. (27)\n");
            return BAD;
        }

        /*************************************************/
        /**** first read inode  **************************/
        ret = fread(&ino, sizeof(inode), 1, fp); 
        if (ret != 1) {
            printf("Bad inode read. 2\n");
            return BAD;
        }
        if (verbose_sel){
        	print_inode();
        }

        /*if (verbose_sel) {
            printf("current value of strings:\n");
            printf("curr_str: %30s\n", curr_str);
            printf("next_str: %30s\n", next_str);
        }*/
        /*************************************************/
        /**** find out regular file or directory *********/
        /*** if regular file **/
        if (ino.mode & REGULAR_FILE_MASK && ino.mode & DIRECTORY_FILE_MASK) {
            printf("Bad File. Cannot be a regular file & a directory\n");
            return BAD;
        }
        if (ino.mode & REGULAR_FILE_MASK) {
        	ret = print_regular_file();

        	return ret;
        }

        /***if directory **/
        if (ino.mode & DIRECTORY_FILE_MASK) {
            /** is this the last string? yes **/
            if (!strlen(curr_str) || no_src) {
            	ret = print_directory_file();
            	return ret;
            }
            else {
    /*****************  HAS A NEXT LOOP **************************/
                /** is last string? no **/
                /** figure out how many zones (start maximum 7) **/
                /**  go to the first inode data zone  ************/
                ret = fseek(fp, start_addr + (ino.zone[0] * zonesize),
                  SEEK_SET);
                if (ret) {
                    printf("Bap inode seek. 7\n");
                    return BAD;
                }
                /*************************************************/
                /** reset info pertaining to directory ***********/
                drs_count = 0;
                count = 0;
                inode_data_size = ino.size;
                memset(drs, 0, sizeof(dir) * DIRECTORY_MAX_ENTRIES);
                k = 1;

                if (inode_data_size > zonesize * 7) {
                	printf("Indirect zone access not yet coded. \
                      Exiting program.\n");
                	return BAD;
                }

                /** fill directory **/
                while (count < inode_data_size && count < zonesize) {
                    ret = fread(&d, sizeof(dir), 1, fp);
                    if (ret != 1) {
                        printf("Bad directory read. 4\n");
                        return BAD;
                    }
                    if (verbose_sel) {
                    	ret = print_dir(d);
                        if (ret == BAD) {
                            return BAD;
                        }
                    }

                    count += sizeof(dir);
                    drs[drs_count] = d;
                    drs_count++;

                    if (count == zonesize) {
                       	count = 0;
                       	inode_data_size -= zonesize;
                       	/** next inode data zone up**/
                       	ret = fseek(fp, start_addr + (ino.zone[k] * zonesize),
                          SEEK_SET);
                        if (ret) {
                            printf("Bap inode seek. 8\n");
                            return BAD;
                        }
                        k++;
                        drs_count = 0;
                    }

                    if (!strcmp((const char *)d.name, curr_str)) {
                        break;
                    }

                }
            }
            /************************************************/
            /** find next inode # and seek to location for next loop **/
            d_ind = search_directory_for_name(curr_str);
            if (verbose_sel) {
                printf("string: %s has dir index: %u and inode num: %u\n",
                 curr_str, d_ind, drs[d_ind].inode);
            }

            if (d_ind < 0 || d_ind > DIRECTORY_MAX_ENTRIES) {
                printf("Could not find file: %s\n", curr_str);
                return BAD;
            }
            if (verbose_sel) {
                printf("fseeking to : %li + (%u * %i)\n", inode_start_addr,
                  drs[d_ind].inode - 1, INODE_SIZE);
            }

            ret = fseek(fp, inode_start_addr + 
              ((drs[d_ind].inode - 1) * INODE_SIZE), SEEK_SET);
            if (ret) {
                printf("Bap inode seek. 1\n");
                return BAD;
            }
        	
        }
        else {   /** else this is neither a dir file or reg file. Quit**/
            printf("Not a file or directory. Mode is %hu\n", ino.mode);
            return BAD;
    	}
    }
    if (verbose_sel) {
        print_inode();
    }
    return 0;
}

/*** using global 'dir d' for directory to search**/
int search_directory_for_name(char *s) {
    int d_entry_count = ino.size / sizeof(dir);  
    int i = 0;

    while (i < d_entry_count) {
        if (!strcmp(s, (const char *)drs[i].name)) {
            break;
        }
        i++;
    }

    return i < d_entry_count ? i : -1;
}

void print_partition_header() {
    printf("\nPartition Table:\n");
    printf("        ");    /* 8 spaces */
    printf("--Boot--    "); /* 4 spaces */
    printf("--Type--    "); /* 4 spaces */
    printf("--lFirst--    "); /* 4 spaces */
    printf("--size--    "); /* 4 spaces */
    printf("\n");
}

void print_subpartition_header() {
    printf("\nSubpartition Table:\n");
    printf("        ");    /* 8 spaces */
    printf("--Boot--    "); /* 4 spaces */
    printf("--Type--    "); /* 4 spaces */
    printf("--lFirst--    "); /* 4 spaces */
    printf("--size--    "); /* 4 spaces */
    printf("\n");
}

void print_partition() {
    printf("        ");
    printf("  0x%02X      ", pt.bootind);
    printf("  0x%02X      ", pt. type);
    printf("%12u", pt.lFirst);
    printf("    ");
    printf("%12u\n", pt.size);

}

void print_inode() {
    printf("\nInode Found:\n");
    printf("  Mode: %hu\n", ino.mode);
    printf("  Links: %hu\n", ino.links);
    printf("  Size : %u\n", ino.size);
    printf("  Uid : %hu\n", ino.uid);
    printf("  Zone[0] : %u\n", ino.zone[0]);
    printf("  Zone[1] : %u\n", ino.zone[1]);
    printf("  Zone[2] : %u\n", ino.zone[2]);
    printf("  Zone[3] : %u\n", ino.zone[3]);
    printf("  Zone[4] : %u\n", ino.zone[4]);
    printf("  Zone[5] : %u\n", ino.zone[5]);
    printf("  Zone[6] : %u\n", ino.zone[6]);
}

int print_dir(dir d) {
    fpos_t pos = {0};
    unsigned char *str;
    inode i = {0};
    int ret = 0;

    if (!d.inode) {
        return 0;
    }

    /** get position so our can return back to it **/
    fgetpos(fp, &pos);

    str = d.name;

    /** go to inode **/
    ret = fseek(fp, inode_start_addr + ((d.inode - 1) * INODE_SIZE), SEEK_SET);
    if (ret) {
        printf("Bap inode seek. 1\n");
        return BAD;
    }    

    /** get inode data **/
    ret = fread(&i, sizeof(inode), 1, fp); 
    if (ret != 1) {
        printf("Bad inode read. 21\n");
        return BAD;
    }

    if (i.mode & DIRECTORY_FILE_MASK) {
        printf("d");
    }
    else {
        printf("-");
    }
    print_permissions(i);
    printf(" ");
    printf("%9u", i.size);
    printf(" %s\n", str);

    /** return back to position **/
    ret = fseek(fp, pos.__pos, SEEK_SET);
    if (ret) {
        printf("Bap seek to last position. 1\n");
        return BAD;
    }

    return 0;
}

int print_regular_file() {
    /****  go through 10 bits and print out letter or '-'***/
    printf("-"); /* we know it's not a directory if its in this function*/

    print_permissions(ino);

    /** print size & filename **/
    printf("        ");
    printf("%u  ", ino.size);
    printf("%s", source_path_copy);
    printf("\n");

    return 0;
}


int print_directory_file() {
    int count = 0;
    unsigned int inode_data_size = 0;
    dir d = {0};
    int k = 0;
    int ret = 0;

    if (!strlen(source_path_copy)) {
        printf("/:\n");
    }
    else {
        if (*source_path_copy != '/') {
            printf("/");
        }
        printf("%s:\n", source_path_copy);
    }

    inode_data_size = ino.size;
    drs_count = 0;
    k = 1;

    if (inode_data_size > zonesize * 7) {
        printf("Indirect zone access not yet coded. Exiting program.\n");
        return BAD;
    }

    /** seek to first data zone **/
    ret = fseek(fp, start_addr + (ino.zone[0] * zonesize), SEEK_SET);  
    if (ret) {
        printf("Bap inode seek. 8\n");
        return BAD;
    }

	/** fill directory **/
    while (count < inode_data_size && count < zonesize) {
        ret = fread(&d, sizeof(dir), 1, fp);
        if (ret != 1) {
            printf("Bad directory read. 4\n");
            return BAD;
        }

        print_dir(d);

        count += sizeof(dir);
        drs[drs_count] = d;
        drs_count++;

        if (count == zonesize) {
            count = 0;
            inode_data_size -= zonesize;

            /** next inode data zone up**/
            ret = fseek(fp, start_addr + (ino.zone[k] * zonesize), SEEK_SET);
            if (ret) {
                printf("Bap inode seek. 8\n");
                return BAD;
            }

            k++;
            drs_count = 0;
        }
    }

    return 0;
}

void print_permissions(inode i) {

        /******   OWNER   ********************/
    if (i.mode & OWNER_READ) {
        printf("r");
    }
    else {
        printf("-");
    }
    if (i.mode & OWNER_WRITE) {
        printf("w");
    }
    else {
        printf("-");
    }
    if (i.mode & OWNER_EXECUTE) {
        printf("x");
    }
    else {
        printf("-");
    }
    /******   GROUP   ********************/
    if (i.mode & GROUP_READ) {
        printf("r");
    }
    else {
        printf("-");
    }
    if (i.mode & GROUP_WRITE) {
        printf("w");
    }
    else {
        printf("-");
    }
    if (i.mode & GROUP_EXECUTE) {
        printf("x");
    }
    else {
        printf("-");
    }
    /******   OTHER   *******************/
    if (i.mode & OTHER_READ) {
        printf("r");
    }
    else {
        printf("-");
    }
    if (i.mode & OTHER_WRITE) {
        printf("w");
    }
    else {
        printf("-");
    }
    if (i.mode & OTHER_EXECUTE) {
        printf("x");
    }
    else {
        printf("-");
    }

}
