from CubeDrawer import *
from time import sleep
import itertools
import random


cd = CubeDrawer(True, (16, 16, 16))

while True:
    for a in range(16):
        cd.clear()
        cd.circle((7, 7, a), a)
        cd.draw()
        sleep(0.1)
