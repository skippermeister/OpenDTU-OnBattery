export interface HuaweiConfig {
    auto_power_enabled: boolean;
    auto_power_batterysoc_limits_enabled: boolean;
    voltage_limit: number;
    enable_voltage_limit: number;
    lower_power_limit: number;
    upper_power_limit: number;
    emergency_charge_enabled: boolean;
    stop_batterysoc_threshold: number;
    target_power_consumption: number;
}

export interface MeanwellConfig {
    pollinterval: number;
    min_voltage: number;
    max_voltage: number;
    min_current: number;
    max_current: number;
    hysteresis: number;
    EEPROMwrites: number;
    mustInverterProduce: boolean;
}

export interface AcChargerConfig {
  enabled: boolean;
  verbose_logging: boolean;
  updatesonly: boolean;
  chargerType: string;
  io_providername: string;

  can_controller_frequency: number;

  meanwell: MeanwellConfig;
  huawei: HuaweiConfig;
}
