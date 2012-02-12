#!/bin/sh

err () {
  [ -e error.log ] && cat error.log
  rm -f error.log
  echo "ERROR: $*" >&2
  exit 1
}

[ X"$BOARD" = X"" ] && err "build_binaries.sh should be included in configuration."

echo "Build image file"
make -C $ROOT_DIRECTORY retro >error.log 2>&1 || err "Failed to builed original retro"
cat $MODULES \
| $ROOT_DIRECTORY/retro --shrink --image $ROOT_DIRECTORY/retroImage >error.log 2>&1 \
|| err "Failed to build retro image"
make -C $ROOT_DIRECTORY images >error.log 2>&1 || err "Failed to convert retro image"
rm -f error.log
ls -l $ROOT_DIRECTORY/retroImage16

echo "Create image header"
hexdump -v \
  -e "\"{\" $IMAGE_BLOCK_SIZE/2 \"%d,\" \"},\n\"" \
  $ROOT_DIRECTORY/retroImage16 \
| sed -e 's/,,* *},/}/' \
| awk "BEGIN { curr = 0; }
{ print \"static const prog_int16_t image_\" curr \"[$IMAGE_BLOCK_SIZE] PROGMEM = \" \$0 \";\";
  curr++;
}
END {
  print \"#define IMAGE_CELLS \" ($IMAGE_BLOCK_SIZE * curr);
  print \"static int16_t image_read(int16_t x) {\";
  print \"  switch(x / ${IMAGE_BLOCK_SIZE}) {\";
  for (i = 0; i < curr; i++) { \
    print \"    case \" i \": return pgm_read_word(&(image_\" i \"[x % $IMAGE_BLOCK_SIZE]));\";
  };
  print \"  } return 0; }\";
}" > image.h \
|| err "Failed to build image header"
ls -l image.h

echo "Build native executable (for testing)"
echo $CC $CFLAGS $NATCFLAGS -o retro.nat retro.c
$CC $CFLAGS $NATCFLAGS -o retro.nat retro.c >error.log 2>&1 \
|| err "Failed to build native executable"
rm -f error.log
ls -l retro.nat

echo "Build AVR executable (for uploading)"
echo $AVRCC $CFLAGS $AVRCFLAGS -o retro.avr retro.c
$AVRCC $CFLAGS $AVRCFLAGS -o retro.avr retro.c >error.log 2>&1 \
|| err "Failed to build AVR executable"
$AVROBJCOPY -S -O ihex -R .eeprom retro.avr retro.hex >error.log 2>&1 \
|| err "Failed to create upload file"
rm -f error.log
ls -l retro.avr retro.hex

echo "Everything done!"
echo "Examples of upload commands:"
echo "  With ISP> avrdude -V -c usbtiny -p $BOARD -U flash:w:retro.hex"
echo "  Directly> avrdude -V -c stk500v2 -p $BOARD -b 19200 -P /dev/ttyU0 -U flash:w:retro.hex"
echo "Or start it with> ./retro.nat"
