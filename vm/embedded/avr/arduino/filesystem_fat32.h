#ifndef FILESYSTEM_FS_H
#define FILESYSTEM_FS_H

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

    fs_directory_entry DE;
    unsigned long firstDirSector;
    uint8_t buffer[STORAGE_SECTOR_SIZE]; // the buffer cannot be any smaller, SD cards are read/written in blocks of 512 uint8_ts

    struct {
        char filename[13];
        uint16_t currentCluster;
        uint32_t fileSize;
        uint32_t currentPos;
        uint8_t fileMode;
    } currFile;

    int	DEcnt;
};

static uint8_t fs_initialize(struct fs_t *self) {
    if (RES_OK == storage_read_sector(self->buffer, 0)) {
        if ((self->buffer[0x01FE] == 0x55) && (self->buffer[0x01FF] == 0xAA)) {
            self->MBR.part1Type = self->buffer[450];
            self->MBR.part1Start = uint16_t(self->buffer[454]) 
                + (uint16_t(self->buffer[455])<<8)
                + (uint32_t(self->buffer[456])<<16)
                + (uint32_t(self->buffer[457])<<24);
            self->MBR.part1Size = uint16_t(self->buffer[458])
                + (uint16_t(self->buffer[459])<<8)
                + (uint32_t(self->buffer[460])<<16)
                + (uint32_t(self->buffer[461])<<24);
        }
        else return ERROR_MBR_SIGNATURE;
    } else return ERROR_MBR_READ_ERROR;

    if ((MBR.part1Type!=0x04) && (MBR.part1Type!=0x06) \
            && (MBR.part1Type!=0x86))
        return ERROR_MBR_INVALID_FS;

    if (RES_OK == storage_read_sector(self->buffer, self->MBR.part1Start)) {
        if ((self->buffer[0x01FE] == 0x55)
                && (self->buffer[0x01FF] == 0xAA))
        {
            self->BS.sectorsPerCluster = self->buffer[0x0D];
            self->BS.reservedSectors = uint16_t(self->buffer[0x0E])
                + (uint16_t(self->buffer[0x0F])<<8);
            self->BS.fatCopies = self->buffer[0x10];
            self->BS.rootDirectoryEntries = uint16_t(self->buffer[0x11])
                + (uint16_t(self->buffer[0x12])<<8);
            self->BS.totalFilesystemSectors = uint16_t(self->buffer[0x13])
                + (uint16_t(self->buffer[0x14])<<8);
            if (self->BS.totalFilesystemSectors == 0)
                self->BS.totalFilesystemSectors = uint16_t(self->buffer[0x20])
                    + (uint16_t(self->buffer[0x21])<<8)
                    + (uint32_t(self->buffer[0x22])<<16)
                    + (uint32_t(self->buffer[0x23])<<24);
            self->BS.sectorsPerFAT = uint16_t(self->buffer[0x16])
                + (uint16_t(self->buffer[0x17])<<8);
            self->BS.hiddenSectors = uint16_t(self->buffer[0x1C])
                + (uint16_t(self->buffer[0x1D])<<8)
                + (uint32_t(self->buffer[0x1E])<<16)
                + (uint32_t(self->buffer[0x1F])<<24);
            self->BS.partitionSerialNum = uint16_t(self->buffer[0x27])
                + (uint16_t(self->buffer[0x28])<<8)
                + (uint32_t(self->buffer[0x29])<<16)
                + (uint32_t(self->buffer[0x2A])<<24);
            self->firstDirSector = self->MBR.part1Start
                + self->BS.reservedSectors
                + (self->BS.fatCopies * self->BS.sectorsPerFAT);
            self->BS.fat1Start = self->MBR.part1Start 
                + self->BS.reservedSectors;
            self->BS.fat2Start = self->BS.fat1Start 
                + self->BS.sectorsPerFAT;
            self->BS.partitionSize =
                float((self->MBR.part1Size * STORAGE_SECTOR_SIZE) / float(1048576));
        }
        else return ERROR_BOOTSEC_SIGNATURE;
    }
    else return ERROR_BOOTSEC_READ_ERROR;
    return NO_ERROR;
}

static uint8_t fs_find_first_file(struct fs_t *self, fs_directory_entry *DE) {
    unsigned long currSec = firstDirSector;
    uint16_t offset = 0;

    self->DEcnt = 0;
    storage_read_sector(self->buffer, currSec);

    if (self->buffer[0] == 0x00) return ERROR_NO_MORE_FILES;
    else {
        while ((self->buffer[offset + 0x0B] & 0x08) \
                || (self->buffer[offset + 0x0B] & 0x10) \
                || (self->buffer[offset] == 0xE5))
        {
            offset+=32;
            self->DEcnt++;
            if (offset == STORAGE_SECTOR_SIZE) {
                currSec++;
                storage_read_sector(self->buffer, currSec);
                offset = 0;
            } 
            if (self->buffer[offset] == 0x00)
                return ERROR_NO_MORE_FILES;
        }

        for (int i = 0; i<8; i++)
            DE->filename[i] = self->buffer[i + offset];
        for (int i = 0; i<3; i++)
            DE->fileext[i] = self->buffer[i + 0x08 + offset];

        DE->filename[8] = 0;
        DE->fileext[3] = 0;
        DE->attributes = self->buffer[0x0B + offset];
        DE->time = uint16_t(self->buffer[0x0E + offset])
            + (uint16_t(self->buffer[0x0F + offset])<<8);
        DE->date = uint16_t(self->buffer[0x10 + offset])
            + (uint16_t(self->buffer[0x11 + offset])<<8);
        DE->startCluster = uint16_t(self->buffer[0x1A + offset])
            + (uint16_t(self->buffer[0x1B + offset])<<8);
        DE->fileSize = uint16_t(self->buffer[offset + 0x1C])
            | (uint16_t(self->buffer[offset + 0x1D])<<8)
            | (uint32_t(self->buffer[offset + 0x1E])<<16)
            | (uint32_t(self->buffer[offset + 0x1F])<<24);
        self->DEcnt++;
    }
    return NO_ERROR;
}

static uint8_t fs_find_next_file(struct fs_t *self, fs_directory_entry *DE) {
    unsigned long currSec = self->firstDirSector;
    uint16_t offset = self->DEcnt * 32;

    while (offset >= STORAGE_SECTOR_SIZE)
    {
        currSec++;
        offset -= STORAGE_SECTOR_SIZE;
    }

    storage_read_sector(self->buffer, currSec);

    if (self->buffer[offset] == 0x00)
        return ERROR_NO_MORE_FILES;
    else {
        while ((self->buffer[offset + 0x0B] & 0x08) \
                || (self->buffer[offset + 0x0B] & 0x10) \
                || (self->buffer[offset] == 0xE5))
        {
            offset+=32;
            DEcnt++;
            if (offset == STORAGE_SECTOR_SIZE) {
                currSec++;
                storage_read_sector(self->buffer, currSec);
                offset = 0;
            } 
            if (self->buffer[offset] == 0x00)
                return ERROR_NO_MORE_FILES;
        }

        for (int i = 0; i<8; i++)
            DE->filename[i] = self->buffer[i + offset];
        for (int i = 0; i<3; i++)
            DE->fileext[i] = self->buffer[i + 0x08 + offset];
        DE->filename[8] = 0;
        DE->fileext[3] = 0;
        DE->attributes = self->buffer[0x0B + offset];
        DE->time = uint16_t(self->buffer[0x0E + offset]) 
            + (uint16_t(self->buffer[0x0F + offset])<<8);
        DE->date = uint16_t(self->buffer[0x10 + offset]) 
            + (uint16_t(self->buffer[0x11 + offset])<<8);
        DE->startCluster = uint16_t(self->buffer[0x1A + offset]) 
            + (uint16_t(self->buffer[0x1B + offset])<<8);
        DE->fileSize = uint16_t(self->buffer[offset + 0x1C]) 
            | (uint16_t(self->buffer[offset + 0x1D])<<8) 
            | (uint32_t(self->buffer[offset + 0x1E])<<16) 
            | (uint32_t(self->buffer[offset + 0x1F])<<24);
        self->DEcnt++;

        return NO_ERROR;
    }
}

static uint8_t fs_open_file(struct fat32_t *self, char *fn, uint8_t mode) {
    fs_directory_entry tmpDE;
    char tmpFN[13];
    byte res;
    int i, j;

    if (self->currFile.filename[0]!=0x00)
        return ERROR_ANOTHER_FILE_OPEN;

    for (i = 0; i < strlen(fn); i++)
        fn[i] = fat32_uCase(fn[i]);

    res = fs_find_first_file(&tmpDE);
    if (res == ERROR_NO_MORE_FILES) return ERROR_FILE_NOT_FOUND;
    else {
        i = j = 0;
        while ((tmpDE.filename[i]!=0x20) && (i<8))
            tmpFN[i] = tmpDE.filename[i++];
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
            return NO_ERROR;
        }
        while (res == NO_ERROR) {
            res = file.findNextFile(&tmpDE);
            if (res == NO_ERROR)
            {
                i = j = 0;
                while ((tmpDE.filename[i]!=0x20) && (i<8))
                    tmpFN[i] = tmpDE.filename[i++];
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
                    return NO_ERROR;
                }
            }
        }
    }
    return ERROR_FILE_NOT_FOUND;
}

static uint16_t fs_read_binary(struct fat32_t *self) {
    uint32_t sec;

    if (self->currFile.fileMode == FILEMODE_BINARY) {
        if ((self->currFile.currentPos == 0) && (self->currFile.currentCluster == 0))
            return FILE_IS_EMPTY;
        if (((self->currFile.currentPos % self->BS.sectorsPerCluster) == 0) \
                && (self->currFile.currentPos>0))
            self->currFile.currentCluster =
                fat32_find_next_cluster(self->currFile.currentCluster);
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
    else if (self->currFile.fileMode == 0x00) return ERROR_NO_FILE_OPEN;
    else return ERROR_WRONG_FILEMODE;
}

static uint16_t fs_read_ln(struct fat32_t *self, char *st, int buf_size) {
    uint32_t sec;
    int bufIndex = 0;

    for (int i = 0; i <= bufSize; i++) st[i] = 0;
    if (self->currFile.fileMode == FILEMODE_TEXT_READ) {
        if ((self->currFile.currentPos == 0) && (self->currFile.currentCluster == 0))
            return FILE_IS_EMPTY;
        sec = (self->BS.reservedSectors 
                + (self->BS.fatCopies * self->BS.sectorsPerFAT) 
                + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE) 
                + ((self->currFile.currentCluster-2) * self->BS.sectorsPerCluster) 
                + self->BS.hiddenSectors) 
            + ((self->currFile.currentPos / STORAGE_SECTOR_SIZE) % self->BS.sectorsPerCluster);
        storage_read_sector(self->buffer, sec);
        while ((self->currFile.currentPos<self->currFile.fileSize) 
                && (self->buffer[self->currFile.currentPos % STORAGE_SECTOR_SIZE] != 10)
                && (self->buffer[self->currFile.currentPos % STORAGE_SECTOR_SIZE] != 13)
                && (bufIndex < bufSize))
        {
            st[bufIndex] = self->buffer[self->currFile.currentPos % STORAGE_SECTOR_SIZE];
            bufIndex++;
            self->currFile.currentPos++;
            if ((self->currFile.currentPos % STORAGE_SECTOR_SIZE) == 0)
            {
                sec++;
                if (((self->currFile.currentPos / STORAGE_SECTOR_SIZE) % self->BS.sectorsPerCluster) == 0)
                {
                    self->currFile.currentCluster = fat32_find_next_cluster(self->currFile.currentCluster);
                    sec = (self->BS.reservedSectors
                            + (self->BS.fatCopies * self->BS.sectorsPerFAT)
                            + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE)
                            + ((self->currFile.currentCluster - 2) * self->BS.sectorsPerCluster)
                            + self->BS.hiddenSectors);
                }
                storage_read_sector(self->buffer, sec);
            }
        }
        if (self->currFile.currentPos>=self->currFile.fileSize) return EOF;
        else if ((self->buffer[(self->currFile.currentPos % STORAGE_SECTOR_SIZE)] == 13)
                && (self->buffer[(self->currFile.currentPos % STORAGE_SECTOR_SIZE) + 1] == 10))
        {
            self->currFile.currentPos += 2;
            return bufIndex;
        }
        else if ((self->buffer[(self->currFile.currentPos % STORAGE_SECTOR_SIZE)] == 13)
                || (self->buffer[(self->currFile.currentPos % STORAGE_SECTOR_SIZE)] == 10))
        {
            self->currFile.currentPos++;
            return bufIndex;
        }
        else return BUFFER_OVERFLOW;
    }
    else if (self->currFile.fileMode == 0x00) return ERROR_NO_FILE_OPEN;
    else return ERROR_WRONG_FILEMODE;
}

static uint16_t fs_write_ln(struct fat32_t *self, char *st) {
    unsigned long currSec = firstDirSector;
    uint16_t nextCluster = 0;
    uint16_t offset = -32;
    uint32_t sec;
    char tmpFN[13];
    int i, j;
    int bufIndex = 0;
    uint8_t done = false;

    if (self->currFile.fileMode == FILEMODE_TEXT_WRITE) {
        if (self->currFile.currentCluster == 0) {
            self->currFile.currentCluster = fat32_find_free_cluster();

            storage_read_sector(self->buffer, currSec);
            while (!done) {
                offset += 32;
                if (offset == STORAGE_SECTOR_SIZE) {
                    currSec++;
                    storage_read_sector(self->buffer, currSec);
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

                if (!strcmp(tmpFN, self->currFile.filename))
                {
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

                    done = true;
                }
            }
        }

        if ((((self->currFile.fileSize % STORAGE_SECTOR_SIZE) + strlen(st))<=510)
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
            for (int i = 0; i<strlen(st); i++)
                self->buffer[(self->currFile.fileSize%STORAGE_SECTOR_SIZE) + i] = st[i];
            self->buffer[(self->currFile.fileSize%STORAGE_SECTOR_SIZE) + strlen(st)] = 0x0D;
            self->buffer[(self->currFile.fileSize%STORAGE_SECTOR_SIZE) + strlen(st) + 1] = 0x0A;
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
                for (int i = 0; i < (STORAGE_SECTOR_SIZE-(self->currFile.fileSize%STORAGE_SECTOR_SIZE)); i++)
                {
                    self->buffer[(self->currFile.fileSize%STORAGE_SECTOR_SIZE) + i] = st[i];
                    bufIndex++;
                }
                storage_write_sector(self->buffer, currSec);
                currSec++;
            } else bufIndex = 0;

            if (((currSec-(self->BS.reservedSectors
                                + (self->BS.fatCopies * self->BS.sectorsPerFAT)
                                + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE)
                                + self->BS.hiddenSectors))
                        % self->BS.sectorsPerCluster) == 0)
            {
                nextCluster = fat32_find_free_cluster();

                storage_read_sector(self->buffer, self->BS.fat1Start + (self->currFile.currentCluster>>8));
                self->buffer[(self->currFile.currentCluster & 0xFF) * 2] = nextCluster & 0xFF;
                self->buffer[((self->currFile.currentCluster & 0xFF) * 2) + 1] = nextCluster>>8;
                if ((nextCluster>>8) == (self->currFile.currentCluster>>8))
                {
                    self->buffer[(nextCluster & 0xFF) * 2] = 0xFF;
                    self->buffer[((nextCluster & 0xFF) * 2) + 1] = 0xFF;
                    storage_write_sector(self->buffer, self->BS.fat1Start + (self->currFile.currentCluster>>8));
                }
                else
                {
                    storage_write_sector(self->buffer, self->BS.fat1Start + (self->currFile.currentCluster>>8));
                    storage_read_sector(self->buffer, self->BS.fat1Start + (nextCluster>>8));
                    self->buffer[(nextCluster & 0xFF) * 2] = 0xFF;
                    self->buffer[((nextCluster & 0xFF) * 2) + 1] = 0xFF;
                    storage_write_sector(self->buffer, self->BS.fat1Start + (nextCluster>>8));
                }

                storage_read_sector(self->buffer, self->BS.fat2Start + (self->currFile.currentCluster>>8));
                self->buffer[(self->currFile.currentCluster & 0xFF) * 2] = nextCluster & 0xFF;
                self->buffer[((self->currFile.currentCluster & 0xFF) * 2) + 1] = nextCluster>>8;
                if ((nextCluster>>8) == (self->currFile.currentCluster>>8))
                {
                    self->buffer[(nextCluster & 0xFF) * 2] = 0xFF;
                    self->buffer[((nextCluster & 0xFF) * 2) + 1] = 0xFF;
                    storage_write_sector(self->buffer, self->BS.fat2Start + (self->currFile.currentCluster>>8));
                }
                else
                {
                    storage_write_sector(self->buffer, self->BS.fat2Start + (self->currFile.currentCluster>>8));
                    storage_read_sector(self->buffer, self->BS.fat2Start + (nextCluster>>8));
                    self->buffer[(nextCluster & 0xFF) * 2] = 0xFF;
                    self->buffer[((nextCluster & 0xFF) * 2) + 1] = 0xFF;
                    storage_write_sector(self->buffer, self->BS.fat2Start + (nextCluster>>8));
                }

                self->currFile.currentCluster = nextCluster;

                currSec = (self->BS.reservedSectors
                        + (self->BS.fatCopies * self->BS.sectorsPerFAT)
                        + ((self->BS.rootDirectoryEntries * 32) / STORAGE_SECTOR_SIZE)
                        + ((self->currFile.currentCluster-2) * self->BS.sectorsPerCluster)
                        + self->BS.hiddenSectors);
            }
            storage_read_sector(self->buffer, currSec);
            for (int i = 0; i<strlen(st)-bufIndex; i++)
                self->buffer[i] = st[i + bufIndex];
            self->buffer[strlen(st)-bufIndex] = 0x0D;
            self->buffer[strlen(st)-bufIndex + 1] = 0x0A;
            storage_write_sector(self->buffer, currSec);


        }

        self->currFile.fileSize+=(strlen(st) + 2);

        currSec = firstDirSector;
        offset = -32;
        done = false;
        storage_read_sector(self->buffer, currSec);
        while (!done)
        {
            offset+=32;
            if (offset == STORAGE_SECTOR_SIZE)
            {
                currSec++;
                storage_read_sector(self->buffer, currSec);
                offset = 0;
            } 

            j = 0;
            for (int i = 0; i<8; i++)
            {
                if (self->buffer[i + offset]!=0x20)
                {
                    tmpFN[j] = self->buffer[i + offset];
                    j++;
                }
            }
            tmpFN[j] = '.';
            j++;
            for (int i = 0; i<3; i++)
            {
                if (self->buffer[i + 0x08 + offset]!=0x20)
                {
                    tmpFN[j] = self->buffer[i + 0x08 + offset];
                    j++;
                }
            }
            tmpFN[j] = 0x00;

            if (!strcmp(tmpFN, self->currFile.filename))
            {
                self->buffer[offset + 0x1C] = self->currFile.fileSize & 0xFF;
                self->buffer[offset + 0x1D] = (self->currFile.fileSize & 0xFF00)>>8;
                self->buffer[offset + 0x1E] = (self->currFile.fileSize & 0xFF0000)>>16;
                self->buffer[offset + 0x1F] = self->currFile.fileSize>>24;

                storage_write_sector(self->buffer, currSec);

                done = true;
            }
        }

        return NO_ERROR;
    } else if (self->currFile.fileMode == 0x00) return ERROR_NO_FILE_OPEN;
    else return ERROR_WRONG_FILEMODE;
}

static void fs_close_file(struct fat32_t *self, void) {
    self->currFile.filename[0] = 0x00;
    self->currFile.fileMode = 0x00;
}

static uint8_t fs_exists(struct fat32_t *self, char *fn) {
    _directory_entry tmpDE;
    char tmpFN[13];
    byte res;
    int i, j;

    for (i = 0; i<strlen(fn); i++)
        fn[i] = fat32_uCase(fn[i]);

    res = findFirstFile(&tmpDE);
    if (res == ERROR_NO_MORE_FILES) return false;
    else {
        i = j = 0;
        while ((tmpDE.filename[i]!=0x20) && (i<8))
            tmpFN[i] = tmpDE.filename[i++];
        tmpFN[i++] = '.';
        while ((tmpDE.fileext[j]!=0x20) && (j<3))
            tmpFN[i++] = tmpDE.fileext[j++];
        tmpFN[i] = 0x00;
        if (!strcmp(tmpFN,fn))
            return true;
        while (res == NO_ERROR) {
            res = file.findNextFile(&tmpDE);
            if (res == NO_ERROR) {
                i = j = 0;
                while ((tmpDE.filename[i]!=0x20) && (i<8))
                    tmpFN[i] = tmpDE.filename[i++];
                tmpFN[i++] = '.';
                while ((tmpDE.fileext[j]!=0x20) && (j<3))
                    tmpFN[i++] = tmpDE.fileext[j++];
                tmpFN[i] = 0x00;
                if (!strcmp(tmpFN,fn))
                    return true;
            }
        }
    }
    return false;
}

static uint8_t fs_rename(struct fat32_t *self, char *fn1, char *fn2) {
    unsigned long currSec = firstDirSector;
    uint16_t offset = -32;
    char tmpFN[13];
    int i, j;
    uint8_t done = false;

    for (i = 0; i<strlen(fn1); i++)
        fn1[i] = fat32_uCase(fn1[i]);

    for (i = 0; i<strlen(fn2); i++)
        if (!fat32_valid_char(fn2[i] = fat32_uCase(fn2[i])))
            return false;

    if (exists(fn1)) {
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
                if (self->buffer[i + offset]!=0x20)
                    tmpFN[j++] = self->buffer[i + offset];
            tmpFN[j++] = '.';
            for (int i = 0; i<3; i++)
                if (self->buffer[i + 0x08 + offset]!=0x20)
                    tmpFN[j++] = self->buffer[i + 0x08 + offset];
            tmpFN[j] = 0x00;
            if (!strcmp(tmpFN, fn1)) {
                for (int i = 0; i < 11; i++)
                    self->buffer[i + offset] = 0x20;
                j = 0;
                for (int i = 0; i<strlen(fn2); i++)
                    if (fn2[i] == '.') j = 8;
                    else self->buffer[(j++) + offset] = fn2[i];
                storage_write_sector(self->buffer, currSec);
                done = true;
            }
        }
        return true;
    } else return false;
}

static uint8_t fs_del_file(struct fat32_t *self, char *fn) {
    unsigned long currSec = firstDirSector;
    uint16_t firstCluster, currCluster, nextCluster;
    uint16_t offset = -32;
    char tmpFN[13];
    int j;
    uint8_t done = false;

    for (int i = 0; i<strlen(fn); i++)
        fn[i] = fat32_uCase(fn[i]);

    if (exists(fn)) {
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
                firstCluster = uint16_t(self->buffer[0x1A + offset]) + (uint16_t(self->buffer[0x1B + offset])<<8);
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

                done = true;
            }
        }
        return true;
    } else return false;
}

static uint8_t fs_create(struct fat32_t *self, char *fn) {
    unsigned long currSec;
    uint16_t offset = 0;
    uint8_t done = false;
    int j;

    for (int i = 0; i<strlen(fn); i++)
        if (!fat32_valid_char(fn[i] = fat32_uCase(fn[i])))
            return false;

    if (!exists(fn)) {
        currSec = firstDirSector;
        storage_read_sector(self->buffer, currSec);
        offset = -32;
        while (!done) {
            offset+=32;
            if (offset == STORAGE_SECTOR_SIZE) {
                storage_read_sector(self->buffer, ++currSec);
                offset = 0;
            } 

            if ((self->buffer[offset] == 0x00) || (self->buffer[offset] == 0xE5)) {
                for (int i = 0; i<11; i++)
                    self->buffer[i + offset] = 0x20;
                j = 0;
                for (int i = 0; i<strlen(fn); i++)
                    if (fn[i] == '.') j = 8;
                    else self->buffer[(j++) + offset] = fn[i];

                for (int i = 0x0b; i<0x20; i++)
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

                done = true;
            }
        }
        return true;
    } else return false;
}

static uint16_t fat32_find_next_cluster(struct fat32_t *self, uint16_t cc) {
    uint16_t nc;
    storage_read_sector(self->buffer, self->BS.fat1Start + int(cc / 256));
    nc = self->buffer[(cc % 256) * 2] + (self->buffer[((cc % 256) * 2) + 1]<<8);
    return nc;
}

static char fat32_uCase(char c) {
    if ((c >= 'a') && (c <= 'z')) return (c - 0x20);
    else return c;
}

static uint8_t fat32_valid_char(char c) {
    const static char valid[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$&'()-@^_`{}~.";

    for (int i = 0; i<strlen(valid); i++)
        if (c == valid[i])
            return true;
    return false;
}

static uint16_t fat32_find_free_cluster(struct fat32_t *self) {
    unsigned long currSec = 0;
    uint16_t firstFreeCluster = 0;
    uint16_t offset = 0;

    while ((firstFreeCluster == 0) && (currSec<=self->BS.sectorsPerFAT))
    {
        storage_read_sector(self->buffer, self->BS.fat1Start + currSec);
        while ((firstFreeCluster == 0) && (offset<=STORAGE_SECTOR_SIZE))
        {
            if ((self->buffer[offset] + (self->buffer[offset + 1]<<8)) == 0)
                firstFreeCluster = (currSec<<8) + (offset / 2);
            else offset+=2;
        }
        offset = 0;
        currSec++;
    }
}


#endif
