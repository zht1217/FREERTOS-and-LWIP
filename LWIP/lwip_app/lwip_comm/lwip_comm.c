#include "lwip_comm.h" 
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/init.h"
#include "ethernetif.h" 
#include "lwip/timers.h"
#include "lwip/tcp_impl.h"
#include "lwip/ip_frag.h"
#include "lwip/tcpip.h" 
#include "malloc.h"
#include "delay.h"
#include "usart.h"  
#include <stdio.h>
#include "semphr.h"

//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK 战舰开发板 V3
//lwip通用驱动 代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2015/3/15
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//*******************************************************************************
//修改信息
//无
////////////////////////////////////////////////////////////////////////////////// 	   
   
__lwip_dev lwipdev;						//lwip控制结构体 
struct netif lwip_netif;				//定义一个全局的网络接口
extern  SemaphoreHandle_t dm9000lock;
extern SemaphoreHandle_t dm9000input;
extern u32 memp_get_memorysize(void);	//在memp.c里面定义
extern u8_t *memp_memory;				//在memp.c里面定义.
extern u8_t *ram_heap;					//在mem.c里面定义.
u32 TCPTimer=0;			//TCP查询计时器
u32 ARPTimer=0;			//ARP查询计时器
u32 lwip_localtime;		//lwip本地时间计数器,单位:ms
unsigned int *TCPIP_THREAD_TASK_STK;
unsigned int *LWIP_DHCP_TASK_STK;
unsigned int *LWIP_DM9000_INPUT_TASK_STK;
//lwip DM9000数据接收处理任务
//设置任务优先级
#define LWIP_DM9000_INPUT_TASK_PRIO		8
//设置任务堆栈大小
#define LWIP_DM9000_INPUT_TASK_SIZE	    512

 //任务优先级
#define LWIP_DHCP_TASK_PRIO		7
//任务堆栈大小	
#define LWIP_DHCP_STK_SIZE 		256
//任务句柄
TaskHandle_t LWIP_DHCP_TaskHandler;
TaskHandle_t LWIP_DM9000_TaskHandler;
//任务函数
void lwip_dhcp_task(void *pvParameters);

//__lwip_dev lwipdev;						//lwip控制结构体 
//struct netif lwip_netif;				//定义一个全局的网络接口
//extern u32 memp_get_memorysize(void);	//在memp.c里面定义
//extern u8_t *memp_memory;				//在memp.c里面定义.
//extern u8_t *ram_heap;					//在mem.c里面定义.


//SemaphoreHandle_t dm9000input;
//lwip中mem和memp的内存申请
//返回值:0,成功;
//    其他,失败
//u8 lwip_comm_mem_malloc(void)
//{
//	u32 mempsize;
//	u32 ramheapsize; 
//	INTX_DISABLE();//关中断
//	mempsize=memp_get_memorysize();			//得到memp_memory数组大小
//	memp_memory=mymalloc(SRAMIN,mempsize);	//为memp_memory申请内存
//	ramheapsize=LWIP_MEM_ALIGN_SIZE(MEM_SIZE)+2*LWIP_MEM_ALIGN_SIZE(4*3)+MEM_ALIGNMENT;//得到ram heap大小
//	ram_heap=mymalloc(SRAMIN,ramheapsize);	//为ram_heap申请内存 
//	INTX_ENABLE();
//	if(!memp_memory||!ram_heap)//有申请失败的
//	{
//		lwip_comm_mem_free();
//		return 1;
//	}
//	return 0;	
//}
//DM9000数据接收处理任务
void lwip_dm9000_input_task(void *pdata)
{
	//从网络缓冲区中读取接收到的数据包并将其发送给LWIP处理 
	ethernetif_input(&lwip_netif);
}

u8 lwip_comm_mem_malloc(void)
{
	u32 mempsize;
	u32 ramheapsize; 
	mempsize=memp_get_memorysize();			//得到memp_memory数组大小
	memp_memory=mymalloc(SRAMIN,mempsize);	//为memp_memory申请内存
	printf("memp_memory内存大小为:%d\r\n",mempsize);
	ramheapsize=LWIP_MEM_ALIGN_SIZE(MEM_SIZE)+2*LWIP_MEM_ALIGN_SIZE(4*3)+MEM_ALIGNMENT;//得到ram heap大小
	ram_heap=mymalloc(SRAMIN,ramheapsize);	//为ram_heap申请内存 
	printf("ram_heap内存大小为:%d\r\n",ramheapsize);
	//TCPIP_THREAD_TASK_STK=mymalloc(SRAMIN,TCPIP_THREAD_STACKSIZE*4);			//给内核任务申请堆栈 
	//LWIP_DHCP_TASK_STK=mymalloc(SRAMIN,LWIP_DHCP_STK_SIZE*4);					//给dhcp任务申请堆栈 
	//LWIP_DM9000_INPUT_TASK_STK=mymalloc(SRAMIN,LWIP_DM9000_INPUT_TASK_SIZE*4);	//给dm9000接收任务申请堆栈 
	if(!memp_memory||!ram_heap)//有申请失败的
	{
		lwip_comm_mem_free();
		return 1;
	}
	return 0;	
}
//lwip中mem和memp内存释放
void lwip_comm_mem_free(void)
{ 	
	myfree(SRAMIN,memp_memory);
	myfree(SRAMIN,ram_heap);
	//myfree(SRAMIN,TCPIP_THREAD_TASK_STK);
	//myfree(SRAMIN,LWIP_DHCP_TASK_STK);
	//myfree(SRAMIN,LWIP_DM9000_INPUT_TASK_STK);
}
//lwip 默认IP设置
//lwipx:lwip控制结构体指针
void lwip_comm_default_ip_set(__lwip_dev *lwipx)
{
	//默认远端IP为:192.168.1.100
	lwipx->remoteip[0]=192;	
	lwipx->remoteip[1]=168;
	lwipx->remoteip[2]=1;
	lwipx->remoteip[3]=100;
	//MAC地址设置(高三字节固定为:2.0.0,低三字节用STM32唯一ID)
	lwipx->mac[0]=dm9000cfg.mac_addr[0];
	lwipx->mac[1]=dm9000cfg.mac_addr[1];
	lwipx->mac[2]=dm9000cfg.mac_addr[2];
	lwipx->mac[3]=dm9000cfg.mac_addr[3];
	lwipx->mac[4]=dm9000cfg.mac_addr[4];
	lwipx->mac[5]=dm9000cfg.mac_addr[5]; 
	//默认本地IP为:192.168.1.30
	lwipx->ip[0]=192;	
	lwipx->ip[1]=168;
	lwipx->ip[2]=1;
	lwipx->ip[3]=30;
	//默认子网掩码:255.255.255.0
	lwipx->netmask[0]=255;	
	lwipx->netmask[1]=255;
	lwipx->netmask[2]=255;
	lwipx->netmask[3]=0;
	//默认网关:192.168.1.1
	lwipx->gateway[0]=192;	
	lwipx->gateway[1]=168;
	lwipx->gateway[2]=1;
	lwipx->gateway[3]=1;	
	lwipx->dhcpstatus=0;//没有DHCP	
} 

//LWIP初始化(LWIP启动的时候使用)
//返回值:0,成功
//      1,内存错误
//      2,DM9000初始化失败
//      3,网卡添加失败.
u8 lwip_comm_init(void)
{
	struct netif *Netif_Init_Flag;		//调用netif_add()函数时的返回值,用于判断网络初始化是否成功
	struct ip_addr ipaddr;  			//ip地址
	struct ip_addr netmask; 			//子网掩码
	struct ip_addr gw;      			//默认网关 
	if (lwip_comm_mem_malloc()) return 1;	//内存申请失败
	dm9000input=xSemaphoreCreateBinary();			//创建数据接收信号量,必须在DM9000初始化之前创建
    if(dm9000input==NULL)
        {
            printf("dm9000input创建失败\r\n");
        }
	dm9000lock=xSemaphoreCreateMutex();	//创建互斥信号量	
	if(DM9000_Init())return 2;			//初始化DM9000
	
	tcpip_init(NULL,NULL);				//初始化tcp ip内核,该函数里面会创建tcpip_thread内核任务
	lwip_comm_default_ip_set(&lwipdev);	//设置默认IP等信息
//tcpip_init(NULL,NULL);	
	
#if LWIP_DHCP		//使用动态IP
	ipaddr.addr = 0;
	netmask.addr = 0;
	gw.addr = 0;
#else				//使用静态IP
	IP4_ADDR(&ipaddr,lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
	IP4_ADDR(&netmask,lwipdev.netmask[0],lwipdev.netmask[1] ,lwipdev.netmask[2],lwipdev.netmask[3]);
	IP4_ADDR(&gw,lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
	printf("网卡en的MAC地址为:................%d.%d.%d.%d.%d.%d\r\n",lwipdev.mac[0],lwipdev.mac[1],lwipdev.mac[2],lwipdev.mac[3],lwipdev.mac[4],lwipdev.mac[5]);
	printf("静态IP地址........................%d.%d.%d.%d\r\n",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
	printf("子网掩码..........................%d.%d.%d.%d\r\n",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);
	printf("默认网关..........................%d.%d.%d.%d\r\n",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
#endif
     //netif_add(&lwip_netif,&ipaddr,&netmask,&gw,NULL,&ethernetif_init,&ethernet_input);
	Netif_Init_Flag=netif_add(&lwip_netif,&ipaddr,&netmask,&gw,NULL,&ethernetif_init,&tcpip_input);//向网卡列表中添加一个网口
	if(Netif_Init_Flag!=NULL) 
    //网卡添加失败 
	//网口添加成功后,设置netif为默认值,并且打开netif网口
	{
		netif_set_default(&lwip_netif); //设置netif为默认网口
		netif_set_up(&lwip_netif);		//打开netif网口
	}
	//创建DM9000任务
	taskENTER_CRITICAL();  //进入临界区
	xTaskCreate((TaskFunction_t)lwip_dm9000_input_task,
						(const char*  )"lwip_dm9000_input_task",
						(uint16_t     )LWIP_DM9000_INPUT_TASK_SIZE,
						(void*        )NULL,
						(UBaseType_t  )LWIP_DM9000_INPUT_TASK_PRIO,
						(TaskHandle_t*)&LWIP_DM9000_TaskHandler);//创建DHCP任务 						
	taskEXIT_CRITICAL();  //退出临界区
//#if	LWIP_DHCP
//	lwip_comm_dhcp_creat();		//创建DHCP任务
//#endif		
	return 0;//操作OK.
}   
#if LWIP_DHCP			//如果使用DHCP的话
	//lwipdev.dhcpstatus=0;	//DHCP标记为0
	//dhcp_start(&lwip_netif);	//开启DHCP服务
	//创建DHCP任务
	void lwip_comm_dhcp_creat(void)
{
	taskENTER_CRITICAL();  //进入临界区
	xTaskCreate((TaskFunction_t)lwip_dhcp_task,
						(const char*  )"lwip_dhcp_task",
						(uint16_t     )LWIP_DHCP_STK_SIZE,
						(void*        )NULL,
						(UBaseType_t  )LWIP_DHCP_TASK_PRIO,
						(TaskHandle_t*)&LWIP_DHCP_TaskHandler);//创建DHCP任务 						
	taskEXIT_CRITICAL();  //退出临界区
}
//删除DHCP任务
void lwip_comm_dhcp_delete(void)
{
	dhcp_stop(&lwip_netif); 		//关闭DHCP
	vTaskDelete(LWIP_DHCP_TaskHandler);	//删除DHCP任务
}
//lwip_dhcp_task任务
 void lwip_dhcp_task(void *pvParameters)
{
			u32 ip=0,netmask=0,gw=0;
			dhcp_start(&lwip_netif);
			lwipdev.dhcpstatus = 0;		//等待通过DHCP获取到的地址
			printf("正在查找DHCP服务器,请稍等...........\r\n");  
		while(1)			//等待获取到IP地址
		{
            printf("正在获取地址...\r\n");
			ip=lwip_netif.ip_addr.addr;		//读取新IP地址
			netmask=lwip_netif.netmask.addr;//读取子网掩码
			gw=lwip_netif.gw.addr;			//读取默认网关 
			if(ip!=0)			//正确获取到IP地址的时候
			{
				lwipdev.dhcpstatus=2;	//DHCP成功
				printf("网卡en的MAC地址为:................%d.%d.%d.%d.%d.%d\r\n",lwipdev.mac[0],lwipdev.mac[1],lwipdev.mac[2],lwipdev.mac[3],lwipdev.mac[4],lwipdev.mac[5]);
				//解析出通过DHCP获取到的IP地址
				lwipdev.ip[3]=(uint8_t)(ip>>24); 
				lwipdev.ip[2]=(uint8_t)(ip>>16);
				lwipdev.ip[1]=(uint8_t)(ip>>8);
				lwipdev.ip[0]=(uint8_t)(ip);
				printf("通过DHCP获取到IP地址..............%d.%d.%d.%d\r\n",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
				//解析通过DHCP获取到的子网掩码地址
				lwipdev.netmask[3]=(uint8_t)(netmask>>24);
				lwipdev.netmask[2]=(uint8_t)(netmask>>16);
				lwipdev.netmask[1]=(uint8_t)(netmask>>8);
				lwipdev.netmask[0]=(uint8_t)(netmask);
				printf("通过DHCP获取到子网掩码............%d.%d.%d.%d\r\n",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);
				//解析出通过DHCP获取到的默认网关
				lwipdev.gateway[3]=(uint8_t)(gw>>24);
				lwipdev.gateway[2]=(uint8_t)(gw>>16);
				lwipdev.gateway[1]=(uint8_t)(gw>>8);
				lwipdev.gateway[0]=(uint8_t)(gw);
				printf("通过DHCP获取到的默认网关..........%d.%d.%d.%d\r\n",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
               //while(1);
               break;
			}
			else if(lwip_netif.dhcp->tries>LWIP_MAX_DHCP_TRIES) //通过DHCP服务获取IP地址失败,且超过最大尝试次数
			{
				lwipdev.dhcpstatus=0XFF;//DHCP超时失败.
				//使用静态IP地址
				IP4_ADDR(&(lwip_netif.ip_addr),lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
				IP4_ADDR(&(lwip_netif.netmask),lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);
				IP4_ADDR(&(lwip_netif.gw),lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
				printf("DHCP服务超时,使用静态IP地址!\r\n");
				printf("网卡en的MAC地址为:................%d.%d.%d.%d.%d.%d\r\n",lwipdev.mac[0],lwipdev.mac[1],lwipdev.mac[2],lwipdev.mac[3],lwipdev.mac[4],lwipdev.mac[5]);
				printf("静态IP地址........................%d.%d.%d.%d\r\n",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
				printf("子网掩码..........................%d.%d.%d.%d\r\n",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);
				printf("默认网关..........................%d.%d.%d.%d\r\n",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
                break;
			}
          delay_ms(250); //延时250ms
		}
      lwip_comm_dhcp_delete();//删除DHCP任务   
    //while(1);
	}
#endif
	
//	if(Netif_Init_Flag==NULL)return 3;//网卡添加失败 
//	else//网口添加成功后,设置netif为默认值,并且打开netif网口
//	{
//		netif_set_default(&lwip_netif); //设置netif为默认网口
//		netif_set_up(&lwip_netif);		//打开netif网口
//	}
//	return 0;//操作OK.
//}   

////当接收到数据后调用 
//void lwip_pkt_handle(void)
//{
//	//从网络缓冲区中读取接收到的数据包并将其发送给LWIP处理 
//	ethernetif_input(&lwip_netif);
//}

////LWIP轮询任务
//void lwip_periodic_handle()
//{
//	
//#if LWIP_TCP
//	//每250ms调用一次tcp_tmr()函数
//	if (lwip_localtime - TCPTimer >= TCP_TMR_INTERVAL)
//	{
//		TCPTimer =  lwip_localtime;
//		tcp_tmr();
//	}
//#endif
//	//ARP每5s周期性调用一次
//	if ((lwip_localtime - ARPTimer) >= ARP_TMR_INTERVAL)
//	{
//		ARPTimer =  lwip_localtime;
//		etharp_tmr();
//	}

//#if LWIP_DHCP //如果使用DHCP的话
//	//每500ms调用一次dhcp_fine_tmr()
//	if (lwip_localtime - DHCPfineTimer >= DHCP_FINE_TIMER_MSECS)
//	{
//		DHCPfineTimer =  lwip_localtime;
//		dhcp_fine_tmr();
//		if ((lwipdev.dhcpstatus != 2)&&(lwipdev.dhcpstatus != 0XFF))
//		{ 
//			lwip_dhcp_process_handle();  //DHCP处理
//		}
//	}

//	//每60s执行一次DHCP粗糙处理
//	if (lwip_localtime - DHCPcoarseTimer >= DHCP_COARSE_TIMER_MSECS)
//	{
//		DHCPcoarseTimer =  lwip_localtime;
//		dhcp_coarse_tmr();
//	}  
//#endif
//}


//如果使能了DHCP
//#if LWIP_DHCP

////DHCP处理任务
//void lwip_dhcp_process_handle(void)
//{
//	u32 ip=0,netmask=0,gw=0;
//	switch(lwipdev.dhcpstatus)
//	{
//		case 0: 	//开启DHCP
//			dhcp_start(&lwip_netif);
//			lwipdev.dhcpstatus = 1;		//等待通过DHCP获取到的地址
//			printf("正在查找DHCP服务器,请稍等...........\r\n");  
//			break;
//		case 1:		//等待获取到IP地址
//		{
//			ip=lwip_netif.ip_addr.addr;		//读取新IP地址
//			netmask=lwip_netif.netmask.addr;//读取子网掩码
//			gw=lwip_netif.gw.addr;			//读取默认网关 
//			
//			if(ip!=0)			//正确获取到IP地址的时候
//			{
//				lwipdev.dhcpstatus=2;	//DHCP成功
//				printf("网卡en的MAC地址为:................%d.%d.%d.%d.%d.%d\r\n",lwipdev.mac[0],lwipdev.mac[1],lwipdev.mac[2],lwipdev.mac[3],lwipdev.mac[4],lwipdev.mac[5]);
//				//解析出通过DHCP获取到的IP地址
//				lwipdev.ip[3]=(uint8_t)(ip>>24); 
//				lwipdev.ip[2]=(uint8_t)(ip>>16);
//				lwipdev.ip[1]=(uint8_t)(ip>>8);
//				lwipdev.ip[0]=(uint8_t)(ip);
//				printf("通过DHCP获取到IP地址..............%d.%d.%d.%d\r\n",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
//				//解析通过DHCP获取到的子网掩码地址
//				lwipdev.netmask[3]=(uint8_t)(netmask>>24);
//				lwipdev.netmask[2]=(uint8_t)(netmask>>16);
//				lwipdev.netmask[1]=(uint8_t)(netmask>>8);
//				lwipdev.netmask[0]=(uint8_t)(netmask);
//				printf("通过DHCP获取到子网掩码............%d.%d.%d.%d\r\n",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);
//				//解析出通过DHCP获取到的默认网关
//				lwipdev.gateway[3]=(uint8_t)(gw>>24);
//				lwipdev.gateway[2]=(uint8_t)(gw>>16);
//				lwipdev.gateway[1]=(uint8_t)(gw>>8);
//				lwipdev.gateway[0]=(uint8_t)(gw);
//				printf("通过DHCP获取到的默认网关..........%d.%d.%d.%d\r\n",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
//			}else if(lwip_netif.dhcp->tries>LWIP_MAX_DHCP_TRIES) //通过DHCP服务获取IP地址失败,且超过最大尝试次数
//			{
//				lwipdev.dhcpstatus=0XFF;//DHCP超时失败.
//				//使用静态IP地址
//				IP4_ADDR(&(lwip_netif.ip_addr),lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
//				IP4_ADDR(&(lwip_netif.netmask),lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);
//				IP4_ADDR(&(lwip_netif.gw),lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
//				printf("DHCP服务超时,使用静态IP地址!\r\n");
//				printf("网卡en的MAC地址为:................%d.%d.%d.%d.%d.%d\r\n",lwipdev.mac[0],lwipdev.mac[1],lwipdev.mac[2],lwipdev.mac[3],lwipdev.mac[4],lwipdev.mac[5]);
//				printf("静态IP地址........................%d.%d.%d.%d\r\n",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
//				printf("子网掩码..........................%d.%d.%d.%d\r\n",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);
//				printf("默认网关..........................%d.%d.%d.%d\r\n",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
//			}
//		}
//		break;
//		default : break;
//	}
//}
//#endif 



























