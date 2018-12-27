# base2 [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

In the same spirit as gnu [base64](https://www.gnu.org/software/coreutils/manual/html_node/base64-invocation.html). `base2` transforms data read from a file, or standard input, into (or from) base2(binary text) encoded form.

Because I was bored.

```
base2 [option]... [file]
base2 --decode [option]... [file]
```

`-w columns`
`-wrap=cols`
When encoding, wraps lines after "cols" characters. "Cols" must be a positive number.
Defaults to `76` characters. A value of `0` disables wrapping altogether.

`-d`
`--decode`
Decodes incoming ascii-binary characters into individual bytes

`-i`
`--ignore-garbage`
When decoding, ignores all non-ascii-binary '0' '1' bytes
