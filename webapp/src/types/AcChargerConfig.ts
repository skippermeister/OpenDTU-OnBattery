export interface AcChargerConfig {
  enabled: boolean;
  verbose_logging: boolean;
  can_controller_frequency: number;
  pollinterval: number;
  updatesonly: boolean;
  min_voltage: number;
  max_voltage: number;
  min_current: number;
  max_current: number;
  hysteresis: number;
  EEPROMwrites: number;
  mustInverterProduce: boolean;
}
