with open("temp.txt", 'rb+') as f:
    f.seek(0,2)                 # end of file
    size=f.tell()               # the size...
    f.truncate(size-1)          # truncate at that size - how ever many characters