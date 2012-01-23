#ifndef FILESYSTEM_FS_H
#define FILESYSTEM_FS_H
#ifndef STORAGE_ACTIVATED
#error "No storage is activated, please activate storage."
#endif
#ifdef FILESYSTEM_ACTIVATED
#error "Only one filesystem can be activated."
#endif
#define FILESYSTEM_ACTIVATED

#define FS_NO_ERROR		    0x00
#define FS_ERROR_NO_MORE_FILES	    0x01
#define FS_ERROR_FILE_NOT_FOUND	    0x10
#define FS_ERROR_ANOTHER_FILE_OPEN  0x11
#define	FS_ERROR_MBR_READ_ERROR	    0xF0
#define	FS_ERROR_MBR_SIGNATURE	    0xF1
#define	FS_ERROR_MBR_INVALID_FS	    0xF2
#define FS_ERROR_BOOTSEC_READ_ERROR 0xE0
#define	FS_ERROR_BOOTSEC_SIGNATURE  0xE1
#define FS_ERROR_NO_FILE_OPEN	    0xFFF0
#define FS_ERROR_WRONG_FILEMODE	    0xFFF1
#define FS_FILE_IS_EMPTY	    0xFFFD
#define FS_BUFFER_OVERFLOW	    0xFFFE
#define FS_EOF			    0xFFFF

#define FS_FILEMODE_BINARY	    0x01
#define FS_FILEMODE_TEXT_READ	    0x02
#define FS_FILEMODE_TEXT_WRITE	    0x03

struct fs_directory_entry {
    char filename[9];
    char fileext[4];
    uint8_t attributes;
    uint16_t time;
    uint16_t date;
    uint16_t startCluster;
    uint32_t fileSize;
};

struct fs_t {
    struct {
        uint8_t part1Type;
        unsigned long part1Start;
        unsigned long part1Size;
    } MBR;

    struct {
        uint8_t sectorsPerCluster;
        uint16_t reservedSectors;
        uint8_t fatCopies;
        uint16_t rootDirectoryEntries;
        uint32_t totalFilesystemSectors;
        uint16_t sectorsPerFAT;
        uint32_t hiddenSectors;
        uint32_t partitionSerialNum;
        uint16_t fat1Start;
        uint16_t fat2Start;
        float partitionSize;
    } BS;

    struct fs_directory_entry DE;
    unsigned long firstDirSector;
    uint8_t buffer[STORAGE_SECTOR_SIZE];

    struct {
        char filename[13];
        uint16_t currentCluster;
        uint32_t fileSize;
        uint32_t currentPos;
        uint8_t fileMode;
    } currFile;

    int	DEcnt;
};

static uint8_t fs_find_first_file(struct fs_t *self, struct fs_directory_entry *DE);
static uint8_t fs_find_next_file(struct fs_t *self, struct fs_directory_entry *DE);
static uint8_t fs_open_file(struct fs_t *self, char *fn, uint8_t mode);
static uint16_t fs_read_binary(struct fs_t *self);
static uint16_t fs_read_ln(struct fs_t *self, char *st, int buf_size);
static uint16_t fs_write_ln(struct fs_t *self, char *st);
static void fs_close_file(struct fs_t *self);
static uint8_t fs_exists(struct fs_t *self, char *fn);
static uint8_t fs_del_file(struct fs_t *self, char *fn);
static uint8_t fs_create(struct fs_t *self, char *fn);
static uint16_t fat32_find_next_cluster(struct fs_t *self, uint16_t cc);
static char fat32_uCase(char c);
static uint8_t fat32_valid_char(char c);
static uint16_t fat32_find_free_cluster(struct fs_t *self);

static uint8_t fs_initialize(struct fs_t *self) {
    if (RES_OK == storage_read_sector(self->buffer, 0)) {
        if ((self->buffer[0x01FE] == 0x55) && (self->buffer[0x01FF] == 0xAA)) {
            self->MBR.part1Type = self->buffer[450];
            self->MBR.part1Start = (uint16_t) self->buffer[454]
                + (((uint16_t)self->buffer[455])<<8)
                + (((uint32_t)self->buffer[456])<<16)
                + (((uint32_t)self->buffer[457])<<24);
            self->MBR.part1Size = (uint16_t)self->buffer[458]
                + (((uint16_t)self->buffer[459])<<8)
                + (((uint32_t)self->buffer[460])<<16)
                + (((uint32_t)self->buffer[461])<<24);
        } else return FS_ERROR_MBR_SIGNATURE;
    } else return FS_ERROR_MBR_READ_ERROR;

    if ((self->MBR.part1Type != 0x04) \
            && (self->MBR.part1Type != 0x06) \
            && (self->MBR.part1Type!=0x86))
        return FS_ERROR_MBR_INVALID_FS;

    if (RES_OK == storage_read_sector(self->buffer, self->MBR.part1Start)) {
        if ((self->buffer[0x01FE] == 0x55)
                && (self->buffer[0x01FF] == 0xAA))
        {
            self->BS.sectorsPerCluster = self->buffer[0x0D];
            self->BS.reservedSectors = (uint16_t)self->buffer[0x0E]
                + (((uint16_t)self->buffer[0x0F])<<8);
            self->BS.fatCopies = self->buffer[0x10];
            self->BS.rootDirectoryEntries = (uint16_t)self->buffer[0x11]
                + (((uint16_t)self->buffer[0x12])<<8);
            self->BS.totalFilesystemSectors = (uint16_t)self->buffer[0x13]
                + (((uint16_t)self->buffer[0x14])<<8);
            if (self->BS.totalFilesystemSectors == 0)
                self->BS.totalFilesystemSectors = (uint16_t)self->buffer[0x20]
                    + (((uint16_t)self->buffer[0x21])<<8)
                    + (((uint32_t)self->buffer[0x22])<<16)
                    + (((uint32_t)self->buffer[0x23])<<24);
            self->BS.sectorsPerFAT = (uint16_t)self->buffer[0x16]
                + (((uint16_t)self->buffer[0x17])<<8);
            self->BS.hiddenSectors = (uint16_t)self->buffer[0x1C]
                + (((uint16_t)self->buffer[0x1D])<<8)
                + (((uint32_t)self->buffer[0x1E])<<16)
                + (((uint32_t)self->buffer[0x1F])<<24);
            self->BS.partitionSerialNum = (uint16_t)self->buffer[0x27]
                + (((uint16_t)self->buffer[0x28])<<8)
                + (((uint32_t)self->buffer[0x29])<<16)
                + (((uint32_t)self->buffer[0x2A])<<24);
            self->firstDirSector = self->MBR.part1Start
                + self->BS.reservedSectors
                + (self->BS.fatCopies * self->BS.sectorsPerFAT);
            self->BS.fat1Start = self->MBR.part1Start 
                + self->BS.reservedSectors;
            self->BS.fat2Start = self->BS.fat1Start 
                + self->BS.sectorsPerFAT;
            self->BS.partitionSize =
                ((float)(self->MBR.part1Size * STORAGE_SECTOR_SIZE)) / ((float)1048576);
        }
        else return FS_ERROR_BOOTSEC_SIGNATURE;
    }
    else return FS_ERROR_BOOTSEC_READ_ERROR;
    return FS_NO_ERROR;
}

static uint8_t fs_find_first_file(struct fs_t *self, struct fs_directory_entry *DE) {
    unsigned long currSec = self->firstDirSector;
    uint16_t offset = 0;

    self->DEcnt = 0;
    storage_read_sector(self->buffer, currSec);

    if (self->buffer[0] == 0x00) return FS_ERROR_NO_MORE_FILES;
    else {
        while ((self->buffer[offset + 0x0B] & 0x08) \
                || (self->buffer[offset + 0x0B] & 0x10) \
                || (self->buffer[offset] == 0xE5))
        {
            offset+=32;
            self->DEcnt++;
            if (offset == STORAGE_SECTOR_SIZE) {
                storage_read_sector(self->buffer, ++currSec);
                offset = 0;
            } 
            if (self->buffer[offset] == 0x00)
                return FS_ERROR_NO_MORE_FILES;
        }

        for (int i = 0; i<8; i++)
            DE->filename[i] = self->buffer[i + offset];
        for (int i = 0; i<3; i++)
            DE->fileext[i] = self->buffer[i + 0x08 + offset];

        DE->filename[8] = 0;
        DE->fileext[3] = 0;
        DE->attributes = self->buffer[0x0B + offset];
        DE->time = (uint16_t)self->buffer[0x0E + offset]
            + (((uint16_t)self->buffer[0x0F + offset])<<8);
        DE->date = (uint16_t)self->buffer[0x10 + offset]
            + (((uint16_t)self->buffer[0x11 + offset])<<8);
        DE->startCluster = (uint16_t)self->buffer[0x1A + offset]
            + (((uint16_t)self->buffer[0x1B + offset])<<8);
        DE->fileSize = (uint16_t)self->buffer[offset + 0x1C]
            | (((uint16_t)self->buffer[offset + 0x1D])<<8)
            | (((uint32_t)self->buffer[offset + 0x1E])<<16)
            | (((uint32_t)self->buffer[offset + 0x1F])<<24);
        self->DEcnt++;
    }
    return FS_NO_ERROR;
}

static uint8_t fs_find_next_file(struct fs_t *self, struct fs_directory_entry *DE) {
    unsigned long currSec = self->firstDirSector;
    uint16_t offset = self->DEcnt * 32;

    while (offset >= STORAGE_SECTOR_SIZE) {
        currSec++;
        offset -= STORAGE_SECTOR_SIZE;
    }

    storage_read_sector(self->buffer, currSec);

    if (self->buffer[offset] == 0x00)
        return FS_ERROR_NO_MORE_FILES;
    else {
        while ((self->buffer[offset + 0x0B] & 0x08) \
                || (self->buffer[offset + 0x0B] & 0x10) \
                || (self->buffer[offset] == 0xE5))
        {
            offset += 32;
            self->DEcnt++;
            if (offset == STORAGE_SECTOR_SIZE) {
                storage_read_sector(self->buffer, ++currSec);
                offset = 0;
            } 
            if (self->buffer[offset] == 0x00)
                return FS_ERROR_NO_MORE_FILES;
        }

        for (int i = 0; i<8; i++)
            DE->filename[i] = self->buffer[i + offset];
        for (int i = 0; i<3; i++)
            DE->fileext[i] = self->buffer[i + 0x08 + offset];
        DE->filename[8] = 0;
        DE->fileext[3] = 0;
        DE->attributes = self->buffer[0x0B + offset];
        DE->time = (uint16_t)self->buffer[0x0E + offset] 
            + (((uint16_t)self->buffer[0x0F + offset])<<8);
        DE->date = (uint16_t)self->buffer[0x10 + offset] 
            + (((uint16_t)self->buffer[0x11 + offset])<<8);
        DE->startCluster = (uint16_t)self->buffer[0x1A + offset] 
            + (((uint16_t)self->buffer[0x1B + offset])<<8);
        DE->fileSize = (uint16_t)self->buffer[offset + 0x1C] 
            | (((uint16_t)self->buffer[offset + 0x1D])<<8) 
            | (((uint32_t)self->buffer[offset + 0x1E])<<16) 
            | (((uint32_t)self->buffer[offset + 0x1F])<<24);
        self->DEcnt++;

        return FS_NO_ERROR;
    }
}

static uint8_t fs_open_file(struct fs_t *self, char *fn, uint8_t mode) {
    struct fs_directory_entry tmpDE;
    char tmpFN[13];
    uint8_t res;
    int i, j, fnlen = strlen(fn);

    if (self->currFile.filename[0]!=0x00)
        return FS_ERROR_ANOTHER_FILE_OPEN;

    for (i = 0; i < fnlen; i++)
        fn[i] = fat32_uCase(fn[i]);

    res = fs_find_first_file(self, &tmpDE);
    if (res == FS_ERROR_NO_MORE_FILES) return FS_ERROR_FILE_NOT_FOUND;
    else {
        for(i = j = 0; (tmpDE.filename[i]!=0x20) && (i < 8); ++i)
            tmpFN[i] = tmpDE.filename[i];
        tmpFN[i++] = '.';
        while ((tmpDE.fileext[j]!=0x20) && (j<3))
            tmpFN[i++] = tmpDE.fileext[j++];
        tmpFN[i] = 0x00;
        if (!strcmp(tmpFN,fn)) {
            for (i = 0; i < 13; i++)
                self->currFile.filename[i] = tmpFN[i];
            self->currFile.currentCluster = tmpDE.startCluster;
            self->currFile.fileSize = tmpDE.fileSize;
            self->currFile.currentPos = 0;
            self->currFile.fileMode = mode;
            return FS_NO_ERROR;
        }
        while (res == FS_NO_ERROR) {
            res = fs_find_next_file(self, &tmpDE);
            if (res == FS_NO_ERROR) {
                for (i = j = 0; (tmpDE.filename[i] != 0x20) && (i < 8); ++i)
                    tmpFN[i] = tmpDE.filename[i];
                tmpFN[i++] = '.';
                while ((tmpDE.fileext[j]!=0x20) && (j<3))
                    tmpFN[i++] = tmpDE.fileext[j++];
                tmpFN[i] = 0x00;
                if (!strcmp(tmpFN ,fn)) {
                    for (i = 0; i < 13; i++)
                        self->currFile.filename[i] = tmpFN[i];
                    self->currFile.currentCluster = tmpDE.startCluster;
                    self->currFile.fileSize = tmpDE.fileSize;
                    self->currFile.currentPos = 0;
                    self->currFile.fileMode = mode;
                    return FS_NO_ERROR;
                }
            }
        }
    }
    return FS_ERROR_FILE_NOT_FOUND;
}

static uint16_t fs_read_binary(struct fs_t *self) {
    uint32_t sec;

    if (self->currFile.fileMode == FS_FILEMODE_BINARY) {
        if ((self->currFile.currentPos == 0) && (self->currFile.currentCluster == 0))
            return FS_FILE_IS_EMPTY;
        if (((self->currFile.currentPos % self->BS.sectorsPerCluster) == 0) \
                && (self->currFile.currentPos>0))
            self->currFile.currentCluster =
                fat32_find_next_cluster(self, self->currFile.currentCluster);
        sec = (self->BS.reservedSectors 
                + (self->BS.fatCopies * self->BS.sectorsPerFAT)
                + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE)
                + ((self->currFile.currentCluster - 2) * self->BS.sectorsPerCluster)
                + self->BS.hiddenSectors)
            + (self->currFile.currentPos % self->BS.sectorsPerCluster);
        storage_read_sector(self->buffer, sec);
        self->currFile.currentPos++;
        if ((self->currFile.currentPos * STORAGE_SECTOR_SIZE) > self->currFile.fileSize)
            return (self->currFile.fileSize - ((self->currFile.currentPos-1) * STORAGE_SECTOR_SIZE));
        else return STORAGE_SECTOR_SIZE;
    }
    else if (self->currFile.fileMode == 0x00) return FS_ERROR_NO_FILE_OPEN;
    else return FS_ERROR_WRONG_FILEMODE;
}

static uint16_t fs_read_ln(struct fs_t *self, char *st, int buf_size) {
    uint32_t sec, currSecPos;
    int bufIndex = 0;
    uint8_t *bufc;

    for (int i = 0; i <= buf_size; i++) st[i] = 0;
    if (self->currFile.fileMode == FS_FILEMODE_TEXT_READ) {
        if ((self->currFile.currentPos == 0) && (self->currFile.currentCluster == 0))
            return FS_FILE_IS_EMPTY;
        sec = (self->BS.reservedSectors 
                + (self->BS.fatCopies * self->BS.sectorsPerFAT) 
                + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE) 
                + ((self->currFile.currentCluster-2) * self->BS.sectorsPerCluster) 
                + self->BS.hiddenSectors) 
            + ((self->currFile.currentPos / STORAGE_SECTOR_SIZE) % self->BS.sectorsPerCluster);
        storage_read_sector(self->buffer, sec);
        bufc = &(self->buffer[currSecPos = (self->currFile.currentPos % STORAGE_SECTOR_SIZE)]);
        while ((self->currFile.currentPos < self->currFile.fileSize) 
                && (*bufc != 10)
                && (*bufc != 13)
                && (bufIndex < buf_size))
        {
            st[bufIndex++] = *bufc;
            self->currFile.currentPos++;
            if ((self->currFile.currentPos % STORAGE_SECTOR_SIZE) == 0)
            {
                sec++;
                if (((self->currFile.currentPos / STORAGE_SECTOR_SIZE) % self->BS.sectorsPerCluster) == 0)
                {
                    self->currFile.currentCluster = fat32_find_next_cluster(self, self->currFile.currentCluster);
                    sec = (self->BS.reservedSectors
                            + (self->BS.fatCopies * self->BS.sectorsPerFAT)
                            + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE)
                            + ((self->currFile.currentCluster - 2) * self->BS.sectorsPerCluster)
                            + self->BS.hiddenSectors);
                }
                storage_read_sector(self->buffer, sec);
            }
            bufc = &(self->buffer[currSecPos = (self->currFile.currentPos % STORAGE_SECTOR_SIZE)]);
        }
        if (self->currFile.currentPos >= self->currFile.fileSize) {
            return EOF;
        } else if ((currSecPos < (STORAGE_SECTOR_SIZE - 2)) && (bufc[0] == 13) && (bufc[1] == 10)) {
            self->currFile.currentPos += 2;
            return bufIndex;
        } else if ((*bufc == 13) || (*bufc == 10)) {
            self->currFile.currentPos++;
            return bufIndex;
        }
        else return FS_BUFFER_OVERFLOW;
    }
    else if (self->currFile.fileMode == 0x00) return FS_ERROR_NO_FILE_OPEN;
    else return FS_ERROR_WRONG_FILEMODE;
}

static uint16_t fs_write_ln(struct fs_t *self, char *st) {
    unsigned long currSec = self->firstDirSector;
    uint16_t nextCluster = 0, x = 0, y = 0, z = 0;
    uint16_t offset = -32;
    uint32_t filess;
    char tmpFN[13];
    int i, j, k, stlen = strlen(st);
    int bufIndex = 0;
    uint8_t done = 0;

    if (self->currFile.fileMode == FS_FILEMODE_TEXT_WRITE) {
        if (self->currFile.currentCluster == 0) {
            self->currFile.currentCluster = fat32_find_free_cluster(self);

            storage_read_sector(self->buffer, currSec);
            while (!done) {
                offset += 32;
                if (offset == STORAGE_SECTOR_SIZE) {
                    storage_read_sector(self->buffer, ++currSec);
                    offset = 0;
                } 

                j = 0;
                for (int i = 0; i < 8; i++)
                    if (self->buffer[i + offset]!=0x20)
                        tmpFN[j++] = self->buffer[i + offset];
                tmpFN[j++] = '.';
                for (int i = 0; i < 3; i++)
                    if (self->buffer[i + 0x08 + offset]!=0x20)
                        tmpFN[j++] = self->buffer[i + 0x08 + offset];
                tmpFN[j] = 0x00;

                if (!strcmp(tmpFN, self->currFile.filename)) {
                    self->buffer[offset + 0x1A] = self->currFile.currentCluster & 0xFF;
                    self->buffer[offset + 0x1B] = self->currFile.currentCluster>>8;

                    storage_write_sector(self->buffer, currSec);

                    storage_read_sector(self->buffer, self->BS.fat1Start + (self->currFile.currentCluster>>8));
                    self->buffer[(self->currFile.currentCluster & 0xFF) * 2] = 0xFF;
                    self->buffer[((self->currFile.currentCluster & 0xFF) * 2) + 1] = 0xFF;
                    storage_write_sector(self->buffer, self->BS.fat1Start + (self->currFile.currentCluster>>8));

                    storage_read_sector(self->buffer, self->BS.fat2Start + (self->currFile.currentCluster>>8));
                    self->buffer[(self->currFile.currentCluster & 0xFF) * 2] = 0xFF;
                    self->buffer[((self->currFile.currentCluster & 0xFF) * 2) + 1] = 0xFF;
                    storage_write_sector(self->buffer, self->BS.fat2Start + (self->currFile.currentCluster>>8));

                    done = 1;
                }
            }
        }

        if ((((self->currFile.fileSize % STORAGE_SECTOR_SIZE) + stlen) <= 510)
                && ((self->currFile.fileSize % (self->BS.sectorsPerCluster * STORAGE_SECTOR_SIZE) != 0)
                    || (self->currFile.fileSize == 0)))
        {
            currSec = (self->BS.reservedSectors
                    + (self->BS.fatCopies * self->BS.sectorsPerFAT)
                    + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE)
                    + ((self->currFile.currentCluster-2) * self->BS.sectorsPerCluster)
                    + self->BS.hiddenSectors)
                + ((self->currFile.fileSize / STORAGE_SECTOR_SIZE) % self->BS.sectorsPerCluster);
            storage_read_sector(self->buffer, currSec);
            filess = self->currFile.fileSize % STORAGE_SECTOR_SIZE;
            for (i = 0; i < stlen; i++)
                self->buffer[filess + i] = st[i];
            self->buffer[filess + stlen] = 0x0D;
            self->buffer[filess + stlen + 1] = 0x0A;
            storage_write_sector(self->buffer, currSec);
        } else {
            currSec = (self->BS.reservedSectors
                    + (self->BS.fatCopies * self->BS.sectorsPerFAT)
                    + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE)
                    + ((self->currFile.currentCluster-2) * self->BS.sectorsPerCluster)
                    + self->BS.hiddenSectors)
                + ((self->currFile.fileSize / STORAGE_SECTOR_SIZE) % self->BS.sectorsPerCluster);

            if ((self->currFile.fileSize%STORAGE_SECTOR_SIZE) != 0) {
                storage_read_sector(self->buffer, currSec);
                j = self->currFile.fileSize % STORAGE_SECTOR_SIZE;
                for (i = 0, k = STORAGE_SECTOR_SIZE - j; i < k; ++i, ++bufIndex)
                    self->buffer[j + i] = st[i];
                storage_write_sector(self->buffer, currSec++);
            } else bufIndex = 0;

            if (((currSec-(self->BS.reservedSectors
                                + (self->BS.fatCopies * self->BS.sectorsPerFAT)
                                + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE)
                                + self->BS.hiddenSectors))
                        % self->BS.sectorsPerCluster) == 0)
            {
                nextCluster = fat32_find_free_cluster(self);

                x = self->currFile.currentCluster >> 8;
                y = nextCluster >> 8;
                z = nextCluster & 0xFF;
                storage_read_sector(self->buffer, self->BS.fat1Start + x);
                self->buffer[(self->currFile.currentCluster & 0xFF) * 2] = nextCluster & 0xFF;
                self->buffer[((self->currFile.currentCluster & 0xFF) * 2) + 1] = y;
                if (y == x) {
                    self->buffer[z * 2] = 0xFF;
                    self->buffer[(z * 2) + 1] = 0xFF;
                    storage_write_sector(self->buffer, self->BS.fat1Start + x);
                } else {
                    storage_write_sector(self->buffer, self->BS.fat1Start + x);
                    storage_read_sector(self->buffer, self->BS.fat1Start + y);
                    self->buffer[z * 2] = 0xFF;
                    self->buffer[(z * 2) + 1] = 0xFF;
                    storage_write_sector(self->buffer, self->BS.fat1Start + y);
                }

                storage_read_sector(self->buffer, self->BS.fat2Start + x);
                self->buffer[(self->currFile.currentCluster & 0xFF) * 2] = nextCluster & 0xFF;
                self->buffer[((self->currFile.currentCluster & 0xFF) * 2) + 1] = y;
                if (y == x) {
                    self->buffer[z * 2] = 0xFF;
                    self->buffer[(z * 2) + 1] = 0xFF;
                    storage_write_sector(self->buffer, self->BS.fat2Start + x);
                }
                else
                {
                    storage_write_sector(self->buffer, self->BS.fat2Start + x);
                    storage_read_sector(self->buffer, self->BS.fat2Start + y);
                    self->buffer[z * 2] = 0xFF;
                    self->buffer[(z * 2) + 1] = 0xFF;
                    storage_write_sector(self->buffer, self->BS.fat2Start + y);
                }

                self->currFile.currentCluster = nextCluster;

                currSec = (self->BS.reservedSectors
                        + (self->BS.fatCopies * self->BS.sectorsPerFAT)
                        + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE)
                        + ((self->currFile.currentCluster - 2) * self->BS.sectorsPerCluster)
                        + self->BS.hiddenSectors);
            }
            storage_read_sector(self->buffer, currSec);
            for (i = 0, j = stlen - bufIndex; i < j; i++)
                self->buffer[i] = st[i + bufIndex];
            self->buffer[j] = 0x0D;
            self->buffer[j + 1] = 0x0A;
            storage_write_sector(self->buffer, currSec);
        }

        self->currFile.fileSize += (stlen + 2);

        currSec = self->firstDirSector;
        offset = -32;
        done = 0;
        storage_read_sector(self->buffer, currSec);
        while (!done) {
            offset+=32;
            if (offset == STORAGE_SECTOR_SIZE) {
                storage_read_sector(self->buffer, ++currSec);
                offset = 0;
            } 

            j = 0;
            for (int i = 0; i<8; i++)
                if (self->buffer[i + offset]!=0x20)
                    tmpFN[j++] = self->buffer[i + offset];
            tmpFN[j++] = '.';
            for (int i = 0; i<3; i++)
                if (self->buffer[i + 0x08 + offset]!=0x20)
                    tmpFN[j++] = self->buffer[i + 0x08 + offset];
            tmpFN[j] = 0x00;

            if (!strcmp(tmpFN, self->currFile.filename))
            {
                self->buffer[offset + 0x1C] = self->currFile.fileSize & 0xFF;
                self->buffer[offset + 0x1D] = (self->currFile.fileSize & 0xFF00)>>8;
                self->buffer[offset + 0x1E] = (self->currFile.fileSize & 0xFF0000)>>16;
                self->buffer[offset + 0x1F] = self->currFile.fileSize>>24;

                storage_write_sector(self->buffer, currSec);

                done = 1;
            }
        }

        return FS_NO_ERROR;
    } else if (self->currFile.fileMode == 0x00)
        return FS_ERROR_NO_FILE_OPEN;
    return FS_ERROR_WRONG_FILEMODE;
}

static void fs_close_file(struct fs_t *self) {
    self->currFile.filename[0] = 0x00;
    self->currFile.fileMode = 0x00;
}

static uint8_t fs_exists(struct fs_t *self, char *fn) {
    struct fs_directory_entry tmpDE;
    char tmpFN[13];
    uint8_t res;
    int i, j, fnlen = strlen(fn);

    for (i = 0; i < fnlen; i++)
        fn[i] = fat32_uCase(fn[i]);

    res = fs_find_first_file(self, &tmpDE);
    if (res == FS_ERROR_NO_MORE_FILES) return 0;
    else {
        for (i = j = 0; (tmpDE.filename[i] != 0x20) && (i<8); ++i)
            tmpFN[i] = tmpDE.filename[i];
        tmpFN[i++] = '.';
        while ((tmpDE.fileext[j] != 0x20) && (j<3))
            tmpFN[i++] = tmpDE.fileext[j++];
        tmpFN[i] = 0x00;
        if (!strcmp(tmpFN,fn))
            return 1;
        while (res == FS_NO_ERROR) {
            res = fs_find_next_file(self, &tmpDE);
            if (res == FS_NO_ERROR) {
                for (i = j = 0; (tmpDE.filename[i] != 0x20) && (i < 8); ++i)
                    tmpFN[i] = tmpDE.filename[i];
                tmpFN[i++] = '.';
                while ((tmpDE.fileext[j]!=0x20) && (j<3))
                    tmpFN[i++] = tmpDE.fileext[j++];
                tmpFN[i] = 0x00;
                if (!strcmp(tmpFN,fn))
                    return 1;
            }
        }
    }
    return 0;
}

static uint8_t fs_rename(struct fs_t *self, char *fn1, char *fn2) {
    unsigned long currSec = self->firstDirSector;
    uint16_t offset = -32;
    char tmpFN[13];
    int i, j, fn1len = strlen(fn1), fn2len = strlen(fn2);
    uint8_t done = 0;

    for (i = 0; i < fn1len; i++)
        fn1[i] = fat32_uCase(fn1[i]);

    for (i = 0; i < fn2len; i++)
        if (!fat32_valid_char(fn2[i] = fat32_uCase(fn2[i])))
            return 0;

    if (fs_exists(self, fn1)) {
        storage_read_sector(self->buffer, currSec);
        while (!done)
        {
            offset+=32;
            if (offset == STORAGE_SECTOR_SIZE) {
                storage_read_sector(self->buffer, ++currSec);
                offset = 0;
            } 

            j = 0;
            for (int i = 0; i<8; i++)
                if (self->buffer[i + offset] != 0x20)
                    tmpFN[j++] = self->buffer[i + offset];
            tmpFN[j++] = '.';
            for (int i = 0; i<3; i++)
                if (self->buffer[i + 0x08 + offset] != 0x20)
                    tmpFN[j++] = self->buffer[i + 0x08 + offset];
            tmpFN[j] = 0x00;
            if (!strcmp(tmpFN, fn1)) {
                for (int i = 0; i < 11; i++)
                    self->buffer[i + offset] = 0x20;
                j = 0;
                for (int i = 0; i < fn2len; i++)
                    if (fn2[i] == '.') j = 8;
                    else self->buffer[(j++) + offset] = fn2[i];
                storage_write_sector(self->buffer, currSec);
                done = 1;
            }
        }
        return 1;
    }
    return 0;
}

static uint8_t fs_del_file(struct fs_t *self, char *fn) {
    unsigned long currSec = self->firstDirSector;
    uint16_t firstCluster, currCluster, nextCluster;
    uint16_t offset = -32;
    char tmpFN[13];
    int j, fnlen = strlen(fn);
    uint8_t done = 0;

    for (int i = 0; i < fnlen; i++)
        fn[i] = fat32_uCase(fn[i]);

    if (fs_exists(self, fn)) {
        storage_read_sector(self->buffer, currSec);
        while (!done)
        {
            offset += 32;
            if (offset == STORAGE_SECTOR_SIZE) {
                storage_read_sector(self->buffer, ++currSec);
                offset = 0;
            } 

            j = 0;
            for (int i = 0; i<8; i++)
                if (self->buffer[i + offset] != 0x20)
                    tmpFN[j++] = self->buffer[i + offset];
            tmpFN[j++] = '.';
            for (int i = 0; i<3; i++)
                if (self->buffer[i + 0x08 + offset]!=0x20)
                    tmpFN[j++] = self->buffer[i + 0x08 + offset];
            tmpFN[j] = 0x00;
            if (!strcmp(tmpFN, fn)) {
                self->buffer[offset] = 0xE5;
                firstCluster = (uint16_t)(self->buffer[0x1A + offset])
                    + ((uint16_t)(self->buffer[0x1B + offset])<<8);
                storage_write_sector(self->buffer, currSec);

                if (firstCluster!=0) {
                    currSec = firstCluster / 256;
                    storage_read_sector(self->buffer, self->BS.fat1Start + currSec);
                    currCluster = firstCluster;
                    nextCluster = 0;
                    while (nextCluster!=0xFFFF)
                    {
                        nextCluster = self->buffer[(currCluster % 256) * 2]
                            + (self->buffer[((currCluster % 256) * 2) + 1]<<8);
                        self->buffer[(currCluster % 256) * 2] = 0;
                        self->buffer[((currCluster % 256) * 2) + 1] = 0;
                        if (((currCluster / 256) != (nextCluster / 256)) && (nextCluster!=0xFFFF))
                        {
                            storage_write_sector(self->buffer, self->BS.fat1Start + currSec);
                            currSec = nextCluster / 256;
                            storage_read_sector(self->buffer, self->BS.fat1Start + currSec);

                        }
                        currCluster = nextCluster;
                    }
                    storage_write_sector(self->buffer, self->BS.fat1Start + currSec);

                    currSec = firstCluster / 256;
                    storage_read_sector(self->buffer, self->BS.fat2Start + currSec);
                    currCluster = firstCluster;
                    nextCluster = 0;
                    while (nextCluster != 0xFFFF)
                    {
                        nextCluster = self->buffer[(currCluster % 256) * 2]
                            + (self->buffer[((currCluster % 256) * 2) + 1]<<8);
                        self->buffer[(currCluster % 256) * 2] = 0;
                        self->buffer[((currCluster % 256) * 2) + 1] = 0;
                        if (((currCluster / 256) != (nextCluster / 256)) && (nextCluster!=0xFFFF))
                        {
                            storage_write_sector(self->buffer, self->BS.fat2Start + currSec);
                            currSec = nextCluster / 256;
                            storage_read_sector(self->buffer, self->BS.fat2Start + currSec);

                        }
                        currCluster = nextCluster;
                    }
                    storage_write_sector(self->buffer, self->BS.fat2Start + currSec);
                }

                done = 1;
            }
        }
        return 1;
    }
    return 0;
}

static uint8_t fs_create(struct fs_t *self, char *fn) {
    unsigned long currSec;
    uint16_t offset = 0;
    uint8_t done = 0;
    int j, fnlen = strlen(fn);

    for (int i = 0; i < fnlen; i++)
        if (!fat32_valid_char(fn[i] = fat32_uCase(fn[i])))
            return 0;

    if (!fs_exists(self, fn)) {
        currSec = self->firstDirSector;
        storage_read_sector(self->buffer, currSec);
        offset = -32;
        while (!done) {
            offset += 32;
            if (offset == STORAGE_SECTOR_SIZE) {
                storage_read_sector(self->buffer, ++currSec);
                offset = 0;
            } 

            if ((self->buffer[offset] == 0x00) || (self->buffer[offset] == 0xE5)) {
                for (int i = 0; i < 11; i++)
                    self->buffer[i + offset] = 0x20;
                j = 0;
                for (int i = 0; i < fnlen; i++)
                    if (fn[i] == '.') j = 8;
                    else self->buffer[(j++) + offset] = fn[i];

                for (int i = 0x0b; i < 0x20; i++)
                    self->buffer[offset + i] = 0;
                self->buffer[offset + 0x0b] = 0x20;
                self->buffer[offset + 0x0f] = 0x60;
                self->buffer[offset + 0x10] = 0x21;
                self->buffer[offset + 0x11] = 0x3E;
                self->buffer[offset + 0x12] = 0x21;
                self->buffer[offset + 0x13] = 0x3E;
                self->buffer[offset + 0x17] = 0x60;
                self->buffer[offset + 0x18] = 0x21;
                self->buffer[offset + 0x19] = 0x3E;

                storage_write_sector(self->buffer, currSec);

                done = 1;
            }
        }
        return 1;
    } else return 0;
}

static uint16_t fat32_find_next_cluster(struct fs_t *self, uint16_t cc) {
    uint16_t nc;
    storage_read_sector(self->buffer, self->BS.fat1Start + ((int)(cc / 256)));
    nc = self->buffer[(cc % 256) * 2] + (self->buffer[((cc % 256) * 2) + 1] << 8);
    return nc;
}

static char fat32_uCase(char c) {
    if ((c >= 'a') && (c <= 'z')) return (c - 0x20);
    else return c;
}

static uint8_t fat32_valid_char(char c) {
    const static char valid[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$&'()-@^_`{}~.";

    for (int i = 0; i < 52; i++)
        if (c == valid[i])
            return 1;
    return 0;
}

static uint16_t fat32_find_free_cluster(struct fs_t *self) {
    unsigned long currSec = 0;
    uint16_t firstFreeCluster = 0;
    uint16_t offset = 0;

    while ((firstFreeCluster == 0) && (currSec <= self->BS.sectorsPerFAT))
    {
        storage_read_sector(self->buffer, self->BS.fat1Start + currSec);
        while ((firstFreeCluster == 0) && (offset <= STORAGE_SECTOR_SIZE))
        {
            if ((self->buffer[offset] + (self->buffer[offset + 1] << 8)) == 0)
                firstFreeCluster = (currSec << 8) + (offset / 2);
            else offset+=2;
        }
        offset = 0;
        currSec++;
    }
    return firstFreeCluster;
}

#endif
