/* Ngaro VM for Arduino boards ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Copyright (c) 2011 - 2012, Oleksandr Kozachuk
   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Predefines ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Board types */
#define native 1
#define mega2560 2
#define mega328p 3

/* Image storage types */
#define roflash 1
#define rwstorage 2

/* Display types */
#define nokia3110 1

/* Storage types */
#define sdcard 1

/* General includes ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#include <stdlib.h>
#if BOARD == native
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#endif

/* Board specific includes ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#if BOARD == mega2560 || BOARD == mega328p

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/setbaud.h>

#else
#if BOARD == native
#include <termios.h>
#else
#error "Unknown platform or -DBOARD=<type> not defined."
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

#if BOARD != native

static void spi_master_init(void);
static void spi_slave_init(void);
static uint8_t __attribute__((always_inline)) spi_transfer_byte(uint8_t data);
static CELL __attribute__((always_inline)) spi_transfer_cell(CELL data);

#endif

/* Change store ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define CELL_CACHE_END IMAGE_CACHE_SIZE
#define CELL_CHANGED (((CELL)1) << (sizeof(CELL) * 8 - 2))
typedef CELL_CACHE_TYPE cell_cache_pointer_t;

static CELL *cell_cache_keys;
static cell_cache_pointer_t *cell_cache_nexts;
static CELL *cell_cache_values;
static cell_cache_pointer_t cell_cache_first;

/* Image operations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
static CELL img_get(CELL k);
static void img_put(CELL k, CELL v);

/* Board and image setup ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#if BOARD == mega2560 || BOARD == mega328p

#include "console_atmega.h"

#if IMAGE_MODE == roflash
#include "image.h"
#include "eeprom_atmel.h"
#endif

#if DISPLAY == nokia3110
#include "display_nokia3110.h"
#endif

#else
#if BOARD == native

#include "console_native.h"

#define PROGMEM
#define prog_int32_t int32_t
#define prog_int16_t int16_t
#define pgm_read_word(x) (*(x))
#define _SFR_IO8(x) SFR_IO8[x]
static int SFR_IO8[64];
#define _delay_ms(x) sleep(x/1000)

#if IMAGE_MODE == roflash
#include "image.h"
#include "eeprom_native.h"
#endif

#endif
#endif

#if STORAGE == sdcard
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
#if BOARD != native

static void spi_master_init(void) {
    SPI_DDR &= ~(1 << SPI_MISO);
    SPI_DDR |=  (1 << SPI_SCK);
    SPI_DDR |=  (1 << SPI_SS);
    SPI_DDR |=  (1 << SPI_MOSI);
    SPI_PORT |= (1 << SPI_SS);
    SPCR = (1 << SPE) | (1 << MSTR) | (0 << SPR1) | (0 << SPR0);
    SPSR &= ~(1 << SPI2X);
}

static void spi_slave_init(void) {
    SPI_DDR |=  (1 << SPI_MISO);
    SPI_DDR &= ~(1 << SPI_MOSI);
    SPI_DDR &= ~(1 << SPI_SCK);
    SPI_DDR &= ~(1 << SPI_SS);
    SPI_PORT &= ~(1 << SPI_SS);
    SPCR = (1 << SPE) | (1 << SPR1) | (1 << SPR0);
    SPSR &= ~(1 << SPI2X);
}

static uint8_t spi_transfer_byte(uint8_t data) {
    SPDR = data;
    while ((SPSR & (1 << SPIF)) == 0);
    return SPDR;
}

static CELL spi_transfer_cell(CELL data) {
    union { CELL l; uint8_t c[sizeof(CELL)]; } d;
    d.l = data;
    for (int8_t i = sizeof(CELL) - 1; i >= 0; --i)
        d.c[i] = spi_transfer_byte(d.c[i]);
    return d.l;
}

#endif

/* Image read and write ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#if IMAGE_MODE == roflash

#define img_storage_get(k) image_read(k)
#define img_storage_put(k, v) 0
#define img_storage_sync()

#else
#if IMAGE_MODE == rwstorage

#ifndef STORAGE_ACTIVATED
#error "Image mode 'rwstorage' is needs an enabled storage."
#endif

#define IMAGE_SECTOR_SIZE (STORAGE_SECTOR_SIZE/sizeof(CELL))

static CELL *image_sector_data;
static struct {
    uint8_t changed:1;
    uint32_t sector_num:31;
} image_sector_flags;

static void img_storage_load(uint32_t sec) {
    if (sec != image_sector_flags.sector_num) {
        if (image_sector_flags.changed != 0) {
            if (0 != storage_write_sector(
                        (uint8_t*) image_sector_data,
                        image_sector_flags.sector_num)) {
                console_puts("\nERROR: failed to write sector ");
                _delay_ms(1000);
            } else image_sector_flags.changed = 0;
        }
        if (image_sector_flags.changed == 0) {
            if (0 != storage_read_sector((uint8_t*) image_sector_data, sec)) {
                console_puts("\nERROR: failed to read sector ");
                _delay_ms(1000);
            } else image_sector_flags.sector_num = sec;
        }
    }
}

static inline CELL __attribute__((always_inline)) img_storage_get(CELL k) {
    img_storage_load(k / IMAGE_SECTOR_SIZE);
    return image_sector_data[k % IMAGE_SECTOR_SIZE];
}

static inline uint8_t __attribute__((always_inline)) img_storage_put(CELL k, CELL v) {
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

static void _img_lru(cell_cache_pointer_t cur, cell_cache_pointer_t prev) {
    if (prev != CELL_CACHE_END) {
        cell_cache_nexts[prev] = cell_cache_nexts[cur];
        cell_cache_nexts[cur] = cell_cache_first;
        cell_cache_first = cur;
    }
}

static void _img_find(CELL k, cell_cache_pointer_t *pointers) {
    for (; pointers[0] != CELL_CACHE_END; pointers[0] = cell_cache_nexts[pointers[0]]) {
        if ((cell_cache_keys[pointers[0]] & (~CELL_CHANGED)) == k) {
            _img_lru(pointers[0], pointers[1]);
            return;
        }
        if ((cell_cache_keys[pointers[0]] & CELL_CHANGED) == 0) {
            pointers[3] = pointers[1];
            pointers[2] = pointers[0];
        }
        pointers[1] = pointers[0];
    }
    if (pointers[2] == CELL_CACHE_END) {
        console_puts("\nERROR: Out of memory ");
        while(1) _delay_ms(1000);
    }
}

static CELL img_get(CELL k) {
    cell_cache_pointer_t pointers[4] = {
        cell_cache_first,
        CELL_CACHE_END, CELL_CACHE_END, CELL_CACHE_END
    };

    _img_find(k, pointers);
    if (pointers[0] != CELL_CACHE_END)
        return cell_cache_values[pointers[0]];

    _img_lru(pointers[2], pointers[3]);
    cell_cache_keys[cell_cache_first] = k;
    return (cell_cache_values[cell_cache_first] = img_storage_get(k));
}

static void img_put(CELL k, CELL v) {
    cell_cache_pointer_t pointers[4] = {
        cell_cache_first,
        CELL_CACHE_END, CELL_CACHE_END, CELL_CACHE_END
    };

    _img_find(k, pointers);
    if (pointers[0] != CELL_CACHE_END) {
        if (cell_cache_values[cell_cache_first] != v) {
            cell_cache_values[cell_cache_first] = v;
            cell_cache_keys[cell_cache_first] |= CELL_CHANGED;
        }
        return;
    }
    if (img_storage_get(k) == v)
        return;

    _img_lru(pointers[2], pointers[3]);
    cell_cache_keys[cell_cache_first] = (k | CELL_CHANGED);
    cell_cache_values[cell_cache_first] = v;
}

#define img_sync()

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
    CELL *data;
    CELL *address;
    CELL *ports;
    CELL a, b;

    _delay_ms(1000);

#ifdef DISPLAY_ACTIVATED
    char string_buffer[STRING_BUFFER_SIZE+1];
#endif

    console_prepare();
    console_puts("\nInitialize Ngaro VM.\n\n");

#if BOARD != native
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

#if IMAGE_MODE == rwstorage
    image_sector_data = (CELL*)malloc(STORAGE_SECTOR_SIZE);
    if (image_sector_data == NULL) {
        console_puts("\nERROR: failed to allocate sector buffer ");
        goto finish;
    }
    image_sector_flags.changed = 0;
    image_sector_flags.sector_num = 1;
    img_storage_load(0);
#endif

    cell_cache_keys = (CELL*) malloc(sizeof(CELL) * IMAGE_CACHE_SIZE);
    cell_cache_nexts = (cell_cache_pointer_t*) malloc(sizeof(cell_cache_pointer_t) * IMAGE_CACHE_SIZE);
    cell_cache_values = (CELL*) malloc(sizeof(CELL) * IMAGE_CACHE_SIZE);
    if (cell_cache_keys == NULL || cell_cache_nexts == NULL || cell_cache_values == NULL) {
        console_puts("\nERROR: failed to allocate cell cache ");
        goto finish;
    }
    for (j = 0; j < IMAGE_CACHE_SIZE; ++j) {
        cell_cache_keys[j] = j;
        cell_cache_values[j] = img_storage_get(j);
        cell_cache_nexts[j] = j + 1;
    }
    cell_cache_nexts[j-1] = CELL_CACHE_END;
    cell_cache_first = 0;

    data = (CELL*) malloc(sizeof(CELL) * STACK_DEPTH);
    address = (CELL*) malloc(sizeof(CELL) * ADDRESSES);
    ports = (CELL*) malloc(sizeof(CELL) * PORTS);
    if (data == NULL || address == NULL || ports == NULL) {
        console_puts("\nERROR: failed to allocate stacks or ports ");
        goto finish;
    }
    for (j = 0; j < STACK_DEPTH; ++j) data[j] = 0;
    for (j = 0; j < ADDRESSES; ++j) address[j] = 0;
    for (j = 0; j < PORTS; ++j) ports[j] = 0;

    for (S_IP = 0; S_IP >= 0 && S_IP < IMAGE_SIZE; ++S_IP) {
        register CELL op = img_get(S_IP);
        switch(op) {
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
#if IMAGE_MODE == roflash
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
#if BOARD != native
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
#if BOARD != native
    while(1) _delay_ms(100);
#endif
    return 0;
}
// vim:sts=4:sw=4:expandtab:
