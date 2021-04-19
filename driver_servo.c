/*
*** sudo i2cdetect -y 1
*** sudo apt install raspberrypi-kernel-headers
*** Enable I2C bus
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>


#define I2C_BUS_AVAILABLE   (               1 ) // Số thiết bị I2C đang hoạt động
#define SLAVE_DEVICE_NAME   ( "PCA9685_SERVO" ) // Tên thiết bị
#define PWM_I2C_Addr        (            0x40 ) // Địa chỉ I2C Bus của PCA9685

static struct i2c_adapter *etx_i2c_adapter     = NULL;  // I2C Adapter Structure
static struct i2c_client  *etx_i2c_client_servo = NULL;  // I2C Client Structure

 /* Setup thanh ghi cho PCA9685*/ 
#define SUBADR1          0x02
#define SUBADR2          0x03
#define SUBADR3          0x04
#define MODE1            0x00
#define MODE2            0x01
#define PRESCALE         0xFE
#define LED0_ON_L        0x06
#define LED0_ON_H        0x07
#define LED0_OFF_L       0x08
#define LED0_OFF_H       0x09
#define ALLLED_ON_L      0xFA
#define ALLLED_ON_H      0xFB
#define ALLLED_OFF_L     0xFC
#define ALLLED_OFF_H     0xFD

//===========================Đọc ghi dữ liệu============================//
// Hàm ghi dữ liệu ra PCA9685
static int I2C_Write(unsigned char reg, unsigned char value)
{   
    unsigned char buf[2];
    buf[0] = reg;
    buf[1] = value;
    /*
    ** Gửi điều kiện bắt đầu, địa chỉ slave, và data
    */ 
    int ret = i2c_master_send(etx_i2c_client_servo, buf, 2);
    
    return ret;
}

// Hàm ghi dữ liệu ra PCA9685
static int I2C_Read(unsigned char reg)
{
    /*
    ** Gửi điều kiện bắt đầu, địa chỉ slave, và data
    */ 
    int ret = i2c_smbus_read_byte_data(etx_i2c_client_servo, reg);
    
    return ret;
}
//=========================================================================//

//=============================Config PCA9685==============================//
// Hàm set xung PWM
static void setPWMFreg(int freg){
    float prescaleval;
    unsigned char oldmode, newmode;
    int prescale;
    prescaleval = 25000000.0;  // Tần số  dao động set = 25MHz

    /*
    ** Công thức tính prescale value
    prescale = round (25MHz / (4096*freg)) - 1;
    trong đó freg là tần số hoạt động truyền vào
    */
    prescaleval /= 4096.0;
    prescaleval /= (float)freg;
    prescaleval -= 1.0;

    prescaleval += 0.5;
    prescale = (int)prescaleval + 1;

    oldmode = I2C_Read(MODE1);
    newmode = (oldmode & 0xf7) | 0x10;    // Sleep
    I2C_Write(MODE1, newmode);
    I2C_Write(PRESCALE, prescale);
    I2C_Write(MODE1, oldmode);
    msleep(5);
    I2C_Write(MODE1, oldmode | 0x80);
    I2C_Write(MODE2, 0x04);
}

/* Hàm set duty cycle( điều chế độ rộng xung )
** tham số truyền vào: - channel: kênh cần gửi tín hiệu trong PCA9685
                       - on: Led ON
                       - off: Led OFF
** Lý thuyết: Thời gian active mức 1 của LED và chu kỳ làm việc của PWM có thể được điều 
    khiển độc lập bằng cách sử dụng các thanh ghi Led on và led off
    Sẽ có 2 thanh ghi 12 bit cho mỗi đầu ra LED, cả 2 thanh ghi này sẽ lưu giá trị từ 0 - 4095
    một thanh ghi giữ thời gian bật và một thanh ghi giữ thời gian tắt. thời gian bật tắt được so
    sánh với giá trị của bộ đếm 12 bit chạy từ 0000h - 0fffh
*/
static void setPWM(uint8_t channel, uint16_t on, uint16_t off)
{
    I2C_Write(LED0_ON_L + 4*channel, on & 0xFF);
    I2C_Write(LED0_ON_H + 4*channel, on >> 8);
    I2C_Write(LED0_OFF_L + 4*channel, off & 0xFF);
    I2C_Write(LED0_OFF_H + 4*channel, off >> 8);
}


static void setServoPulse(uint8_t channel, uint16_t pulse)
{
    pulse = pulse*4096/20000;
    setPWM(channel, 0, (int)pulse);
}

/* Hàm set góc quay cho servo
** Tham số truyền vào: - channel: Kênh cần gửi tín hiệu
                       - angle: góc quay( từ 10 -> 170 độ)
*/
static void setRotationAngle(uint8_t channel, uint8_t angle)
{
    uint16_t temp;
    if(angle<0 && angle>180)
    {
        pr_err("Angle out of range \n");
    }
    temp = angle * (2000/180) + 501;
    setServoPulse(channel, temp);
}
//=========================================================================//

//=======================Device driver I2C bus client======================//
/*
** Hàm khởi tạo cho PCA9685
*/
static int Init_PCA9685(void)
{
    I2C_Write(MODE1, 0x00);
    setPWMFreg(50);
    setRotationAngle(1, 0);

    setRotationAngle(0, 90);
    setRotationAngle(1, 90);
    return 0;
}

// *Hàm sẽ được gọi đến khi tìm thấy Slave
static int etx_servo_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    Init_PCA9685();
 
    pr_info("PCA9685 Probed!!!\n");
    return 0;
}

// *Hàm sẽ được gọi đến khi Slave bị removed
static int etx_servo_remove(struct i2c_client *client)
{   
    setRotationAngle(0,40);
    setRotationAngle(1,40);
    pr_info("servo Removed!!!\n");
    return 0;
}

// *Cấu trúc slave device ID
static const struct i2c_device_id etx_servo_id[] = {
        { SLAVE_DEVICE_NAME, 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, etx_servo_id);


// *Cấu trúc I2C driver sẽ được thêm vào hệ thống
static struct i2c_driver etx_servo_driver = {
        .driver = {
            .name   = SLAVE_DEVICE_NAME,
            .owner  = THIS_MODULE,
        },
        .probe          = etx_servo_probe,
        .remove         = etx_servo_remove,
        .id_table       = etx_servo_id,
};

// *Khởi tạo I2C board Info
static struct i2c_board_info servo_i2c_board_info = {
    I2C_BOARD_INFO(SLAVE_DEVICE_NAME, PWM_I2C_Addr)
};
//=========================================================================//


// *Hàm khởi tạo driver - hàm này giống hàm main trong lập trình C thông thường
static int __init etx_driver_init(void)
{
    int ret = -1;
    etx_i2c_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE); // Bước 1: Get I2C adapter
    
    if( etx_i2c_adapter != NULL )
    {
        // *Tạo mới client device
        etx_i2c_client_servo = i2c_new_client_device(etx_i2c_adapter, &servo_i2c_board_info);
        
        if( etx_i2c_client_servo != NULL )
        {
            i2c_add_driver(&etx_servo_driver); // Thêm I2C driver đã khởi tạo vào hệ thống I2C
            ret = 0;
        }
        
        i2c_put_adapter(etx_i2c_adapter);
    }
    
    pr_info("Driver Added!!!\n");
    return ret;
}

// *Hàm unload driver khỏi hệ thống
static void __exit etx_driver_exit(void)
{
    I2C_Write(MODE2, 0x00);
    i2c_unregister_device(etx_i2c_client_servo);
    i2c_del_driver(&etx_servo_driver);
    pr_info("Driver Removed!!!\n");
}
// *Khai báo hàm khởi tạo và hàm unload
module_init(etx_driver_init);
module_exit(etx_driver_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dao Cong Minh");
MODULE_DESCRIPTION("I2C interface with PCA9685 to control servo");
MODULE_VERSION("0.1");
