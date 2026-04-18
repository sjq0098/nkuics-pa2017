# ICS2017 Programming Assignment

This project is the programming assignment of the class ICS(Introduction to Computer System) in Department of Computer Science and Technology, Nanjing University.

For the guide of this programming assignment,
refer to http://nju-ics.gitbooks.io/ics2017-programming-assignment/content/

To initialize, run
```bash
bash init.sh
```

The following subprojects/components are included. Some of them are not fully implemented.
* [NEMU](https://github.com/NJU-ProjectN/nemu)
* [Nexus-am](https://github.com/NJU-ProjectN/nexus-am)
* [Nanos-lite](https://github.com/NJU-ProjectN/nanos-lite)
* [Navy-apps](https://github.com/NJU-ProjectN/navy-apps)

## Local Notes

We changed `nemu/Makefile` so that `make`, `make run`, and `make gdb` no longer create a git commit by default.
Automatic commits are now optional and only happen when `AUTO_COMMIT=1` is provided explicitly.

Current NEMU workflow:

```bash
cd nemu
make
./build/nemu
```

You can also use:

```bash
cd nemu
make run
make gdb
```

If the old behavior is ever needed:

```bash
cd nemu
make AUTO_COMMIT=1
make run AUTO_COMMIT=1
make gdb AUTO_COMMIT=1
```
