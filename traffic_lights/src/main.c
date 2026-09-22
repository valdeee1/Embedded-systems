/*

Ajanotto ja printk-tulostukset löytyy

*/

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>
#include <stdlib.h>
#include <string.h>

static const struct gpio_dt_spec red   = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue  = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

#define BUTTON_0 DT_ALIAS(sw0)
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

static const struct gpio_dt_spec button_0 = GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});
static struct gpio_callback button_0_data;
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

#define STACKSIZE 500
#define PRIORITY 5

K_FIFO_DEFINE(data_fifo);

struct data_t {
    void *fifo_reserved;
    char msg[20];
};

K_MUTEX_DEFINE(led_mutex);
K_CONDVAR_DEFINE(cv_red);
K_CONDVAR_DEFINE(cv_yellow);
K_CONDVAR_DEFINE(cv_green);

K_MUTEX_DEFINE(release_mutex);
K_CONDVAR_DEFINE(cv_release);
static bool task_done = false;

static char active_color = 0;
static int active_duration_ms = 1000;
uint64_t elapsed_time_us_red = 0;
uint64_t elapsed_time_us_yellow = 0;
uint64_t elapsed_time_us_green = 0;

static volatile bool system_paused = false;

void red_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);
void uart_rx_task(void *, void *, void *);
static void dispatcher_task(void *, void *, void *);
void debug_task(void *, void *, void*);
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
int init_uart(void);
int init_led(void);
int init_button(void);

K_THREAD_DEFINE(red_thread,    STACKSIZE, red_led_task,    NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(yellow_thread, STACKSIZE, yellow_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(green_thread,  STACKSIZE, green_led_task,  NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(dis_thread,    STACKSIZE, dispatcher_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(uart_thread,   STACKSIZE, uart_rx_task,    NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(debug_thread,  STACKSIZE, debug_task,      NULL, NULL, NULL, PRIORITY, 0, 0);

int main(void)
{
    init_led();
    init_button();
    timing_init();

    if (init_uart() != 0) {
        printk("Error: UART init failed\n");
        return 1;
    }
    return 0;
}

int init_led(void)
{
    if (gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE) < 0 ||
        gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE) < 0 ||
        gpio_pin_configure_dt(&blue, GPIO_OUTPUT_INACTIVE) < 0) {
        printk("Error: Led configure failed\n");
        return -1;
    }
    printk("Leds initialized ok\n");
    return 0;
}

int init_button(void)
{
    if (!gpio_is_ready_dt(&button_0)) {
        return -1;
    }
    gpio_pin_configure_dt(&button_0, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&button_0_data, button_0_handler, BIT(button_0.pin));
    gpio_add_callback(button_0.port, &button_0_data);
    return 0;
}

int init_uart(void)
{
    if (!device_is_ready(uart_dev)) {
        return 1;
    }
    return 0;
}

void uart_rx_task(void *unused1, void *unused2, void *unused3)
{
    printk("UART RX thread started\n");
    unsigned char rx_data = 0;
    char uart_msg[20];
    int uart_msg_cnt = 0;
    memset(uart_msg, 0, sizeof(uart_msg));

    while (true) {
        while (uart_poll_in(uart_dev, &rx_data) == 0) {
            if (rx_data != '\r' && rx_data != '\n') {
                if (uart_msg_cnt < sizeof(uart_msg) - 1) {
                    uart_msg[uart_msg_cnt++] = (char)rx_data;
                }
            } else {
                if (uart_msg_cnt > 0) {
                    uart_msg[uart_msg_cnt] = '\0';

                    struct data_t *buf = k_malloc(sizeof(struct data_t));
                    if (buf == NULL) {
                        printk("Error: Out of memory\n");
                    } else {
                        strncpy(buf->msg, uart_msg, sizeof(buf->msg) - 1);
                        buf->msg[sizeof(buf->msg) - 1] = '\0';
                        k_fifo_put(&data_fifo, buf);
                    }
                    uart_msg_cnt = 0;
                    memset(uart_msg, 0, sizeof(uart_msg));
                }
            }
        }
        k_msleep(1);
    }
}

static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
    printk("Dispatcher thread started\n");

    while (true) {
        struct data_t *rec_item = k_fifo_get(&data_fifo, K_FOREVER);
        if (rec_item == NULL) {
            continue;
        }

        char sequence[21];
        strncpy(sequence, rec_item->msg, sizeof(sequence) - 1);
        sequence[sizeof(sequence) - 1] = '\0';
        k_free(rec_item);

        char color = sequence[0];
        int duration = 1000;
        char *comma_ptr = strchr(sequence, ',');
        if (comma_ptr != NULL) {
            duration = atoi(comma_ptr + 1);
        }

        printk("Dispatcher -> Väri: %c, Kesto: %d ms\n", color, duration);

        while (system_paused) {
            k_msleep(100);
        }

        k_mutex_lock(&release_mutex, K_FOREVER);
        task_done = false;

        k_mutex_lock(&led_mutex, K_FOREVER);
        active_color = color;
        active_duration_ms = duration;

        if (color == 'R' || color == 'r') {
            k_condvar_signal(&cv_red);
        } else if (color == 'Y' || color == 'y') {
            k_condvar_signal(&cv_yellow);
        } else if (color == 'G' || color == 'g') {
            k_condvar_signal(&cv_green);
        } else {
            printk("Tuntematon väri: %c\n", color);
            active_color = 0;
            task_done = true;
        }
        k_mutex_unlock(&led_mutex);

        while (!task_done) {
            k_condvar_wait(&cv_release, &release_mutex, K_FOREVER);
        }
        k_mutex_unlock(&release_mutex);
    }
}

void red_led_task(void *p1, void *p2, void *p3)
{
    printk("Red led thread started\n");
    timing_t start_time, end_time;

    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        while (active_color != 'R' && active_color != 'r') {
            k_condvar_wait(&cv_red, &led_mutex, K_FOREVER);
        }
        int burn_time = active_duration_ms;
        active_color = 0;
        k_mutex_unlock(&led_mutex);

        /* Käynnistetään ajastin ennen aloitusaikaleimaa */
        timing_start();
        start_time = timing_counter_get();

        gpio_pin_set_dt(&red, 1);
        k_msleep(burn_time);
        gpio_pin_set_dt(&red, 0);

        /* Otetaan lopetusaikaleima ja pysäytetään ajastin heti */
        end_time = timing_counter_get();
        timing_stop();

        uint64_t cycles = timing_cycles_get(&start_time, &end_time);
        uint64_t freq = timing_freq_get();
        elapsed_time_us_red = (cycles * 1000000ULL) / freq;

        printk("Red LED ON duration: %llu us\n", elapsed_time_us_red);

        k_mutex_lock(&release_mutex, K_FOREVER);
        task_done = true;
        k_condvar_signal(&cv_release);
        k_mutex_unlock(&release_mutex);
    }
}

void yellow_led_task(void *p1, void *p2, void *p3)
{
    printk("Yellow led thread started\n");
    timing_t start_time, end_time;

    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        while (active_color != 'Y' && active_color != 'y') {
            k_condvar_wait(&cv_yellow, &led_mutex, K_FOREVER);
        }
        int burn_time = active_duration_ms;
        active_color = 0;
        k_mutex_unlock(&led_mutex);

        /* Käynnistetään ajastin ennen aloitusaikaleimaa */
        timing_start();
        start_time = timing_counter_get();

        gpio_pin_set_dt(&red, 1);
        gpio_pin_set_dt(&green, 1);
        k_msleep(burn_time);
        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);

        /* Otetaan lopetusaikaleima ja pysäytetään ajastin heti */
        end_time = timing_counter_get();
        timing_stop();

        uint64_t cycles = timing_cycles_get(&start_time, &end_time);
        uint64_t freq = timing_freq_get();
        elapsed_time_us_yellow = (cycles * 1000000ULL) / freq;

        printk("Yellow LED ON duration: %llu us\n", elapsed_time_us_yellow);

        k_mutex_lock(&release_mutex, K_FOREVER);
        task_done = true;
        k_condvar_signal(&cv_release);
        k_mutex_unlock(&release_mutex);
    }
}

void green_led_task(void *p1, void *p2, void *p3)
{
    printk("Green led thread started\n");
    timing_t start_time, end_time;

    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        while (active_color != 'G' && active_color != 'g') {
            k_condvar_wait(&cv_green, &led_mutex, K_FOREVER);
        }
        int burn_time = active_duration_ms;
        active_color = 0;
        k_mutex_unlock(&led_mutex);

        /* Käynnistetään ajastin ennen aloitusaikaleimaa */
        timing_start();
        start_time = timing_counter_get();

        gpio_pin_set_dt(&green, 1);
        k_msleep(burn_time);
        gpio_pin_set_dt(&green, 0);

        /* Otetaan lopetusaikaleima ja pysäytetään ajastin heti */
        end_time = timing_counter_get();
        timing_stop();

        uint64_t cycles = timing_cycles_get(&start_time, &end_time);
        uint64_t freq = timing_freq_get();
        elapsed_time_us_green = (cycles * 1000000ULL) / freq;

        printk("Green LED ON duration: %llu us\n", elapsed_time_us_green);

        uint64_t total_elapsed_time = elapsed_time_us_red + elapsed_time_us_yellow + elapsed_time_us_green;
        printk("Total elapsed time: %llu us\n", total_elapsed_time);

        k_mutex_lock(&release_mutex, K_FOREVER);
        task_done = true;
        k_condvar_signal(&cv_release);
        k_mutex_unlock(&release_mutex);
    }
}

void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    system_paused = !system_paused;
    printk("Button pressed! Paused = %d\n", system_paused);
}

void debug_task(void *, void *, void*)
{
}