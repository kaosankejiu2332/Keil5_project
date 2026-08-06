#include "uart.h"
#include "spi_25q32.h"
#include "mdelay.h"

typedef void (*app_func_t)(void);
app_func_t app_entry;
void jump_to_app(uint32_t app_addr);

int main()
{
	uart_init();
	spi_x_init();
	
	printf("INIT...\n");
	
	printf("man_id is %X\n", spi_flash_read_jedec_id());
	
	//���������־
	uint8_t info[8];
	spi_flash_read_data(GD25Q32_INFO_ADDR, info, 8);
	uint32_t firmware_size = (info[0] << 24) | (info[1] << 16) | (info[2] << 8) | info[3];
	uint32_t magic = (info[4] << 24) | (info[5] << 16) | (info[6] << 8) | info[7];
	
	//	printf("magic: 0x%08X\n", magic);
	
	if((magic == 0xAA55AA55) && (firmware_size > 0))
		{
		printf("size: %d...\n",firmware_size);
		
		//�����ڲ�flash ��0x08020000��ʼ
		fmc_unlock();//����
		for(uint32_t addr = 0x08020000; addr < 0x08100000; addr += 0x2000){
			uint32_t sector;
			if((addr >= 0x08020000) && (addr < 0x08040000)){
				sector = CTL_SECTOR_NUMBER_5;
			}else if((addr >= 0x08040000) && (addr < 0x08060000)){
				sector = CTL_SECTOR_NUMBER_6;
			}else if((addr >= 0x08060000) && (addr < 0x08080000)){
				sector = CTL_SECTOR_NUMBER_7;
			}else if((addr >= 0x08080000) && (addr < 0x080A0000)){
				sector = CTL_SECTOR_NUMBER_8;
			}else if((addr >= 0x080A0000) && (addr < 0x080C0000)){
				sector = CTL_SECTOR_NUMBER_9;
			}else if((addr >= 0x080C0000) && (addr < 0x080E0000)){
				sector = CTL_SECTOR_NUMBER_10;
			}else{
				sector = CTL_SECTOR_NUMBER_11;
			}
			fmc_sector_erase(sector);//
		}
		
		//��GD25Q32��ȡ�̼���д���ڲ�Flash
		uint8_t buf[256];
		for(uint32_t i = 0; i < firmware_size; i += 256){
			spi_flash_read_data(GD25Q32_UPDATE_ADDR + i, buf, 256);
			for(uint32_t j = 0; j < 256; j += 4){
				uint32_t word = *(uint32_t *)&buf[j];
				fmc_word_program(0x08020000 + i + j, word);
			}
		}
		fmc_lock();//����
		
		//���������־
		spi_flash_erase_sector(GD25Q32_INFO_ADDR / 4096);
		printf("UPDATA_OK...\n");
		}
	
	printf("jump...\n");
/**/		
	//��ת
//	typedef void (*app_func_t)(void);
//	app_func_t app_entry;
	uint32_t app_addr = 0x08020000;
	jump_to_app(app_addr);
	//���ջ����ַ�Ƿ���RAM��Χ��
//	if(((*(uint32_t *)app_addr) & 0x2FF00000) == 0x20000000){
////����
//		__disable_irq();
//		
//		usart_disable(USART0);
//		spi_disable(SPIx);
//		
//		//__set_MSP(*(__IO uint32_t *)app_addr);//����ջ��
//		app_entry = (app_func_t)*(__IO uint32_t *)(app_addr + 4);	
//		
//		app_entry();
//	}else{
//		printf("ERROR\n");
//	}
/**/	
	while(1){
	}
}

// void jump_to_app(uint32_t app_addr)
// {
// 	if(((*(__IO uint32_t *)app_addr) & 0x2FF00000) == 0x20000000){
// //		usart_disable(USART0);
// 		app_entry = (app_func_t)*(__IO uint32_t *)(app_addr + 4);
// 		__set_MSP(*(__IO uint32_t *)app_addr);
// 		app_entry();//执行函数
// 	}else{
// 		printf("ERROR\n");
// 	}
// }
