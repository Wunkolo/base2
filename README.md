# base2 [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

In the same spirit as the gnu coreutils software [base64](https://www.gnu.org/software/coreutils/manual/html_node/base64-invocation.html), `base2` transforms data read from a file, or standard input, into (or from) base2(binary text) encoded form.

Because I was bored.

```
base2 - Wunkolo <wunkolo@gmail.com>
Usage: base2 [Options]... [File]
       base2 --decode [Options]... [File]
Options:
  -h, --help            Display this help/usage information
  -d, --decode          Decode's incoming binary ascii into bytes
  -i, --ignore-garbage  When decoding, ignores non-ascii-binary `0`, `1` bytes
  -w, --wrap=Columns    Wrap encoded binary output within columns
                        Default is `76`. `0` Disables linewrapping
```
---
Encoding:
```
% base2 <<< 'QWERTY'
01010001010101110100010101010010010101000101100100001010
% base2 --wrap=8 <<< 'QWERTY'
01010001                          # 'Q'
01010111                          # 'W'
01000101                          # 'E'
01010010                          # 'R'
01010100                          # 'T'
01011001                          # 'Y'
00001010                          # '\n'
```
Decoding:
```
% base2 -d <<< '01010001010101110100010101010010010101000101100100001010'
QWERTY
% base2 -d <<< '010100010101
011101000garbage1010blah101001001010garbage1000101100100001010'
QWFz*J�B
% base2 -d -i <<< '010100010
101011101000garbage1010blah101001001010garbage1000101100100001010'
QWERTY
```

---

Did I mention its fast:

```
inxi -C
CPU:       Topology: Dual Core model: Intel Core i3-6100 bits: 64 type: MT MCP L2 cache: 3072 KiB 
           Speed: 3700 MHz min/max: 800/3700 MHz Core speeds (MHz): 1: 3700 2: 3700 3: 3700 4: 3700 
```

Generic:
```
./base2 --wrap=0 /dev/urandom | pv > /dev/null
	47GiB 0:00:10 [ 656MiB/s]
```

BMI2
```
./base2 --wrap=0 /dev/urandom | pv > /dev/null
	60GiB 0:00:10 [ 787MiB/s]
```
