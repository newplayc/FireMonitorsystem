#include "DS18B20.h"
#include "main.h"

/* DWT计数器用于精确延时 (Cortex-M3/M4内核特性) */
#define DWT_CYCCNT  (*((volatile uint32_t *)0xE0001004))

/****************************************************************************
函数名：DWT_Init
功能：初始化DWT计数器（用于精确延时）
输入：无
输出：无
返回值：无
备注：只需初始化一次
****************************************************************************/
static void DWT_Init(void)
{
    static int initialized = 0;
    if (!initialized) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  /* 使能DWT */
        DWT_CYCCNT = 0;                                   /* 清零计数器 */
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;             /* 启动计数器 */
        initialized = 1;
    }
}

/****************************************************************************
函数名：delay_us
功能：微秒级精确延时（使用DWT计数器）
输入：延时数据（微秒）
输出：无
返回值：无
备注：72MHz时钟下，1us = 72个周期，比原延时函数精确得多
****************************************************************************/
void delay_us(uint32_t us)
{
    DWT_Init();  /* 确保DWT已初始化 */

    uint32_t startTick = DWT_CYCCNT;
    uint32_t delayTicks = us * (SystemCoreClock / 1000000);  /* 72MHz: us * 72 */

    while ((DWT_CYCCNT - startTick) < delayTicks) {
        /* 等待 */
    }
}

/****************************************************************************
函数名：DS18B20_IO_IN
功能：使DS18B20_DQ引脚变为输入模式
输入：无
输出：无
返回值：无
备注：DQ引脚为PA5
****************************************************************************/
void DS18B20_IO_IN(void){
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.Pin = DQ_Pin;
    GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
    GPIO_InitStructure.Pull = GPIO_PULLUP;  /* 内部上拉（约40KΩ） */
    HAL_GPIO_Init(DQ_GPIO_Port,&GPIO_InitStructure);
}


/****************************************************************************
函数名：DS18B20_IO_OUT
功能：使DS18B20_DQ引脚变为开漏输出模式
输入：无
输出：无
返回值：无
备注：DQ引脚为PA5，开漏输出更适合单总线协议
****************************************************************************/
void DS18B20_IO_OUT(void){
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.Pin = DQ_Pin;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD;  /* 改为开漏输出 */
    GPIO_InitStructure.Pull = GPIO_PULLUP;          /* 内部上拉 */
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DQ_GPIO_Port,&GPIO_InitStructure);
}


/***************************************************************************
函数名：DS18B20_Rst
功  能：发送复位信号
输  入: 无
输  出：无
返回值：无
备  注：
***************************************************************************/
void DS18B20_Rst(void){
    DS18B20_IO_OUT();//引脚输出模式

    //拉低总线并延时480us-960us（DS18B20要求）
    DS18B20_DQ_OUT_LOW;
    delay_us(480);  /* 修改：使用更标准的480us */

    //释放总线为高电平并延时等待
    DS18B20_DQ_OUT_HIGH;
    delay_us(60);   /* 等待60us后检测存在脉冲 */
}


/***************************************************************************
函数名：DS18B20_Check
功  能：检测DS18B20返回的存在脉冲
输  入: 无
输  出：无
返回值：0:成功  1：失败   2:释放总线失败
备  注：
***************************************************************************/
uint8_t DS18B20_Check(void){
    uint8_t retry = 0;

    //引脚设为输入模式
    DS18B20_IO_IN();

    //等待DS18B20拉低总线（存在脉冲）
    while(DS18B20_DQ_IN && retry < 200){
        retry++;
        delay_us(1);
    }

    if(retry >= 200)
        return 1;  /* 超时，设备未响应 */
    else
        retry = 0;

    //等待DS18B20释放总线
    while(!DS18B20_DQ_IN && retry < 240){
        retry++;
        delay_us(1);
    }

    if(retry >= 240)
        return 2;  /* 释放失败 */

    return 0;  /* 成功 */
}



/***************************************************************************
函数名：DS18B20_Write_Byte
功  能：向DS18B20写一个字节
输  入: 要写入的字节
输  出：无
返回值：无
备  注：
***************************************************************************/
void DS18B20_Write_Byte(uint8_t data){
    uint8_t j;
    uint8_t databit;
    DS18B20_IO_OUT();
    for(j=1;j<=8;j++){
        databit=data&0x01;//取数据最低位
        data=data>>1;     //右移一位
        if(databit){      //当前位写1
            DS18B20_DQ_OUT_LOW;
            delay_us(2);
            DS18B20_DQ_OUT_HIGH;
            delay_us(60);
        }else{          //当前位写0
            DS18B20_DQ_OUT_LOW;
            delay_us(60);
            DS18B20_DQ_OUT_HIGH;
            delay_us(2);
        }
    }
}

/***************************************************************************
函数名：DS18B20_Read_Bit
功  能：向DS18B20读一个位
输  入: 无
输  出：无
返回值：读入数据
备  注：
***************************************************************************/
uint8_t DS18B20_Read_Bit(void){
    uint8_t data;
    DS18B20_IO_OUT();
    DS18B20_DQ_OUT_LOW;
    delay_us(2);
    DS18B20_DQ_OUT_HIGH;
    DS18B20_IO_IN();
    delay_us(12);

    if(DS18B20_DQ_IN)
        data = 1;
    else
        data = 0;

    delay_us(50);
    return data;
}


/***************************************************************************
函数名：DS18B20_Read_Byte
功  能：向DS18B20读一个字节
输  入: 无
输  出：无
返回值：读入数据
备  注：
***************************************************************************/
uint8_t DS18B20_Read_Byte(void){
    uint8_t i,j,data;
    data = 0;
    for(i=1;i<=8;i++){
        j = DS18B20_Read_Bit();
        data = (j<<7)|(data>>1);
        /*j=0或1，j<<7=0x00或0x80，和data右移一位相或，即把1/0写入最高位，下次再往右移位*/

    }
    return data;
}

/***************************************************************************
函数名：DS18B20_Start
功  能：DS18B20开启温度转换
输  入: 无
输  出：无
返回值：无
备  注：
***************************************************************************/
void DS18B20_Start(void){
    DS18B20_Rst();
    DS18B20_Check();
    DS18B20_Write_Byte(0xcc);//跳过ROM
    DS18B20_Write_Byte(0x44);//温度变换命令
}


/***************************************************************************
函数名：DS18B20_Init
功  能：DS18B20初始化
输  入: 无
输  出：无
返回值：0=成功，非0=失败
备  注：
***************************************************************************/
uint8_t DS18B20_Init(void){
    //引脚初始化
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.Pin = DQ_Pin;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStructure.Pull = GPIO_PULLUP;  /* 软件上拉 */
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DQ_GPIO_Port,&GPIO_InitStructure);

    // 初始化DWT用于精确延时
    DWT_Init();

    DS18B20_Rst();
    return DS18B20_Check();
}

/***************************************************************************
函数名：DS18B20_Read_Temperature
功  能：读取一次温度
输  入: 无
输  出：无
返回值：读取到的温度数据（扩大10倍，如255表示25.5°C）
备  注：适用于总线上只有一个DS18B20的情况
***************************************************************************/
short DS18B20_Get_Temperature(void){
    uint8_t temp;
    uint8_t TL,TH;
    short temperature;

    DS18B20_Start();
    HAL_Delay(750);  /* 等待温度转换完成（12位精度需要750ms） */

    DS18B20_Rst();
    if(DS18B20_Check() != 0) {
        return -999;  /* 读取失败 */
    }
    DS18B20_Write_Byte(0xcc);//跳过ROM
    DS18B20_Write_Byte(0xbe);//读暂存器
    TL = DS18B20_Read_Byte();//低八位
    TH = DS18B20_Read_Byte();//高八位

    //判断温度值是否为负数
    if(TH>0x70){
        TH = ~TH;
        TL = ~TL;
        temp = 0;
    }else
        temp = 1;

    temperature = TH;
    temperature <<= 8;
    temperature += TL;
    temperature = (float)temperature*0.625;
    if(temperature)
        return temperature;
    else
        return -temperature;
}

// 兼容性别名函数
void DS18B20_WriteByte(uint8_t data){
    DS18B20_Write_Byte(data);
}
