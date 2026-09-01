#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

// Configure buttons
#define BUTTON_0 DT_ALIAS(sw0)
// #define BUTTON_1 DT_ALIAS(sw1)
static const struct gpio_dt_spec button_0 = GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});
static struct gpio_callback button_0_data;

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
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
int init_led();
int init_button();
K_THREAD_DEFINE(red_thread,STACKSIZERED,red_led_task,NULL,NULL,NULL,PRIORITYRED,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZEGREEN,green_led_task,NULL,NULL,NULL,PRIORITYGREEN,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZEYELLOW,yellow_led_task,NULL,NULL,NULL,PRIORITYYELLOW,0,0);
int led_state;
int previous_state = 0;

// Main program
int main(void)
{
	init_led();
	init_button();

	return 0;
}

// Initialize leds
int init_led() {

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

// Button initialization
int init_button() {

	int ret;
	if (!gpio_is_ready_dt(&button_0)) {
		printk("Error: button 0 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_0, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_0_data, button_0_handler, BIT(button_0.pin));
	gpio_add_callback(button_0.port, &button_0_data);
	printk("Set up button 0 ok\n");
	
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
					if(led_state != 4){
						led_state = 1;
					}
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
					if(led_state != 4){
						led_state = 2;
					}
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
					if(led_state != 4){
						led_state = 0;
					}
		        // 4. sleep for 2 seconds
                } 
                k_sleep(K_SECONDS(1));

	}
}

// Button interrupt handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button pressed\n");
	if(led_state != 4){
		previous_state = led_state;
		led_state = 4;
	}else{
		led_state = previous_state;
	}
}




