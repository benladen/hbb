import struct
from StringIO import StringIO

IP = "<IP ADDRESS HERE>"

sd = "HBBCFG"
sd += struct.pack("<B", 1) #Version
sd += struct.pack("<H", 5) #Entry Count
#Entry ID, Entry Length, Entry Data
sd += struct.pack("<BHH", 1, 5+len(IP), len(IP))+IP #IP
sd += struct.pack("<BHH", 2, 5, 40111) #Port
sd += struct.pack("<BHB", 3, 4, 0) #Download update if available (0 = No, 1 = Yes)
sd += struct.pack("<BHB", 4, 4, 1) #Use front touchscreen (0 = No, 1 = Yes)
sd += struct.pack("<BHB", 5, 4, 0) #Use rear touchpad (0 = No, 1 = Yes)

print repr(sd)

f = open("./config.dat", "wb")
f.write(sd)
f.close()