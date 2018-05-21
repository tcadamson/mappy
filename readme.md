# mappy
Takes as input a single character sheet to produce an AngelCode BMFont description and an accompanying packed texture atlas. The input character sheet must consist of only two colors and have no padding on the edges.

![Sample output](https://my.mixtape.moe/dwdzjq.png)

## Usage
```
mappy [input file] [options]
-o output file
-w char width
-h char height
-c column separation
-r row separation
-s char sequence
```

The output file option ignores any extension provided. It's used as the name for the exported .png and .fnt files. Default is [input file (name part)]\_output. Char width, char height, column separation, and row separation are all in pixels and assumed to be constant throughout the sheet. The char sequence defines all chars included in the sheet in the order they appear, left to right, top to bottom. If a custom char sequence is not provided, ASCII characters 33-126 will be used:
```
!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
```

## Example
```
mappy input.png -o output -w 6 -h 11 -s 0123456789abcdefg#?!
```