#include <application.h>

#define TEMPERATURE_UPDATE_INTERVAL (5 * 1000)
#define TEMPERATURE_SEND_INTERVAL (30 * 1000)

// how often is send alarm if temperature is low, high or changed
#define ALARM_REPEAT_INTERVAL (15 * 1000)
// Sends temperature if limit is achived
#define LIMIT_LOW_TEMPERATURE 0.0f
#define LIMIT_HIGH_TEMPERATURE 4.0f
#define LIMIT_DELTA_TEMPERATURE 1.0f

// LED instance
bc_led_t led;

// On board thermometer
// http://www.ti.com/product/TMP112#
bc_tmp112_t temp;


bool send_alarm = true;
bc_scheduler_task_id_t send_alarm_task_id = 0;

bool send_temperature_measurement = true;
bc_scheduler_task_id_t send_regular_measurement_task_id = 0;


void allow_alarm()
{
    send_alarm = true;
}

void allow_send_temperature()
{
    send_temperature_measurement = true;
}

void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    if (event != BC_TMP112_EVENT_UPDATE)
    {
        return;
    }

    float value;

    // read meassured temperature
    if (!bc_tmp112_get_temperature_celsius(&temp, &value))
    {
        return;
    }

    // Temeprature is to low
    if ((value < LIMIT_LOW_TEMPERATURE) && send_alarm)
    {
        bc_radio_pub_float("low", &value);
        send_alarm = false;
        bc_scheduler_plan_from_now(send_alarm_task_id, ALARM_REPEAT_INTERVAL);
    }

    // Temeprature is to high
    if ((value > LIMIT_HIGH_TEMPERATURE) && send_alarm)
    {
        bc_radio_pub_float("high", &value);
        send_alarm = false;
        bc_scheduler_plan_from_now(send_alarm_task_id, ALARM_REPEAT_INTERVAL);
    }

    // Regular measurement
    if (send_temperature_measurement)
    {
        bc_radio_pub_temperature(0, &value);
        send_temperature_measurement = false;
        bc_scheduler_plan_from_now(send_regular_measurement_task_id, TEMPERATURE_SEND_INTERVAL);
    }

    char bufferTemp[13];
    sprintf(bufferTemp, "%05.2f", value);
    bufferTemp[12] = '\0';

    bc_module_lcd_clear();
    bc_module_lcd_draw_string(5, 30, &bufferTemp[0], true);
    bc_module_lcd_draw_string(100, 30, "°C", true);
    bc_module_lcd_update();

    bc_led_pulse(&led, 200);
}

void application_init(void)
{
    // Initialize radio
    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_ON);

    // Initialize LCD
    bc_module_lcd_init();
    bc_module_lcd_set_font(&bc_font_ubuntu_15);

    // Initialize TMP112 sensor
    bc_tmp112_init(&temp, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_event_handler(&temp, tmp112_event_handler, NULL);
    bc_tmp112_set_update_interval(&temp, TEMPERATURE_UPDATE_INTERVAL);

    // tasks controling sending intervals
    send_alarm_task_id = bc_scheduler_register(allow_alarm, NULL, bc_tick_get() + ALARM_REPEAT_INTERVAL);
    send_regular_measurement_task_id = bc_scheduler_register(allow_send_temperature, NULL, bc_tick_get() + TEMPERATURE_SEND_INTERVAL);

    bc_radio_pairing_request("fridge-monitoring", VERSION);

    bc_led_pulse(&led, 2000);
}
