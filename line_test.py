from CubeDrawer import *
from time import sleep
import itertools
import random
import math


cd = CubeDrawer("pallete.txt")
cd.draw()
# cd = CubeDrawer("4bit_2shades.txt")

cd.push_matrix()
cd.translate((8, 8, 8))
while True:
    cd.set_color(
        (random.randint(0, 255), random.randint(0, 255), random.randint(0, 255))
    )
    cd.clear()
    cd.circle((0, 0, 0), 5)
    cd.draw()
    cd.rotate((0, 0.1, 0))
    sleep(0.1)

# while True:
#     cd.clear()
#     cd.rotate((0.01, 0.01, 0.01))
#     cd.circle((0, 0, 0), 5)
#     cd.draw()
#     sleep(0.01)
