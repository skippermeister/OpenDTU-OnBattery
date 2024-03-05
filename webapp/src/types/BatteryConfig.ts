export interface BatteryConfig {
    enabled: boolean;
    numberOfBatteries: number;
    pollinterval: number;
    updatesonly: boolean;
    verbose_logging: boolean;
    provider: number;
    can_controller_frequency: number;
    jkbms_interface: number;
    jkbms_polling_interval: number;
    mqtt_soc_topic: string;
    mqtt_voltage_topic: string;
    min_charge_temp: number;
    max_charge_temp: number;
    min_discharge_temp: number;
    max_discharge_temp: number;
}
