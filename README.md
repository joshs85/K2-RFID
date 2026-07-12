# CFS-RFID
K2/K1/HI/CFS RFID Programming.<br>

The tags required are <a href=https://en.wikipedia.org/wiki/MIFARE>MIFARE</a> Classic 1k tags.<br>


<br>
<a href=https://github.com/DnG-Crafts/K2-RFID/tree/main/Android/SpoolID>Android Code</a>
<br>
<br>

<a href=https://github.com/DnG-Crafts/K2-RFID/tree/main/Arduino>Arduino Code</a>
<br>
<br>

<a href=https://github.com/DnG-Crafts/K2-RFID/tree/main/Windows>Windows Code</a>
<br>
<br>

<a href=https://github.com/DnG-Crafts/K2-RFID/tree/main/Flipper>Flipper Zero Code</a>
<br>
<br>

[![https://www.youtube.com/watch?v=6EA4t7zgq90](https://img.youtube.com/vi/6EA4t7zgq90/0.jpg)](https://www.youtube.com/watch?v=6EA4t7zgq90)

https://www.youtube.com/watch?v=6EA4t7zgq90<br>


<br><br><br>
The android app is available on google play<br>
<a href="https://play.google.com/store/apps/details?id=dngsoftware.spoolid&hl=en"><img src=https://github.com/DnG-Crafts/K2-RFID/blob/main/docs/gp.webp width="30%" height="30%"></a>
<br>



## Tag Format
```
Creality RFID Tag Data

 AB1240276A21010010FFFFFF0165000001000000
 AB1240276A21010010C12E1F0165000001000000
 9A2240276A210100100000000165000001000000
 AB1240276A21010010C12E1F0165000001000000
 
    date             
|  M DD YY  | venderId | batch | filamentId |  color  | filamentLen | serialNum | reserve |
|           |          |       |            |         |             |           |         |
|  A B1 24  |   0276   |  A2   |   101001   | 0FFFFFF |    0165     |   000001  |  000000 |
|           |          |       |            |         |             |           |         |
|  A B1 24  |   0276   |  A2   |   101001   | 0C12E1F |    0165     |   000001  |  000000 |
|           |          |       |            |         |             |           |         |
|  9 A2 24  |   0276   |  A2   |   101001   | 0000000 |    0165     |   000001  |  000000 |
|           |          |       |            |         |             |           |         |
|  A B1 24  |   0276   |  A2   |   101001   | 0C12E1F |    0165     |   000001  |  000000 |
  
```

<img src=https://github.com/DnG-Crafts/K2-RFID/blob/main/docs/ghi.jpg width=50% height=50%>




## Files of interest
```
 /mnt/UDISK/creality/userdata/box/tn_data.json
 /mnt/UDISK/creality/userdata/box/material_box_info.json
 /mnt/UDISK/creality/userdata/box/material_database.json
 /mnt/UDISK/creality/userdata/box/material_modify_info.json
```


# Arduino

<a href=https://github.com/DnG-Crafts/K2-RFID/tree/main/Arduino/ESP32>Code for ESP32 boards</a><br>
<a href=https://github.com/DnG-Crafts/K2-RFID/tree/main/Arduino/ESP8266>Code for ESP8266 boards</a><br>
<a href=https://github.com/DnG-Crafts/K2-RFID/tree/main/Arduino/Pico_W>Code for Pico W boards</a><br>

<br><br>

Flash Code:

[![https://www.youtube.com/watch?v=WOznbT7NbWI](https://img.youtube.com/vi/WOznbT7NbWI/0.jpg)](https://www.youtube.com/watch?v=WOznbT7NbWI)

https://www.youtube.com/watch?v=WOznbT7NbWI<br>

