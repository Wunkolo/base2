# base2 [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

In the same spirit as the gnu coreutils software [base64](https://www.gnu.org/software/coreutils/manual/html_node/base64-invocation.html), `base2` transforms data read from a file, or standard input, into (or from) base2(binary text) encoded form.

Because I was bored.

```
base2 [Options]... [File]
base2 --decode [Options]... [File]
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
---
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
Options:
```
-w columns
-wrap=cols
```

When encoding, wraps lines after "cols" characters. "Cols" must be a positive number.
Defaults to `76` characters. A value of `0` disables wrapping altogether.

```
-d
--decode
```
Decodes incoming ascii-binary characters into individual bytes

```
-i
--ignore-garbage
```

When decoding, ignores all non-ascii-binary `0`, `1` bytes