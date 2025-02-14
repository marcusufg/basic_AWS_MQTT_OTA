infile = open('build.txt', 'r')
lines = infile.readlines()
for line in lines:
    # check if string present on a current line
    if line.find('bootloader.bin') != -1:
        print('Line Number:', lines.index(line))
        print('Line:', line)