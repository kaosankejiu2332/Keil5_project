#include "my_flash.h"
#include "gd32f4xx_fmc.h"
#include "my_gd25q32.h"
#include "boot.h"

#define APP_FLASH_SECTOR_SIZE  0x00020000UL
#define COPY_BUFFER_SIZE       512U

static const uint32_t app_sectors[] = {
    CTL_SECTOR_NUMBER_5,
    CTL_SECTOR_NUMBER_6,
    CTL_SECTOR_NUMBER_7,
    CTL_SECTOR_NUMBER_8,
    CTL_SECTOR_NUMBER_9,
    CTL_SECTOR_NUMBER_10,
    CTL_SECTOR_NUMBER_11
};

uint8_t flash_sector_erase(uint32_t firmware_size)
{
    uint32_t sector_count;
    uint32_t index;

    if((firmware_size == 0U) || (firmware_size > APP_MAX_SIZE)) {
        return 0U;
    }

    sector_count = (firmware_size + APP_FLASH_SECTOR_SIZE - 1U)
                   / APP_FLASH_SECTOR_SIZE;
    if(sector_count > (sizeof(app_sectors) / sizeof(app_sectors[0]))) {
        return 0U;
    }

    for(index = 0U; index < sector_count; index++) {
        if(FMC_READY != fmc_sector_erase(app_sectors[index])) {
            return 0U;
        }
    }
    return 1U;
}

uint8_t flash_update_data(uint32_t firmware_size)
{
    static uint8_t data[COPY_BUFFER_SIZE];
    uint32_t offset = 0U;

    if((firmware_size == 0U) || (firmware_size > APP_MAX_SIZE)) {
        return 0U;
    }

    while(offset < firmware_size) {
        uint32_t remaining = firmware_size - offset;
        uint16_t read_len = (remaining > COPY_BUFFER_SIZE)
                            ? COPY_BUFFER_SIZE
                            : (uint16_t)remaining;
        uint16_t index = 0U;

        gd25q32_read_data(GD25Q32_UPDATE_ADDR + offset, data, read_len);

        while((uint16_t)(index + 4U) <= read_len) {
            uint32_t word = ((uint32_t)data[index])
                            | ((uint32_t)data[index + 1U] << 8)
                            | ((uint32_t)data[index + 2U] << 16)
                            | ((uint32_t)data[index + 3U] << 24);

            if(FMC_READY != fmc_word_program(APP_START_ADDR + offset + index,
                                             word)) {
                return 0U;
            }
            index = (uint16_t)(index + 4U);
        }

        if(index < read_len) {
            uint32_t word = 0xFFFFFFFFUL;
            uint8_t tail_index = 0U;

            while(index < read_len) {
                word &= ~(0xFFUL << (tail_index * 8U));
                word |= (uint32_t)data[index] << (tail_index * 8U);
                index++;
                tail_index++;
            }

            if(FMC_READY != fmc_word_program(APP_START_ADDR + offset
                                             + (read_len & ~3U), word)) {
                return 0U;
            }
        }

        offset += read_len;
    }

    return 1U;
}
