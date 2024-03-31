export interface AcChargerConfig {
  enabled: boolean;
  can_controller_frequency: number;
  pollinterval: number;
  updatesonly: boolean;
  min_voltage: number;
  max_voltage: number;
  min_current: number;
  max_current: number;
  hysteresis: number;
  verbose_logging: boolean;
  EEPROMwrites: number;
  mustInverterProduce: boolean;
}
