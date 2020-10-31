from multiprocessing import shared_memory, Semaphore
from math import floor, pi, sin, cos
import ast


class CubeDrawer:
    def __init__(self, pallete_file):
        self.size = (16, 16, 16)
        try:
            print(shared_memory._USE_POSIX)
            self.shm_obj = shared_memory.SharedMemory(name="VirtualCubeSHMemmory")
        except:
            raise Exception("Was not able to create shared memmory object.")
        print(
            "Successfuly connected to shared memmory object, with size: {}".format(
                self.shm_obj.size
            )
        )
        self.load_new_pallete(pallete_file)
        self.clear()
        self._init_drawing_()
        self.transform_list = list()

    def load_new_pallete(self, file_name):
        print("Loading pallete, from file {}".format(file_name))

        self.shm_obj.buf[-1] = 64  # setting flag for new pallete

        fp = open(file_name, "r")
        lines = fp.readlines()

        self.col_bits = int(lines[0][12::])
        self.col_amount = int(lines[1][14::])
        self.col_shades = int(lines[2][7::])
        self.shades_min = int(lines[3][10::])  # [0, ]
        print(
            "Pallete parametrs. Color deph: {}, Colors amout: {}, Shades: {}, Minimal shade value: {}".format(
                self.col_bits, self.col_amount, self.col_shades, self.shades_min
            )
        )

        self.shm_obj.buf[-1] = 64
        b1, b2 = self.col_bits.to_bytes(2, "big")
        self.shm_obj.buf[0] = b2
        self.shm_obj.buf[1] = b1
        b1, b2 = self.col_amount.to_bytes(2, "big")
        self.shm_obj.buf[2] = b2
        self.shm_obj.buf[3] = b1
        b1, b2 = self.col_shades.to_bytes(2, "big")
        self.shm_obj.buf[4] = b2
        self.shm_obj.buf[5] = b1
        b1, b2 = self.shades_min.to_bytes(2, "big")
        self.shm_obj.buf[6] = b2
        self.shm_obj.buf[7] = b1

        self.shm_obj.buf[-1] |= 128

        print("Loading pallete to threads...")
        while self.shm_obj.buf[-1] & 128:
            pass

        print("Pallete was loaded to arduinos")

    def _init_drawing_(self):
        self.colors = bytearray(self.size[0] * self.size[1] * self.size[2] * 3)
        self.current_color = (255, 255, 255)

    def draw(self):
        # self.transform_list.clear()
        # RASP draw

        while self.shm_obj.buf[-1] & 128:
            pass

        self.shm_obj.buf[-1] = 0

        print("Starting to write to shred memmory")

        self.shm_obj.buf[
            0 : len(self.colors)
        ] = self.colors  # loading from python array to shared memmory

        self.shm_obj.buf[-1] |= 128 + 32
        print("All data loaded, ready flag setted")

    def clear(self, color=None):
        self.colors = bytearray(self.size[0] * self.size[1] * self.size[2] * 3)

    def set_size(self, size):
        self.cursor_size = size

    def set_color(self, color):
        self.current_color = color

    def pixel_at(self, p):

        for tran in self.transform_list:
            xx, yy, zz = p

            rx, ry, rz = tran[1]
            p[0] = (
                (cos(rx) * cos(ry) * cos(rz) - sin(rx) * sin(rz)) * xx
                + (sin(rx) * cos(ry) * cos(rz) + cos(rx) * sin(rz)) * yy
                + (-sin(ry) * cos(rz)) * zz
            )

            p[1] = (
                (-cos(rx) * cos(ry) * sin(rz) - sin(rx) * cos(rz)) * xx
                + (-sin(rx) * cos(ry) * sin(rz) + cos(rx) * cos(rz)) * yy
                + (sin(ry) * sin(rz)) * zz
            )

            p[2] = (cos(rx) * sin(ry)) * xx + (sin(rx) * sin(ry)) * yy + (cos(ry)) * zz

            p[0] += tran[0][0]
            p[1] += tran[0][1]
            p[2] += tran[0][2]

        p = (floor(p[0] + 0.5), floor(p[1] + 0.5), floor(p[2] + 0.5))

        for a in p:
            if a not in range(16):
                return

        cur_p = (self.size[2] * self.size[1] * p[2] + self.size[0] * p[1] + p[0]) * 3
        self.colors[cur_p] = self.current_color[0]
        self.colors[cur_p + 1] = self.current_color[1]
        self.colors[cur_p + 2] = self.current_color[2]

    def push_matrix(self):
        self.transform_list.append([[0, 0, 0], [0, 0, 0], [1, 1, 1]])

    def translate(self, p):
        self.transform_list[-1][0][0] += p[0]
        self.transform_list[-1][0][1] += p[1]
        self.transform_list[-1][0][2] += p[2]

    def rotate(self, p):  # Radians
        self.transform_list[-1][1][0] += p[0]
        self.transform_list[-1][1][1] += p[1]
        self.transform_list[-1][1][2] += p[2]

    def pop_matrix(self):
        if len(self.transform_list) > 0:
            self.transform_list.pop()

    def line(self, p1, p2):
        line = (p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2])

        length = (line[0] ** 2 + line[1] ** 2 + line[2] ** 2) ** 0.5
        for i in range(floor(length + 0.5) + 1):
            t = i / length
            x = p1[0] + line[0] * t
            y = p1[1] + line[1] * t
            z = p1[2] + line[2] * t
            self.pixel_at([x, y, z])

    def box(self, p, size):
        p1 = []
        p2 = []
        for a, b in zip(p, size):
            p1.append(a - b / 2)
            p2.append(a + b / 2)

        self.line(p1, [p2[0], p1[0], p1[2]])
        self.line(p1, [p1[0], p2[0], p1[2]])
        self.line(p1, [p1[0], p1[0], p2[2]])

        self.line(p2, [p1[0], p2[0], p2[2]])
        self.line(p2, [p2[0], p1[0], p2[2]])
        self.line(p2, [p2[0], p2[0], p1[2]])

        self.line([p2[0], p1[0], p1[2]], [p1[0], p2[0], p2[2]])
        self.line([p2[0], p1[0], p1[2]], [p2[0], p1[0], p2[2]])

        self.line([p1[0], p2[0], p1[2]], [p1[0], p2[0], p2[2]])
        self.line([p1[0], p2[0], p1[2]], [p2[0], p1[0], p2[2]])

        self.line([p1[0], p1[0], p2[2]], [p1[0], p2[0], p2[2]])
        self.line([p1[0], p1[0], p2[2]], [p2[0], p1[0], p2[2]])

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

            self.pixel_at([x + p[0], y + p[1], p[2]])
            self.pixel_at([-x + p[0], y + p[1], p[2]])
            self.pixel_at([x + p[0], -y + p[1], p[2]])
            self.pixel_at([-x + p[0], -y + p[1], p[2]])

            if x != y:
                self.pixel_at([y + p[0], x + p[1], p[2]])
                self.pixel_at([-y + p[0], x + p[1], p[2]])
                self.pixel_at([y + p[0], -x + p[1], p[2]])
                self.pixel_at([-y + p[0], -x + p[1], p[2]])

        self.pixel_at([p[0] + radius, p[1], p[2]])
        self.pixel_at([p[0] - radius, p[1], p[2]])
        self.pixel_at([p[0], p[1] + radius, p[2]])
        self.pixel_at([p[0], p[1] - radius, p[2]])