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
twr_led_t led;

// Button instance
twr_button_t button;

// Thermometer instance
twr_tmp112_t tmp112;

// Temperature tag instance
twr_tag_temperature_t temperature;
event_param_t temperature_event_param = { .next_pub = 0 };

// Humidity tag instance
twr_tag_humidity_t humidity;
event_param_t humidity_event_param = { .next_pub = 0 };

// Barometer tag instance
twr_tag_barometer_t barometer;
event_param_t barometer_event_param = { .next_pub = 0 };

// VOC Instance
twr_tag_voc_lp_t tag_voc_lp;
event_param_t voc_lp_event_param = { .next_pub = 0 };

// CO2
event_param_t co2_event_param = { .next_pub = 0 };

// Pointer to GFX instance
twr_gfx_t *pgfx;

void calibration_task(void *param)
{
    (void) param;

    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    twr_module_co2_calibration(TWR_LP8_CALIBRATION_BACKGROUND_FILTERED);

    twr_scheduler_unregister(twr_scheduler_get_current_task_id());
}

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == TWR_BUTTON_EVENT_PRESS)
    {
        twr_led_pulse(&led, 100);
    }
    else if (event == TWR_BUTTON_EVENT_HOLD)
    {
        twr_led_set_mode(&led, TWR_LED_MODE_BLINK);

        twr_scheduler_register(calibration_task, NULL, twr_tick_get() + CALIBRATION_DELAY);
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    float voltage;

    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (twr_module_battery_get_voltage(&voltage))
        {
            twr_radio_pub_battery(&voltage);
        }
    }
}

void temperature_tag_event_handler(twr_tag_temperature_t *self, twr_tag_temperature_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == TWR_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        if (twr_tag_temperature_get_temperature_celsius(self, &value))
        {
            if ((fabsf(value - param->value) >= TEMPERATURE_TAG_PUB_VALUE_CHANGE) || (param->next_pub < twr_scheduler_get_spin_tick()))
            {
                twr_radio_pub_temperature(param->channel, &value);
                param->value = value;
                param->next_pub = twr_scheduler_get_spin_tick() + TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

void humidity_tag_event_handler(twr_tag_humidity_t *self, twr_tag_humidity_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != TWR_TAG_HUMIDITY_EVENT_UPDATE)
    {
        return;
    }

    if (twr_tag_humidity_get_humidity_percentage(self, &value))
    {
        if ((fabsf(value - param->value) >= HUMIDITY_TAG_PUB_VALUE_CHANGE) || (param->next_pub < twr_scheduler_get_spin_tick()))
        {
            twr_radio_pub_humidity(param->channel, &value);
            param->value = value;
            param->next_pub = twr_scheduler_get_spin_tick() + HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL;
        }
    }
}

#ifdef BAROMETER_ENABLED
void barometer_tag_event_handler(twr_tag_barometer_t *self, twr_tag_barometer_event_t event, void *event_param)
{
    float pascal;
    float meter;
    event_param_t *param = (event_param_t *)event_param;

    if (event != TWR_TAG_BAROMETER_EVENT_UPDATE)
    {
        return;
    }

    if (!twr_tag_barometer_get_pressure_pascal(self, &pascal))
    {
        return;
    }

    if ((fabsf(pascal - param->value) >= BAROMETER_TAG_PUB_VALUE_CHANGE) || (param->next_pub < twr_scheduler_get_spin_tick()))
    {

        if (!twr_tag_barometer_get_altitude_meter(self, &meter))
        {
            return;
        }

        twr_radio_pub_barometer(param->channel, &pascal, &meter);
        param->value = pascal;
        param->next_pub = twr_scheduler_get_spin_tick() + BAROMETER_TAG_PUB_NO_CHANGE_INTEVAL;
    }
}
#endif

void voc_lp_tag_event_handler(twr_tag_voc_lp_t *self, twr_tag_voc_lp_event_t event, void *event_param)
{
    uint16_t value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != TWR_TAG_VOC_LP_EVENT_UPDATE)
    {
        return;
    }

    if (!twr_tag_voc_lp_get_tvoc_ppb(self, &value))
    {
        return;
    }

    if ((fabsf(value - param->value) >= VOC_PUB_VALUE_CHANGE) || (param->next_pub < twr_scheduler_get_spin_tick()))
    {
        int voc_value = (int)value;
        twr_radio_pub_int("voc-lp/-/tvoc", &voc_value);
        param->value = value;
        param->next_pub = twr_scheduler_get_spin_tick() + VOC_PUB_NO_CHANGE_INTEVAL;
    }
}

void co2_event_handler(twr_module_co2_event_t event, void *event_param)
{
    event_param_t *param = (event_param_t *) event_param;
    float value;

    if (event == TWR_MODULE_CO2_EVENT_UPDATE)
    {
        if (twr_module_co2_get_concentration_ppm(&value))
        {
            if ((fabsf(value - param->value) >= CO2_PUB_VALUE_CHANGE) || (param->next_pub < twr_scheduler_get_spin_tick()))
            {
                twr_radio_pub_co2(&value);
                param->value = value;
                param->next_pub = twr_scheduler_get_spin_tick() + CO2_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

void switch_to_normal_mode_task(void *param)
{
    twr_tag_temperature_set_update_interval(&temperature, TEMPERATURE_UPDATE_NORMAL_INTERVAL);

    twr_tag_humidity_set_update_interval(&humidity, HUMIDITY_TAG_UPDATE_NORMAL_INTERVAL);

    twr_module_co2_set_update_interval(CO2_UPDATE_NORMAL_INTERVAL);

    twr_scheduler_unregister(twr_scheduler_get_current_task_id());
}

void lcd_task(void *param)
{
    twr_gfx_set_font(pgfx, &twr_font_ubuntu_15);

    twr_gfx_clear(pgfx);
    int y = 5;

    twr_gfx_set_font(pgfx, &twr_font_ubuntu_24);
    twr_gfx_printf(pgfx, 2, y, true, "%.1f°C %.0f%%", temperature_event_param.value, humidity_event_param.value);
    y = 30;

    #ifdef BAROMETER_ENABLED
    twr_gfx_set_font(pgfx, &twr_font_ubuntu_15);
    twr_gfx_printf(pgfx, 2, y, true, "Pressure %.1f hPa", barometer_event_param.value / 100);
    y += 25;
    #endif

    twr_gfx_set_font(pgfx, &twr_font_ubuntu_15);
    twr_gfx_printf(pgfx, 2, y, true, "CO2");
    twr_gfx_printf(pgfx, 95, y, true, "ppm");
    twr_gfx_set_font(pgfx, &twr_font_ubuntu_24);
    twr_gfx_printf(pgfx, 35, y, true, "%.0f", co2_event_param.value);
    y += 30;

    twr_gfx_set_font(pgfx, &twr_font_ubuntu_15);
    twr_gfx_printf(pgfx, 2, y, true, "VOC");
    twr_gfx_printf(pgfx, 95, y, true, "ppb");
    twr_gfx_set_font(pgfx, &twr_font_ubuntu_24);
    twr_gfx_printf(pgfx, 35, y, true, "%.0f", voc_lp_event_param.value);
    y += 30;

    twr_gfx_update(pgfx);

    twr_scheduler_plan_current_relative(LCD_REFRESH_PERIOD);
}

void application_init(void)
{
    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_hold_time(&button, 10000);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize temperature
    temperature_event_param.channel = TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;
    twr_tag_temperature_init(&temperature, TWR_I2C_I2C0, TWR_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT);
    twr_tag_temperature_set_update_interval(&temperature, TEMPERATURE_UPDATE_SERVICE_INTERVAL);
    twr_tag_temperature_set_event_handler(&temperature, temperature_tag_event_handler, &temperature_event_param);

    // Initialize humidity
    humidity_event_param.channel = TWR_RADIO_PUB_CHANNEL_R3_I2C0_ADDRESS_DEFAULT;
    twr_tag_humidity_init(&humidity, TWR_TAG_HUMIDITY_REVISION_R3, TWR_I2C_I2C0, TWR_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    twr_tag_humidity_set_update_interval(&humidity, HUMIDITY_TAG_UPDATE_SERVICE_INTERVAL);
    twr_tag_humidity_set_event_handler(&humidity, humidity_tag_event_handler, &humidity_event_param);

    #ifdef BAROMETER_ENABLED
    // Initialize barometer
    barometer_event_param.channel = TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;
    twr_tag_barometer_init(&barometer, TWR_I2C_I2C0);
    twr_tag_barometer_set_update_interval(&barometer, BAROMETER_TAG_UPDATE_SERVICE_INTERVAL);
    twr_tag_barometer_set_event_handler(&barometer, barometer_tag_event_handler, &barometer_event_param);
    #endif

    // Init VOC-LP
    twr_tag_voc_lp_init(&tag_voc_lp, TWR_I2C_I2C0);
    twr_tag_voc_lp_set_event_handler(&tag_voc_lp, voc_lp_tag_event_handler, &voc_lp_event_param);
    twr_tag_voc_lp_set_update_interval(&tag_voc_lp, VOC_TAG_UPDATE_INTERVAL);

    // Initialize CO2
    twr_module_co2_init();
    twr_module_co2_set_update_interval(CO2_UPDATE_SERVICE_INTERVAL);
    twr_module_co2_set_event_handler(co2_event_handler, &co2_event_param);

    twr_radio_pairing_request("co2-monitor", VERSION);

    twr_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_INTERVAL_INTERVAL);

    // LCD Module
    twr_module_lcd_init();
    pgfx = twr_module_lcd_get_gfx();
    // LCD Task
    twr_scheduler_register(lcd_task, NULL, 0);

    twr_led_pulse(&led, 2000);
}
