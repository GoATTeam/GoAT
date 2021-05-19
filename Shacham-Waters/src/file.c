#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "utils.h"
#include "common.h"

struct file_t* create_file(char *file_name, int num_blocks, int num_sectors, int sector_size) {
    FILE* fd = fopen(file_name, "w");
    if (!fd) {
        printf("Cannot create file, exiting\n");
        exit(1);
    }

    struct file_t*  file_info;
    file_info = (struct file_t*) malloc (sizeof (struct file_t));
    file_info->num_blocks = num_blocks;
    file_info->blocks = (struct file_block_t*) 
            malloc(sizeof(struct file_block_t) * num_blocks);

    if( ! file_info->blocks) {
        free(file_info);
        fclose(fd);
        printf("Error #1\n");
        exit(0);
    }

    char *str;
    char *ptr;

    for (size_t i = 0; i < num_blocks; i++) {
        struct file_block_t* block_info = file_info->blocks + i;
        block_info->index = i;
        block_info->num_sectors = num_sectors;
        block_info->sectors = (struct file_sector_t*)
                malloc(sizeof(struct file_sector_t) * num_sectors);

        if (! block_info->sectors) {
            free(file_info);
            free(file_info->blocks);
            fclose(fd);
            printf("Error #2\n");
            exit(0);
        }

        for (size_t j = 0; j < num_sectors; j++) {
            struct file_sector_t *sector_info = block_info->sectors + j;
            sector_info->sector_size = sector_size;
            sector_info->data = malloc(sector_size);
            sector_info->index = j;

            str = sector_info->data;
            memset(str, 0, sector_size);
            sprintf(str, "%ld %ld", block_info->index, sector_info->index);
            fwrite(str, 1, sector_size, fd);

            if (strtol(str, &ptr, 10) != block_info->index || 
                    strtol(ptr, NULL, 10) != sector_info->index) {
                perror("File IO error\n");
                exit(1);
            }
        }
    }
    fclose(fd);

    return file_info;
}

struct file_t* read_file(
    char *file_name, 
    int num_blocks, 
    int num_sectors, 
    int sector_size
    ) {
    FILE* fd = fopen(file_name, "r");
    if (!fd) {
        printf("Cannot read file, exiting\n");
        exit(1);
    }

    struct file_t*  file_info;
    file_info = (struct file_t*) malloc (sizeof (struct file_t));
    file_info->num_blocks = num_blocks;
    file_info->blocks = (struct file_block_t*) 
            malloc(sizeof(struct file_block_t) * num_blocks);

    if( ! file_info->blocks) {
        free(file_info);
        fclose(fd);
        printf("Error #1\n");
        exit(0);
    }

    char *ptr;
    for (size_t i = 0; i < num_blocks; i++) {
        struct file_block_t* block_info = file_info->blocks + i;
        block_info->num_sectors = num_sectors;
        block_info->sectors = (struct file_sector_t*)
                malloc(sizeof(struct file_sector_t) * num_sectors);

        if (! block_info->sectors) {
            free(file_info);
            free(file_info->blocks);
            fclose(fd);
            printf("Error #2\n");
            exit(0);
        }

        for (size_t j = 0; j < num_sectors; j++) {
            struct file_sector_t *sector_info = block_info->sectors + j;
            sector_info->sector_size = sector_size;
            sector_info->data = malloc(sector_size);

            fread(sector_info->data, 1, sector_size, fd);

            if (strtol(sector_info->data, &ptr, 10) != i 
                    || strtol(ptr, NULL, 10) != j) {
                perror("File IO error\n");
                exit(1);
            }

        }
    }
    
    fclose(fd);
    return file_info;
}

struct file_block_t** read_select_file_blocks(
    char *file_name, 
    uint32_t *indices,
    int length,
    int num_sectors,
    int sector_size
) {
    FILE* fd = fopen(file_name, "r");
    if (!fd) {
        printf("Cannot read file, exiting\n");
        exit(1);
    }

    struct file_block_t** selected = 
        (struct file_block_t**) malloc(sizeof(struct file_block_t*) * length);

    uint32_t index;
    char *ptr;
    for (size_t i = 0; i < length; i++) {
        selected[i] = malloc(sizeof(struct file_block_t));
        struct file_block_t* block_info = selected[i];

        index = indices[i];
        block_info->index = index;
        block_info->num_sectors = num_sectors;
        block_info->sectors = (struct file_sector_t*)
                malloc(sizeof(struct file_sector_t) * num_sectors);
        fseek(fd, index * num_sectors * sector_size, SEEK_SET);
        
        for (size_t j = 0; j < num_sectors; j++) {
            struct file_sector_t *sector_info = block_info->sectors + j;
            sector_info->sector_size = sector_size;
            sector_info->data = malloc(sector_size);
            sector_info->index = j;

            fread(sector_info->data, 1, sector_size, fd);

            if (strtol(sector_info->data, &ptr, 10) != block_info->index 
                    || strtol(ptr, NULL, 10) != sector_info->index) {
                perror("File IO error\n");
                exit(1);
            }
        }
    }

    fclose(fd);
    return selected;
}

void export_tag(char *file_name, struct file_t* file_info) {
    FILE* fd = fopen(file_name, "w");
    if (!fd) {
        printf("Cannot create file, exiting\n");
        exit(1);
    }
    struct file_block_t* block;
    unsigned char data[1000];
    for (int i = 0; i < file_info->num_blocks; i++) {
        block = file_info->blocks + i;
        int len = element_to_bytes_compressed(data, block->sigma);
        fwrite(data, 1, len, fd);
    }
    fclose(fd);
}

void import_tag(char *file_name, struct file_t *file_info) {
    FILE* fd = fopen(file_name, "r");
    if (!fd) {
        printf("Cannot read file, exiting\n");
        exit(1);
    }

    pairing_t pairing;
    pbc_pairing_init(pairing);
    int G1_LEN_COMPRESSED = pairing_length_in_bytes_compressed_G1(pairing);

    struct file_block_t* block;
    unsigned char data[G1_LEN_COMPRESSED];
    for (int i = 0; i < file_info->num_blocks; i++) {
        block = file_info->blocks + i;
        fread(data, 1, G1_LEN_COMPRESSED, fd);
        element_init_G1(block->sigma, pairing);
        element_from_bytes_compressed(block->sigma, data);
    }
    fclose(fd);
}

void import_select_tags(char *file_name, 
                        struct file_block_t** blocks,
                        int length) {
    FILE* fd = fopen(file_name, "r");
    if (!fd) {
        printf("Cannot read file, exiting\n");
        exit(1);
    }

    pairing_t pairing;
    pbc_pairing_init(pairing);
    int G1_LEN_COMPRESSED = pairing_length_in_bytes_compressed_G1(pairing);

    struct file_block_t* block;
    unsigned char data[G1_LEN_COMPRESSED];
    for (int i = 0; i < length; i++) {
        block = blocks[i];
        fseek(fd, G1_LEN_COMPRESSED * block->index, SEEK_SET);
        fread(data, 1, G1_LEN_COMPRESSED, fd);
        element_init_G1(block->sigma, pairing);
        element_from_bytes_compressed(block->sigma, data);
    }

    fclose(fd);
}

void free_file_t(struct file_t *file_info, 
                 int num_blocks, 
                 int num_sectors) {
    for (size_t i = 0; i < num_blocks; i++) {
        struct file_block_t* block_info = file_info->blocks + i;
        for (size_t j = 0; j < num_sectors; j++) {
            struct file_sector_t *sector_info = block_info->sectors + j;
            free(sector_info->data);
        }
        free(block_info->sectors);
    }

    free(file_info->blocks);
    free(file_info);
}

void free_blocks(struct file_block_t** blocks, 
                 int num_blocks, 
                 int num_sectors) {
    for (size_t i = 0; i < num_blocks; i++) {
        struct file_block_t* block_info = blocks[i];
        for (size_t j = 0; j < num_sectors; j++) {
            struct file_sector_t *sector_info = block_info->sectors + j;
            free(sector_info->data);
        }
        free(block_info->sectors);
        free(block_info);
    }

    free(blocks);
}
