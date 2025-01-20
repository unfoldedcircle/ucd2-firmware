# ESP eFuses

## Layout BLOCK3

- 1 byte version field (0x00). Starting with 0x00 in development
- 8 byte serial number string ('12345678')
- 7 byte model number string ('UCD2')
- 4 bytes empty (ADC space)
- 6 bytes hw revision string ('5.3')
- 6 bytes empty. For future use.

## Testing with virtual device

Since blowing efuses is permanent, use the virtual device mode first to make sure everything is ok.

- The `virt.bin` file is automatically created. To start from scratch, simply delete the file.
- The `device.bin` is a binary file to write. Use Hex Fiend tool on Mac or similar to create test data.
  - It's best that this file is exactly 32 bytes long, so it matches the complete BLOCK3. Otherwise use `--offset` parameter.

```
espefuse.py -c esp32 --virt --path-efuse-file virt.bin burn_block_data BLOCK3 device.bin
espefuse.py v3.1
[03] BLOCK3               size=32 bytes, offset=00 - > [aa 31 32 33 34 35 36 37 38 55 43 44 32 00 00 00 00 00 00 00 35 2e 32 00 00 00 00 00 00 00 00 00].

Check all blocks for burn...
idx, BLOCK_NAME,          Conclusion
[03] BLOCK3               is empty, will burn the new value
. 
This is an irreversible operation!
Type 'BURN' (all capitals) to continue.
BURN
BURN BLOCK3  - OK (write block == read block)
Reading updated efuses...
Successful
```

Dump efuses:
```
espefuse.py -c esp32 --virt --path-efuse-file virt.bin dump                             
...
BLOCK3          (                ) [3 ] read_regs: 333231aa 37363534 44435538 00000032 00000000 00322e35 00000000 00000000
espefuse.py v3.1
```

Get device summary:
```
espefuse.py -c esp32 --virt --path-efuse-file virt.bin summary                                      
...
BLOCK3 (BLOCK3):                         Variable Block 3                                  
   = aa 31 32 33 34 35 36 37 38 55 43 44 32 00 00 00 00 00 00 00 35 2e 32 00 00 00 00 00 00 00 00 00 R/W 
```

## Dump efuse

```shell
espefuse.py --port $SERIAL_DEV dump
```

## Write efuse

‼️ Test with a virtual device first before writing efuses in a real device!

```shell
espefuse.py --port $SERIAL_DEV burn_efuse BLOCK3 $DATA
espefuse.py --port $SERIAL_DEV burn_bit BLOCK3 $BIT_NUMBER
espefuse.py --port $SERIAL_DEV burn_block_data --offset 0 BLOCK3 file.bin
```

## Seal the deal

```shell
espefuse.py --port $SERIAL_DEV burn_efuse BLK3_PART_RESERVE
```

## Resources

- https://docs.espressif.com/projects/esptool/en/latest/esp32/espefuse/index.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/efuse.html
