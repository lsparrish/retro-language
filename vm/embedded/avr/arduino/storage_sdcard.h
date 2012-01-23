#ifndef STORAGE_SDCARD_H
#define STORAGE_SDCARD_H
#ifdef STORAGE_ACTIVATED
#error "Only one storage device can be activated."
#endif
#define STORAGE_ACTIVATED

#define STORAGE_SECTOR_SIZE     512

#define CONFIG_SD_AUTO_RETRIES 5
#define SD_SUPPLY_VOLTAGE (1L<<18)

#define STA_NOINIT 0x01 	/* Drive not initialized */
#define STA_NODISK 0x02 	/* No medium in the drive */
#define STA_PROTECT 0x04 	/* Write protected */

#define GO_IDLE_STATE           0
#define SEND_OP_COND            1
#define SWITCH_FUNC             6
#define SEND_IF_COND            8
#define SEND_CSD                9
#define SEND_CID               10
#define STOP_TRANSMISSION      12
#define SEND_STATUS            13
#define SET_BLOCKLEN           16
#define READ_SINGLE_BLOCK      17
#define READ_MULTIPLE_BLOCK    18
#define WRITE_BLOCK            24
#define WRITE_MULTIPLE_BLOCK   25
#define PROGRAM_CSD            27
#define SET_WRITE_PROT         28
#define CLR_WRITE_PROT         29
#define SEND_WRITE_PROT        30
#define ERASE_WR_BLK_STAR_ADDR 32
#define ERASE_WR_BLK_END_ADDR  33
#define ERASE                  38
#define LOCK_UNLOCK            42
#define APP_CMD                55
#define GEN_CMD                56
#define READ_OCR               58
#define CRC_ON_OFF             59

#define SD_STATUS                 13
#define SD_SEND_NUM_WR_BLOCKS     22
#define SD_SET_WR_BLK_ERASE_COUNT 23
#define SD_SEND_OP_COND           41
#define SD_SET_CLR_CARD_DETECT    42
#define SD_SEND_SCR               51

#define STATUS_IN_IDLE          1
#define STATUS_ERASE_RESET      2
#define STATUS_ILLEGAL_COMMAND  4
#define STATUS_CRC_ERROR        8
#define STATUS_ERASE_SEQ_ERROR 16
#define STATUS_ADDRESS_ERROR   32
#define STATUS_PARAMETER_ERROR 64

typedef enum {
  RES_OK = 0,		/* 0: Successful */
  RES_ERROR,		/* 1: R/W Error */
  RES_WRPRT,		/* 2: Write Protected */
  RES_NOTRDY,		/* 3: Not Ready */
  RES_PARERR		/* 4: Invalid Parameter */
} 
DRESULT;

enum diskstates
{ 
  DISK_CHANGED = 0,
  DISK_REMOVED,
  DISK_OK,
  DISK_ERROR
};

static volatile enum diskstates disk_state;

static char sd_response(uint8_t expected) {
    unsigned short count = 0x0FFF;
    while ((spi_transfer_byte(0xFF) != expected) && count--);
    return (count != 0);
}

static char sd_wait_write_finish(void) {
    unsigned short count = 0xFFFF;
    while ((spi_transfer_byte(0xFF) == 0) && count--);
    return (count != 0);
}

static void sd_deselect_card(void) {
    SDCARD_PORT |= (1<<SDCARD_SS);
    spi_transfer_byte(0xff);
}

static uint8_t crc7update(uint8_t crc, uint8_t data) {
    uint8_t i;
    uint8_t bit;
    uint8_t c;

    c = data;
    for (i = 0x80; i > 0; i >>= 1) {
        bit = crc & 0x40;
        if (c & i) {
            bit = !bit;
        }
        crc <<= 1;
        if (bit) {
            crc ^= 0x09;
        }
    }
    crc &= 0x7f;
    return crc & 0x7f;
}

static int sd_send_command(uint8_t command, uint32_t parameter, uint8_t deselect) {
    union { unsigned long l; unsigned char c[4]; } long2char;
    uint8_t i, crc, errorcount;
    uint16_t counter;

    long2char.l = parameter;
    crc = crc7update(0  , 0x40+command);
    crc = crc7update(crc, long2char.c[3]);
    crc = crc7update(crc, long2char.c[2]);
    crc = crc7update(crc, long2char.c[1]);
    crc = crc7update(crc, long2char.c[0]);
    crc = (crc << 1) | 1;

    errorcount = 0;
    while (errorcount < CONFIG_SD_AUTO_RETRIES) {
        SDCARD_PORT &= ~(1<<SDCARD_SS);
        spi_transfer_byte(0x40+command);
        spi_transfer_long(parameter);
        spi_transfer_byte(crc);
        counter = 0;
        do { i = spi_transfer_byte(0xff); counter++; } 
        while (i & 0x80 && counter < 0x1000);
        if (deselect && (i & STATUS_CRC_ERROR)) {
            sd_deselect_card();
            errorcount++;
            continue;
        }

        if (deselect) sd_deselect_card();
        break;
    }

    return i;
}

static uint8_t storage_initialize(uint8_t speed) {
    uint8_t  i;
    uint16_t counter;
    uint32_t answer;

    disk_state = DISK_ERROR;

    SDCARD_PORT |= (1<<SDCARD_SS);

    for (i=0; i<10; i++) spi_transfer_byte(0xFF);

    i = sd_send_command(GO_IDLE_STATE, 0, 1);
    if (i != 1) return STA_NOINIT | STA_NODISK;

    counter = 0xffff;
    do { i = sd_send_command(READ_OCR, 0, 0); } 
    while (i > 1 && counter-- > 0);

    if (counter > 0) {
        answer = spi_transfer_long(0);
        if (!(answer & SD_SUPPLY_VOLTAGE)) {
            sd_deselect_card();
            return STA_NOINIT | STA_NODISK;
        }
    }

    counter = 0xffff;
    do { i = sd_send_command(SEND_OP_COND, 1L<<30, 1); counter--; } 
    while (i != 0 && counter > 0);

    if (counter==0) return STA_NOINIT | STA_NODISK;
    i = sd_send_command(SET_BLOCKLEN, 512, 1);
    if (i != 0) return STA_NOINIT | STA_NODISK;
    disk_state = DISK_OK;
    return RES_OK;
}

static uint8_t storage_read_sector(uint8_t *buffer, uint32_t sector)
{
    uint8_t res,tmp,errorcount;
    uint16_t crc,recvcrc;

    errorcount = 0;
    while (errorcount < CONFIG_SD_AUTO_RETRIES)
    {
        res = sd_send_command(READ_SINGLE_BLOCK, (sector) << 9, 0);

        if (res != 0)
        {
            SDCARD_PORT |= (1 << SDCARD_SS);
            disk_state = DISK_ERROR;
            return RES_ERROR;
        }

        // Wait for data token
        if (!sd_response(0xFE))
        {
            SDCARD_PORT |= (1 << SDCARD_SS);
            disk_state = DISK_ERROR;
            return RES_ERROR;
        }

        uint16_t i;

        // Get data
        crc = 0;
        for (i=0; i<512; i++)
        {
            tmp = spi_transfer_byte(0xff);
            *(buffer++) = tmp;
        }

        // Check CRC
        recvcrc = (spi_transfer_byte(0xFF) << 8) + spi_transfer_byte(0xFF);

        break;
    }
    sd_deselect_card();

    if (errorcount >= CONFIG_SD_AUTO_RETRIES) return RES_ERROR;

    return RES_OK;
}

static uint8_t storage_write_sector(uint8_t *buffer, uint32_t sector)
{
    uint8_t res,errorcount,status;
    uint16_t crc;

    errorcount = 0;
    while (errorcount < CONFIG_SD_AUTO_RETRIES)
    {
        res = sd_send_command(WRITE_BLOCK, (sector)<<9, 0);

        if (res != 0)
        {
            SDCARD_PORT |= (1<<SDCARD_SS);
            disk_state = DISK_ERROR;
            return RES_ERROR;
        }

        // Send data token
        spi_transfer_byte(0xFE);

        uint16_t i;
        uint8_t *oldbuffer = buffer;

        // Send data
        crc = 0;
        for (i=0; i<512; i++)
        {
            spi_transfer_byte(*(buffer++));
        }

        // Send CRC
        spi_transfer_byte(crc >> 8);
        spi_transfer_byte(crc & 0xff);

        // Get and check status feedback
        status = spi_transfer_byte(0xFF);

        // Retry if neccessary
        if ((status & 0x0F) != 0x05)
        {
            sd_deselect_card();
            errorcount++;
            buffer = oldbuffer;
            continue;
        }

        // Wait for write finish
        if (!sd_wait_write_finish())
        {
            SDCARD_PORT |= (1<<SDCARD_SS);
            disk_state = DISK_ERROR;
            return RES_ERROR;
        }
        break;
    }
    return RES_OK;
}

#endif
