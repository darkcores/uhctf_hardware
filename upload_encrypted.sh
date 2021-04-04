echo $(pwd)
rm bootloader.bin partitions.bin firmware.bin
echo "Encrypting firmware files"
~/.platformio/packages/tool-esptoolpy/espsecure.py encrypt_flash_data --keyfile my_flash_encryption_key.bin --address 0x1000 -o bootloader.bin .pio/build/nodemcu-32s/bootloader.bin
~/.platformio/packages/tool-esptoolpy/espsecure.py encrypt_flash_data --keyfile my_flash_encryption_key.bin --address 0x8000 -o partitions.bin .pio/build/nodemcu-32s/partitions.bin 
~/.platformio/packages/tool-esptoolpy/espsecure.py encrypt_flash_data --keyfile my_flash_encryption_key.bin --address 0x10000 -o firmware.bin .pio/build/nodemcu-32s/firmware.bin  
# ~/.platformio/packages/tool-esptoolpy/espsecure.py encrypt_flash_data --keyfile my_flash_encryption_key.bin --address 0x210000 -o data.bin unencrypted.bin 
echo "Flashing firmware"
py="/home/jorrit/.platformio/penv/bin/python"
esptool="/home/jorrit/.platformio/packages/tool-esptoolpy/esptool.py"
flags='--chip esp32 --port /dev/ttyUSB0 --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect'
$py $esptool $flags 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
