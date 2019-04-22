#include "test_adc.h"
#include "include.h"

#if TEST_ADC_DEMO
#include "string.h"
#include "stdlib.h"
#include "mem_pub.h"
#include "uart_pub.h"
#include "gpio_pub.h"
#include "gpio.h"
#include "rtos_pub.h"
#include "Error.h"
#include "saradc_pub.h"

beken_timer_t *test_adc_demo_timer = NULL;
saradc_desc_t test_adc_demo_adc1;
DD_HANDLE test_adc_demo_handle = -1;

void test_adc_demo_isr_cb(void)
{
    uint32_t mode = 0x00;

    if(test_adc_demo_adc1.data_buff_size == test_adc_demo_adc1.current_sample_data_cnt)
    {
        ddev_control(test_adc_demo_handle, SARADC_CMD_RUN_OR_STOP_ADC, &mode);
    }
}

VOID test_adc_demo_init(void)
{
    uint32_t mode = 0x01;
    UINT32 status;

    test_adc_demo_adc1.channel = 1;
    test_adc_demo_adc1.data_buff_size = 20;
    test_adc_demo_adc1.mode = 3;
    test_adc_demo_adc1.current_read_data_cnt = 0;
    test_adc_demo_adc1.current_sample_data_cnt = 0;
    test_adc_demo_adc1.has_data = 0;
    test_adc_demo_adc1.p_Int_Handler = test_adc_demo_isr_cb;
    test_adc_demo_adc1.pData = os_malloc(sizeof(UINT16) * test_adc_demo_adc1.data_buff_size);
    if(!test_adc_demo_adc1.pData)
    {
        os_printf("malloc failed\n");
        return;
    }

    test_adc_demo_handle = ddev_open(SARADC_DEV_NAME, &status, (UINT32)&test_adc_demo_adc1);
    if(status != 0)
    {
        os_printf("adc_open failed\n");
    }
}

void test_adc_demo_timer_alarm( void *arg )
{
    uint32_t mode = 0x01;
    uint32_t i;

    if(test_adc_demo_adc1.current_sample_data_cnt)
    {
        os_printf("adc_cnt:%d\n", test_adc_demo_adc1.current_sample_data_cnt);
        for(i = 0; i < test_adc_demo_adc1.current_sample_data_cnt; i++)
        {
            os_printf("%x\n", test_adc_demo_adc1.pData[i]);
        }
    }

    memset(test_adc_demo_adc1.pData, 0, test_adc_demo_adc1.data_buff_size);
    ddev_control(test_adc_demo_handle, SARADC_CMD_CLEAR_INT, 0);
    test_adc_demo_adc1.current_sample_data_cnt = 0;
    ddev_control(test_adc_demo_handle, SARADC_CMD_RUN_OR_STOP_ADC, &mode);
}

int create_test_adc_demo_timer(void)
{
    OSStatus err;

    if(test_adc_demo_timer == NULL)
    {
        test_adc_demo_timer = (beken_timer_t *) os_malloc( sizeof(beken_timer_t) );
    }

    if(test_adc_demo_timer == NULL)
    {
        os_printf("test_adc_demo_timer error!\r\n");
        return kGeneralErr;
    }

    err = rtos_init_timer(test_adc_demo_timer, 500, test_adc_demo_timer_alarm, 0);
    if(kNoErr != err)
    {
        if(test_adc_demo_timer)
        {
            os_free(test_adc_demo_timer);
            test_adc_demo_timer = NULL;
        }
        os_printf("test_adc_demo_timer init failed!\r\n");
    }

    return err;
}

void destroy_test_adc_demo_timer( void )
{
    rtos_deinit_timer( test_adc_demo_timer );
    if(test_adc_demo_timer)
    {
        os_free(test_adc_demo_timer);
        test_adc_demo_timer = NULL;
    }
}

int demo_start(void)
{
    test_adc_demo_init();

    if(create_test_adc_demo_timer() == kNoErr)
    {
        if(kNoErr == rtos_start_timer(test_adc_demo_timer))
        {
            return kNoErr;
        }

        destroy_test_adc_demo_timer();
        os_printf("test_adc_timer start failed!\r\n");
    }

    return kGeneralErr;
}

#endif /*TEST_ADC_DEMO*/

// EOF

