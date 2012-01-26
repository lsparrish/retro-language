#ifndef STORAGE_SDCARD_H
#define STORAGE_SDCARD_H
#ifdef STORAGE_ACTIVATED
#error "Only one storage device can be activated."
#endif
#define STORAGE_ACTIVATED

#define STORAGE_SECTOR_SIZE     512

static int fd;

static uint8_t storage_initialize(uint8_t speed) {
    fd = fopen(STORAGE_IMAGE_FILE, "r+");
    return 0;
}

static uint8_t storage_read_sector(uint8_t *buffer, uint32_t sector) {
    return 0;
}

static uint8_t storage_write_sector(uint8_t *buffer, uint32_t sector) {
    return 0;
}

#endif
