/* Ngaro VM for Arduino boards ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Copyright (c) 2011 - 2012, Oleksandr Kozachuk
   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Configuration ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define CELL            int16_t
#define IMAGE_SIZE        31000
#define IMAGE_CACHE_SIZE    800
#define ADDRESSES            64
#define STACK_DEPTH          64
#define PORTS                15
#define STRING_BUFFER_SIZE   32

/* General includes ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Hash table ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define HASH_FUNCTION HASH_BER
#include "uthash.h"

/* Board specific includes ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#ifdef BOARD_mega2560
#define BAUD 38400

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/setbaud.h>

#define SPI_PORT     PORTB
#define SPI_DDR      DDRB
#define SPI_MISO     PB3
#define SPI_MOSI     PB2
#define SPI_SCK      PB1
#define SPI_SS       PB0
#define SPI_SPEED    0

#define DISPLAY_PORT PORTL
#define DISPLAY_DDR  DDRL
#define DISPLAY_SS   PL2
#define DISPLAY_DC   PL1
#define DISPLAY_RST  PL0

#define SDCARD_PORT  PORTL
#define SDCARD_DDR   DDRL
#define SDCARD_SS    PL7

#else
#ifdef BOARD_native

#include <termios.h>

#else
#error "Unknown platform or -DBOARD_<type> not defined."
#endif
#endif

/* Opcodes and functions ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
enum vm_opcode {VM_NOP, VM_LIT, VM_DUP, VM_DROP, VM_SWAP, VM_PUSH, VM_POP,
                VM_LOOP, VM_JUMP, VM_RETURN, VM_GT_JUMP, VM_LT_JUMP,
                VM_NE_JUMP,VM_EQ_JUMP, VM_FETCH, VM_STORE, VM_ADD,
                VM_SUB, VM_MUL, VM_DIVMOD, VM_AND, VM_OR, VM_XOR, VM_SHL,
                VM_SHR, VM_ZERO_EXIT, VM_INC, VM_DEC, VM_IN, VM_OUT,
                VM_WAIT };

static void console_puts(char *s);

#ifndef BOARD_native

static void spi_master_init(void);
static void spi_slave_init(void);
static uint8_t spi_transfer_byte(uint8_t data);
static CELL spi_transfer_cell(CELL data);
static uint32_t spi_transfer_long(uint32_t data);

#endif

/* Change store ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
typedef struct {
    uint8_t changed;
    CELL key;
    CELL value;
    UT_hash_handle hh;
} cell_cache_element_t;

static cell_cache_element_t *cell_cache;

/* Image operations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
static inline CELL img_get(CELL k);
static inline void img_put(CELL k, CELL v);
static void img_sync(void);

/* Board and image setup ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#ifdef BOARD_mega2560

#include "console_mega2560.h"

#ifdef IMAGE_MODE_roflash
#include "image.hex.h"
#include "eeprom_atmel.h"
#endif

#ifdef DISPLAY_nokia3110
#include "display_nokia3110.h"
#endif

#else
#ifdef BOARD_native

#include "console_native.h"

#define PROGMEM
#define prog_int32_t int32_t
#define prog_int16_t int16_t
#define pgm_read_word(x) (*(x))
#define _SFR_IO8(x) SFR_IO8[x]
static int SFR_IO8[64];

#ifdef IMAGE_MODE_roflash
#include "image.hex.h"
#include "eeprom_native.h"
#endif

#endif
#endif

#ifdef STORAGE_sdcard
#include "storage_sdcard.h"
#endif

/* Macros ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define S_DROP do { data[S_SP] = 0; if (--S_SP < 0) S_IP = IMAGE_SIZE; } while(0)
#define S_TOS  data[S_SP]
#define S_NOS  data[S_SP-1]
#define S_TORS address[S_RSP]

/* Console ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
static void console_puts(char *s) {
    for (char *x = s; *x; ++x)
        console_putc(*x);
}

/* SPI ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#ifndef BOARD_native

static void spi_master_init(void) {
    SPI_DDR &= ~(1 << SPI_MISO);
    SPI_DDR |=  (1 << SPI_MOSI);
    SPI_DDR |=  (1 << SPI_SCK);
    SPI_DDR |=  (1 << SPI_SS);
    SPI_PORT |= (1 << SPI_SS);
    SPCR = (1 << SPE) | (1 << MSTR) | SPI_SPEED;
}

static void spi_slave_init(void) {
    SPI_DDR |=  (1 << SPI_MISO);
    SPI_DDR &= ~(1 << SPI_MOSI);
    SPI_DDR &= ~(1 << SPI_SCK);
    SPI_DDR &= ~(1 << SPI_SS);
    SPI_PORT &= ~(1 << SPI_SS);
    SPCR = (1 << SPE) | SPI_SPEED;
}

static inline uint8_t spi_transfer_byte(uint8_t data) {
    SPDR = data;
    while ((SPSR & (1 << SPIF)) == 0);
    return SPDR;
}

static inline CELL spi_transfer_cell(CELL data) {
    union { CELL l; uint8_t c[sizeof(CELL)]; } d;
    d.l = data;
    for (int8_t i = sizeof(CELL) - 1; i >= 0; --i)
        d.c[i] = spi_transfer_byte(d.c[i]);
    return d.l;
}

static inline uint32_t spi_transfer_long(uint32_t data) {
    union { uint32_t l; uint8_t c[sizeof(long)]; } d;
    d.l = data;
    for (int8_t i = sizeof(uint32_t) - 1; i >= 0; --i)
        d.c[i] = spi_transfer_byte(d.c[i]);
    return d.l;
}

#endif

/* Image read and write ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#ifdef IMAGE_MODE_roflash

static inline CELL img_storage_get(CELL k) { return image_read(k); }
static inline uint8_t img_storage_put(CELL k, CELL v) { return 0; }
static void img_storage_sync(void) { }

#else
#ifdef IMAGE_MODE_rwstorage

#ifndef STORAGE_ACTIVATED
#error "Image mode 'rwstorage' is needs an enabled storage."
#endif

#define IMAGE_SECTOR_SIZE (STORAGE_SECTOR_SIZE/sizeof(CELL))

static CELL *image_sector_data;
static struct {
    uint8_t changed:1;
    uint32_t sector_num:31;
} image_sector_flags;

static inline void img_storage_load(uint32_t sec) {
    if (sec != image_sector_flags.sector_num) {
        if (image_sector_flags.changed != 0) {
            if (0 != storage_write_sector(
                        (uint8_t*) image_sector_data,
                        image_sector_flags.sector_num)) {
                console_puts("\nERROR: failed to write sector ");
#ifndef BOARD_native
                _delay_ms(1000);
#else
                sleep(10);
#endif
            } else image_sector_flags.changed = 0;
        }
        if (image_sector_flags.changed == 0) {
            if (0 != storage_read_sector((uint8_t*) image_sector_data, sec)) {
                console_puts("\nERROR: failed to read sector ");
#ifndef BOARD_native
                _delay_ms(1000);
#else
                sleep(10);
#endif
            } else image_sector_flags.sector_num = sec;
        }
    }
}

static inline CELL img_storage_get(CELL k) {
    img_storage_load(k / IMAGE_SECTOR_SIZE);
    return image_sector_data[k % IMAGE_SECTOR_SIZE];
}

static inline uint8_t img_storage_put(CELL k, CELL v) {
    img_storage_load(k / IMAGE_SECTOR_SIZE);
    image_sector_data[k % IMAGE_SECTOR_SIZE] = v;
    image_sector_flags.changed = 1;
    return 1;
}

static void img_storage_sync(void) {
    if (image_sector_flags.changed != 0)
        if (0 != storage_write_sector(
                    (uint8_t*) image_sector_data,
                    image_sector_flags.sector_num))
            console_puts("\nERROR: failed to write sector ");
}

#else
#error "Unsupported image mode."
#endif
#endif

static inline cell_cache_element_t* _cache_add(CELL key, uint8_t changed, CELL value) {
    cell_cache_element_t *elem = malloc(sizeof(cell_cache_element_t));
    elem->key = key;
    elem->changed = changed;
    elem->value = value;
    HASH_ADD(hh, cell_cache, key, sizeof(key), elem);
    return elem;
}

static inline CELL img_get(CELL k) {
    cell_cache_element_t *elem;
    HASH_FIND(hh, cell_cache, &k, sizeof(k), elem);
    if (elem == NULL) {
        CELL v = img_storage_get(k);
        elem = _cache_add(k, 0, v);
    } else {
        HASH_DELETE(hh, cell_cache, elem);
        HASH_ADD(hh, cell_cache, key, sizeof(CELL), elem);
    }
    return elem->value;
}

static inline void img_put(CELL k, CELL v) {
    cell_cache_element_t *elem;
    HASH_FIND(hh, cell_cache, &k, sizeof(k), elem);
    if (elem != NULL) {
        if (elem->value != v) {
            elem->value = v;
            elem->changed = 1;
        }
    } else {
        _cache_add(k, 1, v);
        if (HASH_COUNT(cell_cache) > IMAGE_CACHE_SIZE) {
            cell_cache_element_t *temp;
            HASH_ITER(hh, cell_cache, elem, temp) {
                if (elem->changed) {
                    if (HASH_COUNT(cell_cache) < (IMAGE_CACHE_SIZE+5)) continue;
                    if (!img_storage_put(k, v)) continue;
                }
                HASH_DEL(cell_cache, elem);
                free(elem);
                if (HASH_COUNT(cell_cache) < IMAGE_CACHE_SIZE)
                    break;
            }
        }
    }
}

static inline void img_sync() {
    cell_cache_element_t *elem, *temp;
    HASH_ITER(hh, cell_cache, elem, temp) {
        if (elem->changed) {
            img_storage_put(elem->key, elem->value);
            elem->changed = 0;
        }
    }
    img_storage_sync();
}

static inline void img_string(CELL starting, char *buffer, CELL buffer_len)
{
    CELL i = 0, j = starting;
    for(; i < buffer_len && 0 != (buffer[i] = img_get(j)); ++i, ++j);
    buffer[i] = 0;
}

/* Main ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
int main(void)
{
    size_t j; size_t t;
    register CELL S_SP = 0, S_RSP = 0, S_IP = 0;
    CELL data[STACK_DEPTH];
    CELL address[ADDRESSES];
    CELL ports[PORTS];
    CELL a, b;
#ifdef FILESYSTEM_ACTIVATED
    struct fs_t *fs;
#endif

#ifndef BOARD_native
    _delay_ms(1000);
#endif

#ifdef DISPLAY_ACTIVATED
    char string_buffer[STRING_BUFFER_SIZE+1];
#endif

    console_prepare();
    console_puts("\nInitialize Ngaro VM.\n\n");

#ifndef BOARD_native
    spi_master_init();
#endif

#ifdef DISPLAY_ACTIVATED
    display_init();
    display_clear();
    display_set_xy(0, 0);
#endif

#ifdef STORAGE_ACTIVATED
    if (0 != storage_init()) {
        console_puts("\nERROR: failed to initialize storage ");
        goto finish;
    }
#endif

#ifdef IMAGE_MODE_roflash
    for (j = 0; j < IMAGE_CACHE_SIZE; ++j)
        image_cache[j].key = -1;
#endif

#ifdef IMAGE_MODE_rwstorage
    image_sector_data = (CELL*)malloc(STORAGE_SECTOR_SIZE);
    if (image_sector_data == NULL) {
        console_puts("\nERROR: failed to allocate sector buffer ");
        goto finish;
    }
    image_sector_flags.changed = 0;
    image_sector_flags.sector_num = 1;
    img_storage_load(0);
#endif

    for (j = 0; j < STACK_DEPTH; ++j) data[j] = 0;
    for (j = 0; j < ADDRESSES; ++j) address[j] = 0;
    for (j = 0; j < PORTS; ++j) ports[j] = 0;


    for (S_IP = 0; S_IP >= 0 && S_IP < IMAGE_SIZE; ++S_IP) {
        switch(img_get(S_IP)) {
            case     VM_NOP: break;
            case     VM_LIT: S_SP++; S_IP++; S_TOS = img_get(S_IP); break;
            case     VM_DUP: S_SP++; S_TOS = S_NOS; break;
            case    VM_DROP: S_DROP; break;
            case    VM_SWAP: a = S_TOS; S_TOS = S_NOS; S_NOS = a; break;
            case    VM_PUSH: S_RSP++; S_TORS = S_TOS; S_DROP; break;
            case     VM_POP: S_SP++; S_TOS = S_TORS; S_RSP--; break;
            case    VM_LOOP: S_TOS--; S_IP++;
                             if (S_TOS != 0 && S_TOS > -1)
                                 S_IP = img_get(S_IP) - 1;
                             else S_DROP;
                             break;
            case    VM_JUMP: S_IP++; S_IP = img_get(S_IP) - 1;
                             if (S_IP < 0) S_IP = IMAGE_SIZE;
                             break;
            case  VM_RETURN: S_IP = S_TORS; S_RSP--;
                             if (S_IP < 0) S_IP = IMAGE_SIZE;
                             break;
            case VM_GT_JUMP: S_IP++;
                             if (S_NOS > S_TOS) S_IP = img_get(S_IP) - 1;
                             S_DROP; S_DROP;
                             break;
            case VM_LT_JUMP: S_IP++;
                             if (S_NOS < S_TOS) S_IP = img_get(S_IP) - 1;
                             S_DROP; S_DROP;
                             break;
            case VM_NE_JUMP: S_IP++;
                             if (S_TOS != S_NOS) S_IP = img_get(S_IP) - 1;
                             S_DROP; S_DROP;
                             break;
            case VM_EQ_JUMP: S_IP++;
                             if (S_TOS == S_NOS) S_IP = img_get(S_IP) - 1;
                             S_DROP; S_DROP;
                             break;
            case   VM_FETCH: S_TOS = img_get(S_TOS); break;
            case   VM_STORE: img_put(S_TOS, S_NOS); S_DROP; S_DROP; break;
            case     VM_ADD: S_NOS += S_TOS; S_DROP; break;
            case     VM_SUB: S_NOS -= S_TOS; S_DROP; break;
            case     VM_MUL: S_NOS *= S_TOS; S_DROP; break;
            case  VM_DIVMOD: a = S_TOS; b = S_NOS; S_TOS = b / a; S_NOS = b % a; break;
            case     VM_AND: a = S_TOS; b = S_NOS; S_DROP; S_TOS = a & b; break;
            case      VM_OR: a = S_TOS; b = S_NOS; S_DROP; S_TOS = a | b; break;
            case     VM_XOR: a = S_TOS; b = S_NOS; S_DROP; S_TOS = a ^ b; break;
            case     VM_SHL: a = S_TOS; b = S_NOS; S_DROP; S_TOS = b << a; break;
            case     VM_SHR: a = S_TOS; S_DROP; S_TOS >>= a; break;
            case VM_ZERO_EXIT: if (S_TOS == 0) { S_DROP; S_IP = S_TORS; S_RSP--; } break;
            case     VM_INC: S_TOS += 1; break;
            case     VM_DEC: S_TOS -= 1; break;
            case      VM_IN: a = S_TOS; S_TOS = ports[a]; ports[a] = 0; break;
            case     VM_OUT: t = (size_t) S_TOS; ports[0] = 0; ports[t] = S_NOS; S_DROP; S_DROP; break;
            case    VM_WAIT:
                if (ports[0] != 1) {
                    /* Input */
                    if (ports[0] == 0 && ports[1] == 1) {
                        ports[1] = console_getc();
                        ports[0] = 1;
                    }
                    /* Output (character generator) */
                    if (ports[2] == 1) {
                        console_putc(S_TOS); S_DROP;
                        ports[2] = 0;
                        ports[0] = 1;
                    }
                    /* File IO and Image Saving */
                    if (ports[4] != 0) {
                        ports[0] = 1;
                        switch (ports[4]) {
#ifdef IMAGE_MODE_roflash
                            case 1: ports[4] = save_to_eeprom(); break;
                            case 2: ports[4] = load_from_eeprom(); break;
#endif
                            default: ports[4] = 0;
                        }
                    }
                    /* Capabilities */
                    if (ports[5] != 0) {
                        ports[0] = 1;
                        switch(ports[5]) {
                            case -1:  ports[5] = IMAGE_SIZE; break;
                            case -5:  ports[5] = S_SP; break;
                            case -6:  ports[5] = S_RSP; break;
                            case -9:  ports[5] = 0; S_IP = IMAGE_SIZE; break;
                            case -10: ports[5] = 0; S_DROP;
                                      img_put(S_TOS, 0); S_DROP;
                                      break;
                            case -11: ports[5] = 80; break;
                            case -12: ports[5] = 25; break;
                            default:  ports[5] = 0;
                        }
                    }
                    /* Arduino ports */
                    if (ports[13] != 0) {
                        ports[0] = 1;
                        switch(ports[13]) {
                            /* IO8 Ports */
                            case -1: ports[13] = _SFR_IO8(S_TOS); S_DROP; break;
                            case -2:
                                _SFR_IO8(S_TOS) = S_NOS;
                                ports[13] = 0; S_DROP; S_DROP;
                                break;
                            case -3:
                                a = S_TOS; b = S_NOS; S_DROP; S_DROP;
                                ports[13] = ((_SFR_IO8(a) & (1 << b)) != 0);
                                break;
                            case -4:
                                a = S_TOS; b = S_NOS; S_DROP; S_DROP;
                                if (S_TOS) _SFR_IO8(a) |= (1 << b);
                                else _SFR_IO8(a) &= ~(1 << b);
                                ports[13] = 0;
                                S_DROP; break;
#ifndef BOARD_native
                            case -5: a = S_TOS; S_DROP;
                                     _delay_ms(a);
                                     ports[13] = 0;
                                     break;
                            case -6: spi_master_init(); ports[13] = 0; break;
                            case -7: spi_slave_init(); ports[13] = 0; break;
                            case -8:
                                a = S_TOS; S_DROP;
                                ports[13] = spi_transfer_byte(a);
                                break;
                            case -9:
                                a = S_TOS; S_DROP;
                                ports[13] = spi_transfer_cell(a);
                                break;
#endif
                            default: ports[13] = 0;
                        }
                    }
                    if (ports[14] != 0) {
                        ports[0] = 1;
#ifdef DISPLAY_ACTIVATED
                        switch(ports[14]) {
                            case -1: display_write_char(S_TOS, S_NOS);
                                     ports[14] = 0;
                                     S_DROP; S_DROP; break;
                            case -2: img_string(S_TOS, string_buffer, STRING_BUFFER_SIZE);
                                     display_write_string(string_buffer, S_NOS);
                                     ports[14] = 0;
                                     S_DROP; S_DROP; break;
                            case -3: display_set_xy(S_NOS, S_TOS);
                                     ports[14] = 0;
                                     S_DROP; S_DROP; break;
                            case -4: display_clear();
                                     ports[14] = 0;
                                     break;
                            case -5: ports[14] = DISPLAY_WIDTH; break;
                            case -6: ports[14] = DISPLAY_HEIGHT; break;
                            default: ports[14] = 0;
                        }
#else
                        switch(ports[14]) {
                            case -1: ports[14] = 0; S_DROP; S_DROP; break;
                            case -2: ports[14] = 0; S_DROP; S_DROP; break;
                            case -3: ports[14] = 0; S_DROP; S_DROP; break;
                            default: ports[14] = 0;
                        }
#endif
                    }
                }
                break;
            default:
                S_RSP++;
                S_TORS = S_IP;
                S_IP = img_get(S_IP) - 1;
                break;
        }
        ports[3] = 1;
    }

finish:
    img_sync();
    console_puts("\n\nNgaro VM is down.\n");
    console_finish();
#ifndef BOARD_native
    while(1) _delay_ms(100);
#endif
    return 0;
}
// vim:sts=4:sw=4:expandtab:
