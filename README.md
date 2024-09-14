Tuya https://developer.tuya.com/en/docs/iot/tuya-cloud-universal-serial-port-access-protocol?id=K9hhi0xxtn9cb

Send ESP (Pido version hard)
0x55AA0001000000

Recibe
55 aa 00 01 00 24 7b 22 70 22 3a 22 71 77 67 74 75 34 31 75 35 76 66 78  Uª...${"p":"qwgtu41u5vfx
34 33 78 74 22 2c 22 76 22 3a 22 31 2e 31 2e 32 22 7d 90 6d 3d 30 2c 20  43xt","v":"1.1.2"}m=0, 
73 69 67 3d 32 37 0a                                                     sig=27.                 

------------------------------------------------------

Send ESP (Pooling)
0x55AA000200010305

Recibe
0x55aa0002000001 
0x53746174653a332c203139340a State:3, 194\n  (lo repite incrementando el último número )

------------------------------------------------------

Send ESP (Mando a dormir)
0x55AA000200010406 (Mando a dormir)

Recibe
55 aa 00 02 00 00 01 53 74 61 74 65 3a 34 2c 20 34 33 0a 55 aa 00 05 00  Uª.....State:4, 43.Uª...
05 01 04 00 01 01 10 55 aa 00 05 00 05 10 01 00 01 00 1b 55 aa 00 05 00  .......Uª..........Uª...
05 0e 04 00 01 02 1e 55 aa 00 05 00 05 0b 05 00 01 00 1a 55 aa 00 0a 00  .......Uª..........Uª...
00 09                                                                    ..                      
55 70 67 72 61 64 65 3a 31 0a 57 69 66 69 6f 66 66 fa                    Upgrade:1.Wifioffú      

------------------------------------------------------


Send ESP
0x55AA032D00002f ()

Recibe

------------------------------------------------------




Desde el micro de humo 

Recibe (al inicar desde 0)  Recibe cuando pulso el SW  al despertarse o iniciarse desde 0
0x85
85 6d 3d 30 2c 20 73 69 67 3d 31 0a
0a 4d 6f 64 65 6c 3a 20 58 53 30 31 5f 57 54 28 35 32 35 30 29 5f 43 42  .Model: XS01_WT(5250)_CB
33 53 2c 56 65 72 3a 20 31 2e 31 2e 32 0a 54 68 72 65 73 68 6f 6c 64 3a  3S,Ver: 1.1.2.Threshold:
33 32 30 28 6d 56 29 0a 42 61 74 3a 32 38 39 39 28 6d 56 29 0a 53 65 6e  320(mV).Bat:2899(mV).Sen
73 6f 72 74 69 76 65 3a 31 36 35 35 28 6d 56 29 0a 57 69 66 69 46 61 63  sortive:1655(mV).WifiFac
74 6f 72 79 46 6c 61 67 3a 31 41 0a 69 6e 69 74 3d 34 0a 6d 3d 30 2c 20  toryFlag:1A.init=4.m=0, 
73 69 67 3d 31 0a                                                        sig=1

- Recibe cuando pulso el Test 
  m=7, sig=3 o m=0, sig=3

- Recibe cuando pulso el Test  por unos segundos
  m=0, sig=7


- Recibe cuando pulso link
  m=0, sig=9
  m=7, sig=1
  Wifioff
  init=1

- Recibe cuando pulso link un rato
  m=0, sig=13
  m=8, sig=1
  Wifioff
  init=2


- Recibe Cuando se va a dormir
  Wifioff.m=7, sig=29.
  Wifioff.m=0, sig=1.

- Al tirarle humo HOLTEC
  m=3, sig=18  (Lo repite 4 veces)
  m=3, sig=18
  m=3, sig=18
  m=3, sig=18




55 aa 00 01 00 00 00   get product info (al Holtec)                 
55 aa 00 02 00 01 03 05 get network status 
55 aa 00 02 00 01 04 06 Manda a dormir
55 aa 00 05 00 01 00 05 Manda falla                                  
unos minutos
55 aa 00 05 00 01 00 05                




55 aa 00 05 00 05 0e 04 00 01 01 1d recibo del holtec bateria baja
55 aa 00 05 00 05 0e 04 00 01 02 1e recibo del holtec bateria ok
