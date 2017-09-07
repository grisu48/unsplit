# unsplit

In-situ binary file merger

This is a really simple tool to merge file that have been splitted using the 'split' command. The tools appends all the given files to the first one on-place.

## Build instructions

A simple make should do the job for most systems

    make

The Makefile is trivial and can be edited easily. For installing the program in the system use

    sudo make install


## Usage

For instance to merge the files xaa xab xac and xad, type:

    unsplit xaa xab xac xad

The file xaa is the merged file after the call.

## Features

Fast in-sito tool for merging splitted files. If something went wrong during the process (e.g. no more disk space available) the program tries to truncate the file back to it's original state.

In contrast to the ''cat'' tool that is supposed to be the canonical way of merging files together, this program tries to recover the original file in case something went wrong.
