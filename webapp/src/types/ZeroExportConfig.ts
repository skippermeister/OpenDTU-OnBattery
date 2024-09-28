export interface ZeroExportInverterInfo {
    pos: number;
    name: string;
    //    poll_enable_day: boolean;
    //    poll_enable_night: boolean;
    //    command_enable_day: boolean;
    //    command_enable_night: boolean;
    type: string;
    selected: boolean;
}

// meta-data not directly part of the DPL settings,
// to control visibility of DPL settings
export interface ZeroExportMetaData {
    powerlimiter_inverter_serial: string;
    inverters: { [key: string]: ZeroExportInverterInfo };
}

export interface ZeroExportConfig {
    enabled: boolean;
    pollinterval: number;
    updatesonly: boolean;
    verbose_logging: boolean;
    InverterId: number;
    serials: string[];
    MaxGrid: number;
    PowerHysteresis: number;
    MinimumLimit: number;
    Tn: number;
}
