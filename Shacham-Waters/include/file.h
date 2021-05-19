#pragma once

#include <pbc/pbc.h>

struct file_sector_t {
    size_t index;
    void *data;
    uint32_t sector_size;
};

struct file_block_t {
    size_t index;
    struct file_sector_t *sectors;
    uint32_t num_sectors;
    element_t sigma; // tag
};

struct file_t {
    struct file_block_t *blocks;
    uint32_t num_blocks;
};

// Creates file and populates file_t
struct file_t* create_file(char *file_name, int num_blocks, int num_sectors, int sector_size);
struct file_t* read_file(char *file_name, int num_blocks, int num_sectors, int sector_size);

// A limited version of read_file that just reads few blocks
struct file_block_t** read_select_file_blocks(
    char *file_name, 
    uint32_t *indices,
    int length,
    int num_sectors,
    int sector_size
);

// Free allocated memory
void free_file_t(struct file_t *file_info, 
                 int num_blocks, 
                 int num_sectors);
void free_blocks(struct file_block_t** blocks,
                 int num_blocks, 
                 int num_sectors);

// Import & export tag
void export_tag(char *file_name, struct file_t* file);
void import_tag(char *file_name, struct file_t* file);
void import_select_tags(char *file_name, 
                        struct file_block_t** blocks,
                        int length);