# LABUBU IA

<p align="center">
  <img src="https://github.com/user-attachments/assets/959a0107-54a6-4b48-913b-0b7b8dc01773" width="40%" />
</p>

## Hardware

###  Specs
- ESP32-S3
- MEMS Micro ICS-43434
- Amplifier Max 98357
- Built-in LTC4065 lipo charger 
- Built-in 1s lipo and micro sd card
- WIFI/BLE antenna connector
- Push button for application purpose
- connector for external on/off switch, external USB, external speaker and adressable led.



###  PCB
- 4 layers under 900mm2

### Some view 

<p align="center">
  <img src="https://github.com/user-attachments/assets/fd53adc7-e1c8-418c-b5bc-c38ddaeb4619" width="40%" />
  <img src="https://github.com/user-attachments/assets/a6eadc54-a15e-4646-8f9a-342e1f9b7d83" width="40%" />
</p>

### 3D pcb case 
<p align="center">
  <img src="https://github.com/user-attachments/assets/cf706744-64ca-4314-bd8e-0baaf3899d64" width="40%" />
  <img src="https://github.com/user-attachments/assets/9f9a9def-6549-4253-abef-15309e9caa36" width="40%" />
</p>

##  Software

### Audio_Player 
-Scan button at startup: if pressed goes into msc mode and expose sd card on usb to add new song (sd in spi mode too extremely slow)
-then scan all the .wav in the memory and if you short presse toggle the play of the current song and if you long press, play the next song in alphabetical order.

### IA_assistant

-in progress 


## Result 

Picture of the pcb ( cable on top are soldered because i had not the jst ghs 8pin in stock )

<p align="center">
  <img src="https://github.com/user-attachments/assets/e5b87701-a43d-44e3-83ab-39c4b8db57b9" width="40%" />
  <img src="https://github.com/user-attachments/assets/dfe7ec52-2d7b-4920-959d-96cb94206613" width="40%" />
</p>

View of the pcb with the 1s 150mah lipo battery connected
<p align="center">
  <img src="https://github.com/user-attachments/assets/f34a2ed3-16d6-47fb-852d-9891cc1dbef1" width="40%" />
</p>

View of the External power switch (on one side battery is disconnected from ldo and usb is connected to ldo and on other side usb is disconnected from ldo and battery is connected ) 
and view of the usb port both are hidden in the labubu's feet.

<p align="center">
  <img src="https://github.com/user-attachments/assets/962e7cf9-644d-4ace-b583-c2c3e7e92327" width="40%" />
  <img src="https://github.com/user-attachments/assets/2bc4ffe3-c1af-4333-9784-445c148c4a6d" width="40%" />
</p>

