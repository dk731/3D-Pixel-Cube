#include <iostream>
#include <thread>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

const unsigned char FLAG_CSHM = 128; // if shm is 1 than ready for copy
const unsigned char FLAG_RTW = 64; // buf is ready to be written to arduino's
const unsigned char FLAG_NP = 32; // data comming shoud be loaded as new pallete

const int BUFFER_SIZE = 32;

struct shm_buf {
    unsigned char buf[12288]{0}; // 16^3 * 3 - max size in bytes
    unsigned char color_size; // size in bits
    unsigned short int states; // states of each arduino
    unsigned char flags; // general purpose flags
};

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

void rewrite_bit(void* to, int indt, void* from, int indf) // rewrites bit from one buffer to other at specific bit positions
{
    int bit_shiftf = indf % 8;
    int amount_of_full_bytesf = (indf - bit_shiftf) / 8;

    int bit_shiftt = indt % 8;
    int amount_of_full_bytest = (indt - bit_shiftt) / 8;

    if (*((unsigned char *)from + amount_of_full_bytesf) & (128 >> bit_shiftf))
        *((unsigned char *)to + amount_of_full_bytest) |= (128 >> bit_shiftt);
}

void print_buf(void *buf, int buf_size, int bytes_inline) // Used in debug purpose, buf_size % bytes_inline == 0  !!!
{
    for (int i = 0; i < buf_size / bytes_inline; i ++)
    {
        for (int j = 0; j < bytes_inline; j++)
        {
            for (int l = 0; l < 8; l++)
            {
                //std::cout << "l: " << l << " i: " << i << " j: " << j << (l + (i * bytes_inline * 8) + j*8) << std::endl;
                std::cout << static_cast<unsigned>(get_bit_at(buf, l + (i * bytes_inline * 8) + j * 8));
            }
            std::cout << "   ";
        }
        std::cout << std::endl;
    }
}

void i2c_writer_thread(int i2c_port, int arduino_id, shm_buf* buffer) // arduino id - id of arduinos's pack of 4
{
    char filename[11] = "/dev/i2c-";
    filename[9] = i2c_port + '0';
    
    int i2cf = open(filename, O_RDWR);

    if (i2cf < 0)
    {
        buffer->color_size |= 1 << arduino_id;
        return;
    }

    std::cout << "Sucssefuly oppened device: " << filename << std::endl;


    for (int i = 0; i < 4; i++)
    {
        //if (ioctl(i2cf, I2C_SLAVE, i) >= 0)
        if (true)
            buffer->states |= 1 << (arduino_id * 4 + i);
    }


    while (true)
    {
        if (buffer->flags & FLAG_RTW)
        {
            if (buffer->flags & FLAG_NP)
            {
                std::cout << "Uploding new pallete" << std::endl;

                continue;
            }

            int size_of_layer = buffer->color_size * 16 * 16; // in bits
            int pack_start_bit = arduino_id * 4 * size_of_layer; 

            for (int i = 0; i < 4; i++)
            {
                std::cout << "Arduino id: " << i << std::endl;
                //if (ioctl(i2cf, I2C_SLAVE, i) >= 0)
                if (false)
                {
                    std::cout << "Error, was not able to find device with id: " << i << std::endl;
                    continue;
                }
                int layer_start_bit = i * size_of_layer + pack_start_bit;

                for (int l = 0; l < buffer->color_size; l++) // Loop for wrting data in full chunks ( each 32 bytes )
                {
                    unsigned char send_buffer[32]{0};
                    int chunk_start_bit = l * 256 + layer_start_bit;

                    std::cout << "Packs start bit: " << pack_start_bit << ",  Layer start bit: " << layer_start_bit << ",  Chunk start bit: " << chunk_start_bit << std::endl;

                    for (int b = 0; b < 256; b++)
                        rewrite_bit(&send_buffer, b, buffer, chunk_start_bit + b);

                    // std::cout << "Buffer: " << l << "" << std::endl
                    //           << std::endl;
                    // print_buf(send_buffer, 32, 4);

                    //write(i2cf, send_buffer, 32);
                }
            }
            buffer->states |= 15 << arduino_id;
            return;
        }
    }
    


    /*
        On init check all devices
    */


}



void i2c_mem_cpy(shm_buf* shm_b, shm_buf* c_buf)
{

    std::cout << "Generating and loading debug data ..." << std::endl;
    int debug_buf_size = 384 * 2;
    unsigned char buf[debug_buf_size]{0};

    for (int i = 0; i < debug_buf_size; i++)
    {
        int a = rand() % 255;
        buf[i] = a;
        std::cout << static_cast<unsigned>(buf[i]) << "; ";
    }
    std::cout << std::endl;

    std::cout << "Printing debug data: " << std::endl << std::endl;

    memcpy(c_buf, &buf, debug_buf_size);
    print_buf(c_buf, debug_buf_size, 16);

    std::cout << std::endl;

    c_buf->color_size = 3;

    std::cout << "Loaded all data and flags" << std::endl;
    c_buf->flags |= FLAG_RTW;
    return;

    ///////////////////////////////////////

    std::cout << "Wait for data" << std::endl;
    while (true)
    {
        if (shm_b->flags & FLAG_CSHM)
        {
            while (true)
            {
                if (c_buf->states == 65535)
                    break;
            }
            memcpy(c_buf, shm_b, sizeof(shm_buf));
            shm_b->flags &= ~FLAG_CSHM;
        }
        else
            usleep(1000);
    }
}

int main()
{
    srand (time(NULL));


    shm_buf cbuf;
    int shmid = shm_open("VirtualCubeSHMemmory", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shmid == -1)
    {
        std::cout << "Was not able to create shared memmory object." << std::endl;
        return 1;
    }
    std::cout << "Sucssesfuly created SHM object, with id: " << shmid <<  std::endl;

    if (ftruncate(shmid, sizeof(shm_buf)) == -1)
    {
        std::cout << "Was not able to truncate file with length: 12292" << std::endl;
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

    std::thread th1w(i2c_writer_thread, 1, 0, &cbuf);

    std::thread copy_thread(i2c_mem_cpy, shmb, &cbuf);

    
    th1w.join();
    copy_thread.join();

    // std::cin >> a;

    // for (int i = 0; i < 256; i++)
    // {
    //     std::cout << static_cast<unsigned>(shmb->buf[i * 3]) << ", " << static_cast<unsigned>(shmb->buf[i * 3 + 1]) << ", " << static_cast<unsigned>(shmb->buf[i * 3 + 2]) << std::endl;
    // }

    // std::cin >> a;
    shm_unlink("VirtualCubeSHMemmory");

    return 0;
}