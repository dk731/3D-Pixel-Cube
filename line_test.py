from CubeDrawer import *
from time import sleep
import itertools
import random
import math

cd = CubeDrawer("8bit_8shades.txt")
# cd = CubeDrawer("4bit_2shades.txt")

# cd.push_matrix()
# cd.translate((8, 8, 8))
# while True:
#     cd.clear()
#     cd.circle((0, 0, 0), 5)
#     cd.draw()
#     cd.rotate((0, 0.1, 0))
#     sleep(0.1)
cd.push_matrix()
cd.translate((7, 7, 7))

while True:
    cd.clear()
    cd.rotate((0.01, 0.01, 0.01))
    cd.circle((0, 0, 0), 5)
    cd.draw()
    sleep(0.01)
