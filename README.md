# CubeCell LoRa Weather Transceiver
A VSCode / Platform IO project for the HelTec CubeCell HTCC-AB01 LoRa board wired to an Adafruit BMP390 sensor that periodically sends air pressure and temperature readings to a LoRaWAN network (The Things Network / Helium) in CayenneLPP format. The sketch can also receive downlinks to trigger the board to reset. For a list of components and a wiring diagram, visit the Documentation folder.

# Notes
1. This project is intended to be used in picoballoon applications in North America, though terrestrial applications are possible. If you do not live in NA, you must set the ACTIVE_REGION setting to your desired region. For intercontinental applications, consider the repo at https://github.com/wodeeken/CubeCell-LoRa-Weather-Transceiver-FreqChange. This will give the user the ability to dynamically alter the frequency of the board so that it matches the frequency of your region. You cannot use the default HTCC-AB01 library provided by PlatformIO, as it does not allow change of the frequency after compile time. You will need to update portions of this library with those located at https://github.com/wodeeken/CubeCell-Library-LoraWAN-Region-Change.
# Usage
1. This program was created using the PlatformIO extension for VSCode. You will need to add the libraries for HTCC-AB01, Adafruit BMP390, and CayenneLPP.
2. The user must configure OTAA (Dev/App EUI and key) or ABP (nwk/app key and addr) params located at main.cpp:21-27 before building and uploading the program.
3. When the LoRa board first boots up, the board will send a pressure/temperature reading every 5 minutes. After an hour, the board will only send once every 10 mins. The board sleeps in between readings.
4. If the air pressure increases by 4 hPa or more since the previous reading, the board will only sleep for 5 minutes. If the increase is 7 hPa or more the board will sleep for only 1 minute.
5. If the board receives a downlink payload of 0x52 ("R"), the board will reset.
