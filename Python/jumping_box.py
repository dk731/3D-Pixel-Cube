from CubeDrawer import *
from time import sleep
import random

import math


cd = CubeDrawer(True, (16, 16, 16))
cd.clear()

cd.box((5, 5, 5), (4, 4, 4))
cd.draw()
