#include "driver/uart.h"
#include "freertos/semphr.h"
#include "freertos/FreeRTOS.h"

//LED PINS DEFINED IN FREERTOS TASKS
#define BTN_PIN 19

#define GPIO_OUT_REG (*((volatile uint32_t *)0x3FF44004)) //GPIO_OUT_REG address
#define GPIO_ENABLE_REG (*((volatile uint32_t *)0x3FF44020)) //GPIO_ENABLE_REG address
#define GPIO_IN_REG (*((volatile uint32_t *)0x3FF4403C)) //GPIO_IN_REG address
SemaphoreHandle_t gpio_mutex;
volatile char mode = 'b'; //default mode is blink
volatile int paused = 0;

//uart
void uart_init(void) //used in main to inititialize uart
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .stop_bits = UART_STOP_BITS_1,
        .parity = UART_PARITY_DISABLE,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
}

char uart_read_command(void) //used in led_task to read data and decide mode
{
    char data = '\0';
    int len = uart_read_bytes(UART_NUM_0, &data, 1, 100 / portTICK_PERIOD_MS);
    if (len > 0)
    {
       uart_write_bytes(UART_NUM_0, &data, 1);
    }
    if (len > 0 && (data == '1' || data == '0' || data == 'b'))
    {
        return data;
    }
    return '\0';
}

void uart_task(void *pvParameters)
{
    while(1)
    {
        char cmd = uart_read_command();
        if (cmd != '\0')
        {
            mode = cmd; //update mode if valid command received
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void btn_task(void *pvParameters)
{
    while(1)
    {
        if (!(GPIO_IN_REG & (1 << BTN_PIN)))
        {
            paused = 1;
        }
        else
        {
            paused = 0;
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

//led task using a single function for multiple leds
typedef struct 
{
    int PIN;
    int duration;
} led_config_t;

void led_task(void *pvParameters)
{
    led_config_t *cfg = (led_config_t *)pvParameters; //so cfg -> PIN and cfg -> duration can be used 
    uint32_t elapsed = 0;
    while(1)
    {
        if (paused)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS); //if paused, wait for 100ms and check again
            continue;
        }
        if (mode == '1')
        {
            //turn on led using cfg->PIN
            xSemaphoreTake(gpio_mutex, portMAX_DELAY);
            GPIO_OUT_REG |= (1 << cfg -> PIN);
            xSemaphoreGive(gpio_mutex);
        }
        else if (mode == '0')
        {
            //turn off led using cfg->PIN
            xSemaphoreTake(gpio_mutex, portMAX_DELAY);
            GPIO_OUT_REG &= ~(1 << cfg -> PIN);
            xSemaphoreGive(gpio_mutex);
        }
        else if (mode == 'b')
        {
            //blink led
            elapsed += 50;
            if (elapsed >= cfg -> duration)
            {
                xSemaphoreTake(gpio_mutex, portMAX_DELAY);
                GPIO_OUT_REG ^= (1 << cfg -> PIN);
                xSemaphoreGive(gpio_mutex);
                elapsed = 0;
            }
            
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    //initialize uart
    uart_init();
    static led_config_t led1_config = {.PIN = 2, .duration = 500};
    static led_config_t led2_config = {.PIN = 4, .duration = 1000};
    static led_config_t led3_config = {.PIN = 5, .duration = 1500};
    GPIO_ENABLE_REG |= (1 << led1_config.PIN) | (1 << led2_config.PIN) | (1 << led3_config.PIN); //enable GPIO pins
    gpio_mutex = xSemaphoreCreateMutex(); //create mutex for GPIO access

    xTaskCreate(uart_task, "uart_task", 2048, NULL, 5, NULL);
    xTaskCreate(btn_task, "btn_task", 2048, NULL, 5, NULL);
    xTaskCreate(led_task, "led_task_1", 2048, &led1_config, 4, NULL);
    xTaskCreate(led_task, "led_task_2", 2048, &led2_config, 4, NULL);
    xTaskCreate(led_task, "led_task_3", 2048, &led3_config, 4, NULL);
}
