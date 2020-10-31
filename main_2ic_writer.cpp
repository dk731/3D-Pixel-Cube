#include <iostream>
#include <thread>
#include <cmath>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

const unsigned char FLAG_CSHM = 128; // if shm is 1 than ready for copy
const unsigned char FLAG_NP = 64;    // new pallete flag
const unsigned char FLAG_LD = 32;    // ready for loading frame data

const unsigned char FLAG_ERRI = 128; // if error happend during initialization, flag will be 1
const unsigned char FLAG_ERRD = 64;  // if error happend during i2c handle mapping ( device was not found )
const unsigned char FLAG_WR = 1;     // all arduinos were written

const unsigned char COMMAND_LD = 1; // loading new data.
const unsigned char COMMAND_NP = 2; // command to arduino, new pallete is comming

struct pallete
{
    unsigned short int size = 0; // one color size in bits
    unsigned short int colors_amount = 0;
    unsigned short int shades_amount = 0;
    unsigned short int unmapped_min = 0; // [0,  65535]
    unsigned short int grey_amount = 0;
    float hdelta = 0.0f;
    float sdelta = 0.0f;
    float bdelta = 0.0f;
    float min_val = 0.0f;

    void init()
    {
        hdelta = 1.0f / colors_amount;
        min_val = unmapped_min / 65535.0f;
        sdelta = (1.0f - min_val) / shades_amount * 2.0f;
        grey_amount = ((unsigned short int)pow(2, size) - 1) - (colors_amount * (shades_amount + 1)) - 1;
        bdelta = 1.0f / grey_amount;
    }
};

struct shm_buf
{
    unsigned char buf[12288]{0}; // 16^3 * 3 - max size in bytes
    unsigned char flags{0};      // general purpose flags
};

struct c_buf
{
    unsigned char command{0};
    unsigned char buf[3092]{0};
    unsigned int buf_size{0}; // amount of full packets (32 bytes)
    unsigned char states{0};
    unsigned char flags{0};
};

struct hsv
{
    double h; // angle in degrees
    double s; // a fraction between 0 and 1
    double v;
};

hsv rgb2hsv(unsigned char *ptc)
{

    float fR = ptc[0] / 255.0f;
    float fG = ptc[1] / 255.0f;
    float fB = ptc[2] / 255.0f;

    hsv out;

    float fCMax = std::max(std::max(fR, fG), fB);
    float fCMin = std::min(std::min(fR, fG), fB);
    float fDelta = fCMax - fCMin;

    if (fDelta > 0)
    {
        if (fCMax == fR)
        {
            out.h = (fmod(((fG - fB) / fDelta), 6));
        }
        else if (fCMax == fG)
        {
            out.h = (((fB - fR) / fDelta) + 2);
        }
        else if (fCMax == fB)
        {
            out.h = (((fR - fG) / fDelta) + 4);
        }

        if (fCMax > 0)
        {
            out.s = fDelta / fCMax;
        }
        else
        {
            out.s = 0;
        }

        out.v = fCMax;
    }
    else
    {
        out.v = 0;
        out.v = 0;
        out.v = fCMax;
    }
    out.h /= 6.0f;
    if (out.h < 0)
        out.h += 1;
    return out;
}

bool get_bit_at(void *buf, int i)
{
    int bit_shift = i % 8;
    int amount_of_full_bytes = (i - bit_shift) / 8;
    if (*((unsigned char *)buf + amount_of_full_bytes) & (128 >> bit_shift))
        return 1;
    else
        return 0;
}

void set_bit_at(void *buf, int i, bool val) // buf - should be zeros
{
    int bit_shift = i % 8;
    int amount_of_full_bytes = (i - bit_shift) / 8;
    *((unsigned char *)buf + amount_of_full_bytes) |= 1 << bit_shift;
}

void rewrite_color(void *buf, int col_size, int col_ind, unsigned int col)
{
    unsigned char *buff = (unsigned char *)buf;
    int bit_ind = col_size * col_ind;

    *(unsigned int *)&buff[bit_ind >> 3] |= col << (bit_ind % 8);
}

void print_buf(void *buf, int buf_size, int bytes_inline) // Used in debug purpose, buf_size % bytes_inline == 0  !!!
{
    for (int i = 0; i < buf_size / bytes_inline; i++)
    {
        for (int j = 0; j < bytes_inline; j++)
        {
            for (int l = 7; l >= 0; l--)
            {
                //std::cout << "l: " << l << " i: " << i << " j: " << j << (l + (i * bytes_inline * 8) + j*8) << std::endl;
                std::cout << static_cast<unsigned>(get_bit_at(buf, l + (i * bytes_inline * 8) + j * 8));
            }
            std::cout << "   ";
        }
        std::cout << std::endl;
    }
}

void i2c_writer_thread(int i2c_port, int arduino_id, c_buf *buffer) // arduino id - id of arduinos's pack of 4
{
    char filename[11] = "/dev/i2c-";
    filename[9] = i2c_port + '0';

    int i2cf = open(filename, O_RDWR);

    if (i2cf < 0)
    {
        buffer->flags |= FLAG_ERRI;
        return;
    }

    for (int i = 0; i < 4; i++)
    {
        //if (ioctl(i2cf, I2C_SLAVE, i) < 0)
        if (true)
        {
            buffer->flags |= FLAG_ERRD;
            buffer->states |= 1 << (i * 4);
        }
    }

    while (true)
    {
        if (buffer->flags & FLAG_WR == 0)
        {

            // Loading buffer to all arduinos

            for (int i = 0; i < 4; i++)
            {

                int layer_byte = buffer->buf_size * 32 * i;
                //if (ioctl(i2cf, I2C_SLAVE, i) < 0)
                if (false)
                {
                    //std::cout << "Error, was not able to find device with id: " << i << std::endl;
                    buffer->flags |= FLAG_ERRD;
                    buffer->states |= 1 << i;
                    continue;
                }

                write(i2cf, &(buffer->command), 1);

                for (int l = 0; l < buffer->buf_size; l++) // Loop for wrting data in full chunks ( each 32 bytes )
                {
                    write(i2cf, &(buffer->buf) + layer_byte + l * 32, 32);
                }
            }
            buffer->flags |= FLAG_WR;
        }
    }

    /*
        On init check all devices
    */
}

void i2c_mem_cpy(shm_buf *shm_b, c_buf *c_buf)
{
    pallete cur_pallete;
    std::cout << "Waiting for cshm flag" << std::endl;
MAIN_LOOP:
    while (true)
    {
        if (shm_b->flags & FLAG_CSHM)
        {
            std::cout << "Catched cshm flag" << std::endl;
            while (false) // Wait until all threads done their writing to arduinos
            {
                bool ready = true;

                for (int i = 0; i < 4; i++)
                {
                    ready = (c_buf[i].flags & FLAG_WR) == 0;
                }

                //if (ready)
                if (true)
                    break;

                usleep(1000);
            }
            for (int i = 0; i < 4; i++)
                c_buf[i].flags = 0;
            //std::cout << static_cast<unsigned>(shm_b->flags) << std::endl;
            if (shm_b->flags & FLAG_NP) // loads aplletes to all arduinos
            {
                memcpy(&cur_pallete.size, &shm_b->buf, 8);
                cur_pallete.init();

                for (int i = 0; i < 4; i++)
                {
                    c_buf[i].command = COMMAND_NP;
                    c_buf[i].buf_size = 8;
                    for (int j = 0; j < 4; j++)
                        memcpy(&c_buf[i].buf[j * 8], &cur_pallete.size, 8);

                    c_buf[i].flags |= FLAG_WR;
                }
                std::cout << "Loaded new pallete to threads" << std::endl;
                shm_b->flags &= ~FLAG_CSHM;
                goto MAIN_LOOP;
            }

            hsv pcol;

            if (shm_b->flags & FLAG_LD)
            {
                std::cout << "Loading new data flag was detected" << std::endl;
                for (int i = 0; i < 4; i++)
                {
                    memset(&c_buf[i], 0, 3092);
                    std::cout << "Starting loading to leg: " << i << std::endl;
                    c_buf[i].command = COMMAND_LD;
                    int col_ind = 0;
                    for (int l = i << 2; l < ((i << 2) + 4); l++)
                    {
                        int z = l;
                        int dif = (z % 2 == 0) ? 1 : -1;
                        for (int y = 15; y >= 0; y--)
                        {

                            int for_v = (z % 2 == 0) ? 1 : -1;
                            for (int x = ((for_v > 0) ? 0 : 15); (x >= 0 && x < 16); x += for_v)
                            {
                                int indd = (x + (y << 4) + (z << 8)) * 3;
                                pcol = rgb2hsv(&shm_b->buf[indd]);

                                unsigned int ind = (pcol.s < cur_pallete.min_val) ? (cur_pallete.colors_amount * (cur_pallete.shades_amount + 1)) + round(pcol.v / cur_pallete.bdelta) : ((pcol.h / cur_pallete.hdelta) * (cur_pallete.shades_amount + 1)) + (pcol.v >= 0.8 ? (1 - pcol.s) / cur_pallete.sdelta : (1 - pcol.v) / cur_pallete.sdelta + cur_pallete.shades_amount / 2);

                                rewrite_color(&c_buf[i].buf, cur_pallete.size, col_ind++, ind);
                            }
                            z += dif;
                            dif *= -1;
                        }
                    }
                    std::cout << "All data for leg: " << i << " was loaded, priting content: " << std::endl;

                    print_buf(&c_buf[i].buf, cur_pallete.size << 5, 32);
                    std::cout << std::endl;
                    c_buf[i].flags |= FLAG_WR;
                }
            }
            shm_b->flags &= ~FLAG_CSHM;
        }
        else
            usleep(1000);
    }
}

int main()
{

    srand(time(NULL));

    c_buf c_buffers[4];

    int shmid = shm_open("VirtualCubeSHMemmory", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shmid == -1)
    {
        std::cout << "Was not able to create shared memmory object." << std::endl;
        return 1;
    }
    std::cout << "Sucssesfuly created SHM object, with id: " << shmid << std::endl;

    if (ftruncate(shmid, sizeof(shm_buf)) == -1)
    {
        std::cout << "Was not able to truncate file with length: 12288" << std::endl;
        return 1;
    }
    std::cout << "Sucssesfuly truncated to file with length: " << sizeof(shm_buf) << std::endl;
    shm_buf *shmb = (shm_buf *)mmap(NULL, sizeof(shm_buf), PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
    if (shmb == MAP_FAILED)
    {
        std::cout << "Failed to map to pointer" << std::endl;
        return 1;
    }

    int a;

    // std::thread th1w(i2c_writer_thread, 1, 0, &c_buffers[0]);
    // std::thread th2w(i2c_writer_thread, 3, 1, &c_buffers[1]);
    // std::thread th3w(i2c_writer_thread, 4, 2, &c_buffers[2]);
    // std::thread th4w(i2c_writer_thread, 5, 3, &c_buffers[3]);

    usleep(1000000); // wait till threads initialize
    std::thread copy_thread(i2c_mem_cpy, shmb, c_buffers);
    // bool error = false;

    // for (int i = 0; i < 4; i++) // // statuses of i2c legs of rasp
    // {
    //     std::cout << cbuf.color_size << std::endl;
    //     if (cbuf.color_size & (1 << i))
    //     {
    //         std::cout << "Was not able to open i2c with arduino id: " << i << std::endl;
    //         std::cout << "Continue without arduino's with id: " << i << " ? [y/n]: ";

    //         char inp;
    //         std::cin >> inp;
    //         if (inp == 'n' || inp == 'N')
    //         {
    //             std::cout << "Stopping all threads, clossing shared memmory ..." << std::endl;
    //             goto end;
    //         }
    //     }
    //     else
    //     {
    //         std::cout << "Sucssefuly initialized i2c bus in thread with id: " << i << std::endl;
    //     }
    // }

    // for (int i = 0; i < 16; i++)
    // {
    //     if (cbuf.states & (1 << i))
    //     {
    //         std::cout << "Was not able to detect arduino, pack id: " << i / 4 << " with id: " << i % 4 << std::endl;
    //         error = true;
    //     }
    // }
    // if (error)
    // {
    //     std::cout << "Some arduino's were not detected, want to stop?[y/n]: ";
    //     char inp;
    //     std::cin >> inp;
    //     if (inp == 'y' || inp == 'Y')
    //     {
    //         std::cout << "Stopping all threads, clossing shared memmory ..." << std::endl;
    //         goto end;
    //     }

    // }

    // th1w.join();
    copy_thread.join();

    // std::cin >> a;

    // for (int i = 0; i < 256; i++)
    // {
    //     std::cout << static_cast<unsigned>(shmb->buf[i * 3]) << ", " << static_cast<unsigned>(shmb->buf[i * 3 + 1]) << ", " << static_cast<unsigned>(shmb->buf[i * 3 + 2]) << std::endl;
    // }

    // std::cin >> a;
end:
    shm_unlink("VirtualCubeSHMemmory");

    return 0;
}