# TermiArt v0.0.1
## pixel art in your terminal

![TermiArt Logo](termi-logo.png)

## Introduction and Dependencies

TermiArt is a program for creating and displaying pixel art, in your terminal.

It only supports Linux, and you must have g++-14 installed.

## Downloading and Building

Clone the git repository:
```sh
git clone https://github.com/ari-feiglin/termi-art-v2
```
and navigate into the resulting directory:
```sh
cd termi-art-v2
```
Now compile the source:
```sh
g++-14 -std=c++23 termiart.cpp -o tart
```
This will create the binary `tart` which you can run.

## Running

Run `./tart --help` for a help menu.

