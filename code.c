#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 64
#define INODE_BLOCKS 5
#define INODES_PER_BLOCK (BLOCK_SIZE / 256)
#define TOTAL_INODES (INODE_BLOCKS * INODES_PER_BLOCK)
#define FIRST_DATA_BLOCK 8
#define LAST_DATA_BLOCK 63

#pragma pack(1)
struct Superblock {
    uint16_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_bitmap_block;
    uint32_t data_bitmap_block;
    uint32_t inode_table_block;
    uint32_t first_data_block;
    uint32_t inode_size;
    uint32_t inode_count;
    char reserved[4058];
};

struct Inode {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint32_t link_count;
    uint32_t block_count;
    uint32_t direct_block;
    uint32_t single_indirect;
    uint32_t double_indirect;
    uint32_t triple_indirect;
    char reserved[156];
};
#pragma pack()

int is_bit_set(uint8_t *bitmap, int index) {
    int byte_index = index / 8;      
    int bit_offset = index % 8;      
    return (bitmap[byte_index] >> bit_offset) & 1;
}

void set_bit(uint8_t *bitmap, int index) {
    int byte_index = index / 8;
    int bit_offset = index % 8;
    bitmap[byte_index] |= (1 << bit_offset);
}

void clear_bit(uint8_t *bitmap, int index) {
    int byte_index = index / 8;
    int bit_offset = index % 8;
    bitmap[byte_index] &= ~(1 << bit_offset);
}

int main() {
    FILE *fp = fopen("vsfs.img", "rb+");
    if (!fp) {
        printf("Error: Could not open vsfs.img\n");
        return 1;
    }

    printf("VSFS Consistency Check and Fix...\n\n");
    struct Superblock sb;
    fseek(fp, 0, SEEK_SET);
    fread(&sb, sizeof(sb), 1, fp);

    printf("Superblock Check & Fix:\n");
    int fixed = 0;

    if (sb.magic != 0xd34d) {
        printf("Fixing magic number: was 0x%x\n", sb.magic);
        sb.magic = 0xd34d;
        fixed++;
    }
    if (sb.block_size != 4096) {
        printf("Fixing block size: was %u\n", sb.block_size);
        sb.block_size = 4096;
        fixed++;
    }
    if (sb.total_blocks != TOTAL_BLOCKS) {
        printf("Fixing total block count: was %u\n", sb.total_blocks);
        sb.total_blocks = TOTAL_BLOCKS;
        fixed++;
    }
    if (sb.inode_size != 256) {
        printf("Fixing inode size: was %u\n", sb.inode_size);
        sb.inode_size = 256;
        fixed++;
    }
    if (sb.inode_bitmap_block != 1) {
        printf("Fixing inode_bitmap_block: was %u\n", sb.inode_bitmap_block);
        sb.inode_bitmap_block = 1;
        fixed++;
    }
    if (sb.data_bitmap_block != 2) {
        printf("Fixing data_bitmap_block: was %u\n", sb.data_bitmap_block);
        sb.data_bitmap_block = 2;
        fixed++;
    }
    if (sb.inode_table_block != 3) {
        printf("Fixing inode_table_block: was %u\n", sb.inode_table_block);
        sb.inode_table_block = 3;
        fixed++;
    }
    if (sb.first_data_block != 8) {
        printf("Fixing first_data_block: was %u\n", sb.first_data_block);
        sb.first_data_block = 8;
        fixed++;
    }
    if (sb.inode_count != TOTAL_INODES) {
        printf("Fixing inode count: was %u\n", sb.inode_count);
        sb.inode_count = TOTAL_INODES;
        fixed++;
    }

    if (fixed > 0) {
        fseek(fp, 0, SEEK_SET);
        fwrite(&sb, sizeof(sb), 1, fp);
        printf("Superblock fixed (%d issues corrected)\n\n", fixed);
    } else {
        printf("Superblock is valid\n");
        printf("No errors found in Superblock\n\n");
    }

    uint8_t inode_bitmap[BLOCK_SIZE];
    uint8_t data_bitmap[BLOCK_SIZE];

    fseek(fp, BLOCK_SIZE * 1, SEEK_SET);
    fread(inode_bitmap, BLOCK_SIZE, 1, fp);

    fseek(fp, BLOCK_SIZE * 2, SEEK_SET);
    fread(data_bitmap, BLOCK_SIZE, 1, fp);

    struct Inode inodes[TOTAL_INODES];
    fseek(fp, BLOCK_SIZE * 3, SEEK_SET);
    fread(inodes, sizeof(struct Inode), TOTAL_INODES, fp);

    int data_block_usage[TOTAL_BLOCKS] = {0};

    printf("Inode & Data Bitmap Check & Fix:\n");
    int bitmap_fixed = 0;

    for (int i = 0; i < TOTAL_INODES; i++) {
        struct Inode *inode = &inodes[i];
        int bitmap_used = is_bit_set(inode_bitmap, i);
        int is_valid = (inode->link_count > 0) && (inode->dtime == 0);

        if (bitmap_used && !is_valid) {
            printf("Clearing invalid inode %d from inode bitmap\n", i);
            clear_bit(inode_bitmap, i);
            bitmap_fixed++;
        } else if (!bitmap_used && is_valid) {
            printf("Setting missing valid inode %d in inode bitmap\n", i);
            set_bit(inode_bitmap, i);
            bitmap_fixed++;
        }

        if (is_valid && inode->block_count > 0) {
            if (inode->direct_block >= FIRST_DATA_BLOCK && inode->direct_block <= LAST_DATA_BLOCK) {
                data_block_usage[inode->direct_block]++;
                if (!is_bit_set(data_bitmap, inode->direct_block)) {
                    printf("Setting missing data block %d in data bitmap (inode %d)\n",
                           inode->direct_block, i);
                    set_bit(data_bitmap, inode->direct_block);
                    bitmap_fixed++;
                }
            } else if (inode->direct_block != 0) {
                printf("Inode %d: Bad block %d detected, resetting to 0\n", i, inode->direct_block);
                inode->direct_block = 0;
                bitmap_fixed++;
            }
        }
    }

    for (int block = FIRST_DATA_BLOCK; block <= LAST_DATA_BLOCK; block++) {
        if (is_bit_set(data_bitmap, block) && data_block_usage[block] == 0) {
            printf("Clearing unreferenced data block %d from data bitmap\n", block);
            clear_bit(data_bitmap, block);
            bitmap_fixed++;
        }
    }

    if (bitmap_fixed == 0) {
        printf("No errors found in Inode & Data Bitmap\n");
    }

    printf("\nDuplicate Block Check:\n");
    int duplicate_found = 0;
    for (int block = FIRST_DATA_BLOCK; block <= LAST_DATA_BLOCK; block++) {
        if (data_block_usage[block] > 1) {
            printf("Data block %d is referenced by %d inodes (duplicate!)\n",
                   block, data_block_usage[block]);
            duplicate_found = 1;
        }
    }

    if (!duplicate_found) {
        printf("No duplicate blocks found\n");
    }

    fseek(fp, BLOCK_SIZE * 3, SEEK_SET);
    fwrite(inodes, sizeof(struct Inode), TOTAL_INODES, fp);
    fseek(fp, BLOCK_SIZE * 1, SEEK_SET);
    fwrite(inode_bitmap, BLOCK_SIZE, 1, fp);
    fseek(fp, BLOCK_SIZE * 2, SEEK_SET);
    fwrite(data_bitmap, BLOCK_SIZE, 1, fp);
    printf("\nFixes applied. File system image is now consistent.\n");

    fclose(fp);
    return 0;
}

