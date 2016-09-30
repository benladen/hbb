from StringIO import StringIO
import Tkinter, tkFileDialog
import zipfile
import struct

root = Tkinter.Tk()
root.withdraw()

fileLoc = tkFileDialog.askopenfilename()

zf = zipfile.ZipFile(fileLoc, 'r')
fileData = zf.read("sce_sys/param.sfo")
zf.close()
fdio = StringIO(fileData)

header = [fdio.read(4)] + list(struct.unpack("<IIII", fdio.read(16)))
indexTable = []
i = 0
while i < header[4]:
    it = list(struct.unpack("<HHIII", fdio.read(16)))
    indexTable.append(it)
    i += 1

for j in indexTable:
    name = ""
    i = 0
    c = ""
    while c != "\x00":
        c = fileData[header[2]+j[0]+i]
        if c != "\x00":
            name += c
        i += 1
    if name == "TITLE_ID":
        TITLE_ID = fileData[header[3]+j[4]:header[3]+j[4]+j[2]-1]
    elif name == "TITLE":
        TITLE = fileData[header[3]+j[4]:header[3]+j[4]+j[2]-1]

print "TITLE_ID:", repr(TITLE_ID)
print "TITLE:", repr(TITLE)
raw_input("Press Return to exit.")