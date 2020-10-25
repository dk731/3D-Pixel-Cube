#include <iostream>
#include <thread>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

const unsigned char FLAG_CSHM = 128; // if shm is 1 than ready for copy

struct shm_buf {
    unsigned char buf[12288]; // 16^3 * 3
    unsigned char buf_size;
    unsigned char state1;
    unsigned char state2;
    unsigned char flags;
};

void i2c_writer_thread(int i2c_port, int arduino_id, shm_buf* buffer) 
{
    char filename[11] = "/dev/i2c-";
    filename[9] = i2c_port + '0';
    
    int i2cf = open(filename, O_RDWR);

    if (i2cf < 0)
    {
        buffer->buf_size |= 1 << arduino_id;
        return;
    }

    std::cout << "Sucssefuly oppened device: " << filename << std::endl;


    for (int i = 0; i < 4; i++)
    {
        if (ioctl(i2cf, I2C_SLAVE, i) >= 0)
            *(unsigned short int *)(&buffer->state1) |= 1 << (arduino_id * 4 + i);
    }


    while (true)
    {

        for (int i = 0; i < 4; i++)
        {
            if (ioctl(i2cf, I2C_SLAVE, i) >= 0)
            {
                std::cout << "Error, was not able to find device with id: " << i << std::endl;
                continue;
            }

            int resp = write(i2cf, buffer , buffer->buf_size)
        }
    }
    


    /*
        On init check all devices
    */


}

void i2c_mem_cpy(shm_buf* shm_b, shm_buf* c_buf)
{
    std::cout << "Wait for data" << std::endl;
    while (true)
    {
        if (shm_b->flags & FLAG_CSHM)
        {
            memcpy(c_buf, shm_b, sizeof(shm_buf));
            shm_b->flags &= ~FLAG_CSHM;
            shm_b->state1 = 255;
            shm_b->state2 = 255;
        }
        else
            usleep(1000);
    }
}

main()
{
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

    std::cin >> a;

    for (int i = 0; i < 256; i++)
    {
        std::cout << static_cast<unsigned>(shmb->buf[i * 3]) << ", " << static_cast<unsigned>(shmb->buf[i * 3 + 1]) << ", " << static_cast<unsigned>(shmb->buf[i * 3 + 2]) << std::endl;
    }

    std::cin >> a;
    shm_unlink("VirtualCubeSHMemmory");

    return 0;
}