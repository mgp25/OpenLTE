# Open<img src="https://raw.githubusercontent.com/mgp25/OpenLTE/master/assets/lte.png" width=50>

OpenLTE is an open source implementation of the 3GPP LTE specifications. 

This is a clone of [https://sourceforge.net/p/openlte](https://sourceforge.net/p/openlte).

### Contents

* [Prerequesites](#prerequisites)
* [Installation](#installation)
	- [Setup your computer](#setup-your-computer)
	- [Installing GNURadio with UHD](#installing-gnuradio-with-uhd)
	- [Installing OpenLTE](#installing-openlte)
* [Running OpenLTE eNodeB](#running-openlte-enodeb)
* [OpenLTE Tx Configuration](#openlte-tx-configuration)
* [Wireshark Configuration](#wireshark-configuration)
* [Programming your own USIM card](#programming-your-own-usim-card)
	- [Prerequisites](#prerequisites)
	- [Providers](#providers)
	- [Get the SIM programmer](#get-the-sim-programmer)
	- [Get the software (PySIM, PCSCd, Pyscard)](#get-the-software-pysim-pcscd-pyscard)
	- [Programming the SIM card](#programming-the-sim-card)
	- [Adding subscribers](#adding-subscribers)
* [Test captures](#test-captures)


## Prerequisites


- USB 3.0 interface
- Modern multicore CPU (Intel Core i5, Core i7 or equivalent with SSE4.1 SSE4.2 and AVX support)
- UHD driver installed (for Ettus SDRs)
- GNURADIO


## Installation

### Setup your computer

OpenLTE is not only requiring a huge amount of processing power, but it also requires a very low latency due its need to transmit/receive a radio frame every 1ms. If there is any delay in the processing, the system will not going to be able respond in time and will lose samples. Therefor it is recommended to switch of any CPU and/or system features (mostly in your BIOS) which can cause any delays or can slow down the so called context switching time. Intel SpeedStep, deep and deeper sleep states etc. should be turned off. Especially with high bandwidth setups (10, 15 and 20MHz) it is recommended to swtich off the GUI on linux. There is also a low latency edition of the linux kernel, but at this point there is no absolute proof that it actually helps with OpenLTE.

### Installing GNURadio with UHD

With an Ettus radio (B200, B210) you will need the latest UHD driver besides GNURadio:

`sudo apt-get install libuhd-dev libuhd003 uhd-host`

I recomend not to use the binary version but to compile to code with UHD like the following:

As a non-root user, give the following command:

```
mkdir gnuradio
cd gnurdio
wget http://www.sbrac.org/files/build-gnuradio
chmod a+x build-gnuradio

./build-gnuradio -v
```

You will be asked for the root password by the install script. **The whole procedure can take up to 3 hours!** It will download GNURadio , UHD and all the necessary dependencies.

Check the communication with your Ettus SDR:
Connect your SDR to one of the USB3 interfaces, and run:

`uhd_usrp_probe`

The software will load the FPGA code to your device, and queries your device. If you done everything right, you should see something similar:

```
linux; GNU C++ version 4.8.2; Boost_105400; UHD_003.008.001-42-g8c87a524

-- Operating over USB 3.
-- Initialize CODEC control...
-- Initialize Radio control...
-- Performing register loopback test... pass
-- Performing CODEC loopback test... pass
-- Asking for clock rate 32.000000 MHz...
-- Actually got clock rate 32.000000 MHz.
-- Performing timer loopback test... pass
-- Setting master clock rate selection to 'automatic'.
  _____________________________________________________
 /
|       Device: B-Series Device
|     _____________________________________________________
|    /
|   |       Mboard: B200
|   |   revision: 4
|   |   product: 1
|   |   serial: F54xxx
|   |   FW Version: 7.0
|   |   FPGA Version: 4.0
|   |
|   |   Time sources: none, internal, external, gpsdo
|   |   Clock sources: internal, external, gpsdo
|   |   Sensors: ref_locked
|   |     _____________________________________________________
|   |    /
|   |   |       RX DSP: 0
|   |   |   Freq range: -16.000 to 16.000 MHz
|   |     _____________________________________________________
|   |    /
|   |   |       RX Dboard: A
|   |   |     _____________________________________________________
|   |   |    /
|   |   |   |       RX Frontend: A
|   |   |   |   Name: FE-RX2
|   |   |   |   Antennas: TX/RX, RX2
|   |   |   |   Sensors:
|   |   |   |   Freq range: 50.000 to 6000.000 MHz
|   |   |   |   Gain range PGA: 0.0 to 73.0 step 1.0 dB
|   |   |   |   Connection Type: IQ
|   |   |   |   Uses LO offset: No
|   |   |     _____________________________________________________
|   |   |    /
|   |   |   |       RX Codec: A
|   |   |   |   Name: B200 RX dual ADC
|   |   |   |   Gain Elements: None
|   |     _____________________________________________________
|   |    /
|   |   |       TX DSP: 0
|   |   |   Freq range: -16.000 to 16.000 MHz
|   |     _____________________________________________________
|   |    /
|   |   |       TX Dboard: A
|   |   |     _____________________________________________________
|   |   |    /
|   |   |   |       TX Frontend: A
|   |   |   |   Name: FE-TX2
|   |   |   |   Antennas: TX/RX
|   |   |   |   Sensors:
|   |   |   |   Freq range: 50.000 to 6000.000 MHz
|   |   |   |   Gain range PGA: 0.0 to 89.8 step 0.2 dB
|   |   |   |   Connection Type: IQ
|   |   |   |   Uses LO offset: No
|   |   |     _____________________________________________________
|   |   |    /
|   |   |   |       TX Codec: A
|   |   |   |   Name: B200 TX dual DAC
|   |   |   |   Gain Elements: None
```


## Installing OpenLTE

**Dependencies:**

`sudo apt-get install libpolarssl-dev`

**Build and install:**

```
mkdir build
cd build && cmake ..
make
```

(Optional):

`sudo make install`

## Running OpenLTE eNodeB

**First terminal window:**

Do not close this windows during operation!

`LTE_fdd_enodeb`

Output:

```
linux; GNU C++ version 4.8.2; Boost_105400; UHD_003.008.001-42-
g8c87a524
*** LTE FDD ENB ***
Please connect to control port 30000
```

**Second terminal:**

This is the control interface of the eNodeB.

`telnet 127.0.0.1 30000`

Output:

```
Trying 127.0.0.1...
Connected to 127.0.0.1.
Escape character is '^]'.
*** LTE FDD ENB ***
Type help to see a list of commands
```

**Third terminal (Optional):**

This command will provide debug log messages.

`telnet 127.0.0.1 30001`

## OpenLTE Tx Configuration

**Tx configuration:**

```
write band 20
write bandwidth 5
write dl_earfcn 6300
write mcc 214
write mnc 12
write n_ant 1
write rx_gain 30
write tx_gain 86
```

## Wireshark configuration

`Edit -> Preferences -> Protocols -> DLT_USER -> Edit…`
`Click ‘+’ -> DLT = User 0 and Payload protocol = mac-lte-framed`

## Programming your own USIM card

### Prerequisites

`sudo apt-get install python-pip`

```
sudo python -m pip install serial pycrypto
```

### Providers

**sysmoUSIM-SJS1 4FF/nano SIM + USIM Card (10-pack):**

[http://shop.sysmocom.de/products/sysmousim-sjs1-4ff](http://shop.sysmocom.de/products/sysmousim-sjs1-4ff)

### Get the SIM programmer

You need a SIM card programmer which is compatible with the PCSC application on Linux. To have a more or less complete list of the compatible devices, please visit this page:

[http://pcsclite.alioth.debian.org/ccid/supported.html](http://pcsclite.alioth.debian.org/ccid/supported.html)

Don't forget that you need a programmer with APDU support. Personally we use **SCM Microsystems Inc. SCR 3310**, you can find it and many of the above list on Ebay.

### Get the software (PySIM, PCSCd, Pyscard)

**First install dependencies:**

`sudo apt-get install pcscd pcsc-tools libccid libpcsclite-dev`

Connect your SIM card reader, plug thhe programmable SIM card in, and check connectivity by running the following command:

`sudo pcsc_scan`

If your reader and card got recognized, you will see something similar:

```
PC/SC device scanner
V 1.4.22 (c) 2001-2011, Ludovic Rousseau <ludovic.rousseau@free.fr>
Compiled with PC/SC lite version: 1.8.10
Using reader plug'n play mechanism
Scanning present readers...
0: OMNIKEY AG CardMan 3121 01 00

Wed Dec 24 14:56:32 2014
Reader 0: OMNIKEY AG CardMan 3121 01 00
  Card state: Card inserted,
  ATR: 3B 9F 95 80 1F C7 80 31 E0 73 FE 21 13 57 12 29 11 02 01 00 00 C2

ATR: 3B 9F 95 80 1F C7 80 31 E0 73 FE 21 13 57 12 29 11 02 01 00 00 C2
+ TS = 3B --> Direct Convention
+ T0 = 9F, Y(1): 1001, K: 15 (historical bytes)
  TA(1) = 95 --> Fi=512, Di=16, 32 cycles/ETU
    125000 bits/s at 4 MHz, fMax for Fi = 5 MHz => 156250 bits/s
  TD(1) = 80 --> Y(i+1) = 1000, Protocol T = 0
-----
  TD(2) = 1F --> Y(i+1) = 0001, Protocol T = 15 - Global interface bytes following
-----
  TA(3) = C7 --> Clock stop: no preference - Class accepted by the card: (3G) A 5V B 3V C 1.8V
+ Historical bytes: 80 31 E0 73 FE 21 13 57 12 29 11 02 01 00 00
  Category indicator byte: 80 (compact TLV data object)
    Tag: 3, len: 1 (card service data byte)
      Card service data byte: E0
        - Application selection: by full DF name
        - Application selection: by partial DF name
        - BER-TLV data objects available in EF.DIR
        - EF.DIR and EF.ATR access services: by GET RECORD(s) command
        - Card with MF
    Tag: 7, len: 3 (card capabilities)
      Selection methods: FE
        - DF selection by full DF name
        - DF selection by partial DF name
        - DF selection by path
        - DF selection by file identifier
        - Implicit DF selection
        - Short EF identifier supported
        - Record number supported
      Data coding byte: 21
        - Behaviour of write functions: proprietary
        - Value 'FF' for the first byte of BER-TLV tag fields: invalid
        - Data unit in quartets: 2
      Command chaining, length fields and logical channels: 13
        - Logical channel number assignment: by the card
        - Maximum number of logical channels: 4
    Tag: 5, len: 7 (card issuer's data)
      Card issuer data: 12 29 11 02 01 00 00
+ TCK = C2 (correct checksum)

Possibly identified card (using /usr/share/pcsc/smartcard_list.txt):
3B 9F 95 80 1F C7 80 31 E0 73 FE 21 13 57 12 29 11 02 01 00 00 C2
        sysmocom sysmoUSIM-GR1
        http://sysmocom.de/
```

Hit `Ctrl+C` to exit `pcsc_scan`.

### Pyscard

Now you need to download and install Pyscard:

[http://pyscard.sourceforge.net/](http://pyscard.sourceforge.net/)

Download and extract the latest Pyscard version:

[https://sourceforge.net/projects/pyscard/files/pyscard/](https://sourceforge.net/projects/pyscard/files/pyscard/)

Go to the extracted Pyscard folder (where the setup.py file is located) and run the following command:

`sudo /usr/bin/python setup.py build_ext install`

### PySIM

Now get the code of PySIM:

```
git clone git://git.osmocom.org/pysim pysim
cd pysim
```

and run the `/pySim-read.py` to read your card:

`./pySim-read.py`

if you done everything allright, you will see something similar:

```
Reading ...
ICCID: 8901901550000123456
IMSI: 901550000123456
SMSP: fffffffffffffffffffffffffdffffffffffffffffffffffff069186770700f9ffffffffffffffff
ACC: ffff
MSISDN: Not available
Done !
```

Sometimes it is necessary to give the program the number of the card programmer:

`./pySim-read.py -p 0`      or       `./pySim-read.py -p 1`

Now we are ready to program the USIM finally! :-)

### Sysmo USIM Tool

Get Sysmo USIM tool:

`git clone git://git.sysmocom.de/sysmo-usim-tool`

We will need: `sysmo-usim-tool.sjs1.py`

### Programming the SIM card

**Important:**


In order to program the USIM cards, you must use the `zecke/tmp2` branch of Pysim. Please note that with the `zecke/tmp2` branch you can program but cannot read the cards. If you want to read the cards you will need to swtich back to the `master` branch. **If you are not using the** `zecke/tmp2` **branch or you are not giving the ADM1 pin correctly, you can permanently damage your card!!!**

To change branch to `zecke/tmp2`, use this command:

`git checkout zecke/tmp2`

Example to program a SysmoUSIM-SJS1 card:

```
./pySim-prog.py -p 0 --mcc 101 --mnc 02 -t sysmoUSIM-SJS1 --imsi 101020000000003 --iccid 8988211000000012345 --ki 8BAF473F2F8FD09487CCCBD7097C6862 --pin-adm 53770832
```

**IMPORTANT:** Where `-a` is the part where you need to give the `ADM1` for this specific SIM card. Again, if you are not using the `zecke/tmp2` branch or not giving the proper `ADM1` pin when you try to program the Sysmo-USIm S1J1 SIMs, you will likely end up with a permamnently damaged card!

Now lets set MILENAGE algorithm and the OP.

`./sysmo-usim-tool.sjs1.py --adm1 ADM1_KEY --set-op LTE_DEFAULT_OP_CODE -T MILENAGE:MILENAGE`

OpenLTE has a default OP code, which is:

`OP=63bfa50ee6523365ff14c1f45f88737d`

In case you want to change this value with your own, you need to edit `liblte/src/liblte_security.cc` and edit the OP value (this will require to compile OpenLTE again to make this change effective):

```c
static const uint8 OP[16] = {0x63,0xBF,0xA5,0x0E,0xE6,0x52,0x33,0x65,
0xFF,0x14,0xC1,0xF4,0x5F,0x88,0x73,0x7D};
```

**Reference:** [Specification of the MILENAGE algorithm - 3GPP TS 35.206](http://www.etsi.org/deliver/etsi_ts/135200_135299/135206/14.00.00_60/ts_135206v140000p.pdf)

**Reference:** [Sysmo USIM Manual](https://www.sysmocom.de/manuals/sysmousim-manual.pdf)

### Adding subscribers

Previously we created our own USIM cards and we know the Ki key and the IMSI for these cards.

Now we need to add them to the subscriber registry.

Start `LTE_fdd_enodeb` and log in to the control interface:

`telnet 127.0.0.1 30000`

These are the commands for adding, deleting and listing subscribers:

```
add_user imsi=<imsi> imei=<imei> k=<k> - Adds a user to the HSS (<imsi> and <imei> are 15 decimal digits, and <k> is 32 hex digits)
del_user imsi=<imsi>                   - Deletes a user from the HSS
print_users                            - Prints all the users in the HSS
```

To add a subscriber use the following command [k is the Ki key!]:


`add_user imsi=901550000123456 imei=864217022123456 k=D360C2591DE1BF61A11014C33D012246`

The terminal will respond with an `ok` if there were no mistypes in the syntax.

You can list the already added subscribers with:

`print_users`

You can delete the previously added subscriber with:

`del_user imsi=901550000123456`

**Note that the first 3 digits of IMSI is the MCC** (or Mobile Country Code) **and the two digits after the MCC is the MNC** (or Mobile Network Code). In the above example the MCC is 901 and the MNC is 55. It is not necessary but helps the Mobile Station a lot to set the MCC/MNC of your LTE network as your programmed SIM cards dictates. You can change the IMSI value during the SIM card programming stage also to match the specification of a test network: `MCC=001` and `MNC=01`.

**You should NEVER use the MCC/MNC configuration of a commercial provider!**


## Test Captures

In the `Test capture` folder there is a capture that is compatible with the `LTE_fdd_dl_file_scan` application. To use this capture with the octave receiver, use the following octave commands:

```octave
fid = fopen("/path/to/LTE_test_file_int8.bin", "r");
iq_vec = fread(fid, inf, "int8");
iq_split_vec = reshape(iq_vec, 2, []);
lte_fdd_dl_receive(iq_split_vec(1,:) + i*iq_split_vec(2,:));
```
