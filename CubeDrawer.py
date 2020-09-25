from multiprocessing import shared_memory
from math import floor, pi, sin, cos


class CubeDrawer:
    def __init__(self, debug, size):
        self.size = size
        self.debug = debug
        if self.debug:
            try:
                self.shm_obj = shm_a = shared_memory.SharedMemory(
                    name="VirtualCubeSHMemmory"
                )
            except Exception as e:
                raise Exception(
                    "Was not able to connect to shared memmory, probably Virtual Cube was not started"
                )
            print("Successfuly connected to virtual cube")
            print("Shared memmory block with: ", self.shm_obj.size, "bytes")
            print(
                "State byte: ",
                self.shm_obj.buf[0],
            )
        self.clear()
        self._init_drawing_()
        self.transform_list = list()

    def _init_drawing_(self):
        self.colors = bytearray(self.size[0] * self.size[1] * self.size[2] * 3)
        self.current_color = (255, 255, 255)
        self.cursor_size = 1

    def draw(self):
        # RASP draw
        if not self.debug:
            pass
        else:
            while self.shm_obj.buf[0] == 2:
                pass
            self.shm_obj.buf[1 : len(self.colors) + 1] = self.colors
            self.shm_obj.buf[0] = 1

    def clear(self, color=None):
        if not color:
            self.colors = bytearray(self.size[0] * self.size[1] * self.size[2] * 3)
        else:
            self.colors

    def set_size(self, size):
        self.cursor_size = size

    def set_color(self, color):
        self.current_color = color

    def pixel_at(self, p):
        for a in p:
            if a not in range(16):
                return

        cur_p = (self.size[2] * self.size[1] * p[2] + self.size[0] * p[1] + p[0]) * 3
        self.colors[cur_p] = self.current_color[0]
        self.colors[cur_p + 1] = self.current_color[1]
        self.colors[cur_p + 2] = self.current_color[2]

    def push_matrix():
        self.transform_list.push(dict())

    def translate(p):
        self.transform_list[-1]

    def pop_matrix():
        if self.transform_list.size() > 0:
            self.transform_list.pop()

    def line(self, p1, p2):
        line = (p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2])

        length = (line[0] ** 2 + line[1] ** 2 + line[2] ** 2) ** 0.5
        print(length)
        for i in range(floor(length + 0.5) + 1):
            t = i / length
            x = p1[0] + line[0] * t
            y = p1[1] + line[1] * t
            z = p1[2] + line[2] * t
            self.pixel_at((floor(x + 0.5), floor(y + 0.5), floor(z + 0.5)))

    def circle(self, p, radius):
        x = radius
        y = 0
        P = 1 - radius

        while x > y:
            y += 1
            if P <= 0:
                P = P + 2 * y + 1

            else:
                x -= 1
                P = P + 2 * y - 2 * x + 1

            if x < y:
                break

            self.pixel_at((x + p[0], y + p[1], p[2]))
            self.pixel_at((-x + p[0], y + p[1], p[2]))
            self.pixel_at((x + p[0], -y + p[1], p[2]))
            self.pixel_at((-x + p[0], -y + p[1], p[2]))

            if x != y:
                self.pixel_at((y + p[0], x + p[1], p[2]))
                self.pixel_at((-y + p[0], x + p[1], p[2]))
                self.pixel_at((y + p[0], -x + p[1], p[2]))
                self.pixel_at((-y + p[0], -x + p[1], p[2]))

        self.pixel_at((p[0] + radius, p[1], p[2]))
        self.pixel_at((p[0] - radius, p[1], p[2]))
        self.pixel_at((p[0], p[1] + radius, p[2]))
        self.pixel_at((p[0], p[1] - radius, p[2]))