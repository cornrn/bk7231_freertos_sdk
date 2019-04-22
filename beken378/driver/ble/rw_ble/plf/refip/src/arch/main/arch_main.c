/**
 ****************************************************************************************
 *
 * @file arch_main.c
 *
 * @brief Main loop of the application.
 *
 * Copyright (C) RivieraWaves 2009-2015
 *
 *
 ******** ********************************************************************************
 */

 
/*
 * INCLUDES
 ****************************************************************************************
 */
#include "rwip_config.h" // RW SW configuration

#include "architect.h"      // architectural platform definitions
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition
#include "boot.h"      // boot definition
#include "rwip.h"      // RW SW initialization
#if (BLE_APP_PRESENT)
#include "application.h"       // application functions
#endif // BLE_APP_PRESENT
#include "app_task.h"
#include "uart.h"

#include "RomCallFlash.h"
#include "error.h"
#include "ble_pub.h"

/**
 ****************************************************************************************
 * @addtogroup DRIVERS
 * @{
 *
 *
 * ****************************************************************************************
 */

/*
 * DEFINES
 ****************************************************************************************
 */
// NVDS location in FLASH : 0x00041000 (260KB )
#define NVDS_FLASH_ADDRESS          (0x00041000) 

#define NVDS_FLASH_SIZE             (0x00001000) // NVDS SIZE at flash Use 0x1000

/// Default BD address
struct bd_addr common_default_bdaddr={{0x89, 0x77, 0x22, 0x31, 0x02, 0x00}};

/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */

/// Description of unloaded RAM area content
struct unloaded_area_tag
{
    // status error
    uint32_t error;
};


/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

struct rwip_eif_api uart_api;

extern void ble_uart_read(uint8_t *bufptr, uint32_t size, void (*callback) (void*, uint8_t), void* dummy);
extern void ble_uart_write(uint8_t *bufptr, uint32_t size, void (*callback) (void*, uint8_t), void* dummy);
extern void ble_uart_flow_on(void);
extern void ble_uart_flow_off(void);

beken_semaphore_t ble_interrupt_sema;

int ble_sem_create(void)
{
#if CFG_SUPPORT_ALIOS
    return rtos_init_semaphore(&ble_interrupt_sema, 0);
#else
    return rtos_init_semaphore(&ble_interrupt_sema, 1);
#endif
}

int ble_sem_set(void)
{
    return rtos_set_semaphore(&ble_interrupt_sema);
}

int ble_sem_wait(uint32_t ms)
{
	return rtos_get_semaphore(&ble_interrupt_sema, ms);
}


void rwip_eif_api_init(void)
{
	uart_api.read = &ble_uart_read;
	uart_api.write = &ble_uart_write;
	uart_api.flow_on = &ble_uart_flow_on;
	uart_api.flow_off = &ble_uart_flow_off;
}


#if !(BLE_EMB_PRESENT) && !(BT_EMB_PRESENT)
// Creation of uart second external interface api

#endif // !BLE_EMB_PRESENT && !(BT_EMB_PRESENT)

#if (PLF_DEBUG)
/// Variable to enable infinite loop on assert
volatile int dbg_assert_block = 1;
#endif //PLF_DEBUG


/// Variable storing the reason of platform reset
uint32_t error = RESET_NO_ERROR;

uint32_t critical_sec_cnt = 0;

/*
 * LOCAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

static void Stack_Integrity_Check(void);


/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */


/*
 * EXPORTED FUNCTION DEFINITIONS
 ****************************************************************************************
 */

#if (PLF_DEBUG)
void assert_err(const char *condition, const char * file, int line)
{
	UART_PRINTF("%s,condition %s,file %s,line = %d\r\n",__func__,condition,file,line);
    // Trigger assert message
    //rwip_assert_err(file, line, 0, 0);
    // Let time for the message transfer
    /*
    for(int i = 0; i<2000;i++){dbg_assert_block = 1;};

    asrt_line_set(line);
    asrt_addr_setf((uint32_t)file);
    asrt_trigg_setf(1);

    GLOBAL_INT_STOP();

    while(dbg_assert_block);
	*/
}

void assert_param(int param0, int param1, const char * file, int line)
{
	UART_PRINTF("%s,param0 = %d,param1 = %d,file = %s,line = %d\r\n",__func__,param0,param1,file,line);
    // Trigger assert message
    //rwip_assert_err(file, line, param0, param1);
    // Let time for the message transfer
    /*  
    for(int i = 0; i<2000;i++){dbg_assert_block = 1;};

    asrt_line_set(line);
    asrt_addr_setf((uint32_t)file);
    asrt_params_setf(1);
    asrt_param_1_setf(param0);
    asrt_param_2_setf(param1);
    asrt_params_setf(1);
    asrt_trigg_setf(1);

    GLOBAL_INT_STOP();
    while(dbg_assert_block);
	*/
}

void assert_warn(int param0, int param1, const char * file, int line)
{
	 UART_PRINTF("%s,param0 = %d,param1 = %d,file = %s,line = %d\r\n",__func__,param0,param1,file,line);
 
    // Trigger assert message
    //rwip_assert_err(file, line, param0, param1);
 	/*
    asrt_line_set(line);
    asrt_addr_setf((uint32_t)file);
    asrt_params_setf(0);
    asrt_warn_setf(1);
    */
}

void dump_data(uint8_t* data, uint16_t length)
{
	UART_PRINTF("%s,data = %d,length = %d,file = %s,line = %d\r\n",__func__,data,length);
 	/*
    asrt_param_1_setf(length);
    asrt_params_setf(1);
    asrt_addr_setf((uint32_t)data);
    asrt_warn_setf(1);
	*/
}
#endif //PLF_DEBUG

uint16_t get_stack_usage(void)
{
#if 0
    uint8_t *ptr = (uint8_t*)(STACK_BASE_SVC);

    while(*ptr++ == BOOT_PATTERN_SVC);

    return (uint16_t)((uint32_t)STACK_BASE_SVC + (uint32_t)STACK_LEN_SVC - (uint32_t)ptr);
#endif
    return 0;
}

void platform_reset(uint32_t error)
{
    void (*pReset)(void);

    // Disable interrupts
    GLOBAL_INT_STOP();
    printf("-----platform_reset----%x\r\n", error);
    #if PLF_UART
    // Wait UART transfer finished

    #if !(BLE_EMB_PRESENT) && !(BT_EMB_PRESENT)
	//uart2_finish_transfers();
    #endif // !BLE_EMB_PRESENT && !(BT_EMB_PRESENT)
    #endif //PLF_UART


    if(error == RESET_AND_LOAD_FW || error == RESET_TO_ROM)
    {
        // Not yet supported
    }
    else
    {
        // Restart FW
        pReset = (void * )(0x0);
        pReset();
    }
}




/**
 *******************************************************************************
 * @brief RW main function.
 *
 * This function is called right after the booting process has completed.
 *
 * @return status   exit status
 *******************************************************************************
 */

extern struct rom_env_tag rom_env;

void rw_main(void)
{
    /*
     ***************************************************************************
     * Platform initialization
     ***************************************************************************
     */
#if DEEP_SLEEP	
	uint8_t sleep_type = 0;
    ble_sem_create();
#endif
    // Initialize random process
    srand(1);

    rwip_eif_api_init();

#if  (PLF_NVDS)
    // Initialize NVDS module
    nvds_init((uint32_t *)NVDS_FLASH_ADDRESS, NVDS_FLASH_SIZE);
#endif 
    rom_env_init(&rom_env);

   
	printf("-----rw_main task init----\r\n");
	//krhino_task_sleep(5000);
		
    /*
      ***************************************************************************
      * RW SW stack initialization
      ***************************************************************************
      */
    // Initialize RW SW stack
	rwip_reg_init();
    rwip_init(0);
	
    // finally start interrupt handling
    GLOBAL_INT_START();

    printf("-----rw_main  start----\r\n");
    /*
     ***************************************************************************
     * Main loop
     ***************************************************************************
     */

    while(1)
    {	
        OSStatus err;
        BLE_MSG_T msg;
        
        err = rtos_pop_from_queue(&ble_msg_que, &msg, BEKEN_WAIT_FOREVER);
        if(kNoErr == err)
        {
        	switch(msg.data) {
                case BLE_MSG_POLL:
                {
                  	//schedule all pending events      
                 	rwip_schedule();	  
            	
            	    // Checks for sleep have to be done with interrupt disabled
                	GLOBAL_INT_DIS();
            	 
                    #if DEEP_SLEEP	
            		// Check if the processor clock can be gated
                	sleep_type = rwip_sleep();	
            		if(sleep_type ==  RW_MCU_DEEP_SLEEP)
                	{	
            			ble_sem_wait(BEKEN_WAIT_FOREVER);
                	}
                    #endif
            		Stack_Integrity_Check();
                	GLOBAL_INT_RES();
                }
                break;
                case BLE_MSG_EXIT: 
                {
                    os_printf("ble is stopped,%d\r\n", ble_flag);
                    goto ble_main_exit;
                }
                break;

                default:
                break;
        	}
        }
        
    }

ble_main_exit:
    os_printf("ble main exit\r\n");
    
	rwip_reg_deinit();
}

const struct rwip_eif_api* rwip_eif_get(uint8_t type)
{
    const struct rwip_eif_api* ret = NULL;
    switch(type)
    {
        case RWIP_EIF_AHI:
        {
            ret = &uart_api;
        }
        break;
        #if (BLE_EMB_PRESENT) || (BT_EMB_PRESENT)
        case RWIP_EIF_HCIC:
        {
            ret = &uart_api;
        }
        break;
        #elif !(BLE_EMB_PRESENT) || !(BT_EMB_PRESENT)
        case RWIP_EIF_HCIH:
        {
            ret = &uart_api;
        }
        break;
        #endif 
        default:
        {
            ASSERT_INFO(0, type, 0);
        }
        break;
    }
    
    return ret;
}

static void Stack_Integrity_Check(void)
{

	
	
}


/// @} DRIVERS
