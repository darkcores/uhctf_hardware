echo $(pwd)
rm bootloader.bin partitions.bin firmware.bin

echo "Encrypting firmware files"
~/.platformio/packages/tool-esptoolpy/espsecure.py encrypt_flash_data --keyfile my_flash_encryption_key.bin --address 0x1000 -o bootloader.bin .pio/build/nodemcu-32s/bootloader.bin
~/.platformio/packages/tool-esptoolpy/espsecure.py encrypt_flash_data --keyfile my_flash_encryption_key.bin --address 0x8000 -o partitions.bin .pio/build/nodemcu-32s/partitions.bin 
~/.platformio/packages/tool-esptoolpy/espsecure.py encrypt_flash_data --keyfile my_flash_encryption_key.bin --address 0x10000 -o firmware.bin .pio/build/nodemcu-32s/firmware.bin  
# ~/.platformio/packages/tool-esptoolpy/espsecure.py encrypt_flash_data --keyfile my_flash_encryption_key.bin --address 0x210000 -o data.bin unencrypted.bin 

echo "Building firmware image"
dd if=/dev/zero of=image.bin bs=1k count=2000
dd if=bootloader.bin of=image.bin conv=notrunc 
dd if=partitions.bin of=image.bin conv=notrunc bs=1k seek=28
dd if=firmware.bin of=image.bin conv=notrunc bs=1k seek=60  

echo "Flashing firmware"
py="/home/jorrit/.platformio/penv/bin/python"
esptool="/home/jorrit/.platformio/packages/tool-esptoolpy/esptool.py"
flags='--chip esp32 --port /dev/ttyUSB0 --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect'
$py $esptool $flags 0x1000 image.bin
