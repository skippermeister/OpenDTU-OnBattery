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
    mqtt_soc_json_path: string;
    mqtt_voltage_topic: string;
    mqtt_voltage_json_path: string;
    mqtt_voltage_unit: number;
    min_charge_temp: number;
    max_charge_temp: number;
    min_discharge_temp: number;
    max_discharge_temp: number;
    stop_charging_soc: number;
    recommended_charge_voltage: number;
    recommended_discharge_voltage: number;
}
