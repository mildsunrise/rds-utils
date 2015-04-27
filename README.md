# rds-utils

Small collection of utilities to deal with [Radio Data System][RDS] in general.

This is old code I rescued from GNU Radio and adapted to make independent utilities.
Work in progress.

## Install

First get LibXML2:

    sudo apt-get install libxml2-dev

Then, compile the utilities:

    make

That's it. You can optionally install them with `sudo make install`.

## Usage

For now there is only `rdsencode`, which reads an XML file and outputs an RDS blob.
You can see an example in `example.xml`.

    rdsencode example.xml > example.rds




[RDS]: https://en.wikipedia.org/wiki/Radio_Data_System
