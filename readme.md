# UHCTF Hardware Challenge 2021

This code was used for the hardware challenge from the [uhctf 2021](https://ctf.edm.uhasselt.be) event. Install [esptool](https://github.com/espressif/esptool) for the scripts that are used below.

## Building

Open with platformio, build image. The build command should also take care of generating the encrypted image.bin file. Files are encrypted by `upload_encrypted.sh`.

To burn the key encryption key onto the device: `espefuse.py --port PORT burn_key flash_encryption my_flash_encryption_key.bin`

Burn encrypted image to device: `esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash 0x1000 image.bin`

## Solution

### Key 1

Connect usb serial, reset device, and read device output.

### Key 2

1. Enable maintenance mode by grounding pin 5.
2. Connect with wifi to the device (passwd in description).
3. Visit webpage on device IP.

### Key 3

1. Enter your wifi credentials on the maintenance page
2. Connect buzzer.
3. Listen to outputs from the buzzer when grounding the KEYPAD_PINS, different sound is made for correct input.
4. When the correct code is entered visit the webpage on the device.

### Key 4

1. Run wireshark/tcpdump between this device (eg create hotspot, other MITM technique, ... device needs internet access)
2. Check for updates in device
3. Check wireshark for http traffic (POST request)

### Key 5

1. Enter all previous keys on the maintenance page.
2. Download key
3. Use key to decrypt `image.bin` (keep in mind offset starts at `0x1000`) `espsecure.py decrypt_flash_data --keyfile my_flash_encryption_key.bin --address 0x1000 image.bin`
4. In decrypted image search for `UHCTF`

## Use the esp32 device as normal

If you have esptool installed, just run `espefuse.py burn_efuse FLASH_CRYPT_CNT` and type `BURN` ONCE, everytime you do this you disable/enable encryption, and there are a limited amount of fuses to do this.