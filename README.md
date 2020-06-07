gqrx-memory
===========

Global keyboard shortcuts quickly changing gqrx settings, such as frequency or demodulator mode. This makes it easier to monitor several frequencies and quickly switch between them, without the need to create or edit bookmarks inside the gqrx interface.

Building
---

```
$ git clone https://github.com/eoineoineoin/gqrx-memory.git
$ mkdir gqrx-memory/build
$ cd gqrx-memory/build
$ cmake .. && make
```

Usage
---

```$ ./gqrx-memory -h
gqrx-memory 1.0
Arguments:
  -h, --help             : This help message
  -s, --savefile <file>  : Load/save memory to <file>
  -c, --host <hostname>  : Connect to gqrx on <hostname> (default: localhost)
  -p, --port <port>      : Connect to gqrx on <port> (default: 7356)
```

Running gqrx-memory will attempt to connect to the gqrx remote control interface (the option "Remote control via TCP" option must be enabled in gqrx for this to work) where it is then controlled via keypresses F1, F2, F3, (etc.). Press and hold the function key for a second to save the current demodulator config. Just press the key to restore that state.

By specifying a file via ```--savefile /path/to/file```, these settings will be saved to disk keeping memory of configs across multiple runs.

