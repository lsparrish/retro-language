#ifndef STORAGE_FILE_H
#define STORAGE_FILE_H
#ifdef STORAGE_ACTIVATED
#error "Only one storage device can be activated."
#endif
#define STORAGE_ACTIVATED

#define STORAGE_IMAGE_FILE "retroImage16"
#define STORAGE_SECTOR_SIZE     512

static FILE *fd;

static uint8_t storage_initialize(uint8_t speed) {
    fd = fopen(STORAGE_IMAGE_FILE, "r+");
    return 0;
}

static uint8_t storage_read_sector(uint8_t *buffer, uint32_t sector) {
    if (0 != fseek(fd, sector * STORAGE_SECTOR_SIZE, SEEK_SET)) {
        perror("fseek");
        return 1;
    }
    for (int i = 0; i < STORAGE_SECTOR_SIZE; ++i)
        buffer[i] = 0;
    fread(buffer, STORAGE_SECTOR_SIZE, 1, fd);
    return 0;
}

static uint8_t storage_write_sector(uint8_t *buffer, uint32_t sector) {
    if (0 != fseek(fd, sector * STORAGE_SECTOR_SIZE, SEEK_SET)) {
        perror("fwrite");
        return 1;
    }
    if (1 != fwrite(buffer, STORAGE_SECTOR_SIZE, 1, fd)) {
        perror("fwrite");
        return 2;
    }
    fsync(fd);
    return 0;
}

#endif
