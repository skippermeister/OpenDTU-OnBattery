export interface ZendureParameter {
    device_type: number;
    device_serial: string;
    soc_min: number;
    soc_max: number;
    bypass_mode: number;
    max_output: number;
}

export interface MqttParameter {
    soc_topic: string;
    soc_json_path: string;
    voltage_topic: string;
    voltage_json_path: string;
    voltage_unit: number;
    discharge_current_topic: string;
    discharge_current_json_path: string;
    amperage_unit: number;
}

export interface BatteryConfig {
    enabled: boolean;
    numberOfBatteries: number;
    pollinterval: number;
    updatesonly: boolean;
    verbose_logging: boolean;
    provider: number;
    io_providername: string;
    can_controller_frequency: number;

    min_charge_temp: number;
    max_charge_temp: number;
    min_discharge_temp: number;
    max_discharge_temp: number;
    stop_charging_soc: number;
    recommended_charge_voltage: number;
    recommended_discharge_voltage: number;

    enableDischargeCurrentLimit: boolean;
    dischargeCurrentLimit: number;
    useBatteryReportedDischargeCurrentLimit: boolean;

    mqtt: MqttParameter;
    zendure: ZendureParameter;
}
