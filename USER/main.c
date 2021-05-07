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



//�������ȼ�
#define START_TASK_PRIO     1
//�����ջ��С	
#define START_STK_SIZE      128 
//������
TaskHandle_t StartTask_Handler;
//������
void start_task(void *pvParameters);

//�������ȼ�
#define DISPALY_TASK_PRIO   2
//�����ջ��С	
#define DISPALY_STK_SIZE    128
//������
TaskHandle_t DispalyTask_Handler;

//�������ȼ�
#define LED_TASK_PRIO   3
//�����ջ��С	
#define LED_STK_SIZE    64
//������
TaskHandle_t LedTask_Handler;
TaskHandle_t DisplayTask_Handler;
//������
void display_task(void *pdata);
void led_task(void *pdata);
//��LCD����ʾ��ַ��Ϣ
//mode:1 ��ʾDHCP��ȡ���ĵ�ַ
//	  ���� ��ʾ��̬��ַ
void show_address(u8 mode)
{
    u8 buf[30];
    if(mode==2)
    {
        sprintf((char*)buf,"DHCP IP :%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);         //��ӡ��̬IP��ַ
        //LCD_ShowString(30,130,210,16,16,buf); 
        sprintf((char*)buf,"DHCP GW :%d.%d.%d.%d",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]); //��ӡ���ص�ַ
        //LCD_ShowString(30,150,210,16,16,buf); 
        sprintf((char*)buf,"NET MASK:%d.%d.%d.%d",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]); //��ӡ���������ַ
        //LCD_ShowString(30,170,210,16,16,buf); 
        //LCD_ShowString(30,190,210,16,16,"Port:8089!"); 
    }
    else 
    {
        sprintf((char*)buf,"Static IP:%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);    //��ӡ��̬IP��ַ
        //LCD_ShowString(30,130,210,16,16,buf); 
        sprintf((char*)buf,"Static GW:%d.%d.%d.%d",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);    //��ӡ���ص�ַ
        //LCD_ShowString(30,150,210,16,16,buf); 
        sprintf((char*)buf,"NET MASK :%d.%d.%d.%d",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);    //��ӡ���������ַ
        //LCD_ShowString(30,170,210,16,16,buf); 
        //LCD_ShowString(30,190,210,16,16,"Port:8089!"); 
    }
}
int main(void)
{ 

   delay_init();    //��ʱ������ʼ��	  
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4); 	//����NVIC�жϷ���
    uart_init(115200);  //���ڳ�ʼ��Ϊ115200
    LED_Init();     //LED�˿ڳ�ʼ��
    LCD_Init(); //��ʼ��LCD
    KEY_Init(); //��ʼ������
   // usmart_dev.init(72);    //��ʼ��USMART		
    FSMC_SRAM_Init();//��ʼ���ⲿSRAM
    //DM9000_Init();
    my_mem_init(SRAMIN);        //��ʼ���ڲ��ڴ��
    my_mem_init(SRAMEX);        //��ʼ���ⲿ�ڴ��
   
    //������ʼ����
    xTaskCreate((TaskFunction_t )start_task,            //������
                (const char*    )"start_task",          //��������
                (uint16_t       )START_STK_SIZE,        //�����ջ��С
                (void*          )NULL,                  //���ݸ��������Ĳ���
                (UBaseType_t    )START_TASK_PRIO,       //�������ȼ�
                (TaskHandle_t*  )&StartTask_Handler);   //������                
    vTaskStartScheduler();          //�����������
}

//��ʼ����������
void start_task(void *pvParameters)
{
     lwip_comm_init();    //lwip��ʼ��
    
    taskENTER_CRITICAL();           //�����ٽ���

    #if LWIP_DHCP
    lwip_comm_dhcp_creat(); //����DHCP����
    #endif
              //����led����
    xTaskCreate((TaskFunction_t )led_task,             
                (const char*    )"led_task",           
                (uint16_t       )LED_STK_SIZE,        
                (void*          )NULL,                  
                (UBaseType_t    )LED_TASK_PRIO,        
                (TaskHandle_t*  )&LedTask_Handler); 
    //����display����
    xTaskCreate((TaskFunction_t )display_task,             
                (const char*    )"display_task",           
                (uint16_t       )DISPALY_STK_SIZE,        
                (void*          )NULL,                  
                (UBaseType_t    )DISPALY_TASK_PRIO,        
                (TaskHandle_t*  )&DisplayTask_Handler);   

      
    vTaskDelete(StartTask_Handler); //ɾ����ʼ����
    taskEXIT_CRITICAL();            //�˳��ٽ���
               
}
 

//��ʾ��ַ����Ϣ
void display_task(void *pdata)
{
    while(1)
    { 
#if LWIP_DHCP               //������DHCP��ʱ��
        if(lwipdev.dhcpstatus != 0)     //����DHCP
        {
           // show_address(lwipdev.dhcpstatus );	//��ʾ��ַ��Ϣ
            vTaskSuspend(DisplayTask_Handler); 		//��ʾ���ַ��Ϣ�������������
        }
#else
        show_address(0); 						//��ʾ��̬��ַ
        vTaskSuspend(DisplayTask_Handler);  	//��ʾ���ַ��Ϣ�������������
#endif //LWIP_DHCP
        vTaskDelay(1000);
    }
}

//led����
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

