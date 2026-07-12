#include "my_flash.h"
#include "gd32f4xx_fmc.h"
#include "my_gd25q32.h"
#include "boot.h"
#include <string.h>   // memset
#include <stdio.h>    // printf
#include "systick.h"

/*
 * 擦除内部FLASH的扇区       
 */
void flash_sector_erase(void)
{	uint32_t i;
	for(i = APP_START_ADDR; i < 0X08100000; i+= 0x20000){
		uint32_t sector;
		if(i < 0x08040000){
			sector = CTL_SECTOR_NUMBER_5;
		}else if(i < 0x08060000){
			sector = CTL_SECTOR_NUMBER_6;
		}else if(i < 0x08080000){
			sector = CTL_SECTOR_NUMBER_7;
		}else if(i < 0x080A0000){
			sector = CTL_SECTOR_NUMBER_8;
		}else if(i < 0x080C0000){
			sector = CTL_SECTOR_NUMBER_9;
		}else if(i < 0X080E0000){
			sector = CTL_SECTOR_NUMBER_10;
		}else {
			sector = CTL_SECTOR_NUMBER_11;
		}
		fmc_sector_erase(sector);
		}
}

/*
 * 注：调用前先执行擦除APP区
 * GD25Q32中的数据写入MCU Flash
 * 参数: firmware_size  - 记录的文件大小
 *        
 */

void flash_update_data(uint32_t firmware_size)//参数传记录的文件大小
{	
	uint32_t i;
	uint16_t j;

	//先解锁
	static uint8_t data[520];  //static 把数组从栈移到全局数据段，不怕压栈
	//假设固件大小为128KB


	
	if(firmware_size % 4 == 0 )
	{	
		//firmware_size是4的整数倍但不能确保是512的整数倍
		


		//先把512字节整数倍的个数写入
		for(i = 0; i < firmware_size/512; i ++)//假设的一次性取512字节
		{
		
		gd25q32_read_data((uint32_t)GD25Q32_UPDATE_ADDR + i*512,data , 512);
		
		for(j=0;j < 512; j += 4 )
		{
			uint32_t word=*(uint32_t *)(&data[j]);//一次性取4字节
			fmc_word_program(APP_START_ADDR + i*512 + j, word);//4个字节写入
		}


		}

		memset(data,0,512);//数组清零，后续把数据读到data填不满512字节
		//再把剩余字节个数写入，不是512倍数但一定是4倍数。最前面的判断
		if(firmware_size % 512 != 0)
		{
		gd25q32_read_data((uint32_t)GD25Q32_UPDATE_ADDR + i*512,data ,
		firmware_size % 512);
		for(j=0;j < firmware_size % 512; j += 4 )
		{
			uint32_t word=*(uint32_t *)(&data[j]);//一次性取4字节
			fmc_word_program(APP_START_ADDR + i*512 + j, word);//4个字节写入
		}
		}
		
		

	}else
	{	
		fmc_lock();
		while(1)
		{
			printf("firmware size error\r\n");
			delay_ms(500);
		}
		
	}

}

