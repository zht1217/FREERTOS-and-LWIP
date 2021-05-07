#include "led.h"
#include "delay.h"
#include "key.h"
#include "sys.h"
#include "lcd.h"
#include "usart.h"
#include "timer.h"
#include "sram.h"
#include "malloc.h"
#include "string.h"
//#include "usmart.h"
#include "dm9000.h"
#include "lwip/netif.h"
#include "lwip_comm.h"
#include "lwipopts.h"
#include "FreeRTOS.h"
#include "task.h"



//任务优先级
#define START_TASK_PRIO     1
//任务堆栈大小	
#define START_STK_SIZE      128 
//任务句柄
TaskHandle_t StartTask_Handler;
//任务函数
void start_task(void *pvParameters);

//任务优先级
#define DISPALY_TASK_PRIO   2
//任务堆栈大小	
#define DISPALY_STK_SIZE    128
//任务句柄
TaskHandle_t DispalyTask_Handler;

//任务优先级
#define LED_TASK_PRIO   3
//任务堆栈大小	
#define LED_STK_SIZE    64
//任务句柄
TaskHandle_t LedTask_Handler;
TaskHandle_t DisplayTask_Handler;
//任务函数
void display_task(void *pdata);
void led_task(void *pdata);
//在LCD上显示地址信息
//mode:1 显示DHCP获取到的地址
//	  其他 显示静态地址
void show_address(u8 mode)
{
    u8 buf[30];
    if(mode==2)
    {
        sprintf((char*)buf,"DHCP IP :%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);         //打印动态IP地址
        //LCD_ShowString(30,130,210,16,16,buf); 
        sprintf((char*)buf,"DHCP GW :%d.%d.%d.%d",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]); //打印网关地址
        //LCD_ShowString(30,150,210,16,16,buf); 
        sprintf((char*)buf,"NET MASK:%d.%d.%d.%d",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]); //打印子网掩码地址
        //LCD_ShowString(30,170,210,16,16,buf); 
        //LCD_ShowString(30,190,210,16,16,"Port:8089!"); 
    }
    else 
    {
        sprintf((char*)buf,"Static IP:%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);    //打印动态IP地址
        //LCD_ShowString(30,130,210,16,16,buf); 
        sprintf((char*)buf,"Static GW:%d.%d.%d.%d",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);    //打印网关地址
        //LCD_ShowString(30,150,210,16,16,buf); 
        sprintf((char*)buf,"NET MASK :%d.%d.%d.%d",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);    //打印子网掩码地址
        //LCD_ShowString(30,170,210,16,16,buf); 
        //LCD_ShowString(30,190,210,16,16,"Port:8089!"); 
    }
}
int main(void)
{ 

   delay_init();    //延时函数初始化	  
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4); 	//设置NVIC中断分组
    uart_init(115200);  //串口初始化为115200
    LED_Init();     //LED端口初始化
    LCD_Init(); //初始化LCD
    KEY_Init(); //初始化按键
   // usmart_dev.init(72);    //初始化USMART		
    FSMC_SRAM_Init();//初始化外部SRAM
    //DM9000_Init();
    my_mem_init(SRAMIN);        //初始化内部内存池
    my_mem_init(SRAMEX);        //初始化外部内存池
   
    //创建开始任务
    xTaskCreate((TaskFunction_t )start_task,            //任务函数
                (const char*    )"start_task",          //任务名称
                (uint16_t       )START_STK_SIZE,        //任务堆栈大小
                (void*          )NULL,                  //传递给任务函数的参数
                (UBaseType_t    )START_TASK_PRIO,       //任务优先级
                (TaskHandle_t*  )&StartTask_Handler);   //任务句柄                
    vTaskStartScheduler();          //开启任务调度
}

//开始任务任务函数
void start_task(void *pvParameters)
{
     lwip_comm_init();    //lwip初始化
    
    taskENTER_CRITICAL();           //进入临界区

    #if LWIP_DHCP
    lwip_comm_dhcp_creat(); //创建DHCP任务
    #endif
              //创建led任务
    xTaskCreate((TaskFunction_t )led_task,             
                (const char*    )"led_task",           
                (uint16_t       )LED_STK_SIZE,        
                (void*          )NULL,                  
                (UBaseType_t    )LED_TASK_PRIO,        
                (TaskHandle_t*  )&LedTask_Handler); 
    //创建display任务
    xTaskCreate((TaskFunction_t )display_task,             
                (const char*    )"display_task",           
                (uint16_t       )DISPALY_STK_SIZE,        
                (void*          )NULL,                  
                (UBaseType_t    )DISPALY_TASK_PRIO,        
                (TaskHandle_t*  )&DisplayTask_Handler);   

      
    vTaskDelete(StartTask_Handler); //删除开始任务
    taskEXIT_CRITICAL();            //退出临界区
               
}
 

//显示地址等信息
void display_task(void *pdata)
{
    while(1)
    { 
#if LWIP_DHCP               //当开启DHCP的时候
        if(lwipdev.dhcpstatus != 0)     //开启DHCP
        {
           // show_address(lwipdev.dhcpstatus );	//显示地址信息
            vTaskSuspend(DisplayTask_Handler); 		//显示完地址信息后挂起自身任务
        }
#else
        show_address(0); 						//显示静态地址
        vTaskSuspend(DisplayTask_Handler);  	//显示完地址信息后挂起自身任务
#endif //LWIP_DHCP
        vTaskDelay(1000);
    }
}

//led任务
void led_task(void *pdata)
{
    while(1)
    {
        LED0 = !LED0;
        vTaskDelay(1000);
        LED1 =!LED1;
        vTaskDelay(1000);
      
    }
}

