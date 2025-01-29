/*
DCC605 - Sistemas Operacionais
Trabalho Prático 3 - File System Shell
Grupo: Lucas Albano Olive Cruz - 2022036209
       Romana Gallete Mota - 2021039344

Para compilar: gcc -o dcc_fs_shell dcc_fs_shell.c
Como executar: ./dcc_fs_shell <image.ext2>

Comandos disponíveis:
- ls: lista os arquivos e diretórios do diretório atual
- cd <dir>: muda o diretório atual para <dir>
- stat <file>: exibe informações sobre o arquivo <file>
- find <dir>: exibe todos os arquivos e diretórios a partir de <dir> recursivamente
- sb: exibe informações do superbloco
- exit: encerra o programa
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <ext2fs/ext2_fs.h>

#define BASE_OFFSET 1024
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block - 1) * block_size)
#define EXT2_NAME_LEN 255

int fd;
unsigned int block_size;
struct ext2_super_block sb;
struct ext2_group_desc gd;
unsigned int current_inode = 2; // Início na raiz

void read_superblock() {
    lseek(fd, BASE_OFFSET, SEEK_SET);
    read(fd, &sb, sizeof(sb));
    block_size = 1024 << sb.s_log_block_size;
}

void print_superblock() {
    printf("Inodes count: %u\n", sb.s_inodes_count);
    printf("Blocks count: %u\n", sb.s_blocks_count);
    printf("Block size: %u\n", block_size);
    printf("Blocks per group: %u\n", sb.s_blocks_per_group);
    printf("Inodes per group: %u\n", sb.s_inodes_per_group);
    printf("First data block: %u\n", sb.s_first_data_block);
    printf("Free blocks: %u\n", sb.s_free_blocks_count);
    printf("Free inodes: %u\n", sb.s_free_inodes_count);
    printf("Magic number: 0x%x\n", sb.s_magic);
}

void read_group_desc() {
    lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
    read(fd, &gd, sizeof(gd));
}

void read_inode(unsigned int inode_no, struct ext2_inode *inode) {
    unsigned int block_group = (inode_no - 1) / sb.s_inodes_per_group;
    unsigned int index = (inode_no - 1) % sb.s_inodes_per_group;
    unsigned int inode_table_block = gd.bg_inode_table;
    lseek(fd, BLOCK_OFFSET(inode_table_block) + index * sb.s_inode_size, SEEK_SET);
    read(fd, inode, sizeof(struct ext2_inode));
}

void list_directory(unsigned int inode_no) {
    struct ext2_inode inode;
    read_inode(inode_no, &inode);
    char *block = malloc(block_size);
    if (block == NULL) {
        perror("Failed to allocate memory");
        return;
    }
    lseek(fd, BLOCK_OFFSET(inode.i_block[0]), SEEK_SET);
    read(fd, block, block_size);
    struct ext2_dir_entry *entry = (struct ext2_dir_entry *)block;
    while ((char *)entry < block + block_size) {
        printf("%.*s\n", entry->name_len, entry->name);
        entry = (struct ext2_dir_entry *)((char *)entry + entry->rec_len);
    }
}

void stat_file(unsigned int inode_no) {
    struct ext2_inode inode;
    read_inode(inode_no, &inode);
    printf("Size: %u bytes\n", inode.i_size);
    printf("Blocks: %u\n", inode.i_blocks);
    printf("Links: %u\n", inode.i_links_count);
    printf("Mode: %o\n", inode.i_mode);
    printf("Access Time: %u\n", inode.i_atime);
    printf("Modification Time: %u\n", inode.i_mtime);
    printf("Creation Time: %u\n", inode.i_ctime);
}

void change_directory(const char *dir_name) {
    struct ext2_inode inode;
    read_inode(current_inode, &inode);
    char block[block_size];
    lseek(fd, BLOCK_OFFSET(inode.i_block[0]), SEEK_SET);
    read(fd, block, block_size);
    struct ext2_dir_entry *entry = (struct ext2_dir_entry *)block;
    while ((char *)entry < block + block_size) {
        if (strncmp(dir_name, entry->name, entry->name_len) == 0) {
            current_inode = entry->inode;
            return;
        }
        entry = (struct ext2_dir_entry *)((char *)entry + entry->rec_len);
    }
    printf("Directory not found!\n");
}

void find_files(unsigned int inode_no, const char *path, int depth) {
    if (depth == 0) {
        return;
    }
    struct ext2_inode inode;
    read_inode(inode_no, &inode);
    char block[block_size];
    struct ext2_dir_entry *entry;

    for (int i = 0; i < EXT2_NDIR_BLOCKS; i++) {
        if (inode.i_block[i] == 0) {
            continue;
        }

        lseek(fd, BLOCK_OFFSET(inode.i_block[i]), SEEK_SET);
        read(fd, block, block_size);
        entry = (struct ext2_dir_entry *)block;

        while ((char *)entry < block + block_size) {
            if (entry->inode != 0) {
                char new_path[256];
                snprintf(new_path, sizeof(new_path), "%s/%.*s", path, entry->name_len, entry->name);
                printf("%s\n", new_path);

                struct ext2_inode entry_inode;
                read_inode(entry->inode, &entry_inode);

                if (S_ISDIR(entry_inode.i_mode) &&
                    strncmp(entry->name, ".", entry->name_len) != 0 &&
                    strncmp(entry->name, "..", entry->name_len) != 0) {
                    find_files(entry->inode, new_path, depth - 1);
                }
            }
            entry = (struct ext2_dir_entry *)((char *)entry + entry->rec_len);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <image.ext2>\n", argv[0]);
        return 1;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("Failed to open image");
        return 1;
    }
    read_superblock();
    read_group_desc();
    char command[256];
    while (1) {
        printf("dcc-fsshell> ");
        scanf("%s", command);
        if (strcmp(command, "ls") == 0) {
            list_directory(current_inode);
        } else if (strcmp(command, "cd") == 0) {
            char dir_name[EXT2_NAME_LEN];
            scanf("%s", dir_name);
            change_directory(dir_name);
        } else if (strcmp(command, "stat") == 0) {
            char file_name[EXT2_NAME_LEN];
            scanf("%s", file_name);
            stat_file(current_inode);
        } else if (strcmp(command, "find") == 0) {
            char path[256];
            scanf("%s", path);
            find_files(current_inode, path, 10);
        } else if (strcmp(command, "exit") == 0) {
            break;
        } else if (strcmp(command, "sb") == 0){
            print_superblock();
        } else {
            printf("Unknown command!\n");
        }
    }
    close(fd);
    return 0;
}
