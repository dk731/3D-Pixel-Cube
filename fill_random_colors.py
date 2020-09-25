from CubeDrawer import *
from time import sleep
import itertools
import random

all_posses = []
for z in range(16):
    for y in range(16):
        for x in range(16):
            all_posses.append((x, y, z))

all_posses = random.sample(all_posses, 16 ** 3)


cd = CubeDrawer(True, (16, 16, 16))

for p in all_posses:
    cd.set_color(
        (random.randint(0, 255), random.randint(0, 255), random.randint(0, 255))
    )
    cd.pixel_at(p)
    cd.draw()
    # cd.clear()
    sleep(0.1)

# cd.draw()
