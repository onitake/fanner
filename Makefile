PRG            = fanner
OBJ            = fanner.c
OPTIMIZE       = -Os #-fwhole-program

MCU_TARGET     = attiny13a
DEFS           = -DF_CPU=9600000UL
PROGRAMMER     = usbtiny
PROGPORT       = usb
#PROG_TARGET    = $(MCU_TARGET)
PROG_TARGET    = t13

DEFS          += 

LIBS           =

# You should not have to change anything below here.

CC             = avr-gcc
DUDE           = avrdude
PERL           = perl

# Override is only needed by avr-lib build system.

override CFLAGS        = -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS)
override LDFLAGS       = $(OPTIMIZE) -Wl,-Map,$(PRG).map

DUDEFLAGS      = -c $(PROGRAMMER) -p $(PROG_TARGET) -P $(PROGPORT)

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

all: lst text eeprom

$(PRG).elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o *.elf *.lst *.map $(EXTRA_CLEAN_FILES)

lst:  $(PRG).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

# Rules for flashing, EEPROM writing and fuse bit setting

flash: $(PRG).hex $(PRG)_eeprom.hex
	$(DUDE) $(DUDEFLAGS) -U eeprom:w:$(PRG)_eeprom.hex:i -U flash:w:$(PRG).hex:i

flashprg: $(PRG).hex
	$(DUDE) $(DUDEFLAGS) -U flash:w:$(PRG).hex:i

flasheep: $(PRG)_eeprom.hex
	$(DUDE) $(DUDEFLAGS) -U eeprom:w:$(PRG)_eeprom.hex:i

fuse:
	$(DUDE) $(DUDEFLAGS) -U hfuse:w:0xff:m -U lfuse:w:0x6a:m

# Rules for building the .text rom images

text: hex bin srec

hex:  $(PRG).hex
bin:  $(PRG).bin
srec: $(PRG).srec

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

# Rules for building the .eeprom rom images

eeprom: ehex ebin esrec

ehex:  $(PRG)_eeprom.hex
ebin:  $(PRG)_eeprom.bin
esrec: $(PRG)_eeprom.srec

%_eeprom.hex: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@ \
	|| { echo empty $@ not generated; exit 0; }

%_eeprom.srec: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O srec $< $@ \
	|| { echo empty $@ not generated; exit 0; }

%_eeprom.bin: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O binary $< $@ \
	|| { echo empty $@ not generated; exit 0; }

