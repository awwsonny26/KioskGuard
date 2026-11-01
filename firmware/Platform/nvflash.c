#include "board.h"
#include "nvflash.h"
#include <string.h>
#define AUDIT_START 0x08007000U
static bool read_bytes(void *ctx, unsigned offset, void *out, size_t n)
{
    (void)ctx;
    if(offset+n>2*KG_AUDIT_PAGE)return false;
    memcpy(out,(const void *)(AUDIT_START+offset),n);return true;
}
static bool program_word(void *ctx, unsigned offset, const uint8_t bytes[8])
{
    (void)ctx;
    if(offset%8 || offset+8>2*KG_AUDIT_PAGE)return false;
    uint64_t value;memcpy(&value,bytes,8);
    if(HAL_FLASH_Unlock()!=HAL_OK)return false;
    HAL_StatusTypeDef result=HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,AUDIT_START+offset,value);
    HAL_FLASH_Lock();return result==HAL_OK;
}
static bool erase_page(void *ctx, unsigned page)
{
    (void)ctx;if(page>1)return false;
    FLASH_EraseInitTypeDef request={0};uint32_t error;
    request.TypeErase=FLASH_TYPEERASE_PAGES;request.Page=14+page;request.NbPages=1;
    if(HAL_FLASH_Unlock()!=HAL_OK)return false;
    HAL_StatusTypeDef result=HAL_FLASHEx_Erase(&request,&error);
    HAL_FLASH_Lock();
    if(result!=HAL_OK)return false;
    const volatile uint8_t *bytes=(const volatile uint8_t *)(AUDIT_START+page*KG_AUDIT_PAGE);
    for(unsigned i=0;i<KG_AUDIT_PAGE;++i)if(bytes[i]!=0xffU)return false;
    return true;
}
kg_audit_io kg_nvflash_io(void)
{
    kg_audit_io io={NULL,read_bytes,program_word,erase_page};return io;
}
