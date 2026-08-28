#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

// Red led thread initialization
#define STACKSIZERED 500
#define PRIORITYRED 5
// Green led thread initialization
#define STACKSIZEGREEN 500
#define PRIORITYGREEN 5
// Yellow led thread initialization
#define STACKSIZEYELLOW 500
#define PRIORITYYELLOW 5

void red_led_task(void *, void *, void*);
void green_led_task(void *, void *, void*);
void yellow_led_task(void *, void *, void*);
K_THREAD_DEFINE(red_thread,STACKSIZERED,red_led_task,NULL,NULL,NULL,PRIORITYRED,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZEGREEN,green_led_task,NULL,NULL,NULL,PRIORITYGREEN,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZEYELLOW,yellow_led_task,NULL,NULL,NULL,PRIORITYYELLOW,0,0);
int led_state;

// Main program
int main(void)
{
	init_led();

	return 0;
}

// Initialize leds
int  init_led() {

	// Led pin initialization
	int ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Led configure failed\n");		
		return ret;
	}
        ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Led configure failed\n");		
		return ret;
	}
        ret = gpio_pin_configure_dt(&blue, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Led configure failed\n");		
		return ret;
	}
	// set led off
	gpio_pin_set_dt(&red,0);
        gpio_pin_set_dt(&green,0);
        gpio_pin_set_dt(&blue,0);
	printk("Led initialized ok\n");
        led_state = 0;
	
	return 0;
}

// Task to handle red led
void red_led_task(void *, void *, void*) {
	
	printk("Red led thread started\n");
	while (true) {
                if(led_state == 0){
                        gpio_pin_set_dt(&red,1);
		        printk("Red on\n");
		        // 2. sleep for 2 seconds
		        k_sleep(K_SECONDS(1));
		        // 3. set led off
		        gpio_pin_set_dt(&red,0);
		        printk("Red off\n");
                        led_state = 1;
		        // 4. sleep for 2 seconds
                        } 
                k_sleep(K_SECONDS(1));
	}
}

// Task to handle yellow led
void yellow_led_task(void *, void *, void*) {
	
	printk("Yellow led thread started\n");
	while (true) {
                if(led_state == 1){
                        // 1. set led on 
		        gpio_pin_set_dt(&green,1);
                        gpio_pin_set_dt(&red,1);
		        printk("Yellow on\n");
		        // 2. sleep for 2 seconds
		        k_sleep(K_SECONDS(1));
		        // 3. set led off
		        gpio_pin_set_dt(&green,0);
                        gpio_pin_set_dt(&red,0);
		        printk("Yellow off\n");
                        led_state = 2;
		        // 4. sleep for 2 seconds
                } 
                k_sleep(K_SECONDS(1));

	}
}

// Task to handle red and green leds
void green_led_task(void *, void *, void*) {
	
	printk("Green led thread started\n");
	while (true) {
                if(led_state == 2){
                        // 1. set led on 
		        gpio_pin_set_dt(&green,1);
		        printk("Green on\n");
		        // 2. sleep for 2 seconds
		        k_sleep(K_SECONDS(1));
		        // 3. set led off
		        gpio_pin_set_dt(&green,0);
		        printk("Green off\n");
                        led_state = 0;
		        // 4. sleep for 2 seconds
                } 
                k_sleep(K_SECONDS(1));

	}
}



