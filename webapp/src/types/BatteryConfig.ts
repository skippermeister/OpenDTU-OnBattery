export interface BatteryConfig {
    enabled: boolean;
    pollinterval: number;
    updatesonly: boolean;
    verbose_logging: boolean;
    provider: number;
    can_controller_frequency: number;
    jkbms_interface: number;
    jkbms_polling_interval: number;
    mqtt_topic: string;
    min_charge_temp: number;
    max_charge_temp: number;
    min_discharge_temp: number;
    max_discharge_temp: number;
}
