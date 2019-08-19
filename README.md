# Introduction
This repo contains a Windows command-line application that can convert any text file into an array of type `const char` for inclusion in C code.  For any input text file, e.g. `file1.txt` you will get a file `file1.array` containing the variable definition.  For instance, if `file1.txt` contained:

```
This is my text file.
```

...the output would be a file called `file1.array` containing:

```
/* This file was created from input file file1.txt by arrayify */

const char file1[] = "This is my text file.";

// End of file
```

You could then include `file1.array` in your C source code and make use of the variable `file1`.

# Usage
A pre-built binary is included (in the `bin` directory) which should run on any Windows machine.  Run the executable from a command prompt to get command-line help.

# Building
The source code may be built under Microsoft Visual C++ 2010 (and presumably later) Express.  It is pure C++ code and so can probably be built on Linux etc. with a Make file but you may find that the very end of the output files that the executable generates may be missing a closing quotation mark as the code assumes that the write to file will convert a newline, `\n`, into `\r\n`, as is customary on Windows and is not the case on Linux.