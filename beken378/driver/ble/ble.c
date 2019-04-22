#include "include.h"

#include "drv_model_pub.h"
#include "intc_pub.h"
#include "uart_pub.h"

#include "ble.h"
#include "ble_pub.h"
#include "rwble.h"
#include "udebug.h"
//#include "bkDriverUart.h"

#include "typedef.h"
#include "sys_ctrl_pub.h"
#include "icu_pub.h"
#include "intc_pub.h"
#include "common_bt.h"

#include "arm_arch.h"

#if defined(SUPPORT_MIDEA_BLE)
msm_ble_adv_content_t ms_adv_init_info;
#endif

#define BLE_MSG_QUEUE_COUNT          (10)

beken_queue_t ble_msg_que = NULL;
beken_thread_t ble_thread_handle = NULL;;
uint32_t ble_stack_size = 4096;
uint8_t ble_flag = 0;
uint32_t  ble_dut_flag = 0;

typedef struct ble_cfg_st {
    struct bd_addr mac;
    char name[APP_DEVICE_NAME_LENGTH_MAX];
}BLE_CFG_ST, *BLE_CFG_PTR; 
BLE_CFG_ST ble_cfg;

static SDD_OPERATIONS ble_op =
{
    ble_ctrl
};


extern /*const */struct bd_addr common_default_bdaddr;
extern void uart_isr(void);
extern void intc_service_change_handler(UINT8 int_num, FUNCPTR isr);
extern void wifi_get_mac_address(char *mac, u8 type);

void ble_intc_set(uint32_t enable)
{
    if(enable)
        intc_enable(FIQ_BLE);
    else
        intc_disable(FIQ_BLE);
}

void ble_clk_power_up(void)
{
    UINT32 param;
    param = PWD_BLE_CLK_BIT;
    sddev_control(ICU_DEV_NAME, CMD_TL410_CLK_PWR_UP, &param);
}

void ble_clk_power_down(void)
{
    UINT32 param;
    param = PWD_BLE_CLK_BIT;
    sddev_control(ICU_DEV_NAME, CMD_TL410_CLK_PWR_DOWN, &param);
}

void ble_switch_rf_to_wifi(void)
{
    //REG_WRITE((0x00802800+(04*4)), 0x02);

    // if in ble dut mode, no need change back to wifi any more.
    // ble dut mode can not exit until power off
    if(ble_dut_flag)
        return;
    
    sddev_control(SCTRL_DEV_NAME, CMD_BLE_RF_BIT_CLR, NULL);
}

void ble_switch_rf_to_ble(void)
{
    //REG_WRITE((0x00802800+(04*4)), 0x00);
    rwnx_cal_set_txpwr(1, 11);
    sddev_control(SCTRL_DEV_NAME, CMD_BLE_RF_BIT_SET, NULL);
}

void ble_set_power_up(uint32 up)
{
    if(up)
        sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_BLE_POWERUP, NULL);
    else
        sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_BLE_POWERDOWN, NULL);
}

void ble_set_pn9_trx(uint32 param)
{
    UINT32 reg;
    
    if(PN9_RX == param)
    {
        reg = 0x0;
        REG_WRITE(REG_BLE_XVR_TRX_CONFIG_ADDR, reg);
        delay(10);
        reg = 0x2400;
        REG_WRITE(REG_BLE_XVR_TRX_CONFIG_ADDR, reg);
    }
    else
    {
        reg = 0x0;
        REG_WRITE(REG_BLE_XVR_TRX_CONFIG_ADDR, reg);
        delay(10);
        reg = 0x3800;
        REG_WRITE(REG_BLE_XVR_TRX_CONFIG_ADDR, reg);
    }
}

void ble_init(void)
{
	intc_service_register( FIQ_BLE, PRI_FIQ_BLE, ble_isr );

	sddev_register_dev( BLE_DEV_NAME, &ble_op );

	return;
}

void ble_exit(void)
{
	sddev_unregister_dev( BLE_DEV_NAME );

	return;
}

UINT32 ble_ctrl( UINT32 cmd, void *param )
{
    UINT32 reg;
	UINT32 ret = BLE_SUCCESS;

	switch(cmd)
	{
	case CMD_BLE_REG_INIT:
		break;
		
	case CMD_BLE_REG_DEINIT:
		break;

    case CMD_BLE_SET_CHANNEL:
        reg = REG_READ(REG_BLE_XVR_CHANNEL_CONFIG_ADDR);
        reg &= ~(REG_BLE_XVR_CHANNEL_VALUE_MASK << REG_BLE_XVR_CHANNEL_VALUE_POST);
        reg |= (*(UINT32 *)param) << REG_BLE_XVR_CHANNEL_VALUE_POST;
        REG_WRITE(REG_BLE_XVR_CHANNEL_CONFIG_ADDR, reg);
        reg = REG_READ(REG_BLE_XVR_TRX_CONFIG_ADDR);
        reg &= ~(1 << REG_BLE_XVR_TEST_RADIO_POST);
        REG_WRITE(REG_BLE_XVR_TRX_CONFIG_ADDR, reg);
        delay(10);
        reg |= (1 << REG_BLE_XVR_TEST_RADIO_POST);
        REG_WRITE(REG_BLE_XVR_TRX_CONFIG_ADDR, reg);
        break;

    case CMD_BLE_AUTO_CHANNEL_ENABLE:
        reg = REG_READ(REG_BLE_XVR_CHANNEL_CONFIG_ADDR);
        reg |= 1 << REG_BLE_XVR_AUTO_CHANNEL_POST;
        REG_WRITE(REG_BLE_XVR_CHANNEL_CONFIG_ADDR, reg);
        break;

    case CMD_BLE_AUTO_CHANNEL_DISABLE:
        reg = REG_READ(REG_BLE_XVR_CHANNEL_CONFIG_ADDR);
        reg &= ~(1 << REG_BLE_XVR_AUTO_CHANNEL_POST);
        REG_WRITE(REG_BLE_XVR_CHANNEL_CONFIG_ADDR, reg);
        break;

    case CMD_BLE_AUTO_SYNCWD_ENABLE:
        reg = REG_READ(REG_BLE_XVR_CHANNEL_CONFIG_ADDR);
        reg |= 1 << REG_BLE_XVR_AUTO_SYNCWD_POST;
        REG_WRITE(REG_BLE_XVR_CHANNEL_CONFIG_ADDR, reg);
        break;

    case CMD_BLE_AUTO_SYNCWD_DISABLE:
        reg = REG_READ(REG_BLE_XVR_CHANNEL_CONFIG_ADDR);
        reg &= ~(1 << REG_BLE_XVR_AUTO_SYNCWD_POST);
        REG_WRITE(REG_BLE_XVR_CHANNEL_CONFIG_ADDR, reg);
        break;

    case CMD_BLE_SET_PN9_TRX:
        ble_set_pn9_trx(*(UINT32 *)param);
        break;

    case CMD_BLE_SET_GFSK_SYNCWD:
        reg = (*(UINT32 *)param);
        REG_WRITE(REG_BLE_XVR_GFSK_SYNCWD_ADDR, reg);
        break;

    case CMD_BLE_HOLD_PN9_ESTIMATE:
        reg = REG_READ(REG_BLE_XVR_TRX_CONFIG_ADDR);
        reg |= 1 << REG_BLE_XVR_HOLD_ESTIMATE_POST;
        REG_WRITE(REG_BLE_XVR_TRX_CONFIG_ADDR, reg);
        break;

    case CMD_BLE_STOP_COUNTING:
        reg = REG_READ(REG_BLE_XVR_TRX_CONFIG_ADDR);
        reg &= ~(1 << REG_BLE_XVR_PN9_RECV_POST);
        REG_WRITE(REG_BLE_XVR_TRX_CONFIG_ADDR, reg);
        break;

    case CMD_BLE_START_COUNTING:
        reg = REG_READ(REG_BLE_XVR_TRX_CONFIG_ADDR);
        reg |= 1 << REG_BLE_XVR_PN9_RECV_POST;
        reg &= ~(1 << REG_BLE_XVR_HOLD_ESTIMATE_POST);
        REG_WRITE(REG_BLE_XVR_TRX_CONFIG_ADDR, reg);
        break;

	default:
		ret = BLE_FAILURE;
        break;
	}
	
	return ret;
}

void ble_isr(void)
{
	rwble_isr();
	return;
}

static void ble_main( void *arg )
{
    memcpy(&common_default_bdaddr, &ble_cfg.mac, sizeof(struct bd_addr));
    memcpy(&app_dflt_dev_name, &ble_cfg.name, APP_DEVICE_NAME_LENGTH_MAX); 

    {
    UINT8 *mac = (UINT8 *)&ble_cfg.mac;
    
    os_printf("ble name:%s, %02x:%02x:%02x:%02x:%02x:%02x\r\n", 
        app_dflt_dev_name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    ble_flag = 1;
	rw_main();

    rtos_deinit_queue(&ble_msg_que);
    ble_msg_que = NULL;
   
    ble_thread_handle = NULL;
	rtos_delete_thread(NULL);
}

static void ble_thread_start(void)
{
	OSStatus ret; 

    if(!ble_thread_handle && !ble_msg_que)
    {
    	ret = rtos_init_queue(&ble_msg_que, 
    							"ble_msg_queue",
    							sizeof(BLE_MSG_T),
    							BLE_MSG_QUEUE_COUNT);
        ASSERT(0 == ret);
        
    	ret = rtos_create_thread(&ble_thread_handle, 
    			20,
    			"ble", 
    			(beken_thread_function_t)ble_main, 
    			(unsigned short)ble_stack_size, 
    			(beken_thread_arg_t)0);
    	
        ASSERT(0 == ret);
    }
}

void ble_activate(char *ble_name)
{
	OSStatus ret;
    UINT32 len;

//    bk_wlan_stop(1);

    if(!ble_name) {
        os_printf("ble start no ble name\r\n");
        ble_name = APP_DFLT_DEVICE_NAME;
    }

    if(ble_thread_handle) {
        ble_flag = 0;
        ble_send_msg(BLE_MSG_EXIT);

        while(ble_thread_handle)
            rtos_delay_milliseconds(100);
    }

    memset(&ble_cfg, 0, sizeof(BLE_CFG_ST));
    
    wifi_get_mac_address((char *)&ble_cfg.mac, 2);  // get sta's mac addr 
    ble_cfg.mac.addr[0] += 1; // add 1, diff from wifi's mac 

    len = strlen(ble_name);
    len = (len > APP_DEVICE_NAME_LENGTH_MAX)? APP_DEVICE_NAME_LENGTH_MAX:len;
    memcpy(&ble_cfg.name, ble_name, len);
    
    ble_thread_start();
}

void ble_dut_start(void)
{
    OSStatus ret;
    if(!ble_thread_handle) 
    {
        ble_dut_flag = 1;

        os_printf("ble_dut\r\n");
        
        rwnx_cal_set_txpwr(1, 11);

        intc_service_change_handler(IRQ_UART2, uart_isr);    
        
        ble_activate(NULL);
    }
}

UINT8 ble_is_start(void)
{
    return (ble_thread_handle == NULL)? 0:1;
}

UINT8* ble_get_mac_addr(void)
{
    return (UINT8*)&common_default_bdaddr;
}

UINT8* ble_get_name(void)
{
    return (UINT8*)&app_dflt_dev_name;
}

void ble_send_msg(UINT32 data)
{
	OSStatus ret;
	BLE_MSG_T msg;

    if(ble_msg_que) {
    	msg.data = data;
    	
    	ret = rtos_push_to_queue(&ble_msg_que, &msg, BEKEN_NO_WAIT);
    	if(0 != ret)
    	{
    		//os_printf("ble_send_msg failed\r\n");
    	}
    }
}

