import base64
import os
import math
with open("image.png","rb") as image:
    #print(os.path.getsize("image.png"))
    b = base64.b64encode(image.read(3))
    for i in range(math.ceil((os.path.getsize("image.png") / 3))):
        b = b + base64.b64encode(image.read(3))
    out = open("output.txt","w")
    out.write(str(b))
    out.close()
    #print(b)
