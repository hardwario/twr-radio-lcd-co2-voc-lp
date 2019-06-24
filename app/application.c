#include <application.h>

#define SERVICE_INTERVAL_INTERVAL (60 * 60 * 1000)
#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)

#define TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define TEMPERATURE_TAG_PUB_VALUE_CHANGE 0.2f
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (5 * 1000)
#define TEMPERATURE_UPDATE_NORMAL_INTERVAL (10 * 1000)

#define HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define HUMIDITY_TAG_PUB_VALUE_CHANGE 5.0f
#define HUMIDITY_TAG_UPDATE_SERVICE_INTERVAL (5 * 1000)
#define HUMIDITY_TAG_UPDATE_NORMAL_INTERVAL (10 * 1000)

//#define BAROMETER_ENABLED

#ifdef BAROMETER_ENABLED
    #define BAROMETER_TAG_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
    #define BAROMETER_TAG_PUB_VALUE_CHANGE 20.0f
    #define BAROMETER_TAG_UPDATE_SERVICE_INTERVAL (1 * 60 * 1000)
    #define BAROMETER_TAG_UPDATE_NORMAL_INTERVAL  (5 * 60 * 1000)
#endif

#define VOC_TAG_UPDATE_INTERVAL (30 * 1000)
#define VOC_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define VOC_PUB_VALUE_CHANGE 50.0f

#define CO2_PUB_NO_CHANGE_INTERVAL (15 * 60 * 1000)
#define CO2_PUB_VALUE_CHANGE 50.0f
#define CO2_UPDATE_SERVICE_INTERVAL (1 * 60 * 1000)
#define CO2_UPDATE_NORMAL_INTERVAL  (5 * 60 * 1000)

#define CALIBRATION_DELAY (10 * 60 * 1000)
#define LCD_REFRESH_PERIOD (10 * 1000)


// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

// Thermometer instance
bc_tmp112_t tmp112;

// Temperature tag instance
bc_tag_temperature_t temperature;
event_param_t temperature_event_param = { .next_pub = 0 };

// Humidity tag instance
bc_tag_humidity_t humidity;
event_param_t humidity_event_param = { .next_pub = 0 };

// Barometer tag instance
bc_tag_barometer_t barometer;
event_param_t barometer_event_param = { .next_pub = 0 };

// VOC Instance
bc_tag_voc_lp_t tag_voc_lp;
event_param_t voc_lp_event_param = { .next_pub = 0 };

// CO2
event_param_t co2_event_param = { .next_pub = 0 };

// Pointer to GFX instance
bc_gfx_t *pgfx;

void calibration_task(void *param)
{
    (void) param;

    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_module_co2_calibration(BC_LP8_CALIBRATION_BACKGROUND_FILTERED);

    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_pulse(&led, 100);
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
        bc_led_set_mode(&led, BC_LED_MODE_BLINK);

        bc_scheduler_register(calibration_task, NULL, bc_tick_get() + CALIBRATION_DELAY);
    }
}

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    float voltage;

    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (bc_module_battery_get_voltage(&voltage))
        {
            bc_radio_pub_battery(&voltage);
        }
    }
}

void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == BC_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        if (bc_tag_temperature_get_temperature_celsius(self, &value))
        {
            if ((fabsf(value - param->value) >= TEMPERATURE_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_temperature(param->channel, &value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

void humidity_tag_event_handler(bc_tag_humidity_t *self, bc_tag_humidity_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_HUMIDITY_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_humidity_get_humidity_percentage(self, &value))
    {
        if ((fabsf(value - param->value) >= HUMIDITY_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
        {
            bc_radio_pub_humidity(param->channel, &value);
            param->value = value;
            param->next_pub = bc_scheduler_get_spin_tick() + HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL;
        }
    }
}

#ifdef BAROMETER_ENABLED
void barometer_tag_event_handler(bc_tag_barometer_t *self, bc_tag_barometer_event_t event, void *event_param)
{
    float pascal;
    float meter;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_BAROMETER_EVENT_UPDATE)
    {
        return;
    }

    if (!bc_tag_barometer_get_pressure_pascal(self, &pascal))
    {
        return;
    }

    if ((fabsf(pascal - param->value) >= BAROMETER_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
    {

        if (!bc_tag_barometer_get_altitude_meter(self, &meter))
        {
            return;
        }

        bc_radio_pub_barometer(param->channel, &pascal, &meter);
        param->value = pascal;
        param->next_pub = bc_scheduler_get_spin_tick() + BAROMETER_TAG_PUB_NO_CHANGE_INTEVAL;
    }
}
#endif

void voc_lp_tag_event_handler(bc_tag_voc_lp_t *self, bc_tag_voc_lp_event_t event, void *event_param)
{
    uint16_t value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_VOC_LP_EVENT_UPDATE)
    {
        return;
    }

    if (!bc_tag_voc_lp_get_tvoc_ppb(self, &value))
    {
        return;
    }

    if ((fabsf(value - param->value) >= VOC_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
    {
        int voc_value = (int)value;
        bc_radio_pub_int("voc-lp/-/tvoc", &voc_value);
        param->value = value;
        param->next_pub = bc_scheduler_get_spin_tick() + VOC_PUB_NO_CHANGE_INTEVAL;
    }
}

void co2_event_handler(bc_module_co2_event_t event, void *event_param)
{
    event_param_t *param = (event_param_t *) event_param;
    float value;

    if (event == BC_MODULE_CO2_EVENT_UPDATE)
    {
        if (bc_module_co2_get_concentration_ppm(&value))
        {
            if ((fabsf(value - param->value) >= CO2_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_co2(&value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + CO2_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

void switch_to_normal_mode_task(void *param)
{
    bc_tag_temperature_set_update_interval(&temperature, TEMPERATURE_UPDATE_NORMAL_INTERVAL);

    bc_tag_humidity_set_update_interval(&humidity, HUMIDITY_TAG_UPDATE_NORMAL_INTERVAL);

    bc_module_co2_set_update_interval(CO2_UPDATE_NORMAL_INTERVAL);

    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}

void lcd_task(void *param)
{
    bc_gfx_set_font(pgfx, &bc_font_ubuntu_15);

    bc_gfx_clear(pgfx);
    int y = 5;

    bc_gfx_set_font(pgfx, &bc_font_ubuntu_24);
    bc_gfx_printf(pgfx, 2, y, true, "%.1f°C %.0f%%", temperature_event_param.value, humidity_event_param.value);
    y = 30;

    #ifdef BAROMETER_ENABLED
    bc_gfx_set_font(pgfx, &bc_font_ubuntu_15);
    bc_gfx_printf(pgfx, 2, y, true, "Pressure %.1f hPa", barometer_event_param.value / 100);
    y += 25;
    #endif

    bc_gfx_set_font(pgfx, &bc_font_ubuntu_15);
    bc_gfx_printf(pgfx, 2, y, true, "CO2");
    bc_gfx_printf(pgfx, 95, y, true, "ppm");
    bc_gfx_set_font(pgfx, &bc_font_ubuntu_24);
    bc_gfx_printf(pgfx, 35, y, true, "%.0f", co2_event_param.value);
    y += 30;

    bc_gfx_set_font(pgfx, &bc_font_ubuntu_15);
    bc_gfx_printf(pgfx, 2, y, true, "VOC");
    bc_gfx_printf(pgfx, 95, y, true, "ppb");
    bc_gfx_set_font(pgfx, &bc_font_ubuntu_24);
    bc_gfx_printf(pgfx, 35, y, true, "%.0f", voc_lp_event_param.value);
    y += 30;

    bc_gfx_update(pgfx);

    bc_scheduler_plan_current_relative(LCD_REFRESH_PERIOD);
}

void application_init(void)
{
    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_hold_time(&button, 10000);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize temperature
    temperature_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;
    bc_tag_temperature_init(&temperature, BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT);
    bc_tag_temperature_set_update_interval(&temperature, TEMPERATURE_UPDATE_SERVICE_INTERVAL);
    bc_tag_temperature_set_event_handler(&temperature, temperature_tag_event_handler, &temperature_event_param);

    // Initialize humidity
    humidity_event_param.channel = BC_RADIO_PUB_CHANNEL_R3_I2C0_ADDRESS_DEFAULT;
    bc_tag_humidity_init(&humidity, BC_TAG_HUMIDITY_REVISION_R3, BC_I2C_I2C0, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    bc_tag_humidity_set_update_interval(&humidity, HUMIDITY_TAG_UPDATE_SERVICE_INTERVAL);
    bc_tag_humidity_set_event_handler(&humidity, humidity_tag_event_handler, &humidity_event_param);

    #ifdef BAROMETER_ENABLED
    // Initialize barometer
    barometer_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;
    bc_tag_barometer_init(&barometer, BC_I2C_I2C0);
    bc_tag_barometer_set_update_interval(&barometer, BAROMETER_TAG_UPDATE_SERVICE_INTERVAL);
    bc_tag_barometer_set_event_handler(&barometer, barometer_tag_event_handler, &barometer_event_param);
    #endif

    // Init VOC-LP
    bc_tag_voc_lp_init(&tag_voc_lp, BC_I2C_I2C0);
    bc_tag_voc_lp_set_event_handler(&tag_voc_lp, voc_lp_tag_event_handler, &voc_lp_event_param);
    bc_tag_voc_lp_set_update_interval(&tag_voc_lp, VOC_TAG_UPDATE_INTERVAL);

    // Initialize CO2
    bc_module_co2_init();
    bc_module_co2_set_update_interval(CO2_UPDATE_SERVICE_INTERVAL);
    bc_module_co2_set_event_handler(co2_event_handler, &co2_event_param);

    bc_radio_pairing_request("co2-monitor", VERSION);

    bc_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_INTERVAL_INTERVAL);

    // LCD Module
    bc_module_lcd_init();
    pgfx = bc_module_lcd_get_gfx();
    // LCD Task
    bc_scheduler_register(lcd_task, NULL, 0);

    bc_led_pulse(&led, 2000);
}
